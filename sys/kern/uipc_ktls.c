/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014-2019 Netflix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/domainset.h>
#include <sys/endian.h>
#include <sys/ktls.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/refcount.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/kthread.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
#include <machine/pcb.h>
#endif
#include <machine/vmparam.h>
#include <net/if.h>
#include <net/if_var.h>
#ifdef RSS
#include <net/netisr.h>
#include <net/rss_config.h>
#endif
#include <net/route.h>
#include <net/route/nhop.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#include <opencrypto/cryptodev.h>
#include <opencrypto/ktls.h>
#include <vm/uma_dbg.h>
#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>
#include <vm/vm_pagequeue.h>

struct ktls_wq {
	struct mtx	mtx;
	STAILQ_HEAD(, mbuf) m_head;
	STAILQ_HEAD(, socket) so_head;
	bool		running;
	int		lastallocfail;
} __aligned(CACHE_LINE_SIZE);

struct ktls_reclaim_thread {
	uint64_t wakeups;
	uint64_t reclaims;
	struct thread *td;
	int running;
};

struct ktls_domain_info {
	int count;
	int cpu[MAXCPU];
	struct ktls_reclaim_thread reclaim_td;
};

struct ktls_domain_info ktls_domains[MAXMEMDOM];
static struct ktls_wq *ktls_wq;
static struct proc *ktls_proc;
static uma_zone_t ktls_session_zone;
static uma_zone_t ktls_buffer_zone;
static uint16_t ktls_cpuid_lookup[MAXCPU];
static int ktls_init_state;
static struct sx ktls_init_lock;
SX_SYSINIT(ktls_init_lock, &ktls_init_lock, "ktls init");

SYSCTL_NODE(_kern_ipc, OID_AUTO, tls, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Kernel TLS offload");
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, stats, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Kernel TLS offload stats");

#ifdef RSS
static int ktls_bind_threads = 1;
#else
static int ktls_bind_threads;
#endif
SYSCTL_INT(_kern_ipc_tls, OID_AUTO, bind_threads, CTLFLAG_RDTUN,
    &ktls_bind_threads, 0,
    "Bind crypto threads to cores (1) or cores and domains (2) at boot");

static u_int ktls_maxlen = 16384;
SYSCTL_UINT(_kern_ipc_tls, OID_AUTO, maxlen, CTLFLAG_RDTUN,
    &ktls_maxlen, 0, "Maximum TLS record size");

static int ktls_number_threads;
SYSCTL_INT(_kern_ipc_tls_stats, OID_AUTO, threads, CTLFLAG_RD,
    &ktls_number_threads, 0,
    "Number of TLS threads in thread-pool");

unsigned int ktls_ifnet_max_rexmit_pct = 2;
SYSCTL_UINT(_kern_ipc_tls, OID_AUTO, ifnet_max_rexmit_pct, CTLFLAG_RWTUN,
    &ktls_ifnet_max_rexmit_pct, 2,
    "Max percent bytes retransmitted before ifnet TLS is disabled");

static bool ktls_offload_enable;
SYSCTL_BOOL(_kern_ipc_tls, OID_AUTO, enable, CTLFLAG_RWTUN,
    &ktls_offload_enable, 0,
    "Enable support for kernel TLS offload");

static bool ktls_cbc_enable = true;
SYSCTL_BOOL(_kern_ipc_tls, OID_AUTO, cbc_enable, CTLFLAG_RWTUN,
    &ktls_cbc_enable, 1,
    "Enable support of AES-CBC crypto for kernel TLS");

static bool ktls_sw_buffer_cache = true;
SYSCTL_BOOL(_kern_ipc_tls, OID_AUTO, sw_buffer_cache, CTLFLAG_RDTUN,
    &ktls_sw_buffer_cache, 1,
    "Enable caching of output buffers for SW encryption");

static int ktls_max_reclaim = 1024;
SYSCTL_INT(_kern_ipc_tls, OID_AUTO, max_reclaim, CTLFLAG_RWTUN,
    &ktls_max_reclaim, 128,
    "Max number of 16k buffers to reclaim in thread context");

static COUNTER_U64_DEFINE_EARLY(ktls_tasks_active);
SYSCTL_COUNTER_U64(_kern_ipc_tls, OID_AUTO, tasks_active, CTLFLAG_RD,
    &ktls_tasks_active, "Number of active tasks");

static COUNTER_U64_DEFINE_EARLY(ktls_cnt_tx_pending);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, sw_tx_pending, CTLFLAG_RD,
    &ktls_cnt_tx_pending,
    "Number of TLS 1.0 records waiting for earlier TLS records");

static COUNTER_U64_DEFINE_EARLY(ktls_cnt_tx_queued);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, sw_tx_inqueue, CTLFLAG_RD,
    &ktls_cnt_tx_queued,
    "Number of TLS records in queue to tasks for SW encryption");

static COUNTER_U64_DEFINE_EARLY(ktls_cnt_rx_queued);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, sw_rx_inqueue, CTLFLAG_RD,
    &ktls_cnt_rx_queued,
    "Number of TLS sockets in queue to tasks for SW decryption");

static COUNTER_U64_DEFINE_EARLY(ktls_offload_total);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, offload_total,
    CTLFLAG_RD, &ktls_offload_total,
    "Total successful TLS setups (parameters set)");

static COUNTER_U64_DEFINE_EARLY(ktls_offload_enable_calls);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, enable_calls,
    CTLFLAG_RD, &ktls_offload_enable_calls,
    "Total number of TLS enable calls made");

static COUNTER_U64_DEFINE_EARLY(ktls_offload_active);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, active, CTLFLAG_RD,
    &ktls_offload_active, "Total Active TLS sessions");

static COUNTER_U64_DEFINE_EARLY(ktls_offload_corrupted_records);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, corrupted_records, CTLFLAG_RD,
    &ktls_offload_corrupted_records, "Total corrupted TLS records received");

static COUNTER_U64_DEFINE_EARLY(ktls_offload_failed_crypto);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, failed_crypto, CTLFLAG_RD,
    &ktls_offload_failed_crypto, "Total TLS crypto failures");

static COUNTER_U64_DEFINE_EARLY(ktls_switch_to_ifnet);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_to_ifnet, CTLFLAG_RD,
    &ktls_switch_to_ifnet, "TLS sessions switched from SW to ifnet");

static COUNTER_U64_DEFINE_EARLY(ktls_switch_to_sw);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_to_sw, CTLFLAG_RD,
    &ktls_switch_to_sw, "TLS sessions switched from ifnet to SW");

static COUNTER_U64_DEFINE_EARLY(ktls_switch_failed);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_failed, CTLFLAG_RD,
    &ktls_switch_failed, "TLS sessions unable to switch between SW and ifnet");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_disable_fail);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, ifnet_disable_failed, CTLFLAG_RD,
    &ktls_ifnet_disable_fail, "TLS sessions unable to switch to SW from ifnet");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_disable_ok);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, ifnet_disable_ok, CTLFLAG_RD,
    &ktls_ifnet_disable_ok, "TLS sessions able to switch to SW from ifnet");

static COUNTER_U64_DEFINE_EARLY(ktls_destroy_task);
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, destroy_task, CTLFLAG_RD,
    &ktls_destroy_task,
    "Number of times ktls session was destroyed via taskqueue");

SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, sw, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Software TLS session stats");
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, ifnet, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Hardware (ifnet) TLS session stats");
#ifdef TCP_OFFLOAD
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, toe, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "TOE TLS session stats");
#endif

static COUNTER_U64_DEFINE_EARLY(ktls_sw_cbc);
SYSCTL_COUNTER_U64(_kern_ipc_tls_sw, OID_AUTO, cbc, CTLFLAG_RD, &ktls_sw_cbc,
    "Active number of software TLS sessions using AES-CBC");

static COUNTER_U64_DEFINE_EARLY(ktls_sw_gcm);
SYSCTL_COUNTER_U64(_kern_ipc_tls_sw, OID_AUTO, gcm, CTLFLAG_RD, &ktls_sw_gcm,
    "Active number of software TLS sessions using AES-GCM");

static COUNTER_U64_DEFINE_EARLY(ktls_sw_chacha20);
SYSCTL_COUNTER_U64(_kern_ipc_tls_sw, OID_AUTO, chacha20, CTLFLAG_RD,
    &ktls_sw_chacha20,
    "Active number of software TLS sessions using Chacha20-Poly1305");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_cbc);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, cbc, CTLFLAG_RD,
    &ktls_ifnet_cbc,
    "Active number of ifnet TLS sessions using AES-CBC");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_gcm);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, gcm, CTLFLAG_RD,
    &ktls_ifnet_gcm,
    "Active number of ifnet TLS sessions using AES-GCM");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_chacha20);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, chacha20, CTLFLAG_RD,
    &ktls_ifnet_chacha20,
    "Active number of ifnet TLS sessions using Chacha20-Poly1305");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_reset);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset, CTLFLAG_RD,
    &ktls_ifnet_reset, "TLS sessions updated to a new ifnet send tag");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_reset_dropped);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset_dropped, CTLFLAG_RD,
    &ktls_ifnet_reset_dropped,
    "TLS sessions dropped after failing to update ifnet send tag");

static COUNTER_U64_DEFINE_EARLY(ktls_ifnet_reset_failed);
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset_failed, CTLFLAG_RD,
    &ktls_ifnet_reset_failed,
    "TLS sessions that failed to allocate a new ifnet send tag");

static int ktls_ifnet_permitted;
SYSCTL_UINT(_kern_ipc_tls_ifnet, OID_AUTO, permitted, CTLFLAG_RWTUN,
    &ktls_ifnet_permitted, 1,
    "Whether to permit hardware (ifnet) TLS sessions");

#ifdef TCP_OFFLOAD
static COUNTER_U64_DEFINE_EARLY(ktls_toe_cbc);
SYSCTL_COUNTER_U64(_kern_ipc_tls_toe, OID_AUTO, cbc, CTLFLAG_RD,
    &ktls_toe_cbc,
    "Active number of TOE TLS sessions using AES-CBC");

static COUNTER_U64_DEFINE_EARLY(ktls_toe_gcm);
SYSCTL_COUNTER_U64(_kern_ipc_tls_toe, OID_AUTO, gcm, CTLFLAG_RD,
    &ktls_toe_gcm,
    "Active number of TOE TLS sessions using AES-GCM");

static COUNTER_U64_DEFINE_EARLY(ktls_toe_chacha20);
SYSCTL_COUNTER_U64(_kern_ipc_tls_toe, OID_AUTO, chacha20, CTLFLAG_RD,
    &ktls_toe_chacha20,
    "Active number of TOE TLS sessions using Chacha20-Poly1305");
#endif

static MALLOC_DEFINE(M_KTLS, "ktls", "Kernel TLS");

static void ktls_reset_receive_tag(void *context, int pending);
static void ktls_reset_send_tag(void *context, int pending);
static void ktls_work_thread(void *ctx);
static void ktls_reclaim_thread(void *ctx);

static u_int
ktls_get_cpu(struct socket *so)
{
	struct inpcb *inp;
#ifdef NUMA
	struct ktls_domain_info *di;
#endif
	u_int cpuid;

	inp = sotoinpcb(so);
#ifdef RSS
	cpuid = rss_hash2cpuid(inp->inp_flowid, inp->inp_flowtype);
	if (cpuid != NETISR_CPUID_NONE)
		return (cpuid);
#endif
	/*
	 * Just use the flowid to shard connections in a repeatable
	 * fashion.  Note that TLS 1.0 sessions rely on the
	 * serialization provided by having the same connection use
	 * the same queue.
	 */
#ifdef NUMA
	if (ktls_bind_threads > 1 && inp->inp_numa_domain != M_NODOM) {
		di = &ktls_domains[inp->inp_numa_domain];
		cpuid = di->cpu[inp->inp_flowid % di->count];
	} else
#endif
		cpuid = ktls_cpuid_lookup[inp->inp_flowid % ktls_number_threads];
	return (cpuid);
}

static int
ktls_buffer_import(void *arg, void **store, int count, int domain, int flags)
{
	vm_page_t m;
	int i, req;

	KASSERT((ktls_maxlen & PAGE_MASK) == 0,
	    ("%s: ktls max length %d is not page size-aligned",
	    __func__, ktls_maxlen));

	req = VM_ALLOC_WIRED | VM_ALLOC_NODUMP | malloc2vm_flags(flags);
	for (i = 0; i < count; i++) {
		m = vm_page_alloc_noobj_contig_domain(domain, req,
		    atop(ktls_maxlen), 0, ~0ul, PAGE_SIZE, 0,
		    VM_MEMATTR_DEFAULT);
		if (m == NULL)
			break;
		store[i] = (void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	}
	return (i);
}

static void
ktls_buffer_release(void *arg __unused, void **store, int count)
{
	vm_page_t m;
	int i, j;

	for (i = 0; i < count; i++) {
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)store[i]));
		for (j = 0; j < atop(ktls_maxlen); j++) {
			(void)vm_page_unwire_noq(m + j);
			vm_page_free(m + j);
		}
	}
}

static void
ktls_free_mext_contig(struct mbuf *m)
{
	M_ASSERTEXTPG(m);
	uma_zfree(ktls_buffer_zone, (void *)PHYS_TO_DMAP(m->m_epg_pa[0]));
}

static int
ktls_init(void)
{
	struct thread *td;
	struct pcpu *pc;
	int count, domain, error, i;

	ktls_wq = malloc(sizeof(*ktls_wq) * (mp_maxid + 1), M_KTLS,
	    M_WAITOK | M_ZERO);

	ktls_session_zone = uma_zcreate("ktls_session",
	    sizeof(struct ktls_session),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_CACHE, 0);

	if (ktls_sw_buffer_cache) {
		ktls_buffer_zone = uma_zcache_create("ktls_buffers",
		    roundup2(ktls_maxlen, PAGE_SIZE), NULL, NULL, NULL, NULL,
		    ktls_buffer_import, ktls_buffer_release, NULL,
		    UMA_ZONE_FIRSTTOUCH);
	}

	/*
	 * Initialize the workqueues to run the TLS work.  We create a
	 * work queue for each CPU.
	 */
	CPU_FOREACH(i) {
		STAILQ_INIT(&ktls_wq[i].m_head);
		STAILQ_INIT(&ktls_wq[i].so_head);
		mtx_init(&ktls_wq[i].mtx, "ktls work queue", NULL, MTX_DEF);
		if (ktls_bind_threads > 1) {
			pc = pcpu_find(i);
			domain = pc->pc_domain;
			count = ktls_domains[domain].count;
			ktls_domains[domain].cpu[count] = i;
			ktls_domains[domain].count++;
		}
		ktls_cpuid_lookup[ktls_number_threads] = i;
		ktls_number_threads++;
	}

	/*
	 * If we somehow have an empty domain, fall back to choosing
	 * among all KTLS threads.
	 */
	if (ktls_bind_threads > 1) {
		for (i = 0; i < vm_ndomains; i++) {
			if (ktls_domains[i].count == 0) {
				ktls_bind_threads = 1;
				break;
			}
		}
	}

	/* Start kthreads for each workqueue. */
	CPU_FOREACH(i) {
		error = kproc_kthread_add(ktls_work_thread, &ktls_wq[i],
		    &ktls_proc, &td, 0, 0, "KTLS", "thr_%d", i);
		if (error) {
			printf("Can't add KTLS thread %d error %d\n", i, error);
			return (error);
		}
	}

	/*
	 * Start an allocation thread per-domain to perform blocking allocations
	 * of 16k physically contiguous TLS crypto destination buffers.
	 */
	if (ktls_sw_buffer_cache) {
		for (domain = 0; domain < vm_ndomains; domain++) {
			if (VM_DOMAIN_EMPTY(domain))
				continue;
			if (CPU_EMPTY(&cpuset_domain[domain]))
				continue;
			error = kproc_kthread_add(ktls_reclaim_thread,
			    &ktls_domains[domain], &ktls_proc,
			    &ktls_domains[domain].reclaim_td.td,
			    0, 0, "KTLS", "reclaim_%d", domain);
			if (error) {
				printf("Can't add KTLS reclaim thread %d error %d\n",
				    domain, error);
				return (error);
			}
		}
	}

	if (bootverbose)
		printf("KTLS: Initialized %d threads\n", ktls_number_threads);
	return (0);
}

static int
ktls_start_kthreads(void)
{
	int error, state;

start:
	state = atomic_load_acq_int(&ktls_init_state);
	if (__predict_true(state > 0))
		return (0);
	if (state < 0)
		return (ENXIO);

	sx_xlock(&ktls_init_lock);
	if (ktls_init_state != 0) {
		sx_xunlock(&ktls_init_lock);
		goto start;
	}

	error = ktls_init();
	if (error == 0)
		state = 1;
	else
		state = -1;
	atomic_store_rel_int(&ktls_init_state, state);
	sx_xunlock(&ktls_init_lock);
	return (error);
}

static int
ktls_create_session(struct socket *so, struct tls_enable *en,
    struct ktls_session **tlsp, int direction)
{
	struct ktls_session *tls;
	int error;

	/* Only TLS 1.0 - 1.3 are supported. */
	if (en->tls_vmajor != TLS_MAJOR_VER_ONE)
		return (EINVAL);
	if (en->tls_vminor < TLS_MINOR_VER_ZERO ||
	    en->tls_vminor > TLS_MINOR_VER_THREE)
		return (EINVAL);

	if (en->auth_key_len < 0 || en->auth_key_len > TLS_MAX_PARAM_SIZE)
		return (EINVAL);
	if (en->cipher_key_len < 0 || en->cipher_key_len > TLS_MAX_PARAM_SIZE)
		return (EINVAL);
	if (en->iv_len < 0 || en->iv_len > sizeof(tls->params.iv))
		return (EINVAL);

	/* All supported algorithms require a cipher key. */
	if (en->cipher_key_len == 0)
		return (EINVAL);

	/* No flags are currently supported. */
	if (en->flags != 0)
		return (EINVAL);

	/* Common checks for supported algorithms. */
	switch (en->cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		/*
		 * auth_algorithm isn't used, but permit GMAC values
		 * for compatibility.
		 */
		switch (en->auth_algorithm) {
		case 0:
#ifdef COMPAT_FREEBSD12
		/* XXX: Really 13.0-current COMPAT. */
		case CRYPTO_AES_128_NIST_GMAC:
		case CRYPTO_AES_192_NIST_GMAC:
		case CRYPTO_AES_256_NIST_GMAC:
#endif
			break;
		default:
			return (EINVAL);
		}
		if (en->auth_key_len != 0)
			return (EINVAL);
		switch (en->tls_vminor) {
		case TLS_MINOR_VER_TWO:
			if (en->iv_len != TLS_AEAD_GCM_LEN)
				return (EINVAL);
			break;
		case TLS_MINOR_VER_THREE:
			if (en->iv_len != TLS_1_3_GCM_IV_LEN)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CRYPTO_AES_CBC:
		switch (en->auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			break;
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
			if (en->tls_vminor != TLS_MINOR_VER_TWO)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		if (en->auth_key_len == 0)
			return (EINVAL);

		/*
		 * TLS 1.0 requires an implicit IV.  TLS 1.1 and 1.2
		 * use explicit IVs.
		 */
		switch (en->tls_vminor) {
		case TLS_MINOR_VER_ZERO:
			if (en->iv_len != TLS_CBC_IMPLICIT_IV_LEN)
				return (EINVAL);
			break;
		case TLS_MINOR_VER_ONE:
		case TLS_MINOR_VER_TWO:
			/* Ignore any supplied IV. */
			en->iv_len = 0;
			break;
		default:
			return (EINVAL);
		}
		break;
	case CRYPTO_CHACHA20_POLY1305:
		if (en->auth_algorithm != 0 || en->auth_key_len != 0)
			return (EINVAL);
		if (en->tls_vminor != TLS_MINOR_VER_TWO &&
		    en->tls_vminor != TLS_MINOR_VER_THREE)
			return (EINVAL);
		if (en->iv_len != TLS_CHACHA20_IV_LEN)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	error = ktls_start_kthreads();
	if (error != 0)
		return (error);

	tls = uma_zalloc(ktls_session_zone, M_WAITOK | M_ZERO);

	counter_u64_add(ktls_offload_active, 1);

	refcount_init(&tls->refcount, 1);
	if (direction == KTLS_RX) {
		TASK_INIT(&tls->reset_tag_task, 0, ktls_reset_receive_tag, tls);
	} else {
		TASK_INIT(&tls->reset_tag_task, 0, ktls_reset_send_tag, tls);
		tls->inp = so->so_pcb;
		in_pcbref(tls->inp);
		tls->tx = true;
	}

	tls->wq_index = ktls_get_cpu(so);

	tls->params.cipher_algorithm = en->cipher_algorithm;
	tls->params.auth_algorithm = en->auth_algorithm;
	tls->params.tls_vmajor = en->tls_vmajor;
	tls->params.tls_vminor = en->tls_vminor;
	tls->params.flags = en->flags;
	tls->params.max_frame_len = min(TLS_MAX_MSG_SIZE_V10_2, ktls_maxlen);

	/* Set the header and trailer lengths. */
	tls->params.tls_hlen = sizeof(struct tls_record_layer);
	switch (en->cipher_algorithm) {
	case CRYPTO_AES_NIST_GCM_16:
		/*
		 * TLS 1.2 uses a 4 byte implicit IV with an explicit 8 byte
		 * nonce.  TLS 1.3 uses a 12 byte implicit IV.
		 */
		if (en->tls_vminor < TLS_MINOR_VER_THREE)
			tls->params.tls_hlen += sizeof(uint64_t);
		tls->params.tls_tlen = AES_GMAC_HASH_LEN;
		tls->params.tls_bs = 1;
		break;
	case CRYPTO_AES_CBC:
		switch (en->auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			if (en->tls_vminor == TLS_MINOR_VER_ZERO) {
				/* Implicit IV, no nonce. */
				tls->sequential_records = true;
				tls->next_seqno = be64dec(en->rec_seq);
				STAILQ_INIT(&tls->pending_records);
			} else {
				tls->params.tls_hlen += AES_BLOCK_LEN;
			}
			tls->params.tls_tlen = AES_BLOCK_LEN +
			    SHA1_HASH_LEN;
			break;
		case CRYPTO_SHA2_256_HMAC:
			tls->params.tls_hlen += AES_BLOCK_LEN;
			tls->params.tls_tlen = AES_BLOCK_LEN +
			    SHA2_256_HASH_LEN;
			break;
		case CRYPTO_SHA2_384_HMAC:
			tls->params.tls_hlen += AES_BLOCK_LEN;
			tls->params.tls_tlen = AES_BLOCK_LEN +
			    SHA2_384_HASH_LEN;
			break;
		default:
			panic("invalid hmac");
		}
		tls->params.tls_bs = AES_BLOCK_LEN;
		break;
	case CRYPTO_CHACHA20_POLY1305:
		/*
		 * Chacha20 uses a 12 byte implicit IV.
		 */
		tls->params.tls_tlen = POLY1305_HASH_LEN;
		tls->params.tls_bs = 1;
		break;
	default:
		panic("invalid cipher");
	}

	/*
	 * TLS 1.3 includes optional padding which we do not support,
	 * and also puts the "real" record type at the end of the
	 * encrypted data.
	 */
	if (en->tls_vminor == TLS_MINOR_VER_THREE)
		tls->params.tls_tlen += sizeof(uint8_t);

	KASSERT(tls->params.tls_hlen <= MBUF_PEXT_HDR_LEN,
	    ("TLS header length too long: %d", tls->params.tls_hlen));
	KASSERT(tls->params.tls_tlen <= MBUF_PEXT_TRAIL_LEN,
	    ("TLS trailer length too long: %d", tls->params.tls_tlen));

	if (en->auth_key_len != 0) {
		tls->params.auth_key_len = en->auth_key_len;
		tls->params.auth_key = malloc(en->auth_key_len, M_KTLS,
		    M_WAITOK);
		error = copyin(en->auth_key, tls->params.auth_key,
		    en->auth_key_len);
		if (error)
			goto out;
	}

	tls->params.cipher_key_len = en->cipher_key_len;
	tls->params.cipher_key = malloc(en->cipher_key_len, M_KTLS, M_WAITOK);
	error = copyin(en->cipher_key, tls->params.cipher_key,
	    en->cipher_key_len);
	if (error)
		goto out;

	/*
	 * This holds the implicit portion of the nonce for AEAD
	 * ciphers and the initial implicit IV for TLS 1.0.  The
	 * explicit portions of the IV are generated in ktls_frame().
	 */
	if (en->iv_len != 0) {
		tls->params.iv_len = en->iv_len;
		error = copyin(en->iv, tls->params.iv, en->iv_len);
		if (error)
			goto out;

		/*
		 * For TLS 1.2 with GCM, generate an 8-byte nonce as a
		 * counter to generate unique explicit IVs.
		 *
		 * Store this counter in the last 8 bytes of the IV
		 * array so that it is 8-byte aligned.
		 */
		if (en->cipher_algorithm == CRYPTO_AES_NIST_GCM_16 &&
		    en->tls_vminor == TLS_MINOR_VER_TWO)
			arc4rand(tls->params.iv + 8, sizeof(uint64_t), 0);
	}

	*tlsp = tls;
	return (0);

out:
	ktls_free(tls);
	return (error);
}

static struct ktls_session *
ktls_clone_session(struct ktls_session *tls, int direction)
{
	struct ktls_session *tls_new;

	tls_new = uma_zalloc(ktls_session_zone, M_WAITOK | M_ZERO);

	counter_u64_add(ktls_offload_active, 1);

	refcount_init(&tls_new->refcount, 1);
	if (direction == KTLS_RX) {
		TASK_INIT(&tls_new->reset_tag_task, 0, ktls_reset_receive_tag,
		    tls_new);
	} else {
		TASK_INIT(&tls_new->reset_tag_task, 0, ktls_reset_send_tag,
		    tls_new);
		tls_new->inp = tls->inp;
		tls_new->tx = true;
		in_pcbref(tls_new->inp);
	}

	/* Copy fields from existing session. */
	tls_new->params = tls->params;
	tls_new->wq_index = tls->wq_index;

	/* Deep copy keys. */
	if (tls_new->params.auth_key != NULL) {
		tls_new->params.auth_key = malloc(tls->params.auth_key_len,
		    M_KTLS, M_WAITOK);
		memcpy(tls_new->params.auth_key, tls->params.auth_key,
		    tls->params.auth_key_len);
	}

	tls_new->params.cipher_key = malloc(tls->params.cipher_key_len, M_KTLS,
	    M_WAITOK);
	memcpy(tls_new->params.cipher_key, tls->params.cipher_key,
	    tls->params.cipher_key_len);

	return (tls_new);
}

#ifdef TCP_OFFLOAD
static int
ktls_try_toe(struct socket *so, struct ktls_session *tls, int direction)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	inp = so->so_pcb;
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_socket == NULL) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);
	if (!(tp->t_flags & TF_TOE)) {
		INP_WUNLOCK(inp);
		return (EOPNOTSUPP);
	}

	error = tcp_offload_alloc_tls_session(tp, tls, direction);
	INP_WUNLOCK(inp);
	if (error == 0) {
		tls->mode = TCP_TLS_MODE_TOE;
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_toe_cbc, 1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_toe_gcm, 1);
			break;
		case CRYPTO_CHACHA20_POLY1305:
			counter_u64_add(ktls_toe_chacha20, 1);
			break;
		}
	}
	return (error);
}
#endif

/*
 * Common code used when first enabling ifnet TLS on a connection or
 * when allocating a new ifnet TLS session due to a routing change.
 * This function allocates a new TLS send tag on whatever interface
 * the connection is currently routed over.
 */
static int
ktls_alloc_snd_tag(struct inpcb *inp, struct ktls_session *tls, bool force,
    struct m_snd_tag **mstp)
{
	union if_snd_tag_alloc_params params;
	struct ifnet *ifp;
	struct nhop_object *nh;
	struct tcpcb *tp;
	int error;

	INP_RLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_socket == NULL) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);

	/*
	 * Check administrative controls on ifnet TLS to determine if
	 * ifnet TLS should be denied.
	 *
	 * - Always permit 'force' requests.
	 * - ktls_ifnet_permitted == 0: always deny.
	 */
	if (!force && ktls_ifnet_permitted == 0) {
		INP_RUNLOCK(inp);
		return (ENXIO);
	}

	/*
	 * XXX: Use the cached route in the inpcb to find the
	 * interface.  This should perhaps instead use
	 * rtalloc1_fib(dst, 0, 0, fibnum).  Since KTLS is only
	 * enabled after a connection has completed key negotiation in
	 * userland, the cached route will be present in practice.
	 */
	nh = inp->inp_route.ro_nh;
	if (nh == NULL) {
		INP_RUNLOCK(inp);
		return (ENXIO);
	}
	ifp = nh->nh_ifp;
	if_ref(ifp);

	/*
	 * Allocate a TLS + ratelimit tag if the connection has an
	 * existing pacing rate.
	 */
	if (tp->t_pacing_rate != -1 &&
	    (if_getcapenable(ifp) & IFCAP_TXTLS_RTLMT) != 0) {
		params.hdr.type = IF_SND_TAG_TYPE_TLS_RATE_LIMIT;
		params.tls_rate_limit.inp = inp;
		params.tls_rate_limit.tls = tls;
		params.tls_rate_limit.max_rate = tp->t_pacing_rate;
	} else {
		params.hdr.type = IF_SND_TAG_TYPE_TLS;
		params.tls.inp = inp;
		params.tls.tls = tls;
	}
	params.hdr.flowid = inp->inp_flowid;
	params.hdr.flowtype = inp->inp_flowtype;
	params.hdr.numa_domain = inp->inp_numa_domain;
	INP_RUNLOCK(inp);

	if ((if_getcapenable(ifp) & IFCAP_MEXTPG) == 0) {
		error = EOPNOTSUPP;
		goto out;
	}
	if (inp->inp_vflag & INP_IPV6) {
		if ((if_getcapenable(ifp) & IFCAP_TXTLS6) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	} else {
		if ((if_getcapenable(ifp) & IFCAP_TXTLS4) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	}
	error = m_snd_tag_alloc(ifp, &params, mstp);
out:
	if_rele(ifp);
	return (error);
}

/*
 * Allocate an initial TLS receive tag for doing HW decryption of TLS
 * data.
 *
 * This function allocates a new TLS receive tag on whatever interface
 * the connection is currently routed over.  If the connection ends up
 * using a different interface for receive this will get fixed up via
 * ktls_input_ifp_mismatch as future packets arrive.
 */
static int
ktls_alloc_rcv_tag(struct inpcb *inp, struct ktls_session *tls,
    struct m_snd_tag **mstp)
{
	union if_snd_tag_alloc_params params;
	struct ifnet *ifp;
	struct nhop_object *nh;
	int error;

	if (!ktls_ocf_recrypt_supported(tls))
		return (ENXIO);

	INP_RLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_socket == NULL) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}

	/*
	 * Check administrative controls on ifnet TLS to determine if
	 * ifnet TLS should be denied.
	 */
	if (ktls_ifnet_permitted == 0) {
		INP_RUNLOCK(inp);
		return (ENXIO);
	}

	/*
	 * XXX: As with ktls_alloc_snd_tag, use the cached route in
	 * the inpcb to find the interface.
	 */
	nh = inp->inp_route.ro_nh;
	if (nh == NULL) {
		INP_RUNLOCK(inp);
		return (ENXIO);
	}
	ifp = nh->nh_ifp;
	if_ref(ifp);
	tls->rx_ifp = ifp;

	params.hdr.type = IF_SND_TAG_TYPE_TLS_RX;
	params.hdr.flowid = inp->inp_flowid;
	params.hdr.flowtype = inp->inp_flowtype;
	params.hdr.numa_domain = inp->inp_numa_domain;
	params.tls_rx.inp = inp;
	params.tls_rx.tls = tls;
	params.tls_rx.vlan_id = 0;

	INP_RUNLOCK(inp);

	if (inp->inp_vflag & INP_IPV6) {
		if ((if_getcapenable2(ifp) & IFCAP2_RXTLS6) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	} else {
		if ((if_getcapenable2(ifp) & IFCAP2_RXTLS4) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	}
	error = m_snd_tag_alloc(ifp, &params, mstp);

	/*
	 * If this connection is over a vlan, vlan_snd_tag_alloc
	 * rewrites vlan_id with the saved interface.  Save the VLAN
	 * ID for use in ktls_reset_receive_tag which allocates new
	 * receive tags directly from the leaf interface bypassing
	 * if_vlan.
	 */
	if (error == 0)
		tls->rx_vlan_id = params.tls_rx.vlan_id;
out:
	return (error);
}

static int
ktls_try_ifnet(struct socket *so, struct ktls_session *tls, int direction,
    bool force)
{
	struct m_snd_tag *mst;
	int error;

	switch (direction) {
	case KTLS_TX:
		error = ktls_alloc_snd_tag(so->so_pcb, tls, force, &mst);
		if (__predict_false(error != 0))
			goto done;
		break;
	case KTLS_RX:
		KASSERT(!force, ("%s: forced receive tag", __func__));
		error = ktls_alloc_rcv_tag(so->so_pcb, tls, &mst);
		if (__predict_false(error != 0))
			goto done;
		break;
	default:
		__assert_unreachable();
	}

	tls->mode = TCP_TLS_MODE_IFNET;
	tls->snd_tag = mst;

	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		counter_u64_add(ktls_ifnet_cbc, 1);
		break;
	case CRYPTO_AES_NIST_GCM_16:
		counter_u64_add(ktls_ifnet_gcm, 1);
		break;
	case CRYPTO_CHACHA20_POLY1305:
		counter_u64_add(ktls_ifnet_chacha20, 1);
		break;
	default:
		break;
	}
done:
	return (error);
}

static void
ktls_use_sw(struct ktls_session *tls)
{
	tls->mode = TCP_TLS_MODE_SW;
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		counter_u64_add(ktls_sw_cbc, 1);
		break;
	case CRYPTO_AES_NIST_GCM_16:
		counter_u64_add(ktls_sw_gcm, 1);
		break;
	case CRYPTO_CHACHA20_POLY1305:
		counter_u64_add(ktls_sw_chacha20, 1);
		break;
	}
}

static int
ktls_try_sw(struct socket *so, struct ktls_session *tls, int direction)
{
	int error;

	error = ktls_ocf_try(so, tls, direction);
	if (error)
		return (error);
	ktls_use_sw(tls);
	return (0);
}

/*
 * KTLS RX stores data in the socket buffer as a list of TLS records,
 * where each record is stored as a control message containg the TLS
 * header followed by data mbufs containing the decrypted data.  This
 * is different from KTLS TX which always uses an mb_ext_pgs mbuf for
 * both encrypted and decrypted data.  TLS records decrypted by a NIC
 * should be queued to the socket buffer as records, but encrypted
 * data which needs to be decrypted by software arrives as a stream of
 * regular mbufs which need to be converted.  In addition, there may
 * already be pending encrypted data in the socket buffer when KTLS RX
 * is enabled.
 *
 * To manage not-yet-decrypted data for KTLS RX, the following scheme
 * is used:
 *
 * - A single chain of NOTREADY mbufs is hung off of sb_mtls.
 *
 * - ktls_check_rx checks this chain of mbufs reading the TLS header
 *   from the first mbuf.  Once all of the data for that TLS record is
 *   queued, the socket is queued to a worker thread.
 *
 * - The worker thread calls ktls_decrypt to decrypt TLS records in
 *   the TLS chain.  Each TLS record is detached from the TLS chain,
 *   decrypted, and inserted into the regular socket buffer chain as
 *   record starting with a control message holding the TLS header and
 *   a chain of mbufs holding the encrypted data.
 */

static void
sb_mark_notready(struct sockbuf *sb)
{
	struct mbuf *m;

	m = sb->sb_mb;
	sb->sb_mtls = m;
	sb->sb_mb = NULL;
	sb->sb_mbtail = NULL;
	sb->sb_lastrecord = NULL;
	for (; m != NULL; m = m->m_next) {
		KASSERT(m->m_nextpkt == NULL, ("%s: m_nextpkt != NULL",
		    __func__));
		KASSERT((m->m_flags & M_NOTAVAIL) == 0, ("%s: mbuf not avail",
		    __func__));
		KASSERT(sb->sb_acc >= m->m_len, ("%s: sb_acc < m->m_len",
		    __func__));
		m->m_flags |= M_NOTREADY;
		sb->sb_acc -= m->m_len;
		sb->sb_tlscc += m->m_len;
		sb->sb_mtlstail = m;
	}
	KASSERT(sb->sb_acc == 0 && sb->sb_tlscc == sb->sb_ccc,
	    ("%s: acc %u tlscc %u ccc %u", __func__, sb->sb_acc, sb->sb_tlscc,
	    sb->sb_ccc));
}

/*
 * Return information about the pending TLS data in a socket
 * buffer.  On return, 'seqno' is set to the sequence number
 * of the next TLS record to be received, 'resid' is set to
 * the amount of bytes still needed for the last pending
 * record.  The function returns 'false' if the last pending
 * record contains a partial TLS header.  In that case, 'resid'
 * is the number of bytes needed to complete the TLS header.
 */
bool
ktls_pending_rx_info(struct sockbuf *sb, uint64_t *seqnop, size_t *residp)
{
	struct tls_record_layer hdr;
	struct mbuf *m;
	uint64_t seqno;
	size_t resid;
	u_int offset, record_len;

	SOCKBUF_LOCK_ASSERT(sb);
	MPASS(sb->sb_flags & SB_TLS_RX);
	seqno = sb->sb_tls_seqno;
	resid = sb->sb_tlscc;
	m = sb->sb_mtls;
	offset = 0;

	if (resid == 0) {
		*seqnop = seqno;
		*residp = 0;
		return (true);
	}

	for (;;) {
		seqno++;

		if (resid < sizeof(hdr)) {
			*seqnop = seqno;
			*residp = sizeof(hdr) - resid;
			return (false);
		}

		m_copydata(m, offset, sizeof(hdr), (void *)&hdr);

		record_len = sizeof(hdr) + ntohs(hdr.tls_length);
		if (resid <= record_len) {
			*seqnop = seqno;
			*residp = record_len - resid;
			return (true);
		}
		resid -= record_len;

		while (record_len != 0) {
			if (m->m_len - offset > record_len) {
				offset += record_len;
				break;
			}

			record_len -= (m->m_len - offset);
			offset = 0;
			m = m->m_next;
		}
	}
}

int
ktls_enable_rx(struct socket *so, struct tls_enable *en)
{
	struct ktls_session *tls;
	int error;

	if (!ktls_offload_enable)
		return (ENOTSUP);

	counter_u64_add(ktls_offload_enable_calls, 1);

	/*
	 * This should always be true since only the TCP socket option
	 * invokes this function.
	 */
	if (so->so_proto->pr_protocol != IPPROTO_TCP)
		return (EINVAL);

	/*
	 * XXX: Don't overwrite existing sessions.  We should permit
	 * this to support rekeying in the future.
	 */
	if (so->so_rcv.sb_tls_info != NULL)
		return (EALREADY);

	if (en->cipher_algorithm == CRYPTO_AES_CBC && !ktls_cbc_enable)
		return (ENOTSUP);

	error = ktls_create_session(so, en, &tls, KTLS_RX);
	if (error)
		return (error);

	error = ktls_ocf_try(so, tls, KTLS_RX);
	if (error) {
		ktls_free(tls);
		return (error);
	}

	/* Mark the socket as using TLS offload. */
	SOCK_RECVBUF_LOCK(so);
	if (SOLISTENING(so)) {
		SOCK_RECVBUF_UNLOCK(so);
		ktls_free(tls);
		return (EINVAL);
	}
	so->so_rcv.sb_tls_seqno = be64dec(en->rec_seq);
	so->so_rcv.sb_tls_info = tls;
	so->so_rcv.sb_flags |= SB_TLS_RX;

	/* Mark existing data as not ready until it can be decrypted. */
	sb_mark_notready(&so->so_rcv);
	ktls_check_rx(&so->so_rcv);
	SOCK_RECVBUF_UNLOCK(so);

	/* Prefer TOE -> ifnet TLS -> software TLS. */
#ifdef TCP_OFFLOAD
	error = ktls_try_toe(so, tls, KTLS_RX);
	if (error)
#endif
		error = ktls_try_ifnet(so, tls, KTLS_RX, false);
	if (error)
		ktls_use_sw(tls);

	counter_u64_add(ktls_offload_total, 1);

	return (0);
}

int
ktls_enable_tx(struct socket *so, struct tls_enable *en)
{
	struct ktls_session *tls;
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	if (!ktls_offload_enable)
		return (ENOTSUP);

	counter_u64_add(ktls_offload_enable_calls, 1);

	/*
	 * This should always be true since only the TCP socket option
	 * invokes this function.
	 */
	if (so->so_proto->pr_protocol != IPPROTO_TCP)
		return (EINVAL);

	/*
	 * XXX: Don't overwrite existing sessions.  We should permit
	 * this to support rekeying in the future.
	 */
	if (so->so_snd.sb_tls_info != NULL)
		return (EALREADY);

	if (en->cipher_algorithm == CRYPTO_AES_CBC && !ktls_cbc_enable)
		return (ENOTSUP);

	/* TLS requires ext pgs */
	if (mb_use_ext_pgs == 0)
		return (ENXIO);

	error = ktls_create_session(so, en, &tls, KTLS_TX);
	if (error)
		return (error);

	/* Prefer TOE -> ifnet TLS -> software TLS. */
#ifdef TCP_OFFLOAD
	error = ktls_try_toe(so, tls, KTLS_TX);
	if (error)
#endif
		error = ktls_try_ifnet(so, tls, KTLS_TX, false);
	if (error)
		error = ktls_try_sw(so, tls, KTLS_TX);

	if (error) {
		ktls_free(tls);
		return (error);
	}

	/*
	 * Serialize with sosend_generic() and make sure that we're not
	 * operating on a listening socket.
	 */
	error = SOCK_IO_SEND_LOCK(so, SBL_WAIT);
	if (error) {
		ktls_free(tls);
		return (error);
	}

	/*
	 * Write lock the INP when setting sb_tls_info so that
	 * routines in tcp_ratelimit.c can read sb_tls_info while
	 * holding the INP lock.
	 */
	inp = so->so_pcb;
	INP_WLOCK(inp);
	SOCK_SENDBUF_LOCK(so);
	so->so_snd.sb_tls_seqno = be64dec(en->rec_seq);
	so->so_snd.sb_tls_info = tls;
	if (tls->mode != TCP_TLS_MODE_SW) {
		tp = intotcpcb(inp);
		MPASS(tp->t_nic_ktls_xmit == 0);
		tp->t_nic_ktls_xmit = 1;
		if (tp->t_fb->tfb_hwtls_change != NULL)
			(*tp->t_fb->tfb_hwtls_change)(tp, 1);
	}
	SOCK_SENDBUF_UNLOCK(so);
	INP_WUNLOCK(inp);
	SOCK_IO_SEND_UNLOCK(so);

	counter_u64_add(ktls_offload_total, 1);

	return (0);
}

int
ktls_get_rx_mode(struct socket *so, int *modep)
{
	struct ktls_session *tls;
	struct inpcb *inp __diagused;

	if (SOLISTENING(so))
		return (EINVAL);
	inp = so->so_pcb;
	INP_WLOCK_ASSERT(inp);
	SOCK_RECVBUF_LOCK(so);
	tls = so->so_rcv.sb_tls_info;
	if (tls == NULL)
		*modep = TCP_TLS_MODE_NONE;
	else
		*modep = tls->mode;
	SOCK_RECVBUF_UNLOCK(so);
	return (0);
}

/*
 * ktls_get_rx_sequence - get the next TCP- and TLS- sequence number.
 *
 * This function gets information about the next TCP- and TLS-
 * sequence number to be processed by the TLS receive worker
 * thread. The information is extracted from the given "inpcb"
 * structure. The values are stored in host endian format at the two
 * given output pointer locations. The TCP sequence number points to
 * the beginning of the TLS header.
 *
 * This function returns zero on success, else a non-zero error code
 * is returned.
 */
int
ktls_get_rx_sequence(struct inpcb *inp, uint32_t *tcpseq, uint64_t *tlsseq)
{
	struct socket *so;
	struct tcpcb *tp;

	INP_RLOCK(inp);
	so = inp->inp_socket;
	if (__predict_false(so == NULL)) {
		INP_RUNLOCK(inp);
		return (EINVAL);
	}
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}

	tp = intotcpcb(inp);
	MPASS(tp != NULL);

	SOCKBUF_LOCK(&so->so_rcv);
	*tcpseq = tp->rcv_nxt - so->so_rcv.sb_tlscc;
	*tlsseq = so->so_rcv.sb_tls_seqno;
	SOCKBUF_UNLOCK(&so->so_rcv);

	INP_RUNLOCK(inp);

	return (0);
}

int
ktls_get_tx_mode(struct socket *so, int *modep)
{
	struct ktls_session *tls;
	struct inpcb *inp __diagused;

	if (SOLISTENING(so))
		return (EINVAL);
	inp = so->so_pcb;
	INP_WLOCK_ASSERT(inp);
	SOCK_SENDBUF_LOCK(so);
	tls = so->so_snd.sb_tls_info;
	if (tls == NULL)
		*modep = TCP_TLS_MODE_NONE;
	else
		*modep = tls->mode;
	SOCK_SENDBUF_UNLOCK(so);
	return (0);
}

/*
 * Switch between SW and ifnet TLS sessions as requested.
 */
int
ktls_set_tx_mode(struct socket *so, int mode)
{
	struct ktls_session *tls, *tls_new;
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	if (SOLISTENING(so))
		return (EINVAL);
	switch (mode) {
	case TCP_TLS_MODE_SW:
	case TCP_TLS_MODE_IFNET:
		break;
	default:
		return (EINVAL);
	}

	inp = so->so_pcb;
	INP_WLOCK_ASSERT(inp);
	tp = intotcpcb(inp);

	if (mode == TCP_TLS_MODE_IFNET) {
		/* Don't allow enabling ifnet ktls multiple times */
		if (tp->t_nic_ktls_xmit)
			return (EALREADY);

		/*
		 * Don't enable ifnet ktls if we disabled it due to an
		 * excessive retransmission rate
		 */
		if (tp->t_nic_ktls_xmit_dis)
			return (ENXIO);
	}

	SOCKBUF_LOCK(&so->so_snd);
	tls = so->so_snd.sb_tls_info;
	if (tls == NULL) {
		SOCKBUF_UNLOCK(&so->so_snd);
		return (0);
	}

	if (tls->mode == mode) {
		SOCKBUF_UNLOCK(&so->so_snd);
		return (0);
	}

	tls = ktls_hold(tls);
	SOCKBUF_UNLOCK(&so->so_snd);
	INP_WUNLOCK(inp);

	tls_new = ktls_clone_session(tls, KTLS_TX);

	if (mode == TCP_TLS_MODE_IFNET)
		error = ktls_try_ifnet(so, tls_new, KTLS_TX, true);
	else
		error = ktls_try_sw(so, tls_new, KTLS_TX);
	if (error) {
		counter_u64_add(ktls_switch_failed, 1);
		ktls_free(tls_new);
		ktls_free(tls);
		INP_WLOCK(inp);
		return (error);
	}

	error = SOCK_IO_SEND_LOCK(so, SBL_WAIT);
	if (error) {
		counter_u64_add(ktls_switch_failed, 1);
		ktls_free(tls_new);
		ktls_free(tls);
		INP_WLOCK(inp);
		return (error);
	}

	/*
	 * If we raced with another session change, keep the existing
	 * session.
	 */
	if (tls != so->so_snd.sb_tls_info) {
		counter_u64_add(ktls_switch_failed, 1);
		SOCK_IO_SEND_UNLOCK(so);
		ktls_free(tls_new);
		ktls_free(tls);
		INP_WLOCK(inp);
		return (EBUSY);
	}

	INP_WLOCK(inp);
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_tls_info = tls_new;
	if (tls_new->mode != TCP_TLS_MODE_SW) {
		MPASS(tp->t_nic_ktls_xmit == 0);
		tp->t_nic_ktls_xmit = 1;
		if (tp->t_fb->tfb_hwtls_change != NULL)
			(*tp->t_fb->tfb_hwtls_change)(tp, 1);
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCK_IO_SEND_UNLOCK(so);

	/*
	 * Drop two references on 'tls'.  The first is for the
	 * ktls_hold() above.  The second drops the reference from the
	 * socket buffer.
	 */
	KASSERT(tls->refcount >= 2, ("too few references on old session"));
	ktls_free(tls);
	ktls_free(tls);

	if (mode == TCP_TLS_MODE_IFNET)
		counter_u64_add(ktls_switch_to_ifnet, 1);
	else
		counter_u64_add(ktls_switch_to_sw, 1);

	return (0);
}

/*
 * Try to allocate a new TLS receive tag.  This task is scheduled when
 * sbappend_ktls_rx detects an input path change.  If a new tag is
 * allocated, replace the tag in the TLS session.  If a new tag cannot
 * be allocated, let the session fall back to software decryption.
 */
static void
ktls_reset_receive_tag(void *context, int pending)
{
	union if_snd_tag_alloc_params params;
	struct ktls_session *tls;
	struct m_snd_tag *mst;
	struct inpcb *inp;
	struct ifnet *ifp;
	struct socket *so;
	int error;

	MPASS(pending == 1);

	tls = context;
	so = tls->so;
	inp = so->so_pcb;
	ifp = NULL;

	INP_RLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		goto out;
	}

	SOCKBUF_LOCK(&so->so_rcv);
	mst = tls->snd_tag;
	tls->snd_tag = NULL;
	if (mst != NULL)
		m_snd_tag_rele(mst);

	ifp = tls->rx_ifp;
	if_ref(ifp);
	SOCKBUF_UNLOCK(&so->so_rcv);

	params.hdr.type = IF_SND_TAG_TYPE_TLS_RX;
	params.hdr.flowid = inp->inp_flowid;
	params.hdr.flowtype = inp->inp_flowtype;
	params.hdr.numa_domain = inp->inp_numa_domain;
	params.tls_rx.inp = inp;
	params.tls_rx.tls = tls;
	params.tls_rx.vlan_id = tls->rx_vlan_id;
	INP_RUNLOCK(inp);

	if (inp->inp_vflag & INP_IPV6) {
		if ((if_getcapenable2(ifp) & IFCAP2_RXTLS6) == 0)
			goto out;
	} else {
		if ((if_getcapenable2(ifp) & IFCAP2_RXTLS4) == 0)
			goto out;
	}

	error = m_snd_tag_alloc(ifp, &params, &mst);
	if (error == 0) {
		SOCKBUF_LOCK(&so->so_rcv);
		tls->snd_tag = mst;
		SOCKBUF_UNLOCK(&so->so_rcv);

		counter_u64_add(ktls_ifnet_reset, 1);
	} else {
		/*
		 * Just fall back to software decryption if a tag
		 * cannot be allocated leaving the connection intact.
		 * If a future input path change switches to another
		 * interface this connection will resume ifnet TLS.
		 */
		counter_u64_add(ktls_ifnet_reset_failed, 1);
	}

out:
	mtx_pool_lock(mtxpool_sleep, tls);
	tls->reset_pending = false;
	mtx_pool_unlock(mtxpool_sleep, tls);

	if (ifp != NULL)
		if_rele(ifp);
	sorele(so);
	ktls_free(tls);
}

/*
 * Try to allocate a new TLS send tag.  This task is scheduled when
 * ip_output detects a route change while trying to transmit a packet
 * holding a TLS record.  If a new tag is allocated, replace the tag
 * in the TLS session.  Subsequent packets on the connection will use
 * the new tag.  If a new tag cannot be allocated, drop the
 * connection.
 */
static void
ktls_reset_send_tag(void *context, int pending)
{
	struct epoch_tracker et;
	struct ktls_session *tls;
	struct m_snd_tag *old, *new;
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	MPASS(pending == 1);

	tls = context;
	inp = tls->inp;

	/*
	 * Free the old tag first before allocating a new one.
	 * ip[6]_output_send() will treat a NULL send tag the same as
	 * an ifp mismatch and drop packets until a new tag is
	 * allocated.
	 *
	 * Write-lock the INP when changing tls->snd_tag since
	 * ip[6]_output_send() holds a read-lock when reading the
	 * pointer.
	 */
	INP_WLOCK(inp);
	old = tls->snd_tag;
	tls->snd_tag = NULL;
	INP_WUNLOCK(inp);
	if (old != NULL)
		m_snd_tag_rele(old);

	error = ktls_alloc_snd_tag(inp, tls, true, &new);

	if (error == 0) {
		INP_WLOCK(inp);
		tls->snd_tag = new;
		mtx_pool_lock(mtxpool_sleep, tls);
		tls->reset_pending = false;
		mtx_pool_unlock(mtxpool_sleep, tls);
		INP_WUNLOCK(inp);

		counter_u64_add(ktls_ifnet_reset, 1);

		/*
		 * XXX: Should we kick tcp_output explicitly now that
		 * the send tag is fixed or just rely on timers?
		 */
	} else {
		NET_EPOCH_ENTER(et);
		INP_WLOCK(inp);
		if (!(inp->inp_flags & INP_DROPPED)) {
			tp = intotcpcb(inp);
			CURVNET_SET(inp->inp_vnet);
			tp = tcp_drop(tp, ECONNABORTED);
			CURVNET_RESTORE();
			if (tp != NULL)
				counter_u64_add(ktls_ifnet_reset_dropped, 1);
		}
		INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);

		counter_u64_add(ktls_ifnet_reset_failed, 1);

		/*
		 * Leave reset_pending true to avoid future tasks while
		 * the socket goes away.
		 */
	}

	ktls_free(tls);
}

void
ktls_input_ifp_mismatch(struct sockbuf *sb, struct ifnet *ifp)
{
	struct ktls_session *tls;
	struct socket *so;

	SOCKBUF_LOCK_ASSERT(sb);
	KASSERT(sb->sb_flags & SB_TLS_RX, ("%s: sockbuf %p isn't TLS RX",
	    __func__, sb));
	so = __containerof(sb, struct socket, so_rcv);

	tls = sb->sb_tls_info;
	if_rele(tls->rx_ifp);
	if_ref(ifp);
	tls->rx_ifp = ifp;

	/*
	 * See if we should schedule a task to update the receive tag for
	 * this session.
	 */
	mtx_pool_lock(mtxpool_sleep, tls);
	if (!tls->reset_pending) {
		(void) ktls_hold(tls);
		soref(so);
		tls->so = so;
		tls->reset_pending = true;
		taskqueue_enqueue(taskqueue_thread, &tls->reset_tag_task);
	}
	mtx_pool_unlock(mtxpool_sleep, tls);
}

int
ktls_output_eagain(struct inpcb *inp, struct ktls_session *tls)
{

	if (inp == NULL)
		return (ENOBUFS);

	INP_LOCK_ASSERT(inp);

	/*
	 * See if we should schedule a task to update the send tag for
	 * this session.
	 */
	mtx_pool_lock(mtxpool_sleep, tls);
	if (!tls->reset_pending) {
		(void) ktls_hold(tls);
		tls->reset_pending = true;
		taskqueue_enqueue(taskqueue_thread, &tls->reset_tag_task);
	}
	mtx_pool_unlock(mtxpool_sleep, tls);
	return (ENOBUFS);
}

#ifdef RATELIMIT
int
ktls_modify_txrtlmt(struct ktls_session *tls, uint64_t max_pacing_rate)
{
	union if_snd_tag_modify_params params = {
		.rate_limit.max_rate = max_pacing_rate,
		.rate_limit.flags = M_NOWAIT,
	};
	struct m_snd_tag *mst;

	/* Can't get to the inp, but it should be locked. */
	/* INP_LOCK_ASSERT(inp); */

	MPASS(tls->mode == TCP_TLS_MODE_IFNET);

	if (tls->snd_tag == NULL) {
		/*
		 * Resetting send tag, ignore this change.  The
		 * pending reset may or may not see this updated rate
		 * in the tcpcb.  If it doesn't, we will just lose
		 * this rate change.
		 */
		return (0);
	}

	mst = tls->snd_tag;

	MPASS(mst != NULL);
	MPASS(mst->sw->type == IF_SND_TAG_TYPE_TLS_RATE_LIMIT);

	return (mst->sw->snd_tag_modify(mst, &params));
}
#endif

static void
ktls_destroy_help(void *context, int pending __unused)
{
	ktls_destroy(context);
}

void
ktls_destroy(struct ktls_session *tls)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	bool wlocked;

	MPASS(tls->refcount == 0);

	inp = tls->inp;
	if (tls->tx) {
		wlocked = INP_WLOCKED(inp);
		if (!wlocked && !INP_TRY_WLOCK(inp)) {
			/*
			 * rwlocks read locks are anonymous, and there
			 * is no way to know if our current thread
			 * holds an rlock on the inp.  As a rough
			 * estimate, check to see if the thread holds
			 * *any* rlocks at all.  If it does not, then we
			 * know that we don't hold the inp rlock, and
			 * can safely take the wlock
			 */
			if (curthread->td_rw_rlocks == 0) {
				INP_WLOCK(inp);
			} else {
				/*
				 * We might hold the rlock, so let's
				 * do the destroy in a taskqueue
				 * context to avoid a potential
				 * deadlock.  This should be very
				 * rare.
				 */
				counter_u64_add(ktls_destroy_task, 1);
				TASK_INIT(&tls->destroy_task, 0,
				    ktls_destroy_help, tls);
				(void)taskqueue_enqueue(taskqueue_thread,
				    &tls->destroy_task);
				return;
			}
		}
	}

	if (tls->sequential_records) {
		struct mbuf *m, *n;
		int page_count;

		STAILQ_FOREACH_SAFE(m, &tls->pending_records, m_epg_stailq, n) {
			page_count = m->m_epg_enc_cnt;
			while (page_count > 0) {
				KASSERT(page_count >= m->m_epg_nrdy,
				    ("%s: too few pages", __func__));
				page_count -= m->m_epg_nrdy;
				m = m_free(m);
			}
		}
	}

	counter_u64_add(ktls_offload_active, -1);
	switch (tls->mode) {
	case TCP_TLS_MODE_SW:
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_sw_cbc, -1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_sw_gcm, -1);
			break;
		case CRYPTO_CHACHA20_POLY1305:
			counter_u64_add(ktls_sw_chacha20, -1);
			break;
		}
		break;
	case TCP_TLS_MODE_IFNET:
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_ifnet_cbc, -1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_ifnet_gcm, -1);
			break;
		case CRYPTO_CHACHA20_POLY1305:
			counter_u64_add(ktls_ifnet_chacha20, -1);
			break;
		}
		if (tls->snd_tag != NULL)
			m_snd_tag_rele(tls->snd_tag);
		if (tls->rx_ifp != NULL)
			if_rele(tls->rx_ifp);
		if (tls->tx) {
			INP_WLOCK_ASSERT(inp);
			tp = intotcpcb(inp);
			MPASS(tp->t_nic_ktls_xmit == 1);
			tp->t_nic_ktls_xmit = 0;
		}
		break;
#ifdef TCP_OFFLOAD
	case TCP_TLS_MODE_TOE:
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_toe_cbc, -1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_toe_gcm, -1);
			break;
		case CRYPTO_CHACHA20_POLY1305:
			counter_u64_add(ktls_toe_chacha20, -1);
			break;
		}
		break;
#endif
	}
	if (tls->ocf_session != NULL)
		ktls_ocf_free(tls);
	if (tls->params.auth_key != NULL) {
		zfree(tls->params.auth_key, M_KTLS);
		tls->params.auth_key = NULL;
		tls->params.auth_key_len = 0;
	}
	if (tls->params.cipher_key != NULL) {
		zfree(tls->params.cipher_key, M_KTLS);
		tls->params.cipher_key = NULL;
		tls->params.cipher_key_len = 0;
	}
	if (tls->tx) {
		INP_WLOCK_ASSERT(inp);
		if (!in_pcbrele_wlocked(inp) && !wlocked)
			INP_WUNLOCK(inp);
	}
	explicit_bzero(tls->params.iv, sizeof(tls->params.iv));

	uma_zfree(ktls_session_zone, tls);
}

void
ktls_seq(struct sockbuf *sb, struct mbuf *m)
{

	for (; m != NULL; m = m->m_next) {
		KASSERT((m->m_flags & M_EXTPG) != 0,
		    ("ktls_seq: mapped mbuf %p", m));

		m->m_epg_seqno = sb->sb_tls_seqno;
		sb->sb_tls_seqno++;
	}
}

/*
 * Add TLS framing (headers and trailers) to a chain of mbufs.  Each
 * mbuf in the chain must be an unmapped mbuf.  The payload of the
 * mbuf must be populated with the payload of each TLS record.
 *
 * The record_type argument specifies the TLS record type used when
 * populating the TLS header.
 *
 * The enq_count argument on return is set to the number of pages of
 * payload data for this entire chain that need to be encrypted via SW
 * encryption.  The returned value should be passed to ktls_enqueue
 * when scheduling encryption of this chain of mbufs.  To handle the
 * special case of empty fragments for TLS 1.0 sessions, an empty
 * fragment counts as one page.
 */
void
ktls_frame(struct mbuf *top, struct ktls_session *tls, int *enq_cnt,
    uint8_t record_type)
{
	struct tls_record_layer *tlshdr;
	struct mbuf *m;
	uint64_t *noncep;
	uint16_t tls_len;
	int maxlen __diagused;

	maxlen = tls->params.max_frame_len;
	*enq_cnt = 0;
	for (m = top; m != NULL; m = m->m_next) {
		/*
		 * All mbufs in the chain should be TLS records whose
		 * payload does not exceed the maximum frame length.
		 *
		 * Empty TLS 1.0 records are permitted when using CBC.
		 */
		KASSERT(m->m_len <= maxlen && m->m_len >= 0 &&
		    (m->m_len > 0 || ktls_permit_empty_frames(tls)),
		    ("ktls_frame: m %p len %d", m, m->m_len));

		/*
		 * TLS frames require unmapped mbufs to store session
		 * info.
		 */
		KASSERT((m->m_flags & M_EXTPG) != 0,
		    ("ktls_frame: mapped mbuf %p (top = %p)", m, top));

		tls_len = m->m_len;

		/* Save a reference to the session. */
		m->m_epg_tls = ktls_hold(tls);

		m->m_epg_hdrlen = tls->params.tls_hlen;
		m->m_epg_trllen = tls->params.tls_tlen;
		if (tls->params.cipher_algorithm == CRYPTO_AES_CBC) {
			int bs, delta;

			/*
			 * AES-CBC pads messages to a multiple of the
			 * block size.  Note that the padding is
			 * applied after the digest and the encryption
			 * is done on the "plaintext || mac || padding".
			 * At least one byte of padding is always
			 * present.
			 *
			 * Compute the final trailer length assuming
			 * at most one block of padding.
			 * tls->params.tls_tlen is the maximum
			 * possible trailer length (padding + digest).
			 * delta holds the number of excess padding
			 * bytes if the maximum were used.  Those
			 * extra bytes are removed.
			 */
			bs = tls->params.tls_bs;
			delta = (tls_len + tls->params.tls_tlen) & (bs - 1);
			m->m_epg_trllen -= delta;
		}
		m->m_len += m->m_epg_hdrlen + m->m_epg_trllen;

		/* Populate the TLS header. */
		tlshdr = (void *)m->m_epg_hdr;
		tlshdr->tls_vmajor = tls->params.tls_vmajor;

		/*
		 * TLS 1.3 masquarades as TLS 1.2 with a record type
		 * of TLS_RLTYPE_APP.
		 */
		if (tls->params.tls_vminor == TLS_MINOR_VER_THREE &&
		    tls->params.tls_vmajor == TLS_MAJOR_VER_ONE) {
			tlshdr->tls_vminor = TLS_MINOR_VER_TWO;
			tlshdr->tls_type = TLS_RLTYPE_APP;
			/* save the real record type for later */
			m->m_epg_record_type = record_type;
			m->m_epg_trail[0] = record_type;
		} else {
			tlshdr->tls_vminor = tls->params.tls_vminor;
			tlshdr->tls_type = record_type;
		}
		tlshdr->tls_length = htons(m->m_len - sizeof(*tlshdr));

		/*
		 * Store nonces / explicit IVs after the end of the
		 * TLS header.
		 *
		 * For GCM with TLS 1.2, an 8 byte nonce is copied
		 * from the end of the IV.  The nonce is then
		 * incremented for use by the next record.
		 *
		 * For CBC, a random nonce is inserted for TLS 1.1+.
		 */
		if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16 &&
		    tls->params.tls_vminor == TLS_MINOR_VER_TWO) {
			noncep = (uint64_t *)(tls->params.iv + 8);
			be64enc(tlshdr + 1, *noncep);
			(*noncep)++;
		} else if (tls->params.cipher_algorithm == CRYPTO_AES_CBC &&
		    tls->params.tls_vminor >= TLS_MINOR_VER_ONE)
			arc4rand(tlshdr + 1, AES_BLOCK_LEN, 0);

		/*
		 * When using SW encryption, mark the mbuf not ready.
		 * It will be marked ready via sbready() after the
		 * record has been encrypted.
		 *
		 * When using ifnet TLS, unencrypted TLS records are
		 * sent down the stack to the NIC.
		 */
		if (tls->mode == TCP_TLS_MODE_SW) {
			m->m_flags |= M_NOTREADY;
			if (__predict_false(tls_len == 0)) {
				/* TLS 1.0 empty fragment. */
				m->m_epg_nrdy = 1;
			} else
				m->m_epg_nrdy = m->m_epg_npgs;
			*enq_cnt += m->m_epg_nrdy;
		}
	}
}

bool
ktls_permit_empty_frames(struct ktls_session *tls)
{
	return (tls->params.cipher_algorithm == CRYPTO_AES_CBC &&
	    tls->params.tls_vminor == TLS_MINOR_VER_ZERO);
}

void
ktls_check_rx(struct sockbuf *sb)
{
	struct tls_record_layer hdr;
	struct ktls_wq *wq;
	struct socket *so;
	bool running;

	SOCKBUF_LOCK_ASSERT(sb);
	KASSERT(sb->sb_flags & SB_TLS_RX, ("%s: sockbuf %p isn't TLS RX",
	    __func__, sb));
	so = __containerof(sb, struct socket, so_rcv);

	if (sb->sb_flags & SB_TLS_RX_RUNNING)
		return;

	/* Is there enough queued for a TLS header? */
	if (sb->sb_tlscc < sizeof(hdr)) {
		if ((sb->sb_state & SBS_CANTRCVMORE) != 0 && sb->sb_tlscc != 0)
			so->so_error = EMSGSIZE;
		return;
	}

	m_copydata(sb->sb_mtls, 0, sizeof(hdr), (void *)&hdr);

	/* Is the entire record queued? */
	if (sb->sb_tlscc < sizeof(hdr) + ntohs(hdr.tls_length)) {
		if ((sb->sb_state & SBS_CANTRCVMORE) != 0)
			so->so_error = EMSGSIZE;
		return;
	}

	sb->sb_flags |= SB_TLS_RX_RUNNING;

	soref(so);
	wq = &ktls_wq[so->so_rcv.sb_tls_info->wq_index];
	mtx_lock(&wq->mtx);
	STAILQ_INSERT_TAIL(&wq->so_head, so, so_ktls_rx_list);
	running = wq->running;
	mtx_unlock(&wq->mtx);
	if (!running)
		wakeup(wq);
	counter_u64_add(ktls_cnt_rx_queued, 1);
}

static struct mbuf *
ktls_detach_record(struct sockbuf *sb, int len)
{
	struct mbuf *m, *n, *top;
	int remain;

	SOCKBUF_LOCK_ASSERT(sb);
	MPASS(len <= sb->sb_tlscc);

	/*
	 * If TLS chain is the exact size of the record,
	 * just grab the whole record.
	 */
	top = sb->sb_mtls;
	if (sb->sb_tlscc == len) {
		sb->sb_mtls = NULL;
		sb->sb_mtlstail = NULL;
		goto out;
	}

	/*
	 * While it would be nice to use m_split() here, we need
	 * to know exactly what m_split() allocates to update the
	 * accounting, so do it inline instead.
	 */
	remain = len;
	for (m = top; remain > m->m_len; m = m->m_next)
		remain -= m->m_len;

	/* Easy case: don't have to split 'm'. */
	if (remain == m->m_len) {
		sb->sb_mtls = m->m_next;
		if (sb->sb_mtls == NULL)
			sb->sb_mtlstail = NULL;
		m->m_next = NULL;
		goto out;
	}

	/*
	 * Need to allocate an mbuf to hold the remainder of 'm'.  Try
	 * with M_NOWAIT first.
	 */
	n = m_get(M_NOWAIT, MT_DATA);
	if (n == NULL) {
		/*
		 * Use M_WAITOK with socket buffer unlocked.  If
		 * 'sb_mtls' changes while the lock is dropped, return
		 * NULL to force the caller to retry.
		 */
		SOCKBUF_UNLOCK(sb);

		n = m_get(M_WAITOK, MT_DATA);

		SOCKBUF_LOCK(sb);
		if (sb->sb_mtls != top) {
			m_free(n);
			return (NULL);
		}
	}
	n->m_flags |= (m->m_flags & (M_NOTREADY | M_DECRYPTED));

	/* Store remainder in 'n'. */
	n->m_len = m->m_len - remain;
	if (m->m_flags & M_EXT) {
		n->m_data = m->m_data + remain;
		mb_dupcl(n, m);
	} else {
		bcopy(mtod(m, caddr_t) + remain, mtod(n, caddr_t), n->m_len);
	}

	/* Trim 'm' and update accounting. */
	m->m_len -= n->m_len;
	sb->sb_tlscc -= n->m_len;
	sb->sb_ccc -= n->m_len;

	/* Account for 'n'. */
	sballoc_ktls_rx(sb, n);

	/* Insert 'n' into the TLS chain. */
	sb->sb_mtls = n;
	n->m_next = m->m_next;
	if (sb->sb_mtlstail == m)
		sb->sb_mtlstail = n;

	/* Detach the record from the TLS chain. */
	m->m_next = NULL;

out:
	MPASS(m_length(top, NULL) == len);
	for (m = top; m != NULL; m = m->m_next)
		sbfree_ktls_rx(sb, m);
	sb->sb_tlsdcc = len;
	sb->sb_ccc += len;
	SBCHECK(sb);
	return (top);
}

/*
 * Determine the length of the trailing zero padding and find the real
 * record type in the byte before the padding.
 *
 * Walking the mbuf chain backwards is clumsy, so another option would
 * be to scan forwards remembering the last non-zero byte before the
 * trailer.  However, it would be expensive to scan the entire record.
 * Instead, find the last non-zero byte of each mbuf in the chain
 * keeping track of the relative offset of that nonzero byte.
 *
 * trail_len is the size of the MAC/tag on input and is set to the
 * size of the full trailer including padding and the record type on
 * return.
 */
static int
tls13_find_record_type(struct ktls_session *tls, struct mbuf *m, int tls_len,
    int *trailer_len, uint8_t *record_typep)
{
	char *cp;
	u_int digest_start, last_offset, m_len, offset;
	uint8_t record_type;

	digest_start = tls_len - *trailer_len;
	last_offset = 0;
	offset = 0;
	for (; m != NULL && offset < digest_start;
	     offset += m->m_len, m = m->m_next) {
		/* Don't look for padding in the tag. */
		m_len = min(digest_start - offset, m->m_len);
		cp = mtod(m, char *);

		/* Find last non-zero byte in this mbuf. */
		while (m_len > 0 && cp[m_len - 1] == 0)
			m_len--;
		if (m_len > 0) {
			record_type = cp[m_len - 1];
			last_offset = offset + m_len;
		}
	}
	if (last_offset < tls->params.tls_hlen)
		return (EBADMSG);

	*record_typep = record_type;
	*trailer_len = tls_len - last_offset + 1;
	return (0);
}

/*
 * Check if a mbuf chain is fully decrypted at the given offset and
 * length. Returns KTLS_MBUF_CRYPTO_ST_DECRYPTED if all data is
 * decrypted. KTLS_MBUF_CRYPTO_ST_MIXED if there is a mix of encrypted
 * and decrypted data. Else KTLS_MBUF_CRYPTO_ST_ENCRYPTED if all data
 * is encrypted.
 */
ktls_mbuf_crypto_st_t
ktls_mbuf_crypto_state(struct mbuf *mb, int offset, int len)
{
	int m_flags_ored = 0;
	int m_flags_anded = -1;

	for (; mb != NULL; mb = mb->m_next) {
		if (offset < mb->m_len)
			break;
		offset -= mb->m_len;
	}
	offset += len;

	for (; mb != NULL; mb = mb->m_next) {
		m_flags_ored |= mb->m_flags;
		m_flags_anded &= mb->m_flags;

		if (offset <= mb->m_len)
			break;
		offset -= mb->m_len;
	}
	MPASS(mb != NULL || offset == 0);

	if ((m_flags_ored ^ m_flags_anded) & M_DECRYPTED)
		return (KTLS_MBUF_CRYPTO_ST_MIXED);
	else
		return ((m_flags_ored & M_DECRYPTED) ?
		    KTLS_MBUF_CRYPTO_ST_DECRYPTED :
		    KTLS_MBUF_CRYPTO_ST_ENCRYPTED);
}

/*
 * ktls_resync_ifnet - get HW TLS RX back on track after packet loss
 */
static int
ktls_resync_ifnet(struct socket *so, uint32_t tls_len, uint64_t tls_rcd_num)
{
	union if_snd_tag_modify_params params;
	struct m_snd_tag *mst;
	struct inpcb *inp;
	struct tcpcb *tp;

	mst = so->so_rcv.sb_tls_info->snd_tag;
	if (__predict_false(mst == NULL))
		return (EINVAL);

	inp = sotoinpcb(so);
	if (__predict_false(inp == NULL))
		return (EINVAL);

	INP_RLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}

	tp = intotcpcb(inp);
	MPASS(tp != NULL);

	/* Get the TCP sequence number of the next valid TLS header. */
	SOCKBUF_LOCK(&so->so_rcv);
	params.tls_rx.tls_hdr_tcp_sn =
	    tp->rcv_nxt - so->so_rcv.sb_tlscc - tls_len;
	params.tls_rx.tls_rec_length = tls_len;
	params.tls_rx.tls_seq_number = tls_rcd_num;
	SOCKBUF_UNLOCK(&so->so_rcv);

	INP_RUNLOCK(inp);

	MPASS(mst->sw->type == IF_SND_TAG_TYPE_TLS_RX);
	return (mst->sw->snd_tag_modify(mst, &params));
}

static void
ktls_drop(struct socket *so, int error)
{
	struct epoch_tracker et;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp;

	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	if (!(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		CURVNET_SET(inp->inp_vnet);
		tp = tcp_drop(tp, error);
		CURVNET_RESTORE();
		if (tp != NULL)
			INP_WUNLOCK(inp);
	} else {
		so->so_error = error;
		SOCK_RECVBUF_LOCK(so);
		sorwakeup_locked(so);
		INP_WUNLOCK(inp);
	}
	NET_EPOCH_EXIT(et);
}

static void
ktls_decrypt(struct socket *so)
{
	char tls_header[MBUF_PEXT_HDR_LEN];
	struct ktls_session *tls;
	struct sockbuf *sb;
	struct tls_record_layer *hdr;
	struct tls_get_record tgr;
	struct mbuf *control, *data, *m;
	ktls_mbuf_crypto_st_t state;
	uint64_t seqno;
	int error, remain, tls_len, trail_len;
	bool tls13;
	uint8_t vminor, record_type;

	hdr = (struct tls_record_layer *)tls_header;
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	KASSERT(sb->sb_flags & SB_TLS_RX_RUNNING,
	    ("%s: socket %p not running", __func__, so));

	tls = sb->sb_tls_info;
	MPASS(tls != NULL);

	tls13 = (tls->params.tls_vminor == TLS_MINOR_VER_THREE);
	if (tls13)
		vminor = TLS_MINOR_VER_TWO;
	else
		vminor = tls->params.tls_vminor;
	for (;;) {
		/* Is there enough queued for a TLS header? */
		if (sb->sb_tlscc < tls->params.tls_hlen)
			break;

		m_copydata(sb->sb_mtls, 0, tls->params.tls_hlen, tls_header);
		tls_len = sizeof(*hdr) + ntohs(hdr->tls_length);

		if (hdr->tls_vmajor != tls->params.tls_vmajor ||
		    hdr->tls_vminor != vminor)
			error = EINVAL;
		else if (tls13 && hdr->tls_type != TLS_RLTYPE_APP)
			error = EINVAL;
		else if (tls_len < tls->params.tls_hlen || tls_len >
		    tls->params.tls_hlen + TLS_MAX_MSG_SIZE_V10_2 +
		    tls->params.tls_tlen)
			error = EMSGSIZE;
		else
			error = 0;
		if (__predict_false(error != 0)) {
			/*
			 * We have a corrupted record and are likely
			 * out of sync.  The connection isn't
			 * recoverable at this point, so abort it.
			 */
			SOCKBUF_UNLOCK(sb);
			counter_u64_add(ktls_offload_corrupted_records, 1);

			ktls_drop(so, error);
			goto deref;
		}

		/* Is the entire record queued? */
		if (sb->sb_tlscc < tls_len)
			break;

		/*
		 * Split out the portion of the mbuf chain containing
		 * this TLS record.
		 */
		data = ktls_detach_record(sb, tls_len);
		if (data == NULL)
			continue;
		MPASS(sb->sb_tlsdcc == tls_len);

		seqno = sb->sb_tls_seqno;
		sb->sb_tls_seqno++;
		SBCHECK(sb);
		SOCKBUF_UNLOCK(sb);

		/* get crypto state for this TLS record */
		state = ktls_mbuf_crypto_state(data, 0, tls_len);

		switch (state) {
		case KTLS_MBUF_CRYPTO_ST_MIXED:
			error = ktls_ocf_recrypt(tls, hdr, data, seqno);
			if (error)
				break;
			/* FALLTHROUGH */
		case KTLS_MBUF_CRYPTO_ST_ENCRYPTED:
			error = ktls_ocf_decrypt(tls, hdr, data, seqno,
			    &trail_len);
			if (__predict_true(error == 0)) {
				if (tls13) {
					error = tls13_find_record_type(tls, data,
					    tls_len, &trail_len, &record_type);
				} else {
					record_type = hdr->tls_type;
				}
			}
			break;
		case KTLS_MBUF_CRYPTO_ST_DECRYPTED:
			/*
			 * NIC TLS is only supported for AEAD
			 * ciphersuites which used a fixed sized
			 * trailer.
			 */
			if (tls13) {
				trail_len = tls->params.tls_tlen - 1;
				error = tls13_find_record_type(tls, data,
				    tls_len, &trail_len, &record_type);
			} else {
				trail_len = tls->params.tls_tlen;
				error = 0;
				record_type = hdr->tls_type;
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		if (error) {
			counter_u64_add(ktls_offload_failed_crypto, 1);

			SOCKBUF_LOCK(sb);
			if (sb->sb_tlsdcc == 0) {
				/*
				 * sbcut/drop/flush discarded these
				 * mbufs.
				 */
				m_freem(data);
				break;
			}

			/*
			 * Drop this TLS record's data, but keep
			 * decrypting subsequent records.
			 */
			sb->sb_ccc -= tls_len;
			sb->sb_tlsdcc = 0;

			if (error != EMSGSIZE)
				error = EBADMSG;
			CURVNET_SET(so->so_vnet);
			so->so_error = error;
			sorwakeup_locked(so);
			CURVNET_RESTORE();

			m_freem(data);

			SOCKBUF_LOCK(sb);
			continue;
		}

		/* Allocate the control mbuf. */
		memset(&tgr, 0, sizeof(tgr));
		tgr.tls_type = record_type;
		tgr.tls_vmajor = hdr->tls_vmajor;
		tgr.tls_vminor = hdr->tls_vminor;
		tgr.tls_length = htobe16(tls_len - tls->params.tls_hlen -
		    trail_len);
		control = sbcreatecontrol(&tgr, sizeof(tgr),
		    TLS_GET_RECORD, IPPROTO_TCP, M_WAITOK);

		SOCKBUF_LOCK(sb);
		if (sb->sb_tlsdcc == 0) {
			/* sbcut/drop/flush discarded these mbufs. */
			MPASS(sb->sb_tlscc == 0);
			m_freem(data);
			m_freem(control);
			break;
		}

		/*
		 * Clear the 'dcc' accounting in preparation for
		 * adding the decrypted record.
		 */
		sb->sb_ccc -= tls_len;
		sb->sb_tlsdcc = 0;
		SBCHECK(sb);

		/* If there is no payload, drop all of the data. */
		if (tgr.tls_length == htobe16(0)) {
			m_freem(data);
			data = NULL;
		} else {
			/* Trim header. */
			remain = tls->params.tls_hlen;
			while (remain > 0) {
				if (data->m_len > remain) {
					data->m_data += remain;
					data->m_len -= remain;
					break;
				}
				remain -= data->m_len;
				data = m_free(data);
			}

			/* Trim trailer and clear M_NOTREADY. */
			remain = be16toh(tgr.tls_length);
			m = data;
			for (m = data; remain > m->m_len; m = m->m_next) {
				m->m_flags &= ~(M_NOTREADY | M_DECRYPTED);
				remain -= m->m_len;
			}
			m->m_len = remain;
			m_freem(m->m_next);
			m->m_next = NULL;
			m->m_flags &= ~(M_NOTREADY | M_DECRYPTED);

			/* Set EOR on the final mbuf. */
			m->m_flags |= M_EOR;
		}

		sbappendcontrol_locked(sb, data, control, 0);

		if (__predict_false(state != KTLS_MBUF_CRYPTO_ST_DECRYPTED)) {
			sb->sb_flags |= SB_TLS_RX_RESYNC;
			SOCKBUF_UNLOCK(sb);
			ktls_resync_ifnet(so, tls_len, seqno);
			SOCKBUF_LOCK(sb);
		} else if (__predict_false(sb->sb_flags & SB_TLS_RX_RESYNC)) {
			sb->sb_flags &= ~SB_TLS_RX_RESYNC;
			SOCKBUF_UNLOCK(sb);
			ktls_resync_ifnet(so, 0, seqno);
			SOCKBUF_LOCK(sb);
		}
	}

	sb->sb_flags &= ~SB_TLS_RX_RUNNING;

	if ((sb->sb_state & SBS_CANTRCVMORE) != 0 && sb->sb_tlscc > 0)
		so->so_error = EMSGSIZE;

	sorwakeup_locked(so);

deref:
	SOCKBUF_UNLOCK_ASSERT(sb);

	CURVNET_SET(so->so_vnet);
	sorele(so);
	CURVNET_RESTORE();
}

void
ktls_enqueue_to_free(struct mbuf *m)
{
	struct ktls_wq *wq;
	bool running;

	/* Mark it for freeing. */
	m->m_epg_flags |= EPG_FLAG_2FREE;
	wq = &ktls_wq[m->m_epg_tls->wq_index];
	mtx_lock(&wq->mtx);
	STAILQ_INSERT_TAIL(&wq->m_head, m, m_epg_stailq);
	running = wq->running;
	mtx_unlock(&wq->mtx);
	if (!running)
		wakeup(wq);
}

static void *
ktls_buffer_alloc(struct ktls_wq *wq, struct mbuf *m)
{
	void *buf;
	int domain, running;

	if (m->m_epg_npgs <= 2)
		return (NULL);
	if (ktls_buffer_zone == NULL)
		return (NULL);
	if ((u_int)(ticks - wq->lastallocfail) < hz) {
		/*
		 * Rate-limit allocation attempts after a failure.
		 * ktls_buffer_import() will acquire a per-domain mutex to check
		 * the free page queues and may fail consistently if memory is
		 * fragmented.
		 */
		return (NULL);
	}
	buf = uma_zalloc(ktls_buffer_zone, M_NOWAIT | M_NORECLAIM);
	if (buf == NULL) {
		domain = PCPU_GET(domain);
		wq->lastallocfail = ticks;

		/*
		 * Note that this check is "racy", but the races are
		 * harmless, and are either a spurious wakeup if
		 * multiple threads fail allocations before the alloc
		 * thread wakes, or waiting an extra second in case we
		 * see an old value of running == true.
		 */
		if (!VM_DOMAIN_EMPTY(domain)) {
			running = atomic_load_int(&ktls_domains[domain].reclaim_td.running);
			if (!running)
				wakeup(&ktls_domains[domain].reclaim_td);
		}
	}
	return (buf);
}

static int
ktls_encrypt_record(struct ktls_wq *wq, struct mbuf *m,
    struct ktls_session *tls, struct ktls_ocf_encrypt_state *state)
{
	vm_page_t pg;
	int error, i, len, off;

	KASSERT((m->m_flags & (M_EXTPG | M_NOTREADY)) == (M_EXTPG | M_NOTREADY),
	    ("%p not unready & nomap mbuf\n", m));
	KASSERT(ptoa(m->m_epg_npgs) <= ktls_maxlen,
	    ("page count %d larger than maximum frame length %d", m->m_epg_npgs,
	    ktls_maxlen));

	/* Anonymous mbufs are encrypted in place. */
	if ((m->m_epg_flags & EPG_FLAG_ANON) != 0)
		return (ktls_ocf_encrypt(state, tls, m, NULL, 0));

	/*
	 * For file-backed mbufs (from sendfile), anonymous wired
	 * pages are allocated and used as the encryption destination.
	 */
	if ((state->cbuf = ktls_buffer_alloc(wq, m)) != NULL) {
		len = ptoa(m->m_epg_npgs - 1) + m->m_epg_last_len -
		    m->m_epg_1st_off;
		state->dst_iov[0].iov_base = (char *)state->cbuf +
		    m->m_epg_1st_off;
		state->dst_iov[0].iov_len = len;
		state->parray[0] = DMAP_TO_PHYS((vm_offset_t)state->cbuf);
		i = 1;
	} else {
		off = m->m_epg_1st_off;
		for (i = 0; i < m->m_epg_npgs; i++, off = 0) {
			pg = vm_page_alloc_noobj(VM_ALLOC_NODUMP |
			    VM_ALLOC_WIRED | VM_ALLOC_WAITOK);
			len = m_epg_pagelen(m, i, off);
			state->parray[i] = VM_PAGE_TO_PHYS(pg);
			state->dst_iov[i].iov_base =
			    (char *)PHYS_TO_DMAP(state->parray[i]) + off;
			state->dst_iov[i].iov_len = len;
		}
	}
	KASSERT(i + 1 <= nitems(state->dst_iov), ("dst_iov is too small"));
	state->dst_iov[i].iov_base = m->m_epg_trail;
	state->dst_iov[i].iov_len = m->m_epg_trllen;

	error = ktls_ocf_encrypt(state, tls, m, state->dst_iov, i + 1);

	if (__predict_false(error != 0)) {
		/* Free the anonymous pages. */
		if (state->cbuf != NULL)
			uma_zfree(ktls_buffer_zone, state->cbuf);
		else {
			for (i = 0; i < m->m_epg_npgs; i++) {
				pg = PHYS_TO_VM_PAGE(state->parray[i]);
				(void)vm_page_unwire_noq(pg);
				vm_page_free(pg);
			}
		}
	}
	return (error);
}

/* Number of TLS records in a batch passed to ktls_enqueue(). */
static u_int
ktls_batched_records(struct mbuf *m)
{
	int page_count, records;

	records = 0;
	page_count = m->m_epg_enc_cnt;
	while (page_count > 0) {
		records++;
		page_count -= m->m_epg_nrdy;
		m = m->m_next;
	}
	KASSERT(page_count == 0, ("%s: mismatched page count", __func__));
	return (records);
}

void
ktls_enqueue(struct mbuf *m, struct socket *so, int page_count)
{
	struct ktls_session *tls;
	struct ktls_wq *wq;
	int queued;
	bool running;

	KASSERT(((m->m_flags & (M_EXTPG | M_NOTREADY)) ==
	    (M_EXTPG | M_NOTREADY)),
	    ("ktls_enqueue: %p not unready & nomap mbuf\n", m));
	KASSERT(page_count != 0, ("enqueueing TLS mbuf with zero page count"));

	KASSERT(m->m_epg_tls->mode == TCP_TLS_MODE_SW, ("!SW TLS mbuf"));

	m->m_epg_enc_cnt = page_count;

	/*
	 * Save a pointer to the socket.  The caller is responsible
	 * for taking an additional reference via soref().
	 */
	m->m_epg_so = so;

	queued = 1;
	tls = m->m_epg_tls;
	wq = &ktls_wq[tls->wq_index];
	mtx_lock(&wq->mtx);
	if (__predict_false(tls->sequential_records)) {
		/*
		 * For TLS 1.0, records must be encrypted
		 * sequentially.  For a given connection, all records
		 * queued to the associated work queue are processed
		 * sequentially.  However, sendfile(2) might complete
		 * I/O requests spanning multiple TLS records out of
		 * order.  Here we ensure TLS records are enqueued to
		 * the work queue in FIFO order.
		 *
		 * tls->next_seqno holds the sequence number of the
		 * next TLS record that should be enqueued to the work
		 * queue.  If this next record is not tls->next_seqno,
		 * it must be a future record, so insert it, sorted by
		 * TLS sequence number, into tls->pending_records and
		 * return.
		 *
		 * If this TLS record matches tls->next_seqno, place
		 * it in the work queue and then check
		 * tls->pending_records to see if any
		 * previously-queued records are now ready for
		 * encryption.
		 */
		if (m->m_epg_seqno != tls->next_seqno) {
			struct mbuf *n, *p;

			p = NULL;
			STAILQ_FOREACH(n, &tls->pending_records, m_epg_stailq) {
				if (n->m_epg_seqno > m->m_epg_seqno)
					break;
				p = n;
			}
			if (n == NULL)
				STAILQ_INSERT_TAIL(&tls->pending_records, m,
				    m_epg_stailq);
			else if (p == NULL)
				STAILQ_INSERT_HEAD(&tls->pending_records, m,
				    m_epg_stailq);
			else
				STAILQ_INSERT_AFTER(&tls->pending_records, p, m,
				    m_epg_stailq);
			mtx_unlock(&wq->mtx);
			counter_u64_add(ktls_cnt_tx_pending, 1);
			return;
		}

		tls->next_seqno += ktls_batched_records(m);
		STAILQ_INSERT_TAIL(&wq->m_head, m, m_epg_stailq);

		while (!STAILQ_EMPTY(&tls->pending_records)) {
			struct mbuf *n;

			n = STAILQ_FIRST(&tls->pending_records);
			if (n->m_epg_seqno != tls->next_seqno)
				break;

			queued++;
			STAILQ_REMOVE_HEAD(&tls->pending_records, m_epg_stailq);
			tls->next_seqno += ktls_batched_records(n);
			STAILQ_INSERT_TAIL(&wq->m_head, n, m_epg_stailq);
		}
		counter_u64_add(ktls_cnt_tx_pending, -(queued - 1));
	} else
		STAILQ_INSERT_TAIL(&wq->m_head, m, m_epg_stailq);

	running = wq->running;
	mtx_unlock(&wq->mtx);
	if (!running)
		wakeup(wq);
	counter_u64_add(ktls_cnt_tx_queued, queued);
}

/*
 * Once a file-backed mbuf (from sendfile) has been encrypted, free
 * the pages from the file and replace them with the anonymous pages
 * allocated in ktls_encrypt_record().
 */
static void
ktls_finish_nonanon(struct mbuf *m, struct ktls_ocf_encrypt_state *state)
{
	int i;

	MPASS((m->m_epg_flags & EPG_FLAG_ANON) == 0);

	/* Free the old pages. */
	m->m_ext.ext_free(m);

	/* Replace them with the new pages. */
	if (state->cbuf != NULL) {
		for (i = 0; i < m->m_epg_npgs; i++)
			m->m_epg_pa[i] = state->parray[0] + ptoa(i);

		/* Contig pages should go back to the cache. */
		m->m_ext.ext_free = ktls_free_mext_contig;
	} else {
		for (i = 0; i < m->m_epg_npgs; i++)
			m->m_epg_pa[i] = state->parray[i];

		/* Use the basic free routine. */
		m->m_ext.ext_free = mb_free_mext_pgs;
	}

	/* Pages are now writable. */
	m->m_epg_flags |= EPG_FLAG_ANON;
}

static __noinline void
ktls_encrypt(struct ktls_wq *wq, struct mbuf *top)
{
	struct ktls_ocf_encrypt_state state;
	struct ktls_session *tls;
	struct socket *so;
	struct mbuf *m;
	int error, npages, total_pages;

	so = top->m_epg_so;
	tls = top->m_epg_tls;
	KASSERT(tls != NULL, ("tls = NULL, top = %p\n", top));
	KASSERT(so != NULL, ("so = NULL, top = %p\n", top));
#ifdef INVARIANTS
	top->m_epg_so = NULL;
#endif
	total_pages = top->m_epg_enc_cnt;
	npages = 0;

	/*
	 * Encrypt the TLS records in the chain of mbufs starting with
	 * 'top'.  'total_pages' gives us a total count of pages and is
	 * used to know when we have finished encrypting the TLS
	 * records originally queued with 'top'.
	 *
	 * NB: These mbufs are queued in the socket buffer and
	 * 'm_next' is traversing the mbufs in the socket buffer.  The
	 * socket buffer lock is not held while traversing this chain.
	 * Since the mbufs are all marked M_NOTREADY their 'm_next'
	 * pointers should be stable.  However, the 'm_next' of the
	 * last mbuf encrypted is not necessarily NULL.  It can point
	 * to other mbufs appended while 'top' was on the TLS work
	 * queue.
	 *
	 * Each mbuf holds an entire TLS record.
	 */
	error = 0;
	for (m = top; npages != total_pages; m = m->m_next) {
		KASSERT(m->m_epg_tls == tls,
		    ("different TLS sessions in a single mbuf chain: %p vs %p",
		    tls, m->m_epg_tls));
		KASSERT(npages + m->m_epg_npgs <= total_pages,
		    ("page count mismatch: top %p, total_pages %d, m %p", top,
		    total_pages, m));

		error = ktls_encrypt_record(wq, m, tls, &state);
		if (error) {
			counter_u64_add(ktls_offload_failed_crypto, 1);
			break;
		}

		if ((m->m_epg_flags & EPG_FLAG_ANON) == 0)
			ktls_finish_nonanon(m, &state);

		npages += m->m_epg_nrdy;

		/*
		 * Drop a reference to the session now that it is no
		 * longer needed.  Existing code depends on encrypted
		 * records having no associated session vs
		 * yet-to-be-encrypted records having an associated
		 * session.
		 */
		m->m_epg_tls = NULL;
		ktls_free(tls);
	}

	CURVNET_SET(so->so_vnet);
	if (error == 0) {
		(void)so->so_proto->pr_ready(so, top, npages);
	} else {
		ktls_drop(so, EIO);
		mb_free_notready(top, total_pages);
	}

	sorele(so);
	CURVNET_RESTORE();
}

void
ktls_encrypt_cb(struct ktls_ocf_encrypt_state *state, int error)
{
	struct ktls_session *tls;
	struct socket *so;
	struct mbuf *m;
	int npages;

	m = state->m;

	if ((m->m_epg_flags & EPG_FLAG_ANON) == 0)
		ktls_finish_nonanon(m, state);

	so = state->so;
	free(state, M_KTLS);

	/*
	 * Drop a reference to the session now that it is no longer
	 * needed.  Existing code depends on encrypted records having
	 * no associated session vs yet-to-be-encrypted records having
	 * an associated session.
	 */
	tls = m->m_epg_tls;
	m->m_epg_tls = NULL;
	ktls_free(tls);

	if (error != 0)
		counter_u64_add(ktls_offload_failed_crypto, 1);

	CURVNET_SET(so->so_vnet);
	npages = m->m_epg_nrdy;

	if (error == 0) {
		(void)so->so_proto->pr_ready(so, m, npages);
	} else {
		ktls_drop(so, EIO);
		mb_free_notready(m, npages);
	}

	sorele(so);
	CURVNET_RESTORE();
}

/*
 * Similar to ktls_encrypt, but used with asynchronous OCF backends
 * (coprocessors) where encryption does not use host CPU resources and
 * it can be beneficial to queue more requests than CPUs.
 */
static __noinline void
ktls_encrypt_async(struct ktls_wq *wq, struct mbuf *top)
{
	struct ktls_ocf_encrypt_state *state;
	struct ktls_session *tls;
	struct socket *so;
	struct mbuf *m, *n;
	int error, mpages, npages, total_pages;

	so = top->m_epg_so;
	tls = top->m_epg_tls;
	KASSERT(tls != NULL, ("tls = NULL, top = %p\n", top));
	KASSERT(so != NULL, ("so = NULL, top = %p\n", top));
#ifdef INVARIANTS
	top->m_epg_so = NULL;
#endif
	total_pages = top->m_epg_enc_cnt;
	npages = 0;

	error = 0;
	for (m = top; npages != total_pages; m = n) {
		KASSERT(m->m_epg_tls == tls,
		    ("different TLS sessions in a single mbuf chain: %p vs %p",
		    tls, m->m_epg_tls));
		KASSERT(npages + m->m_epg_npgs <= total_pages,
		    ("page count mismatch: top %p, total_pages %d, m %p", top,
		    total_pages, m));

		state = malloc(sizeof(*state), M_KTLS, M_WAITOK | M_ZERO);
		soref(so);
		state->so = so;
		state->m = m;

		mpages = m->m_epg_nrdy;
		n = m->m_next;

		error = ktls_encrypt_record(wq, m, tls, state);
		if (error) {
			counter_u64_add(ktls_offload_failed_crypto, 1);
			free(state, M_KTLS);
			CURVNET_SET(so->so_vnet);
			sorele(so);
			CURVNET_RESTORE();
			break;
		}

		npages += mpages;
	}

	CURVNET_SET(so->so_vnet);
	if (error != 0) {
		ktls_drop(so, EIO);
		mb_free_notready(m, total_pages - npages);
	}

	sorele(so);
	CURVNET_RESTORE();
}

static int
ktls_bind_domain(int domain)
{
	int error;

	error = cpuset_setthread(curthread->td_tid, &cpuset_domain[domain]);
	if (error != 0)
		return (error);
	curthread->td_domain.dr_policy = DOMAINSET_PREF(domain);
	return (0);
}

static void
ktls_reclaim_thread(void *ctx)
{
	struct ktls_domain_info *ktls_domain = ctx;
	struct ktls_reclaim_thread *sc = &ktls_domain->reclaim_td;
	struct sysctl_oid *oid;
	char name[80];
	int error, domain;

	domain = ktls_domain - ktls_domains;
	if (bootverbose)
		printf("Starting KTLS reclaim thread for domain %d\n", domain);
	error = ktls_bind_domain(domain);
	if (error)
		printf("Unable to bind KTLS reclaim thread for domain %d: error %d\n",
		    domain, error);
	snprintf(name, sizeof(name), "domain%d", domain);
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_kern_ipc_tls), OID_AUTO,
	    name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO, "reclaims",
	    CTLFLAG_RD,  &sc->reclaims, 0, "buffers reclaimed");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO, "wakeups",
	    CTLFLAG_RD,  &sc->wakeups, 0, "thread wakeups");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(oid), OID_AUTO, "running",
	    CTLFLAG_RD,  &sc->running, 0, "thread running");

	for (;;) {
		atomic_store_int(&sc->running, 0);
		tsleep(sc, PZERO | PNOLOCK, "-",  0);
		atomic_store_int(&sc->running, 1);
		sc->wakeups++;
		/*
		 * Below we attempt to reclaim ktls_max_reclaim
		 * buffers using vm_page_reclaim_contig_domain_ext().
		 * We do this here, as this function can take several
		 * seconds to scan all of memory and it does not
		 * matter if this thread pauses for a while.  If we
		 * block a ktls worker thread, we risk developing
		 * backlogs of buffers to be encrypted, leading to
		 * surges of traffic and potential NIC output drops.
		 */
		if (!vm_page_reclaim_contig_domain_ext(domain, VM_ALLOC_NORMAL,
		    atop(ktls_maxlen), 0, ~0ul, PAGE_SIZE, 0, ktls_max_reclaim)) {
			vm_wait_domain(domain);
		} else {
			sc->reclaims += ktls_max_reclaim;
		}
	}
}

static void
ktls_work_thread(void *ctx)
{
	struct ktls_wq *wq = ctx;
	struct mbuf *m, *n;
	struct socket *so, *son;
	STAILQ_HEAD(, mbuf) local_m_head;
	STAILQ_HEAD(, socket) local_so_head;
	int cpu;

	cpu = wq - ktls_wq;
	if (bootverbose)
		printf("Starting KTLS worker thread for CPU %d\n", cpu);

	/*
	 * Bind to a core.  If ktls_bind_threads is > 1, then
	 * we bind to the NUMA domain instead.
	 */
	if (ktls_bind_threads) {
		int error;

		if (ktls_bind_threads > 1) {
			struct pcpu *pc = pcpu_find(cpu);

			error = ktls_bind_domain(pc->pc_domain);
		} else {
			cpuset_t mask;

			CPU_SETOF(cpu, &mask);
			error = cpuset_setthread(curthread->td_tid, &mask);
		}
		if (error)
			printf("Unable to bind KTLS worker thread for CPU %d: error %d\n",
				cpu, error);
	}
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	fpu_kern_thread(0);
#endif
	for (;;) {
		mtx_lock(&wq->mtx);
		while (STAILQ_EMPTY(&wq->m_head) &&
		    STAILQ_EMPTY(&wq->so_head)) {
			wq->running = false;
			mtx_sleep(wq, &wq->mtx, 0, "-", 0);
			wq->running = true;
		}

		STAILQ_INIT(&local_m_head);
		STAILQ_CONCAT(&local_m_head, &wq->m_head);
		STAILQ_INIT(&local_so_head);
		STAILQ_CONCAT(&local_so_head, &wq->so_head);
		mtx_unlock(&wq->mtx);

		STAILQ_FOREACH_SAFE(m, &local_m_head, m_epg_stailq, n) {
			if (m->m_epg_flags & EPG_FLAG_2FREE) {
				ktls_free(m->m_epg_tls);
				m_free_raw(m);
			} else {
				if (m->m_epg_tls->sync_dispatch)
					ktls_encrypt(wq, m);
				else
					ktls_encrypt_async(wq, m);
				counter_u64_add(ktls_cnt_tx_queued, -1);
			}
		}

		STAILQ_FOREACH_SAFE(so, &local_so_head, so_ktls_rx_list, son) {
			ktls_decrypt(so);
			counter_u64_add(ktls_cnt_rx_queued, -1);
		}
	}
}

static void
ktls_disable_ifnet_help(void *context, int pending __unused)
{
	struct ktls_session *tls;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct socket *so;
	int err;

	tls = context;
	inp = tls->inp;
	if (inp == NULL)
		return;
	INP_WLOCK(inp);
	so = inp->inp_socket;
	MPASS(so != NULL);
	if (inp->inp_flags & INP_DROPPED) {
		goto out;
	}

	if (so->so_snd.sb_tls_info != NULL)
		err = ktls_set_tx_mode(so, TCP_TLS_MODE_SW);
	else
		err = ENXIO;
	if (err == 0) {
		counter_u64_add(ktls_ifnet_disable_ok, 1);
		/* ktls_set_tx_mode() drops inp wlock, so recheck flags */
		if ((inp->inp_flags & INP_DROPPED) == 0 &&
		    (tp = intotcpcb(inp)) != NULL &&
		    tp->t_fb->tfb_hwtls_change != NULL)
			(*tp->t_fb->tfb_hwtls_change)(tp, 0);
	} else {
		counter_u64_add(ktls_ifnet_disable_fail, 1);
	}

out:
	CURVNET_SET(so->so_vnet);
	sorele(so);
	CURVNET_RESTORE();
	INP_WUNLOCK(inp);
	ktls_free(tls);
}

/*
 * Called when re-transmits are becoming a substantial portion of the
 * sends on this connection.  When this happens, we transition the
 * connection to software TLS.  This is needed because most inline TLS
 * NICs keep crypto state only for in-order transmits.  This means
 * that to handle a TCP rexmit (which is out-of-order), the NIC must
 * re-DMA the entire TLS record up to and including the current
 * segment.  This means that when re-transmitting the last ~1448 byte
 * segment of a 16KB TLS record, we could wind up re-DMA'ing an order
 * of magnitude more data than we are sending.  This can cause the
 * PCIe link to saturate well before the network, which can cause
 * output drops, and a general loss of capacity.
 */
void
ktls_disable_ifnet(void *arg)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	struct socket *so;
	struct ktls_session *tls;

	tp = arg;
	inp = tptoinpcb(tp);
	INP_WLOCK_ASSERT(inp);
	so = inp->inp_socket;
	SOCK_LOCK(so);
	tls = so->so_snd.sb_tls_info;
	if (tp->t_nic_ktls_xmit_dis == 1) {
		SOCK_UNLOCK(so);
		return;
	}

	/*
	 * note that t_nic_ktls_xmit_dis is never cleared; disabling
	 * ifnet can only be done once per connection, so we never want
	 * to do it again
	 */

	(void)ktls_hold(tls);
	soref(so);
	tp->t_nic_ktls_xmit_dis = 1;
	SOCK_UNLOCK(so);
	TASK_INIT(&tls->disable_ifnet_task, 0, ktls_disable_ifnet_help, tls);
	(void)taskqueue_enqueue(taskqueue_thread, &tls->disable_ifnet_task);
}

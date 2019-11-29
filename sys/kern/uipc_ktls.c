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
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/kernel.h>
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
#ifdef RSS
#include <net/netisr.h>
#include <net/rss_config.h>
#endif
#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#endif
#include <netinet/tcp_var.h>
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#include <opencrypto/xform.h>
#include <vm/uma_dbg.h>
#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>

struct ktls_wq {
	struct mtx	mtx;
	STAILQ_HEAD(, mbuf_ext_pgs) head;
	bool		running;
} __aligned(CACHE_LINE_SIZE);

static struct ktls_wq *ktls_wq;
static struct proc *ktls_proc;
LIST_HEAD(, ktls_crypto_backend) ktls_backends;
static struct rmlock ktls_backends_lock;
static uma_zone_t ktls_session_zone;
static uint16_t ktls_cpuid_lookup[MAXCPU];

SYSCTL_NODE(_kern_ipc, OID_AUTO, tls, CTLFLAG_RW, 0,
    "Kernel TLS offload");
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, stats, CTLFLAG_RW, 0,
    "Kernel TLS offload stats");

static int ktls_allow_unload;
SYSCTL_INT(_kern_ipc_tls, OID_AUTO, allow_unload, CTLFLAG_RDTUN,
    &ktls_allow_unload, 0, "Allow software crypto modules to unload");

#ifdef RSS
static int ktls_bind_threads = 1;
#else
static int ktls_bind_threads;
#endif
SYSCTL_INT(_kern_ipc_tls, OID_AUTO, bind_threads, CTLFLAG_RDTUN,
    &ktls_bind_threads, 0,
    "Bind crypto threads to cores or domains at boot");

static u_int ktls_maxlen = 16384;
SYSCTL_UINT(_kern_ipc_tls, OID_AUTO, maxlen, CTLFLAG_RWTUN,
    &ktls_maxlen, 0, "Maximum TLS record size");

static int ktls_number_threads;
SYSCTL_INT(_kern_ipc_tls_stats, OID_AUTO, threads, CTLFLAG_RD,
    &ktls_number_threads, 0,
    "Number of TLS threads in thread-pool");

static bool ktls_offload_enable;
SYSCTL_BOOL(_kern_ipc_tls, OID_AUTO, enable, CTLFLAG_RW,
    &ktls_offload_enable, 0,
    "Enable support for kernel TLS offload");

static bool ktls_cbc_enable = true;
SYSCTL_BOOL(_kern_ipc_tls, OID_AUTO, cbc_enable, CTLFLAG_RW,
    &ktls_cbc_enable, 1,
    "Enable Support of AES-CBC crypto for kernel TLS");

static counter_u64_t ktls_tasks_active;
SYSCTL_COUNTER_U64(_kern_ipc_tls, OID_AUTO, tasks_active, CTLFLAG_RD,
    &ktls_tasks_active, "Number of active tasks");

static counter_u64_t ktls_cnt_on;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, so_inqueue, CTLFLAG_RD,
    &ktls_cnt_on, "Number of TLS records in queue to tasks for SW crypto");

static counter_u64_t ktls_offload_total;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, offload_total,
    CTLFLAG_RD, &ktls_offload_total,
    "Total successful TLS setups (parameters set)");

static counter_u64_t ktls_offload_enable_calls;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, enable_calls,
    CTLFLAG_RD, &ktls_offload_enable_calls,
    "Total number of TLS enable calls made");

static counter_u64_t ktls_offload_active;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, active, CTLFLAG_RD,
    &ktls_offload_active, "Total Active TLS sessions");

static counter_u64_t ktls_offload_failed_crypto;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, failed_crypto, CTLFLAG_RD,
    &ktls_offload_failed_crypto, "Total TLS crypto failures");

static counter_u64_t ktls_switch_to_ifnet;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_to_ifnet, CTLFLAG_RD,
    &ktls_switch_to_ifnet, "TLS sessions switched from SW to ifnet");

static counter_u64_t ktls_switch_to_sw;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_to_sw, CTLFLAG_RD,
    &ktls_switch_to_sw, "TLS sessions switched from ifnet to SW");

static counter_u64_t ktls_switch_failed;
SYSCTL_COUNTER_U64(_kern_ipc_tls_stats, OID_AUTO, switch_failed, CTLFLAG_RD,
    &ktls_switch_failed, "TLS sessions unable to switch between SW and ifnet");

SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, sw, CTLFLAG_RD, 0,
    "Software TLS session stats");
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, ifnet, CTLFLAG_RD, 0,
    "Hardware (ifnet) TLS session stats");
#ifdef TCP_OFFLOAD
SYSCTL_NODE(_kern_ipc_tls, OID_AUTO, toe, CTLFLAG_RD, 0,
    "TOE TLS session stats");
#endif

static counter_u64_t ktls_sw_cbc;
SYSCTL_COUNTER_U64(_kern_ipc_tls_sw, OID_AUTO, cbc, CTLFLAG_RD, &ktls_sw_cbc,
    "Active number of software TLS sessions using AES-CBC");

static counter_u64_t ktls_sw_gcm;
SYSCTL_COUNTER_U64(_kern_ipc_tls_sw, OID_AUTO, gcm, CTLFLAG_RD, &ktls_sw_gcm,
    "Active number of software TLS sessions using AES-GCM");

static counter_u64_t ktls_ifnet_cbc;
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, cbc, CTLFLAG_RD,
    &ktls_ifnet_cbc,
    "Active number of ifnet TLS sessions using AES-CBC");

static counter_u64_t ktls_ifnet_gcm;
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, gcm, CTLFLAG_RD,
    &ktls_ifnet_gcm,
    "Active number of ifnet TLS sessions using AES-GCM");

static counter_u64_t ktls_ifnet_reset;
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset, CTLFLAG_RD,
    &ktls_ifnet_reset, "TLS sessions updated to a new ifnet send tag");

static counter_u64_t ktls_ifnet_reset_dropped;
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset_dropped, CTLFLAG_RD,
    &ktls_ifnet_reset_dropped,
    "TLS sessions dropped after failing to update ifnet send tag");

static counter_u64_t ktls_ifnet_reset_failed;
SYSCTL_COUNTER_U64(_kern_ipc_tls_ifnet, OID_AUTO, reset_failed, CTLFLAG_RD,
    &ktls_ifnet_reset_failed,
    "TLS sessions that failed to allocate a new ifnet send tag");

static int ktls_ifnet_permitted;
SYSCTL_UINT(_kern_ipc_tls_ifnet, OID_AUTO, permitted, CTLFLAG_RWTUN,
    &ktls_ifnet_permitted, 1,
    "Whether to permit hardware (ifnet) TLS sessions");

#ifdef TCP_OFFLOAD
static counter_u64_t ktls_toe_cbc;
SYSCTL_COUNTER_U64(_kern_ipc_tls_toe, OID_AUTO, cbc, CTLFLAG_RD,
    &ktls_toe_cbc,
    "Active number of TOE TLS sessions using AES-CBC");

static counter_u64_t ktls_toe_gcm;
SYSCTL_COUNTER_U64(_kern_ipc_tls_toe, OID_AUTO, gcm, CTLFLAG_RD,
    &ktls_toe_gcm,
    "Active number of TOE TLS sessions using AES-GCM");
#endif

static MALLOC_DEFINE(M_KTLS, "ktls", "Kernel TLS");

static void ktls_cleanup(struct ktls_session *tls);
#if defined(INET) || defined(INET6)
static void ktls_reset_send_tag(void *context, int pending);
#endif
static void ktls_work_thread(void *ctx);

int
ktls_crypto_backend_register(struct ktls_crypto_backend *be)
{
	struct ktls_crypto_backend *curr_be, *tmp;

	if (be->api_version != KTLS_API_VERSION) {
		printf("KTLS: API version mismatch (%d vs %d) for %s\n",
		    be->api_version, KTLS_API_VERSION,
		    be->name);
		return (EINVAL);
	}

	rm_wlock(&ktls_backends_lock);
	printf("KTLS: Registering crypto method %s with prio %d\n",
	       be->name, be->prio);
	if (LIST_EMPTY(&ktls_backends)) {
		LIST_INSERT_HEAD(&ktls_backends, be, next);
	} else {
		LIST_FOREACH_SAFE(curr_be, &ktls_backends, next, tmp) {
			if (curr_be->prio < be->prio) {
				LIST_INSERT_BEFORE(curr_be, be, next);
				break;
			}
			if (LIST_NEXT(curr_be, next) == NULL) {
				LIST_INSERT_AFTER(curr_be, be, next);
				break;
			}
		}
	}
	rm_wunlock(&ktls_backends_lock);
	return (0);
}

int
ktls_crypto_backend_deregister(struct ktls_crypto_backend *be)
{
	struct ktls_crypto_backend *tmp;

	/*
	 * Don't error if the backend isn't registered.  This permits
	 * MOD_UNLOAD handlers to use this function unconditionally.
	 */
	rm_wlock(&ktls_backends_lock);
	LIST_FOREACH(tmp, &ktls_backends, next) {
		if (tmp == be)
			break;
	}
	if (tmp == NULL) {
		rm_wunlock(&ktls_backends_lock);
		return (0);
	}

	if (!ktls_allow_unload) {
		rm_wunlock(&ktls_backends_lock);
		printf(
		    "KTLS: Deregistering crypto method %s is not supported\n",
		    be->name);
		return (EBUSY);
	}

	if (be->use_count) {
		rm_wunlock(&ktls_backends_lock);
		return (EBUSY);
	}

	LIST_REMOVE(be, next);
	rm_wunlock(&ktls_backends_lock);
	return (0);
}

#if defined(INET) || defined(INET6)
static uint16_t
ktls_get_cpu(struct socket *so)
{
	struct inpcb *inp;
	uint16_t cpuid;

	inp = sotoinpcb(so);
#ifdef RSS
	cpuid = rss_hash2cpuid(inp->inp_flowid, inp->inp_flowtype);
	if (cpuid != NETISR_CPUID_NONE)
		return (cpuid);
#endif
	/*
	 * Just use the flowid to shard connections in a repeatable
	 * fashion.  Note that some crypto backends rely on the
	 * serialization provided by having the same connection use
	 * the same queue.
	 */
	cpuid = ktls_cpuid_lookup[inp->inp_flowid % ktls_number_threads];
	return (cpuid);
}
#endif

static void
ktls_init(void *dummy __unused)
{
	struct thread *td;
	struct pcpu *pc;
	cpuset_t mask;
	int error, i;

	ktls_tasks_active = counter_u64_alloc(M_WAITOK);
	ktls_cnt_on = counter_u64_alloc(M_WAITOK);
	ktls_offload_total = counter_u64_alloc(M_WAITOK);
	ktls_offload_enable_calls = counter_u64_alloc(M_WAITOK);
	ktls_offload_active = counter_u64_alloc(M_WAITOK);
	ktls_offload_failed_crypto = counter_u64_alloc(M_WAITOK);
	ktls_switch_to_ifnet = counter_u64_alloc(M_WAITOK);
	ktls_switch_to_sw = counter_u64_alloc(M_WAITOK);
	ktls_switch_failed = counter_u64_alloc(M_WAITOK);
	ktls_sw_cbc = counter_u64_alloc(M_WAITOK);
	ktls_sw_gcm = counter_u64_alloc(M_WAITOK);
	ktls_ifnet_cbc = counter_u64_alloc(M_WAITOK);
	ktls_ifnet_gcm = counter_u64_alloc(M_WAITOK);
	ktls_ifnet_reset = counter_u64_alloc(M_WAITOK);
	ktls_ifnet_reset_dropped = counter_u64_alloc(M_WAITOK);
	ktls_ifnet_reset_failed = counter_u64_alloc(M_WAITOK);
#ifdef TCP_OFFLOAD
	ktls_toe_cbc = counter_u64_alloc(M_WAITOK);
	ktls_toe_gcm = counter_u64_alloc(M_WAITOK);
#endif

	rm_init(&ktls_backends_lock, "ktls backends");
	LIST_INIT(&ktls_backends);

	ktls_wq = malloc(sizeof(*ktls_wq) * (mp_maxid + 1), M_KTLS,
	    M_WAITOK | M_ZERO);

	ktls_session_zone = uma_zcreate("ktls_session",
	    sizeof(struct ktls_session),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_CACHE, 0);

	/*
	 * Initialize the workqueues to run the TLS work.  We create a
	 * work queue for each CPU.
	 */
	CPU_FOREACH(i) {
		STAILQ_INIT(&ktls_wq[i].head);
		mtx_init(&ktls_wq[i].mtx, "ktls work queue", NULL, MTX_DEF);
		error = kproc_kthread_add(ktls_work_thread, &ktls_wq[i],
		    &ktls_proc, &td, 0, 0, "KTLS", "thr_%d", i);
		if (error)
			panic("Can't add KTLS thread %d error %d", i, error);

		/*
		 * Bind threads to cores.  If ktls_bind_threads is >
		 * 1, then we bind to the NUMA domain.
		 */
		if (ktls_bind_threads) {
			if (ktls_bind_threads > 1) {
				pc = pcpu_find(i);
				CPU_COPY(&cpuset_domain[pc->pc_domain], &mask);
			} else {
				CPU_SETOF(i, &mask);
			}
			error = cpuset_setthread(td->td_tid, &mask);
			if (error)
				panic(
			    "Unable to bind KTLS thread for CPU %d error %d",
				     i, error);
		}
		ktls_cpuid_lookup[ktls_number_threads] = i;
		ktls_number_threads++;
	}
	printf("KTLS: Initialized %d threads\n", ktls_number_threads);
}
SYSINIT(ktls, SI_SUB_SMP + 1, SI_ORDER_ANY, ktls_init, NULL);

#if defined(INET) || defined(INET6)
static int
ktls_create_session(struct socket *so, struct tls_enable *en,
    struct ktls_session **tlsp)
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
		case CRYPTO_AES_128_NIST_GMAC:
		case CRYPTO_AES_192_NIST_GMAC:
		case CRYPTO_AES_256_NIST_GMAC:
			break;
		default:
			return (EINVAL);
		}
		if (en->auth_key_len != 0)
			return (EINVAL);
		if ((en->tls_vminor == TLS_MINOR_VER_TWO &&
			en->iv_len != TLS_AEAD_GCM_LEN) ||
		    (en->tls_vminor == TLS_MINOR_VER_THREE &&
			en->iv_len != TLS_1_3_GCM_IV_LEN))
			return (EINVAL);
		break;
	case CRYPTO_AES_CBC:
		switch (en->auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			/*
			 * TLS 1.0 requires an implicit IV.  TLS 1.1+
			 * all use explicit IVs.
			 */
			if (en->tls_vminor == TLS_MINOR_VER_ZERO) {
				if (en->iv_len != TLS_CBC_IMPLICIT_IV_LEN)
					return (EINVAL);
				break;
			}

			/* FALLTHROUGH */
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
			/* Ignore any supplied IV. */
			en->iv_len = 0;
			break;
		default:
			return (EINVAL);
		}
		if (en->auth_key_len == 0)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	tls = uma_zalloc(ktls_session_zone, M_WAITOK | M_ZERO);

	counter_u64_add(ktls_offload_active, 1);

	refcount_init(&tls->refcount, 1);
	TASK_INIT(&tls->reset_tag_task, 0, ktls_reset_send_tag, tls);

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

		/*
		 * TLS 1.3 includes optional padding which we
		 * do not support, and also puts the "real" record
		 * type at the end of the encrypted data.
		 */
		if (en->tls_vminor == TLS_MINOR_VER_THREE)
			tls->params.tls_tlen += sizeof(uint8_t);

		tls->params.tls_bs = 1;
		break;
	case CRYPTO_AES_CBC:
		switch (en->auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			if (en->tls_vminor == TLS_MINOR_VER_ZERO) {
				/* Implicit IV, no nonce. */
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
	default:
		panic("invalid cipher");
	}

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
	 * This holds the implicit portion of the nonce for GCM and
	 * the initial implicit IV for TLS 1.0.  The explicit portions
	 * of the IV are generated in ktls_frame().
	 */
	if (en->iv_len != 0) {
		tls->params.iv_len = en->iv_len;
		error = copyin(en->iv, tls->params.iv, en->iv_len);
		if (error)
			goto out;

		/*
		 * For TLS 1.2, generate an 8-byte nonce as a counter
		 * to generate unique explicit IVs.
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
	ktls_cleanup(tls);
	return (error);
}

static struct ktls_session *
ktls_clone_session(struct ktls_session *tls)
{
	struct ktls_session *tls_new;

	tls_new = uma_zalloc(ktls_session_zone, M_WAITOK | M_ZERO);

	counter_u64_add(ktls_offload_active, 1);

	refcount_init(&tls_new->refcount, 1);

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
#endif

static void
ktls_cleanup(struct ktls_session *tls)
{

	counter_u64_add(ktls_offload_active, -1);
	switch (tls->mode) {
	case TCP_TLS_MODE_SW:
		MPASS(tls->be != NULL);
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_sw_cbc, -1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_sw_gcm, -1);
			break;
		}
		tls->free(tls);
		break;
	case TCP_TLS_MODE_IFNET:
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_ifnet_cbc, -1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_ifnet_gcm, -1);
			break;
		}
		m_snd_tag_rele(tls->snd_tag);
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
		}
		break;
#endif
	}
	if (tls->params.auth_key != NULL) {
		explicit_bzero(tls->params.auth_key, tls->params.auth_key_len);
		free(tls->params.auth_key, M_KTLS);
		tls->params.auth_key = NULL;
		tls->params.auth_key_len = 0;
	}
	if (tls->params.cipher_key != NULL) {
		explicit_bzero(tls->params.cipher_key,
		    tls->params.cipher_key_len);
		free(tls->params.cipher_key, M_KTLS);
		tls->params.cipher_key = NULL;
		tls->params.cipher_key_len = 0;
	}
	explicit_bzero(tls->params.iv, sizeof(tls->params.iv));
}

#if defined(INET) || defined(INET6)

#ifdef TCP_OFFLOAD
static int
ktls_try_toe(struct socket *so, struct ktls_session *tls)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	inp = so->so_pcb;
	INP_WLOCK(inp);
	if (inp->inp_flags2 & INP_FREED) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_socket == NULL) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);
	if (tp->tod == NULL) {
		INP_WUNLOCK(inp);
		return (EOPNOTSUPP);
	}

	error = tcp_offload_alloc_tls_session(tp, tls);
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
	struct rtentry *rt;
	struct tcpcb *tp;
	int error;

	INP_RLOCK(inp);
	if (inp->inp_flags2 & INP_FREED) {
		INP_RUNLOCK(inp);
		return (ECONNRESET);
	}
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
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
	rt = inp->inp_route.ro_rt;
	if (rt == NULL || rt->rt_ifp == NULL) {
		INP_RUNLOCK(inp);
		return (ENXIO);
	}
	ifp = rt->rt_ifp;
	if_ref(ifp);

	params.hdr.type = IF_SND_TAG_TYPE_TLS;
	params.hdr.flowid = inp->inp_flowid;
	params.hdr.flowtype = inp->inp_flowtype;
	params.tls.inp = inp;
	params.tls.tls = tls;
	INP_RUNLOCK(inp);

	if (ifp->if_snd_tag_alloc == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}
	if ((ifp->if_capenable & IFCAP_NOMAP) == 0) {	
		error = EOPNOTSUPP;
		goto out;
	}
	if (inp->inp_vflag & INP_IPV6) {
		if ((ifp->if_capenable & IFCAP_TXTLS6) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	} else {
		if ((ifp->if_capenable & IFCAP_TXTLS4) == 0) {
			error = EOPNOTSUPP;
			goto out;
		}
	}
	error = ifp->if_snd_tag_alloc(ifp, &params, mstp);
out:
	if_rele(ifp);
	return (error);
}

static int
ktls_try_ifnet(struct socket *so, struct ktls_session *tls, bool force)
{
	struct m_snd_tag *mst;
	int error;

	error = ktls_alloc_snd_tag(so->so_pcb, tls, force, &mst);
	if (error == 0) {
		tls->mode = TCP_TLS_MODE_IFNET;
		tls->snd_tag = mst;
		switch (tls->params.cipher_algorithm) {
		case CRYPTO_AES_CBC:
			counter_u64_add(ktls_ifnet_cbc, 1);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			counter_u64_add(ktls_ifnet_gcm, 1);
			break;
		}
	}
	return (error);
}

static int
ktls_try_sw(struct socket *so, struct ktls_session *tls)
{
	struct rm_priotracker prio;
	struct ktls_crypto_backend *be;

	/*
	 * Choose the best software crypto backend.  Backends are
	 * stored in sorted priority order (larget value == most
	 * important at the head of the list), so this just stops on
	 * the first backend that claims the session by returning
	 * success.
	 */
	if (ktls_allow_unload)
		rm_rlock(&ktls_backends_lock, &prio);
	LIST_FOREACH(be, &ktls_backends, next) {
		if (be->try(so, tls) == 0)
			break;
		KASSERT(tls->cipher == NULL,
		    ("ktls backend leaked a cipher pointer"));
	}
	if (be != NULL) {
		if (ktls_allow_unload)
			be->use_count++;
		tls->be = be;
	}
	if (ktls_allow_unload)
		rm_runlock(&ktls_backends_lock, &prio);
	if (be == NULL)
		return (EOPNOTSUPP);
	tls->mode = TCP_TLS_MODE_SW;
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		counter_u64_add(ktls_sw_cbc, 1);
		break;
	case CRYPTO_AES_NIST_GCM_16:
		counter_u64_add(ktls_sw_gcm, 1);
		break;
	}
	return (0);
}

int
ktls_enable_tx(struct socket *so, struct tls_enable *en)
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
	if (so->so_snd.sb_tls_info != NULL)
		return (EALREADY);

	if (en->cipher_algorithm == CRYPTO_AES_CBC && !ktls_cbc_enable)
		return (ENOTSUP);

	/* TLS requires ext pgs */
	if (mb_use_ext_pgs == 0)
		return (ENXIO);

	error = ktls_create_session(so, en, &tls);
	if (error)
		return (error);

	/* Prefer TOE -> ifnet TLS -> software TLS. */
#ifdef TCP_OFFLOAD
	error = ktls_try_toe(so, tls);
	if (error)
#endif
		error = ktls_try_ifnet(so, tls, false);
	if (error)
		error = ktls_try_sw(so, tls);

	if (error) {
		ktls_cleanup(tls);
		return (error);
	}

	error = sblock(&so->so_snd, SBL_WAIT);
	if (error) {
		ktls_cleanup(tls);
		return (error);
	}

	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_tls_info = tls;
	if (tls->mode != TCP_TLS_MODE_SW)
		so->so_snd.sb_flags |= SB_TLS_IFNET;
	SOCKBUF_UNLOCK(&so->so_snd);
	sbunlock(&so->so_snd);

	counter_u64_add(ktls_offload_total, 1);

	return (0);
}

int
ktls_get_tx_mode(struct socket *so)
{
	struct ktls_session *tls;
	struct inpcb *inp;
	int mode;

	inp = so->so_pcb;
	INP_WLOCK_ASSERT(inp);
	SOCKBUF_LOCK(&so->so_snd);
	tls = so->so_snd.sb_tls_info;
	if (tls == NULL)
		mode = TCP_TLS_MODE_NONE;
	else
		mode = tls->mode;
	SOCKBUF_UNLOCK(&so->so_snd);
	return (mode);
}

/*
 * Switch between SW and ifnet TLS sessions as requested.
 */
int
ktls_set_tx_mode(struct socket *so, int mode)
{
	struct ktls_session *tls, *tls_new;
	struct inpcb *inp;
	int error;

	switch (mode) {
	case TCP_TLS_MODE_SW:
	case TCP_TLS_MODE_IFNET:
		break;
	default:
		return (EINVAL);
	}

	inp = so->so_pcb;
	INP_WLOCK_ASSERT(inp);
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

	tls_new = ktls_clone_session(tls);

	if (mode == TCP_TLS_MODE_IFNET)
		error = ktls_try_ifnet(so, tls_new, true);
	else
		error = ktls_try_sw(so, tls_new);
	if (error) {
		counter_u64_add(ktls_switch_failed, 1);
		ktls_free(tls_new);
		ktls_free(tls);
		INP_WLOCK(inp);
		return (error);
	}

	error = sblock(&so->so_snd, SBL_WAIT);
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
		sbunlock(&so->so_snd);
		ktls_free(tls_new);
		ktls_free(tls);
		INP_WLOCK(inp);
		return (EBUSY);
	}

	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_tls_info = tls_new;
	if (tls_new->mode != TCP_TLS_MODE_SW)
		so->so_snd.sb_flags |= SB_TLS_IFNET;
	SOCKBUF_UNLOCK(&so->so_snd);
	sbunlock(&so->so_snd);

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

	INP_WLOCK(inp);
	return (0);
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
		if (!in_pcbrele_wlocked(inp))
			INP_WUNLOCK(inp);

		counter_u64_add(ktls_ifnet_reset, 1);

		/*
		 * XXX: Should we kick tcp_output explicitly now that
		 * the send tag is fixed or just rely on timers?
		 */
	} else {
		NET_EPOCH_ENTER(et);
		INP_WLOCK(inp);
		if (!in_pcbrele_wlocked(inp)) {
			if (!(inp->inp_flags & INP_TIMEWAIT) &&
			    !(inp->inp_flags & INP_DROPPED)) {
				tp = intotcpcb(inp);
				tp = tcp_drop(tp, ECONNABORTED);
				if (tp != NULL)
					INP_WUNLOCK(inp);
				counter_u64_add(ktls_ifnet_reset_dropped, 1);
			} else
				INP_WUNLOCK(inp);
		}
		NET_EPOCH_EXIT(et);

		counter_u64_add(ktls_ifnet_reset_failed, 1);

		/*
		 * Leave reset_pending true to avoid future tasks while
		 * the socket goes away.
		 */
	}

	ktls_free(tls);
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
		in_pcbref(inp);
		tls->inp = inp;
		tls->reset_pending = true;
		taskqueue_enqueue(taskqueue_thread, &tls->reset_tag_task);
	}
	mtx_pool_unlock(mtxpool_sleep, tls);
	return (ENOBUFS);
}
#endif

void
ktls_destroy(struct ktls_session *tls)
{
	struct rm_priotracker prio;

	ktls_cleanup(tls);
	if (tls->be != NULL && ktls_allow_unload) {
		rm_rlock(&ktls_backends_lock, &prio);
		tls->be->use_count--;
		rm_runlock(&ktls_backends_lock, &prio);
	}
	uma_zfree(ktls_session_zone, tls);
}

void
ktls_seq(struct sockbuf *sb, struct mbuf *m)
{
	struct mbuf_ext_pgs *pgs;

	for (; m != NULL; m = m->m_next) {
		KASSERT((m->m_flags & M_NOMAP) != 0,
		    ("ktls_seq: mapped mbuf %p", m));

		pgs = m->m_ext.ext_pgs;
		pgs->seqno = sb->sb_tls_seqno;
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
 * when scheduling encryption of this chain of mbufs.
 */
int
ktls_frame(struct mbuf *top, struct ktls_session *tls, int *enq_cnt,
    uint8_t record_type)
{
	struct tls_record_layer *tlshdr;
	struct mbuf *m;
	struct mbuf_ext_pgs *pgs;
	uint64_t *noncep;
	uint16_t tls_len;
	int maxlen;

	maxlen = tls->params.max_frame_len;
	*enq_cnt = 0;
	for (m = top; m != NULL; m = m->m_next) {
		/*
		 * All mbufs in the chain should be non-empty TLS
		 * records whose payload does not exceed the maximum
		 * frame length.
		 */
		if (m->m_len > maxlen || m->m_len == 0)
			return (EINVAL);
		tls_len = m->m_len;

		/*
		 * TLS frames require unmapped mbufs to store session
		 * info.
		 */
		KASSERT((m->m_flags & M_NOMAP) != 0,
		    ("ktls_frame: mapped mbuf %p (top = %p)\n", m, top));

		pgs = m->m_ext.ext_pgs;

		/* Save a reference to the session. */
		pgs->tls = ktls_hold(tls);

		pgs->hdr_len = tls->params.tls_hlen;
		pgs->trail_len = tls->params.tls_tlen;
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
			 * tls->params.sb_tls_tlen is the maximum
			 * possible trailer length (padding + digest).
			 * delta holds the number of excess padding
			 * bytes if the maximum were used.  Those
			 * extra bytes are removed.
			 */
			bs = tls->params.tls_bs;
			delta = (tls_len + tls->params.tls_tlen) & (bs - 1);
			pgs->trail_len -= delta;
		}
		m->m_len += pgs->hdr_len + pgs->trail_len;

		/* Populate the TLS header. */
		tlshdr = (void *)pgs->hdr;
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
			pgs->record_type = record_type;
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
			pgs->nrdy = pgs->npgs;
			*enq_cnt += pgs->npgs;
		}
	}
	return (0);
}

void
ktls_enqueue_to_free(struct mbuf_ext_pgs *pgs)
{
	struct ktls_wq *wq;
	bool running;

	/* Mark it for freeing. */
	pgs->mbuf = NULL;
	wq = &ktls_wq[pgs->tls->wq_index];
	mtx_lock(&wq->mtx);
	STAILQ_INSERT_TAIL(&wq->head, pgs, stailq);
	running = wq->running;
	mtx_unlock(&wq->mtx);
	if (!running)
		wakeup(wq);
}

void
ktls_enqueue(struct mbuf *m, struct socket *so, int page_count)
{
	struct mbuf_ext_pgs *pgs;
	struct ktls_wq *wq;
	bool running;

	KASSERT(((m->m_flags & (M_NOMAP | M_NOTREADY)) ==
	    (M_NOMAP | M_NOTREADY)),
	    ("ktls_enqueue: %p not unready & nomap mbuf\n", m));
	KASSERT(page_count != 0, ("enqueueing TLS mbuf with zero page count"));

	pgs = m->m_ext.ext_pgs;

	KASSERT(pgs->tls->mode == TCP_TLS_MODE_SW, ("!SW TLS mbuf"));

	pgs->enc_cnt = page_count;
	pgs->mbuf = m;

	/*
	 * Save a pointer to the socket.  The caller is responsible
	 * for taking an additional reference via soref().
	 */
	pgs->so = so;

	wq = &ktls_wq[pgs->tls->wq_index];
	mtx_lock(&wq->mtx);
	STAILQ_INSERT_TAIL(&wq->head, pgs, stailq);
	running = wq->running;
	mtx_unlock(&wq->mtx);
	if (!running)
		wakeup(wq);
	counter_u64_add(ktls_cnt_on, 1);
}

static __noinline void
ktls_encrypt(struct mbuf_ext_pgs *pgs)
{
	struct ktls_session *tls;
	struct socket *so;
	struct mbuf *m, *top;
	vm_paddr_t parray[1 + btoc(TLS_MAX_MSG_SIZE_V10_2)];
	struct iovec src_iov[1 + btoc(TLS_MAX_MSG_SIZE_V10_2)];
	struct iovec dst_iov[1 + btoc(TLS_MAX_MSG_SIZE_V10_2)];
	vm_page_t pg;
	int error, i, len, npages, off, total_pages;
	bool is_anon;

	so = pgs->so;
	tls = pgs->tls;
	top = pgs->mbuf;
	KASSERT(tls != NULL, ("tls = NULL, top = %p, pgs = %p\n", top, pgs));
	KASSERT(so != NULL, ("so = NULL, top = %p, pgs = %p\n", top, pgs));
#ifdef INVARIANTS
	pgs->so = NULL;
	pgs->mbuf = NULL;
#endif
	total_pages = pgs->enc_cnt;
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
		pgs = m->m_ext.ext_pgs;

		KASSERT(pgs->tls == tls,
		    ("different TLS sessions in a single mbuf chain: %p vs %p",
		    tls, pgs->tls));
		KASSERT((m->m_flags & (M_NOMAP | M_NOTREADY)) ==
		    (M_NOMAP | M_NOTREADY),
		    ("%p not unready & nomap mbuf (top = %p)\n", m, top));
		KASSERT(npages + pgs->npgs <= total_pages,
		    ("page count mismatch: top %p, total_pages %d, m %p", top,
		    total_pages, m));

		/*
		 * Generate source and destination ivoecs to pass to
		 * the SW encryption backend.  For writable mbufs, the
		 * destination iovec is a copy of the source and
		 * encryption is done in place.  For file-backed mbufs
		 * (from sendfile), anonymous wired pages are
		 * allocated and assigned to the destination iovec.
		 */
		is_anon = (pgs->flags & MBUF_PEXT_FLAG_ANON) != 0;

		off = pgs->first_pg_off;
		for (i = 0; i < pgs->npgs; i++, off = 0) {
			len = mbuf_ext_pg_len(pgs, i, off);
			src_iov[i].iov_len = len;
			src_iov[i].iov_base =
			    (char *)(void *)PHYS_TO_DMAP(pgs->pa[i]) + off;

			if (is_anon) {
				dst_iov[i].iov_base = src_iov[i].iov_base;
				dst_iov[i].iov_len = src_iov[i].iov_len;
				continue;
			}
retry_page:
			pg = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
			    VM_ALLOC_NOOBJ | VM_ALLOC_NODUMP | VM_ALLOC_WIRED);
			if (pg == NULL) {
				vm_wait(NULL);
				goto retry_page;
			}
			parray[i] = VM_PAGE_TO_PHYS(pg);
			dst_iov[i].iov_base =
			    (char *)(void *)PHYS_TO_DMAP(parray[i]) + off;
			dst_iov[i].iov_len = len;
		}

		npages += i;

		error = (*tls->sw_encrypt)(tls,
		    (const struct tls_record_layer *)pgs->hdr,
		    pgs->trail, src_iov, dst_iov, i, pgs->seqno,
		    pgs->record_type);
		if (error) {
			counter_u64_add(ktls_offload_failed_crypto, 1);
			break;
		}

		/*
		 * For file-backed mbufs, release the file-backed
		 * pages and replace them in the ext_pgs array with
		 * the anonymous wired pages allocated above.
		 */
		if (!is_anon) {
			/* Free the old pages. */
			m->m_ext.ext_free(m);

			/* Replace them with the new pages. */
			for (i = 0; i < pgs->npgs; i++)
				pgs->pa[i] = parray[i];

			/* Use the basic free routine. */
			m->m_ext.ext_free = mb_free_mext_pgs;

			/* Pages are now writable. */
			pgs->flags |= MBUF_PEXT_FLAG_ANON;
		}

		/*
		 * Drop a reference to the session now that it is no
		 * longer needed.  Existing code depends on encrypted
		 * records having no associated session vs
		 * yet-to-be-encrypted records having an associated
		 * session.
		 */
		pgs->tls = NULL;
		ktls_free(tls);
	}

	CURVNET_SET(so->so_vnet);
	if (error == 0) {
		(void)(*so->so_proto->pr_usrreqs->pru_ready)(so, top, npages);
	} else {
		so->so_proto->pr_usrreqs->pru_abort(so);
		so->so_error = EIO;
		mb_free_notready(top, total_pages);
	}

	SOCK_LOCK(so);
	sorele(so);
	CURVNET_RESTORE();
}

static void
ktls_work_thread(void *ctx)
{
	struct ktls_wq *wq = ctx;
	struct mbuf_ext_pgs *p, *n;
	struct ktls_session *tls;
	STAILQ_HEAD(, mbuf_ext_pgs) local_head;

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	fpu_kern_thread(0);
#endif
	for (;;) {
		mtx_lock(&wq->mtx);
		while (STAILQ_EMPTY(&wq->head)) {
			wq->running = false;
			mtx_sleep(wq, &wq->mtx, 0, "-", 0);
			wq->running = true;
		}

		STAILQ_INIT(&local_head);
		STAILQ_CONCAT(&local_head, &wq->head);
		mtx_unlock(&wq->mtx);

		STAILQ_FOREACH_SAFE(p, &local_head, stailq, n) {
			if (p->mbuf != NULL) {
				ktls_encrypt(p);
				counter_u64_add(ktls_cnt_on, -1);
			} else {
				tls = p->tls;
				ktls_free(tls);
				uma_zfree(zone_extpgs, p);
			}
		}
	}
}

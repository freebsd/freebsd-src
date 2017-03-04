/*-
 * Copyright (c) 2003-2009 RMI Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RMI_BSD
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#define __RMAN_RESOURCE_VISIBLE
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/mips_opcode.h>
#include <machine/asm.h>

#include <machine/param.h>
#include <machine/intr_machdep.h>
#include <machine/clock.h>	/* for DELAY */
#include <machine/cpuregs.h>
#include <machine/bus.h>	/* */
#include <machine/resource.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/brgphyreg.h>

#include <mips/rmi/interrupt.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/rmi_mips_exts.h>
#include <mips/rmi/rmi_boot_info.h>
#include <mips/rmi/board.h>

#include <mips/rmi/dev/xlr/debug.h>
#include <mips/rmi/dev/xlr/atx_cpld.h>
#include <mips/rmi/dev/xlr/xgmac_mdio.h>
#include <mips/rmi/dev/xlr/rge.h>

#include "miibus_if.h"

MODULE_DEPEND(rge, ether, 1, 1, 1);
MODULE_DEPEND(rge, miibus, 1, 1, 1);

/* #define DEBUG */

#define RGE_TX_THRESHOLD 1024
#define RGE_TX_Q_SIZE 1024

#ifdef DEBUG
#undef dbg_msg
int mac_debug = 1;

#define dbg_msg(fmt, args...) \
        do {\
            if (mac_debug) {\
                printf("[%s@%d|%s]: cpu_%d: " fmt, \
                __FILE__, __LINE__, __FUNCTION__,  xlr_cpu_id(), ##args);\
            }\
        } while(0);

#define DUMP_PACKETS
#else
#undef dbg_msg
#define dbg_msg(fmt, args...)
int mac_debug = 0;

#endif

#define MAC_B2B_IPG             88

/* frame sizes need to be cacheline aligned */
#define MAX_FRAME_SIZE          1536
#define MAX_FRAME_SIZE_JUMBO    9216

#define MAC_SKB_BACK_PTR_SIZE   SMP_CACHE_BYTES
#define MAC_PREPAD              0
#define BYTE_OFFSET             2
#define XLR_RX_BUF_SIZE (MAX_FRAME_SIZE+BYTE_OFFSET+MAC_PREPAD+MAC_SKB_BACK_PTR_SIZE+SMP_CACHE_BYTES)
#define MAC_CRC_LEN             4
#define MAX_NUM_MSGRNG_STN_CC   128

#define MAX_NUM_DESC		1024
#define MAX_SPILL_SIZE          (MAX_NUM_DESC + 128)

#define MAC_FRIN_TO_BE_SENT_THRESHOLD 16

#define MAX_FRIN_SPILL          (MAX_SPILL_SIZE << 2)
#define MAX_FROUT_SPILL         (MAX_SPILL_SIZE << 2)
#define MAX_CLASS_0_SPILL       (MAX_SPILL_SIZE << 2)
#define MAX_CLASS_1_SPILL       (MAX_SPILL_SIZE << 2)
#define MAX_CLASS_2_SPILL       (MAX_SPILL_SIZE << 2)
#define MAX_CLASS_3_SPILL       (MAX_SPILL_SIZE << 2)

/*****************************************************************
 * Phoenix Generic Mac driver
 *****************************************************************/

extern uint32_t cpu_ltop_map[32];

#ifdef ENABLED_DEBUG
static int port_counters[4][8] __aligned(XLR_CACHELINE_SIZE);

#define port_inc_counter(port, counter) 	atomic_add_int(&port_counters[port][(counter)], 1)
#else
#define port_inc_counter(port, counter)	/* Nothing */
#endif

int xlr_rge_tx_prepend[MAXCPU];
int xlr_rge_tx_done[MAXCPU];
int xlr_rge_get_p2d_failed[MAXCPU];
int xlr_rge_msg_snd_failed[MAXCPU];
int xlr_rge_tx_ok_done[MAXCPU];
int xlr_rge_rx_done[MAXCPU];
int xlr_rge_repl_done[MAXCPU];

/* #define mac_stats_add(x, val) ({(x) += (val);}) */
#define mac_stats_add(x, val) xlr_ldaddwu(val, &x)

#define XLR_MAX_CORE 8
#define RGE_LOCK_INIT(_sc, _name) \
  mtx_init(&(_sc)->rge_mtx, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define RGE_LOCK(_sc)   mtx_lock(&(_sc)->rge_mtx)
#define RGE_LOCK_ASSERT(_sc)  mtx_assert(&(_sc)->rge_mtx, MA_OWNED)
#define RGE_UNLOCK(_sc)   mtx_unlock(&(_sc)->rge_mtx)
#define RGE_LOCK_DESTROY(_sc) mtx_destroy(&(_sc)->rge_mtx)

#define XLR_MAX_MACS     8
#define XLR_MAX_TX_FRAGS 14
#define MAX_P2D_DESC_PER_PORT 512
struct p2d_tx_desc {
	uint64_t frag[XLR_MAX_TX_FRAGS + 2];
};

#define MAX_TX_RING_SIZE (XLR_MAX_MACS * MAX_P2D_DESC_PER_PORT * sizeof(struct p2d_tx_desc))

struct rge_softc *dev_mac[XLR_MAX_MACS];
static int dev_mac_xgs0;
static int dev_mac_gmac0;

static int gmac_common_init_done;


static int rge_probe(device_t);
static int rge_attach(device_t);
static int rge_detach(device_t);
static int rge_suspend(device_t);
static int rge_resume(device_t);
static void rge_release_resources(struct rge_softc *);
static void rge_rx(struct rge_softc *, vm_paddr_t paddr, int);
static void rge_intr(void *);
static void rge_start_locked(struct ifnet *, int);
static void rge_start(struct ifnet *);
static int rge_ioctl(struct ifnet *, u_long, caddr_t);
static void rge_init(void *);
static void rge_stop(struct rge_softc *);
static int rge_shutdown(device_t);
static void rge_reset(struct rge_softc *);

static struct mbuf *get_mbuf(void);
static void free_buf(vm_paddr_t paddr);
static void *get_buf(void);

static void xlr_mac_get_hwaddr(struct rge_softc *);
static void xlr_mac_setup_hwaddr(struct driver_data *);
static void rmi_xlr_mac_set_enable(struct driver_data *priv, int flag);
static void rmi_xlr_xgmac_init(struct driver_data *priv);
static void rmi_xlr_gmac_init(struct driver_data *priv);
static void mac_common_init(void);
static int rge_mii_write(device_t, int, int, int);
static int rge_mii_read(device_t, int, int);
static void rmi_xlr_mac_mii_statchg(device_t);
static int rmi_xlr_mac_mediachange(struct ifnet *);
static void rmi_xlr_mac_mediastatus(struct ifnet *, struct ifmediareq *);
static void xlr_mac_set_rx_mode(struct rge_softc *sc);
void
rmi_xlr_mac_msgring_handler(int bucket, int size, int code,
    int stid, struct msgrng_msg *msg,
    void *data);
static void mac_frin_replenish(void *);
static int rmi_xlr_mac_open(struct rge_softc *);
static int rmi_xlr_mac_close(struct rge_softc *);
static int
mac_xmit(struct mbuf *, struct rge_softc *,
    struct driver_data *, int, struct p2d_tx_desc *);
static int rmi_xlr_mac_xmit(struct mbuf *, struct rge_softc *, int, struct p2d_tx_desc *);
static struct rge_softc_stats *rmi_xlr_mac_get_stats(struct rge_softc *sc);
static void rmi_xlr_mac_set_multicast_list(struct rge_softc *sc);
static int rmi_xlr_mac_change_mtu(struct rge_softc *sc, int new_mtu);
static int rmi_xlr_mac_fill_rxfr(struct rge_softc *sc);
static void rmi_xlr_config_spill_area(struct driver_data *priv);
static int rmi_xlr_mac_set_speed(struct driver_data *s, xlr_mac_speed_t speed);
static int
rmi_xlr_mac_set_duplex(struct driver_data *s,
    xlr_mac_duplex_t duplex, xlr_mac_fc_t fc);
static void serdes_regs_init(struct driver_data *priv);
static int rmi_xlr_gmac_reset(struct driver_data *priv);

/*Statistics...*/
static int get_p2d_desc_failed = 0;
static int msg_snd_failed = 0;

SYSCTL_INT(_hw, OID_AUTO, get_p2d_failed, CTLFLAG_RW,
    &get_p2d_desc_failed, 0, "p2d desc failed");
SYSCTL_INT(_hw, OID_AUTO, msg_snd_failed, CTLFLAG_RW,
    &msg_snd_failed, 0, "msg snd failed");

struct callout xlr_tx_stop_bkp;

static device_method_t rge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, rge_probe),
	DEVMETHOD(device_attach, rge_attach),
	DEVMETHOD(device_detach, rge_detach),
	DEVMETHOD(device_shutdown, rge_shutdown),
	DEVMETHOD(device_suspend, rge_suspend),
	DEVMETHOD(device_resume, rge_resume),

	/* MII interface */
	DEVMETHOD(miibus_readreg, rge_mii_read),
	DEVMETHOD(miibus_statchg, rmi_xlr_mac_mii_statchg),
	DEVMETHOD(miibus_writereg, rge_mii_write),
	{0, 0}
};

static driver_t rge_driver = {
	"rge",
	rge_methods,
	sizeof(struct rge_softc)
};

static devclass_t rge_devclass;

DRIVER_MODULE(rge, iodi, rge_driver, rge_devclass, 0, 0);
DRIVER_MODULE(miibus, rge, miibus_driver, miibus_devclass, 0, 0);

#ifndef __STR
#define __STR(x) #x
#endif
#ifndef STR
#define STR(x) __STR(x)
#endif

void *xlr_tx_ring_mem;

struct tx_desc_node {
	struct p2d_tx_desc *ptr;
	            TAILQ_ENTRY(tx_desc_node) list;
};

#define XLR_MAX_TX_DESC_NODES (XLR_MAX_MACS * MAX_P2D_DESC_PER_PORT)
struct tx_desc_node tx_desc_nodes[XLR_MAX_TX_DESC_NODES];
static volatile int xlr_tot_avail_p2d[XLR_MAX_CORE];
static int xlr_total_active_core = 0;

/*
 * This should contain the list of all free tx frag desc nodes pointing to tx
 * p2d arrays
 */
static
TAILQ_HEAD(, tx_desc_node) tx_frag_desc[XLR_MAX_CORE] =
{
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[0]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[1]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[2]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[3]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[4]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[5]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[6]),
	TAILQ_HEAD_INITIALIZER(tx_frag_desc[7]),
};

/* This contains a list of free tx frag node descriptors */
static
TAILQ_HEAD(, tx_desc_node) free_tx_frag_desc[XLR_MAX_CORE] =
{
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[0]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[1]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[2]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[3]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[4]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[5]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[6]),
	TAILQ_HEAD_INITIALIZER(free_tx_frag_desc[7]),
};

static struct mtx tx_desc_lock[XLR_MAX_CORE];

static inline void
mac_make_desc_rfr(struct msgrng_msg *msg,
    vm_paddr_t addr)
{
	msg->msg0 = (uint64_t) addr & 0xffffffffe0ULL;
	msg->msg1 = msg->msg2 = msg->msg3 = 0;
}

#define MAC_TX_DESC_ALIGNMENT (XLR_CACHELINE_SIZE - 1)

static void
init_p2d_allocation(void)
{
	int active_core[8] = {0};
	int i = 0;
	uint32_t cpumask;
	int cpu;

	cpumask = xlr_hw_thread_mask;

	for (i = 0; i < 32; i++) {
		if (cpumask & (1 << i)) {
			cpu = i;
			if (!active_core[cpu / 4]) {
				active_core[cpu / 4] = 1;
				xlr_total_active_core++;
			}
		}
	}
	for (i = 0; i < XLR_MAX_CORE; i++) {
		if (active_core[i])
			xlr_tot_avail_p2d[i] = XLR_MAX_TX_DESC_NODES / xlr_total_active_core;
	}
	printf("Total Active Core %d\n", xlr_total_active_core);
}


static void
init_tx_ring(void)
{
	int i;
	int j = 0;
	struct tx_desc_node *start, *node;
	struct p2d_tx_desc *tx_desc;
	vm_paddr_t paddr;
	vm_offset_t unmapped_addr;

	for (i = 0; i < XLR_MAX_CORE; i++)
		mtx_init(&tx_desc_lock[i], "xlr tx_desc", NULL, MTX_SPIN);

	start = &tx_desc_nodes[0];
	/* TODO: try to get this from KSEG0 */
	xlr_tx_ring_mem = contigmalloc((MAX_TX_RING_SIZE + XLR_CACHELINE_SIZE),
	    M_DEVBUF, M_NOWAIT | M_ZERO, 0,
	    0x10000000, XLR_CACHELINE_SIZE, 0);

	if (xlr_tx_ring_mem == NULL) {
		panic("TX ring memory allocation failed");
	}
	paddr = vtophys((vm_offset_t)xlr_tx_ring_mem);

	unmapped_addr = MIPS_PHYS_TO_KSEG0(paddr);


	tx_desc = (struct p2d_tx_desc *)unmapped_addr;

	for (i = 0; i < XLR_MAX_TX_DESC_NODES; i++) {
		node = start + i;
		node->ptr = tx_desc;
		tx_desc++;
		TAILQ_INSERT_HEAD(&tx_frag_desc[j], node, list);
		j = (i / (XLR_MAX_TX_DESC_NODES / xlr_total_active_core));
	}
}

static inline struct p2d_tx_desc *
get_p2d_desc(void)
{
	struct tx_desc_node *node;
	struct p2d_tx_desc *tx_desc = NULL;
	int cpu = xlr_core_id();

	mtx_lock_spin(&tx_desc_lock[cpu]);
	node = TAILQ_FIRST(&tx_frag_desc[cpu]);
	if (node) {
		xlr_tot_avail_p2d[cpu]--;
		TAILQ_REMOVE(&tx_frag_desc[cpu], node, list);
		tx_desc = node->ptr;
		TAILQ_INSERT_HEAD(&free_tx_frag_desc[cpu], node, list);
	} else {
		/* Increment p2d desc fail count */
		get_p2d_desc_failed++;
	}
	mtx_unlock_spin(&tx_desc_lock[cpu]);
	return tx_desc;
}
static void
free_p2d_desc(struct p2d_tx_desc *tx_desc)
{
	struct tx_desc_node *node;
	int cpu = xlr_core_id();

	mtx_lock_spin(&tx_desc_lock[cpu]);
	node = TAILQ_FIRST(&free_tx_frag_desc[cpu]);
	KASSERT((node != NULL), ("Free TX frag node list is empty\n"));

	TAILQ_REMOVE(&free_tx_frag_desc[cpu], node, list);
	node->ptr = tx_desc;
	TAILQ_INSERT_HEAD(&tx_frag_desc[cpu], node, list);
	xlr_tot_avail_p2d[cpu]++;
	mtx_unlock_spin(&tx_desc_lock[cpu]);

}

static int
build_frag_list(struct mbuf *m_head, struct msgrng_msg *p2p_msg, struct p2d_tx_desc *tx_desc)
{
	struct mbuf *m;
	vm_paddr_t paddr;
	uint64_t p2d_len;
	int nfrag;
	vm_paddr_t p1, p2;
	uint32_t len1, len2;
	vm_offset_t taddr;
	uint64_t fr_stid;

	fr_stid = (xlr_core_id() << 3) + xlr_thr_id() + 4;

	if (tx_desc == NULL)
		return 1;

	nfrag = 0;
	for (m = m_head; m != NULL; m = m->m_next) {
		if ((nfrag + 1) >= XLR_MAX_TX_FRAGS) {
			free_p2d_desc(tx_desc);
			return 1;
		}
		if (m->m_len != 0) {
			paddr = vtophys(mtod(m, vm_offset_t));
			p1 = paddr + m->m_len;
			p2 = vtophys(((vm_offset_t)m->m_data + m->m_len));
			if (p1 != p2) {
				len1 = (uint32_t)
				    (PAGE_SIZE - (paddr & PAGE_MASK));
				tx_desc->frag[nfrag] = (127ULL << 54) |
				    ((uint64_t) len1 << 40) | paddr;
				nfrag++;
				taddr = (vm_offset_t)m->m_data + len1;
				p2 = vtophys(taddr);
				len2 = m->m_len - len1;
				if (len2 == 0)
					continue;
				if (nfrag >= XLR_MAX_TX_FRAGS)
					panic("TX frags exceeded");

				tx_desc->frag[nfrag] = (127ULL << 54) |
				    ((uint64_t) len2 << 40) | p2;

				taddr += len2;
				p1 = vtophys(taddr);

				if ((p2 + len2) != p1) {
					printf("p1 = %p p2 = %p\n", (void *)p1, (void *)p2);
					printf("len1 = %x len2 = %x\n", len1,
					    len2);
					printf("m_data %p\n", m->m_data);
					DELAY(1000000);
					panic("Multiple Mbuf segment discontiguous\n");
				}
			} else {
				tx_desc->frag[nfrag] = (127ULL << 54) |
				    ((uint64_t) m->m_len << 40) | paddr;
			}
			nfrag++;
		}
	}
	/* set eop in the last tx p2d desc */
	tx_desc->frag[nfrag - 1] |= (1ULL << 63);
	paddr = vtophys((vm_offset_t)tx_desc);
	tx_desc->frag[nfrag] = (1ULL << 63) | (fr_stid << 54) | paddr;
	nfrag++;
	tx_desc->frag[XLR_MAX_TX_FRAGS] = (uint64_t)(intptr_t)tx_desc;
	tx_desc->frag[XLR_MAX_TX_FRAGS + 1] = (uint64_t)(intptr_t)m_head;

	p2d_len = (nfrag * 8);
	p2p_msg->msg0 = (1ULL << 63) | (1ULL << 62) | (127ULL << 54) |
	    (p2d_len << 40) | paddr;

	return 0;
}
static void
release_tx_desc(struct msgrng_msg *msg, int rel_buf)
{
	struct p2d_tx_desc *tx_desc, *chk_addr;
	struct mbuf *m;

	tx_desc = (struct p2d_tx_desc *)MIPS_PHYS_TO_KSEG0(msg->msg0);
	chk_addr = (struct p2d_tx_desc *)(intptr_t)tx_desc->frag[XLR_MAX_TX_FRAGS];
	if (tx_desc != chk_addr) {
		printf("Address %p does not match with stored addr %p - we leaked a descriptor\n",
		    tx_desc, chk_addr);
		return;
	}
	if (rel_buf) {
		m = (struct mbuf *)(intptr_t)tx_desc->frag[XLR_MAX_TX_FRAGS + 1];
		m_freem(m);
	}
	free_p2d_desc(tx_desc);
}


static struct mbuf *
get_mbuf(void)
{
	struct mbuf *m_new = NULL;

	if ((m_new = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR)) == NULL)
		return NULL;

	m_new->m_len = MCLBYTES;
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	return m_new;
}

static void
free_buf(vm_paddr_t paddr)
{
	struct mbuf *m;
	uint64_t mag;
	uint32_t sr;

	sr = xlr_enable_kx();
	m = (struct mbuf *)(intptr_t)xlr_paddr_ld(paddr - XLR_CACHELINE_SIZE);
	mag = xlr_paddr_ld(paddr - XLR_CACHELINE_SIZE + sizeof(uint64_t));
	xlr_restore_kx(sr);
	if (mag != 0xf00bad) {
		printf("Something is wrong kseg:%lx found mag:%lx not 0xf00bad\n",
		    (u_long)paddr, (u_long)mag);
		return;
	}
	if (m != NULL)
		m_freem(m);
}

static void *
get_buf(void)
{
	struct mbuf *m_new = NULL;
	uint64_t *md;
#ifdef INVARIANTS
	vm_paddr_t temp1, temp2;
#endif

	m_new = get_mbuf();
	if (m_new == NULL)
		return NULL;

	m_adj(m_new, XLR_CACHELINE_SIZE - ((uintptr_t)m_new->m_data & 0x1f));
	md = (uint64_t *)m_new->m_data;
	md[0] = (uintptr_t)m_new;	/* Back Ptr */
	md[1] = 0xf00bad;
	m_adj(m_new, XLR_CACHELINE_SIZE);

#ifdef INVARIANTS
	temp1 = vtophys((vm_offset_t)m_new->m_data);
	temp2 = vtophys((vm_offset_t)m_new->m_data + 1536);
	if ((temp1 + 1536) != temp2)
		panic("ALLOCED BUFFER IS NOT CONTIGUOUS\n");
#endif
	return (void *)m_new->m_data;
}

/**********************************************************************
 **********************************************************************/
static void
rmi_xlr_mac_set_enable(struct driver_data *priv, int flag)
{
	uint32_t regval;
	int tx_threshold = 1518;

	if (flag) {
		regval = xlr_read_reg(priv->mmio, R_TX_CONTROL);
		regval |= (1 << O_TX_CONTROL__TxEnable) |
		    (tx_threshold << O_TX_CONTROL__TxThreshold);

		xlr_write_reg(priv->mmio, R_TX_CONTROL, regval);

		regval = xlr_read_reg(priv->mmio, R_RX_CONTROL);
		regval |= 1 << O_RX_CONTROL__RxEnable;
		if (priv->mode == XLR_PORT0_RGMII)
			regval |= 1 << O_RX_CONTROL__RGMII;
		xlr_write_reg(priv->mmio, R_RX_CONTROL, regval);

		regval = xlr_read_reg(priv->mmio, R_MAC_CONFIG_1);
		regval |= (O_MAC_CONFIG_1__txen | O_MAC_CONFIG_1__rxen);
		xlr_write_reg(priv->mmio, R_MAC_CONFIG_1, regval);
	} else {
		regval = xlr_read_reg(priv->mmio, R_TX_CONTROL);
		regval &= ~((1 << O_TX_CONTROL__TxEnable) |
		    (tx_threshold << O_TX_CONTROL__TxThreshold));

		xlr_write_reg(priv->mmio, R_TX_CONTROL, regval);

		regval = xlr_read_reg(priv->mmio, R_RX_CONTROL);
		regval &= ~(1 << O_RX_CONTROL__RxEnable);
		xlr_write_reg(priv->mmio, R_RX_CONTROL, regval);

		regval = xlr_read_reg(priv->mmio, R_MAC_CONFIG_1);
		regval &= ~(O_MAC_CONFIG_1__txen | O_MAC_CONFIG_1__rxen);
		xlr_write_reg(priv->mmio, R_MAC_CONFIG_1, regval);
	}
}

/**********************************************************************
 **********************************************************************/
static __inline__ int
xlr_mac_send_fr(struct driver_data *priv,
    vm_paddr_t addr, int len)
{
	struct msgrng_msg msg;
	int stid = priv->rfrbucket;
	int code, ret;
	uint32_t msgrng_flags;
#ifdef INVARIANTS
	int i = 0;
#endif

	mac_make_desc_rfr(&msg, addr);

	/* Send the packet to MAC */
	dbg_msg("mac_%d: Sending free packet %lx to stid %d\n",
	    priv->instance, (u_long)addr, stid);
	if (priv->type == XLR_XGMAC)
		code = MSGRNG_CODE_XGMAC;        /* WHY? */
	else
		code = MSGRNG_CODE_MAC;

	do {
		msgrng_flags = msgrng_access_enable();
		ret = message_send(1, code, stid, &msg);
		msgrng_restore(msgrng_flags);
		KASSERT(i++ < 100000, ("Too many credit fails\n"));
	} while (ret != 0);

	return 0;
}

/**************************************************************/

static void
xgmac_mdio_setup(volatile unsigned int *_mmio)
{
	int i;
	uint32_t rd_data;

	for (i = 0; i < 4; i++) {
		rd_data = xmdio_read(_mmio, 1, 0x8000 + i);
		rd_data = rd_data & 0xffffdfff;	/* clear isolate bit */
		xmdio_write(_mmio, 1, 0x8000 + i, rd_data);
	}
}

/**********************************************************************
 *  Init MII interface
 *
 *  Input parameters:
 *  	   s - priv structure
 ********************************************************************* */
#define PHY_STATUS_RETRIES 25000

static void
rmi_xlr_mac_mii_init(struct driver_data *priv)
{
	xlr_reg_t *mii_mmio = priv->mii_mmio;

	/* use the lowest clock divisor - divisor 28 */
	xlr_write_reg(mii_mmio, R_MII_MGMT_CONFIG, 0x07);
}

/**********************************************************************
 *  Read a PHY register.
 *
 *  Input parameters:
 *  	   s - priv structure
 *  	   phyaddr - PHY's address
 *  	   regidx = index of register to read
 *
 *  Return value:
 *  	   value read, or 0 if an error occurred.
 ********************************************************************* */

static int
rge_mii_read_internal(xlr_reg_t * mii_mmio, int phyaddr, int regidx)
{
	int i = 0;

	/* setup the phy reg to be used */
	xlr_write_reg(mii_mmio, R_MII_MGMT_ADDRESS,
	    (phyaddr << 8) | (regidx << 0));
	/* Issue the read command */
	xlr_write_reg(mii_mmio, R_MII_MGMT_COMMAND,
	    (1 << O_MII_MGMT_COMMAND__rstat));

	/* poll for the read cycle to complete */
	for (i = 0; i < PHY_STATUS_RETRIES; i++) {
		if (xlr_read_reg(mii_mmio, R_MII_MGMT_INDICATORS) == 0)
			break;
	}

	/* clear the read cycle */
	xlr_write_reg(mii_mmio, R_MII_MGMT_COMMAND, 0);

	if (i == PHY_STATUS_RETRIES) {
		return 0xffffffff;
	}
	/* Read the data back */
	return xlr_read_reg(mii_mmio, R_MII_MGMT_STATUS);
}

static int
rge_mii_read(device_t dev, int phyaddr, int regidx)
{
	struct rge_softc *sc = device_get_softc(dev);

	return rge_mii_read_internal(sc->priv.mii_mmio, phyaddr, regidx);
}

/**********************************************************************
 *  Set MII hooks to newly selected media
 *
 *  Input parameters:
 *  	   ifp - Interface Pointer
 *
 *  Return value:
 *  	   nothing
 ********************************************************************* */
static int
rmi_xlr_mac_mediachange(struct ifnet *ifp)
{
	struct rge_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		mii_mediachg(&sc->rge_mii);

	return 0;
}

/**********************************************************************
 *  Get the current interface media status
 *
 *  Input parameters:
 *  	   ifp  - Interface Pointer
 *  	   ifmr - Interface media request ptr
 *
 *  Return value:
 *  	   nothing
 ********************************************************************* */
static void
rmi_xlr_mac_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct rge_softc *sc = ifp->if_softc;

	/* Check whether this is interface is active or not. */
	ifmr->ifm_status = IFM_AVALID;
	if (sc->link_up) {
		ifmr->ifm_status |= IFM_ACTIVE;
	} else {
		ifmr->ifm_active = IFM_ETHER;
	}
}

/**********************************************************************
 *  Write a value to a PHY register.
 *
 *  Input parameters:
 *  	   s - priv structure
 *  	   phyaddr - PHY to use
 *  	   regidx - register within the PHY
 *  	   regval - data to write to register
 *
 *  Return value:
 *  	   nothing
 ********************************************************************* */
static void
rge_mii_write_internal(xlr_reg_t * mii_mmio, int phyaddr, int regidx, int regval)
{
	int i = 0;

	xlr_write_reg(mii_mmio, R_MII_MGMT_ADDRESS,
	    (phyaddr << 8) | (regidx << 0));

	/* Write the data which starts the write cycle */
	xlr_write_reg(mii_mmio, R_MII_MGMT_WRITE_DATA, regval);

	/* poll for the write cycle to complete */
	for (i = 0; i < PHY_STATUS_RETRIES; i++) {
		if (xlr_read_reg(mii_mmio, R_MII_MGMT_INDICATORS) == 0)
			break;
	}

	return;
}

static int
rge_mii_write(device_t dev, int phyaddr, int regidx, int regval)
{
	struct rge_softc *sc = device_get_softc(dev);

	rge_mii_write_internal(sc->priv.mii_mmio, phyaddr, regidx, regval);
	return (0);
}

static void
rmi_xlr_mac_mii_statchg(struct device *dev)
{
}

static void
serdes_regs_init(struct driver_data *priv)
{
	xlr_reg_t *mmio_gpio = (xlr_reg_t *) (xlr_io_base + XLR_IO_GPIO_OFFSET);

	/* Initialize SERDES CONTROL Registers */
	rge_mii_write_internal(priv->serdes_mmio, 26, 0, 0x6DB0);
	rge_mii_write_internal(priv->serdes_mmio, 26, 1, 0xFFFF);
	rge_mii_write_internal(priv->serdes_mmio, 26, 2, 0xB6D0);
	rge_mii_write_internal(priv->serdes_mmio, 26, 3, 0x00FF);
	rge_mii_write_internal(priv->serdes_mmio, 26, 4, 0x0000);
	rge_mii_write_internal(priv->serdes_mmio, 26, 5, 0x0000);
	rge_mii_write_internal(priv->serdes_mmio, 26, 6, 0x0005);
	rge_mii_write_internal(priv->serdes_mmio, 26, 7, 0x0001);
	rge_mii_write_internal(priv->serdes_mmio, 26, 8, 0x0000);
	rge_mii_write_internal(priv->serdes_mmio, 26, 9, 0x0000);
	rge_mii_write_internal(priv->serdes_mmio, 26, 10, 0x0000);

	/*
	 * GPIO setting which affect the serdes - needs figuring out
	 */
	DELAY(100);
	xlr_write_reg(mmio_gpio, 0x20, 0x7e6802);
	xlr_write_reg(mmio_gpio, 0x10, 0x7104);
	DELAY(100);
	
	/* 
	 * This kludge is needed to setup serdes (?) clock correctly on some
	 * XLS boards
	 */
	if ((xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XI ||
	    xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XII) &&
	    xlr_boot1_info.board_minor_version == 4) {
		/* use 125 Mhz instead of 156.25Mhz ref clock */
		DELAY(100);
		xlr_write_reg(mmio_gpio, 0x10, 0x7103);
		xlr_write_reg(mmio_gpio, 0x21, 0x7103);
		DELAY(100);
	}

	return;
}

static void
serdes_autoconfig(struct driver_data *priv)
{
	int delay = 100000;

	/* Enable Auto negotiation in the PCS Layer */
	rge_mii_write_internal(priv->pcs_mmio, 27, 0, 0x1000);
	DELAY(delay);
	rge_mii_write_internal(priv->pcs_mmio, 27, 0, 0x0200);
	DELAY(delay);

	rge_mii_write_internal(priv->pcs_mmio, 28, 0, 0x1000);
	DELAY(delay);
	rge_mii_write_internal(priv->pcs_mmio, 28, 0, 0x0200);
	DELAY(delay);

	rge_mii_write_internal(priv->pcs_mmio, 29, 0, 0x1000);
	DELAY(delay);
	rge_mii_write_internal(priv->pcs_mmio, 29, 0, 0x0200);
	DELAY(delay);

	rge_mii_write_internal(priv->pcs_mmio, 30, 0, 0x1000);
	DELAY(delay);
	rge_mii_write_internal(priv->pcs_mmio, 30, 0, 0x0200);
	DELAY(delay);

}

/*****************************************************************
 * Initialize GMAC
 *****************************************************************/
static void
rmi_xlr_config_pde(struct driver_data *priv)
{
	int i = 0, cpu = 0, bucket = 0;
	uint64_t bucket_map = 0;

	/* uint32_t desc_pack_ctrl = 0; */
	uint32_t cpumask;

	cpumask = 0x1;
#ifdef SMP
	/*
         * rge may be called before SMP start in a BOOTP/NFSROOT
         * setup. we will distribute packets to other cpus only when
         * the SMP is started.
	 */
	if (smp_started)
		cpumask = xlr_hw_thread_mask;
#endif

	for (i = 0; i < MAXCPU; i++) {
		if (cpumask & (1 << i)) {
			cpu = i;
			bucket = ((cpu >> 2) << 3);
			bucket_map |= (3ULL << bucket);
		}
	}
	printf("rmi_xlr_config_pde: bucket_map=%jx\n", (uintmax_t)bucket_map);

	/* bucket_map = 0x1; */
	xlr_write_reg(priv->mmio, R_PDE_CLASS_0, (bucket_map & 0xffffffff));
	xlr_write_reg(priv->mmio, R_PDE_CLASS_0 + 1,
	    ((bucket_map >> 32) & 0xffffffff));

	xlr_write_reg(priv->mmio, R_PDE_CLASS_1, (bucket_map & 0xffffffff));
	xlr_write_reg(priv->mmio, R_PDE_CLASS_1 + 1,
	    ((bucket_map >> 32) & 0xffffffff));

	xlr_write_reg(priv->mmio, R_PDE_CLASS_2, (bucket_map & 0xffffffff));
	xlr_write_reg(priv->mmio, R_PDE_CLASS_2 + 1,
	    ((bucket_map >> 32) & 0xffffffff));

	xlr_write_reg(priv->mmio, R_PDE_CLASS_3, (bucket_map & 0xffffffff));
	xlr_write_reg(priv->mmio, R_PDE_CLASS_3 + 1,
	    ((bucket_map >> 32) & 0xffffffff));
}

static void
rge_smp_update_pde(void *dummy __unused)
{
	int i;
	struct driver_data *priv;
	struct rge_softc *sc;

	printf("Updating packet distribution for SMP\n");
	for (i = 0; i < XLR_MAX_MACS; i++) {
		sc = dev_mac[i];
		if (!sc)
			continue;
		priv = &(sc->priv);
		rmi_xlr_mac_set_enable(priv, 0);
		rmi_xlr_config_pde(priv);
		rmi_xlr_mac_set_enable(priv, 1);
	}
}

SYSINIT(rge_smp_update_pde, SI_SUB_SMP, SI_ORDER_ANY, rge_smp_update_pde, NULL);


static void
rmi_xlr_config_parser(struct driver_data *priv)
{
	/*
	 * Mark it as no classification The parser extract is gauranteed to
	 * be zero with no classfication
	 */
	xlr_write_reg(priv->mmio, R_L2TYPE_0, 0x00);

	xlr_write_reg(priv->mmio, R_L2TYPE_0, 0x01);

	/* configure the parser : L2 Type is configured in the bootloader */
	/* extract IP: src, dest protocol */
	xlr_write_reg(priv->mmio, R_L3CTABLE,
	    (9 << 20) | (1 << 19) | (1 << 18) | (0x01 << 16) |
	    (0x0800 << 0));
	xlr_write_reg(priv->mmio, R_L3CTABLE + 1,
	    (12 << 25) | (4 << 21) | (16 << 14) | (4 << 10));

}

static void
rmi_xlr_config_classifier(struct driver_data *priv)
{
	int i = 0;

	if (priv->type == XLR_XGMAC) {
		/* xgmac translation table doesn't have sane values on reset */
		for (i = 0; i < 64; i++)
			xlr_write_reg(priv->mmio, R_TRANSLATETABLE + i, 0x0);

		/*
		 * use upper 7 bits of the parser extract to index the
		 * translate table
		 */
		xlr_write_reg(priv->mmio, R_PARSERCONFIGREG, 0x0);
	}
}

enum {
	SGMII_SPEED_10 = 0x00000000,
	SGMII_SPEED_100 = 0x02000000,
	SGMII_SPEED_1000 = 0x04000000,
};

static void
rmi_xlr_gmac_config_speed(struct driver_data *priv)
{
	int phy_addr = priv->phy_addr;
	xlr_reg_t *mmio = priv->mmio;
	struct rge_softc *sc = priv->sc;

	priv->speed = rge_mii_read_internal(priv->mii_mmio, phy_addr, 28);
	priv->link = rge_mii_read_internal(priv->mii_mmio, phy_addr, 1) & 0x4;
	priv->speed = (priv->speed >> 3) & 0x03;

	if (priv->speed == xlr_mac_speed_10) {
		if (priv->mode != XLR_RGMII)
			xlr_write_reg(mmio, R_INTERFACE_CONTROL, SGMII_SPEED_10);
		xlr_write_reg(mmio, R_MAC_CONFIG_2, 0x7117);
		xlr_write_reg(mmio, R_CORECONTROL, 0x02);
		printf("%s: [10Mbps]\n", device_get_nameunit(sc->rge_dev));
		sc->rge_mii.mii_media.ifm_media = IFM_ETHER | IFM_AUTO | IFM_10_T | IFM_FDX;
		sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER | IFM_AUTO | IFM_10_T | IFM_FDX;
		sc->rge_mii.mii_media_active = IFM_ETHER | IFM_AUTO | IFM_10_T | IFM_FDX;
	} else if (priv->speed == xlr_mac_speed_100) {
		if (priv->mode != XLR_RGMII)
			xlr_write_reg(mmio, R_INTERFACE_CONTROL, SGMII_SPEED_100);
		xlr_write_reg(mmio, R_MAC_CONFIG_2, 0x7117);
		xlr_write_reg(mmio, R_CORECONTROL, 0x01);
		printf("%s: [100Mbps]\n", device_get_nameunit(sc->rge_dev));
		sc->rge_mii.mii_media.ifm_media = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
		sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
		sc->rge_mii.mii_media_active = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
	} else {
		if (priv->speed != xlr_mac_speed_1000) {
			if (priv->mode != XLR_RGMII)
				xlr_write_reg(mmio, R_INTERFACE_CONTROL, SGMII_SPEED_100);
			printf("PHY reported unknown MAC speed, defaulting to 100Mbps\n");
			xlr_write_reg(mmio, R_MAC_CONFIG_2, 0x7117);
			xlr_write_reg(mmio, R_CORECONTROL, 0x01);
			sc->rge_mii.mii_media.ifm_media = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
			sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
			sc->rge_mii.mii_media_active = IFM_ETHER | IFM_AUTO | IFM_100_TX | IFM_FDX;
		} else {
			if (priv->mode != XLR_RGMII)
				xlr_write_reg(mmio, R_INTERFACE_CONTROL, SGMII_SPEED_1000);
			xlr_write_reg(mmio, R_MAC_CONFIG_2, 0x7217);
			xlr_write_reg(mmio, R_CORECONTROL, 0x00);
			printf("%s: [1000Mbps]\n", device_get_nameunit(sc->rge_dev));
			sc->rge_mii.mii_media.ifm_media = IFM_ETHER | IFM_AUTO | IFM_1000_T | IFM_FDX;
			sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER | IFM_AUTO | IFM_1000_T | IFM_FDX;
			sc->rge_mii.mii_media_active = IFM_ETHER | IFM_AUTO | IFM_1000_T | IFM_FDX;
		}
	}

	if (!priv->link) {
		sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER;
		sc->link_up = 0;
	} else {
		sc->link_up = 1;
	}
}

/*****************************************************************
 * Initialize XGMAC
 *****************************************************************/
static void
rmi_xlr_xgmac_init(struct driver_data *priv)
{
	int i = 0;
	xlr_reg_t *mmio = priv->mmio;
	int id = priv->instance;
	struct rge_softc *sc = priv->sc;
	volatile unsigned short *cpld;

	cpld = (volatile unsigned short *)0xBD840000;

	xlr_write_reg(priv->mmio, R_DESC_PACK_CTRL,
	    (MAX_FRAME_SIZE << O_DESC_PACK_CTRL__RegularSize) | (4 << 20));
	xlr_write_reg(priv->mmio, R_BYTEOFFSET0, BYTE_OFFSET);
	rmi_xlr_config_pde(priv);
	rmi_xlr_config_parser(priv);
	rmi_xlr_config_classifier(priv);

	xlr_write_reg(priv->mmio, R_MSG_TX_THRESHOLD, 1);

	/* configure the XGMAC Registers */
	xlr_write_reg(mmio, R_XGMAC_CONFIG_1, 0x50000026);

	/* configure the XGMAC_GLUE Registers */
	xlr_write_reg(mmio, R_DMACR0, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR1, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR2, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR3, 0xffffffff);
	xlr_write_reg(mmio, R_STATCTRL, 0x04);
	xlr_write_reg(mmio, R_L2ALLOCCTRL, 0xffffffff);

	xlr_write_reg(mmio, R_XGMACPADCALIBRATION, 0x030);
	xlr_write_reg(mmio, R_EGRESSFIFOCARVINGSLOTS, 0x0f);
	xlr_write_reg(mmio, R_L2ALLOCCTRL, 0xffffffff);
	xlr_write_reg(mmio, R_XGMAC_MIIM_CONFIG, 0x3e);

	/*
	 * take XGMII phy out of reset
	 */
	/*
	 * we are pulling everything out of reset because writing a 0 would
	 * reset other devices on the chip
	 */
	cpld[ATX_CPLD_RESET_1] = 0xffff;
	cpld[ATX_CPLD_MISC_CTRL] = 0xffff;
	cpld[ATX_CPLD_RESET_2] = 0xffff;

	xgmac_mdio_setup(mmio);

	rmi_xlr_config_spill_area(priv);

	if (id == 0) {
		for (i = 0; i < 16; i++) {
			xlr_write_reg(mmio, R_XGS_TX0_BUCKET_SIZE + i,
			    bucket_sizes.
			    bucket[MSGRNG_STNID_XGS0_TX + i]);
		}

		xlr_write_reg(mmio, R_XGS_JFR_BUCKET_SIZE,
		    bucket_sizes.bucket[MSGRNG_STNID_XMAC0JFR]);
		xlr_write_reg(mmio, R_XGS_RFR_BUCKET_SIZE,
		    bucket_sizes.bucket[MSGRNG_STNID_XMAC0RFR]);

		for (i = 0; i < MAX_NUM_MSGRNG_STN_CC; i++) {
			xlr_write_reg(mmio, R_CC_CPU0_0 + i,
			    cc_table_xgs_0.
			    counters[i >> 3][i & 0x07]);
		}
	} else if (id == 1) {
		for (i = 0; i < 16; i++) {
			xlr_write_reg(mmio, R_XGS_TX0_BUCKET_SIZE + i,
			    bucket_sizes.
			    bucket[MSGRNG_STNID_XGS1_TX + i]);
		}

		xlr_write_reg(mmio, R_XGS_JFR_BUCKET_SIZE,
		    bucket_sizes.bucket[MSGRNG_STNID_XMAC1JFR]);
		xlr_write_reg(mmio, R_XGS_RFR_BUCKET_SIZE,
		    bucket_sizes.bucket[MSGRNG_STNID_XMAC1RFR]);

		for (i = 0; i < MAX_NUM_MSGRNG_STN_CC; i++) {
			xlr_write_reg(mmio, R_CC_CPU0_0 + i,
			    cc_table_xgs_1.
			    counters[i >> 3][i & 0x07]);
		}
	}
	sc->rge_mii.mii_media.ifm_media = IFM_ETHER | IFM_AUTO | IFM_10G_SR | IFM_FDX;
	sc->rge_mii.mii_media.ifm_media |= (IFM_AVALID | IFM_ACTIVE);
	sc->rge_mii.mii_media.ifm_cur->ifm_media = IFM_ETHER | IFM_AUTO | IFM_10G_SR | IFM_FDX;
	sc->rge_mii.mii_media_active = IFM_ETHER | IFM_AUTO | IFM_10G_SR | IFM_FDX;
	sc->rge_mii.mii_media.ifm_cur->ifm_media |= (IFM_AVALID | IFM_ACTIVE);

	priv->init_frin_desc = 1;
}

/*******************************************************
 * Initialization gmac
 *******************************************************/
static int
rmi_xlr_gmac_reset(struct driver_data *priv)
{
	volatile uint32_t val;
	xlr_reg_t *mmio = priv->mmio;
	int i, maxloops = 100;

	/* Disable MAC RX */
	val = xlr_read_reg(mmio, R_MAC_CONFIG_1);
	val &= ~0x4;
	xlr_write_reg(mmio, R_MAC_CONFIG_1, val);

	/* Disable Core RX */
	val = xlr_read_reg(mmio, R_RX_CONTROL);
	val &= ~0x1;
	xlr_write_reg(mmio, R_RX_CONTROL, val);

	/* wait for rx to halt */
	for (i = 0; i < maxloops; i++) {
		val = xlr_read_reg(mmio, R_RX_CONTROL);
		if (val & 0x2)
			break;
		DELAY(1000);
	}
	if (i == maxloops)
		return -1;

	/* Issue a soft reset */
	val = xlr_read_reg(mmio, R_RX_CONTROL);
	val |= 0x4;
	xlr_write_reg(mmio, R_RX_CONTROL, val);

	/* wait for reset to complete */
	for (i = 0; i < maxloops; i++) {
		val = xlr_read_reg(mmio, R_RX_CONTROL);
		if (val & 0x8)
			break;
		DELAY(1000);
	}
	if (i == maxloops)
		return -1;

	/* Clear the soft reset bit */
	val = xlr_read_reg(mmio, R_RX_CONTROL);
	val &= ~0x4;
	xlr_write_reg(mmio, R_RX_CONTROL, val);
	return 0;
}

static void
rmi_xlr_gmac_init(struct driver_data *priv)
{
	int i = 0;
	xlr_reg_t *mmio = priv->mmio;
	int id = priv->instance;
	struct stn_cc *gmac_cc_config;
	uint32_t value = 0;
	int blk = id / 4, port = id % 4;

	rmi_xlr_mac_set_enable(priv, 0);

	rmi_xlr_config_spill_area(priv);

	xlr_write_reg(mmio, R_DESC_PACK_CTRL,
	    (BYTE_OFFSET << O_DESC_PACK_CTRL__ByteOffset) |
	    (1 << O_DESC_PACK_CTRL__MaxEntry) |
	    (MAX_FRAME_SIZE << O_DESC_PACK_CTRL__RegularSize));

	rmi_xlr_config_pde(priv);
	rmi_xlr_config_parser(priv);
	rmi_xlr_config_classifier(priv);

	xlr_write_reg(mmio, R_MSG_TX_THRESHOLD, 3);
	xlr_write_reg(mmio, R_MAC_CONFIG_1, 0x35);
	xlr_write_reg(mmio, R_RX_CONTROL, (0x7 << 6));

	if (priv->mode == XLR_PORT0_RGMII) {
		printf("Port 0 set in RGMII mode\n");
		value = xlr_read_reg(mmio, R_RX_CONTROL);
		value |= 1 << O_RX_CONTROL__RGMII;
		xlr_write_reg(mmio, R_RX_CONTROL, value);
	}
	rmi_xlr_mac_mii_init(priv);


#if 0
	priv->advertising = ADVERTISED_10baseT_Full | ADVERTISED_10baseT_Half |
	    ADVERTISED_100baseT_Full | ADVERTISED_100baseT_Half |
	    ADVERTISED_1000baseT_Full | ADVERTISED_Autoneg |
	    ADVERTISED_MII;
#endif

	/*
	 * Enable all MDIO interrupts in the phy RX_ER bit seems to be get
	 * set about every 1 sec in GigE mode, ignore it for now...
	 */
	rge_mii_write_internal(priv->mii_mmio, priv->phy_addr, 25, 0xfffffffe);

	if (priv->mode != XLR_RGMII) {
		serdes_regs_init(priv);
		serdes_autoconfig(priv);
	}
	rmi_xlr_gmac_config_speed(priv);

	value = xlr_read_reg(mmio, R_IPG_IFG);
	xlr_write_reg(mmio, R_IPG_IFG, ((value & ~0x7f) | MAC_B2B_IPG));
	xlr_write_reg(mmio, R_DMACR0, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR1, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR2, 0xffffffff);
	xlr_write_reg(mmio, R_DMACR3, 0xffffffff);
	xlr_write_reg(mmio, R_STATCTRL, 0x04);
	xlr_write_reg(mmio, R_L2ALLOCCTRL, 0xffffffff);
	xlr_write_reg(mmio, R_INTMASK, 0);
	xlr_write_reg(mmio, R_FREEQCARVE, 0);

	xlr_write_reg(mmio, R_GMAC_TX0_BUCKET_SIZE + port,
	    xlr_board_info.bucket_sizes->bucket[priv->txbucket]);
	xlr_write_reg(mmio, R_GMAC_JFR0_BUCKET_SIZE,
	    xlr_board_info.bucket_sizes->bucket[MSGRNG_STNID_GMACJFR_0]);
	xlr_write_reg(mmio, R_GMAC_RFR0_BUCKET_SIZE,
	    xlr_board_info.bucket_sizes->bucket[MSGRNG_STNID_GMACRFR_0]);
	xlr_write_reg(mmio, R_GMAC_JFR1_BUCKET_SIZE,
	    xlr_board_info.bucket_sizes->bucket[MSGRNG_STNID_GMACJFR_1]);
	xlr_write_reg(mmio, R_GMAC_RFR1_BUCKET_SIZE,
	    xlr_board_info.bucket_sizes->bucket[MSGRNG_STNID_GMACRFR_1]);

	dbg_msg("Programming credit counter %d : %d -> %d\n", blk, R_GMAC_TX0_BUCKET_SIZE + port,
	    xlr_board_info.bucket_sizes->bucket[priv->txbucket]);

	gmac_cc_config = xlr_board_info.gmac_block[blk].credit_config;
	for (i = 0; i < MAX_NUM_MSGRNG_STN_CC; i++) {
		xlr_write_reg(mmio, R_CC_CPU0_0 + i,
		    gmac_cc_config->counters[i >> 3][i & 0x07]);
		dbg_msg("%d: %d -> %d\n", priv->instance,
		    R_CC_CPU0_0 + i, gmac_cc_config->counters[i >> 3][i & 0x07]);
	}
	priv->init_frin_desc = 1;
}

/**********************************************************************
 * Set promiscuous mode
 **********************************************************************/
static void
xlr_mac_set_rx_mode(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);
	uint32_t regval;

	regval = xlr_read_reg(priv->mmio, R_MAC_FILTER_CONFIG);

	if (sc->flags & IFF_PROMISC) {
		regval |= (1 << O_MAC_FILTER_CONFIG__BROADCAST_EN) |
		    (1 << O_MAC_FILTER_CONFIG__PAUSE_FRAME_EN) |
		    (1 << O_MAC_FILTER_CONFIG__ALL_MCAST_EN) |
		    (1 << O_MAC_FILTER_CONFIG__ALL_UCAST_EN);
	} else {
		regval &= ~((1 << O_MAC_FILTER_CONFIG__PAUSE_FRAME_EN) |
		    (1 << O_MAC_FILTER_CONFIG__ALL_UCAST_EN));
	}

	xlr_write_reg(priv->mmio, R_MAC_FILTER_CONFIG, regval);
}

/**********************************************************************
 *  Configure LAN speed for the specified MAC.
 ********************************************************************* */
static int
rmi_xlr_mac_set_speed(struct driver_data *s, xlr_mac_speed_t speed)
{
	return 0;
}

/**********************************************************************
 *  Set Ethernet duplex and flow control options for this MAC
 ********************************************************************* */
static int
rmi_xlr_mac_set_duplex(struct driver_data *s,
    xlr_mac_duplex_t duplex, xlr_mac_fc_t fc)
{
	return 0;
}

/*****************************************************************
 * Kernel Net Stack <-> MAC Driver Interface
 *****************************************************************/
/**********************************************************************
 **********************************************************************/
#define MAC_TX_FAIL 2
#define MAC_TX_PASS 0
#define MAC_TX_RETRY 1

int xlr_dev_queue_xmit_hack = 0;

static int
mac_xmit(struct mbuf *m, struct rge_softc *sc,
    struct driver_data *priv, int len, struct p2d_tx_desc *tx_desc)
{
	struct msgrng_msg msg = {0,0,0,0};
	int stid = priv->txbucket;
	uint32_t tx_cycles = 0;
	uint32_t mflags;
	int vcpu = xlr_cpu_id();
	int rv;

	tx_cycles = mips_rd_count();

	if (build_frag_list(m, &msg, tx_desc) != 0)
		return MAC_TX_FAIL;

	else {
		mflags = msgrng_access_enable();
		if ((rv = message_send(1, MSGRNG_CODE_MAC, stid, &msg)) != 0) {
			msg_snd_failed++;
			msgrng_restore(mflags);
			release_tx_desc(&msg, 0);
			xlr_rge_msg_snd_failed[vcpu]++;
			dbg_msg("Failed packet to cpu %d, rv = %d, stid %d, msg0=%jx\n",
			    vcpu, rv, stid, (uintmax_t)msg.msg0);
			return MAC_TX_FAIL;
		}
		msgrng_restore(mflags);
		port_inc_counter(priv->instance, PORT_TX);
	}

	/* Send the packet to MAC */
	dbg_msg("Sent tx packet to stid %d, msg0=%jx, msg1=%jx \n", stid, 
	    (uintmax_t)msg.msg0, (uintmax_t)msg.msg1);
#ifdef DUMP_PACKETS
	{
		int i = 0;
		unsigned char *buf = (char *)m->m_data;

		printf("Tx Packet: length=%d\n", len);
		for (i = 0; i < 64; i++) {
			if (i && (i % 16) == 0)
				printf("\n");
			printf("%02x ", buf[i]);
		}
		printf("\n");
	}
#endif
	xlr_inc_counter(NETIF_TX);
	return MAC_TX_PASS;
}

static int
rmi_xlr_mac_xmit(struct mbuf *m, struct rge_softc *sc, int len, struct p2d_tx_desc *tx_desc)
{
	struct driver_data *priv = &(sc->priv);
	int ret = -ENOSPC;

	dbg_msg("IN\n");

	xlr_inc_counter(NETIF_STACK_TX);

retry:
	ret = mac_xmit(m, sc, priv, len, tx_desc);

	if (ret == MAC_TX_RETRY)
		goto retry;

	dbg_msg("OUT, ret = %d\n", ret);
	if (ret == MAC_TX_FAIL) {
		/* FULL */
		dbg_msg("Msg Ring Full. Stopping upper layer Q\n");
		port_inc_counter(priv->instance, PORT_STOPQ);
	}
	return ret;
}

static void
mac_frin_replenish(void *args /* ignored */ )
{
	int cpu = xlr_core_id();
	int done = 0;
	int i = 0;

	xlr_inc_counter(REPLENISH_ENTER);
	/*
	 * xlr_set_counter(REPLENISH_ENTER_COUNT,
	 * atomic_read(frin_to_be_sent));
	 */
	xlr_set_counter(REPLENISH_CPU, PCPU_GET(cpuid));

	for (;;) {

		done = 0;

		for (i = 0; i < XLR_MAX_MACS; i++) {
			/* int offset = 0; */
			void *m;
			uint32_t cycles;
			struct rge_softc *sc;
			struct driver_data *priv;
			int frin_to_be_sent;

			sc = dev_mac[i];
			if (!sc)
				goto skip;

			priv = &(sc->priv);
			frin_to_be_sent = priv->frin_to_be_sent[cpu];

			/* if (atomic_read(frin_to_be_sent) < 0) */
			if (frin_to_be_sent < 0) {
				panic("BUG?: [%s]: gmac_%d illegal value for frin_to_be_sent=%d\n",
				    __FUNCTION__, i,
				    frin_to_be_sent);
			}
			/* if (!atomic_read(frin_to_be_sent)) */
			if (!frin_to_be_sent)
				goto skip;

			cycles = mips_rd_count();
			{
				m = get_buf();
				if (!m) {
					device_printf(sc->rge_dev, "No buffer\n");
					goto skip;
				}
			}
			xlr_inc_counter(REPLENISH_FRIN);
			if (xlr_mac_send_fr(priv, vtophys(m), MAX_FRAME_SIZE)) {
				free_buf(vtophys(m));
				printf("[%s]: rx free message_send failed!\n", __FUNCTION__);
				break;
			}
			xlr_set_counter(REPLENISH_CYCLES,
			    (read_c0_count() - cycles));
			atomic_subtract_int((&priv->frin_to_be_sent[cpu]), 1);

			continue;
	skip:
			done++;
		}
		if (done == XLR_MAX_MACS)
			break;
	}
}

static volatile uint32_t g_tx_frm_tx_ok=0;

static void
rge_tx_bkp_func(void *arg, int npending)
{
	int i = 0;

	for (i = 0; i < xlr_board_info.gmacports; i++) {
		if (!dev_mac[i] || !dev_mac[i]->active)
			continue;
		rge_start_locked(dev_mac[i]->rge_ifp, RGE_TX_THRESHOLD);
	}
	atomic_subtract_int(&g_tx_frm_tx_ok, 1);
}

/* This function is called from an interrupt handler */
void
rmi_xlr_mac_msgring_handler(int bucket, int size, int code,
    int stid, struct msgrng_msg *msg,
    void *data /* ignored */ )
{
	uint64_t phys_addr = 0;
	unsigned long addr = 0;
	uint32_t length = 0;
	int ctrl = 0, port = 0;
	struct rge_softc *sc = NULL;
	struct driver_data *priv = 0;
	struct ifnet *ifp;
	int vcpu = xlr_cpu_id();
	int cpu = xlr_core_id();

	dbg_msg("mac: bucket=%d, size=%d, code=%d, stid=%d, msg0=%jx msg1=%jx\n",
	    bucket, size, code, stid, (uintmax_t)msg->msg0, (uintmax_t)msg->msg1);

	phys_addr = (uint64_t) (msg->msg0 & 0xffffffffe0ULL);
	length = (msg->msg0 >> 40) & 0x3fff;
	if (length == 0) {
		ctrl = CTRL_REG_FREE;
		port = (msg->msg0 >> 54) & 0x0f;
		addr = 0;
	} else {
		ctrl = CTRL_SNGL;
		length = length - BYTE_OFFSET - MAC_CRC_LEN;
		port = msg->msg0 & 0x0f;
		addr = 0;
	}

	if (xlr_board_info.is_xls) {
		if (stid == MSGRNG_STNID_GMAC1)
			port += 4;
		sc = dev_mac[dev_mac_gmac0 + port];
	} else {
		if (stid == MSGRNG_STNID_XGS0FR)
			sc = dev_mac[dev_mac_xgs0];
		else if (stid == MSGRNG_STNID_XGS1FR)
			sc = dev_mac[dev_mac_xgs0 + 1];
		else
			sc = dev_mac[dev_mac_gmac0 + port];
	}
	if (sc == NULL)
		return;
	priv = &(sc->priv);

	dbg_msg("msg0 = %jx, stid = %d, port = %d, addr=%lx, length=%d, ctrl=%d\n",
	    (uintmax_t)msg->msg0, stid, port, addr, length, ctrl);

	if (ctrl == CTRL_REG_FREE || ctrl == CTRL_JUMBO_FREE) {
		xlr_rge_tx_ok_done[vcpu]++;
		release_tx_desc(msg, 1);
		ifp = sc->rge_ifp;
		if (ifp->if_drv_flags & IFF_DRV_OACTIVE) {
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		}
		if (atomic_cmpset_int(&g_tx_frm_tx_ok, 0, 1))
			rge_tx_bkp_func(NULL, 0);
		xlr_set_counter(NETIF_TX_COMPLETE_CYCLES,
		    (read_c0_count() - msgrng_msg_cycles));
	} else if (ctrl == CTRL_SNGL || ctrl == CTRL_START) {
		/* Rx Packet */
		/* struct mbuf *m = 0; */
		/* int logical_cpu = 0; */

		dbg_msg("Received packet, port = %d\n", port);
		/*
		 * if num frins to be sent exceeds threshold, wake up the
		 * helper thread
		 */
		atomic_add_int(&(priv->frin_to_be_sent[cpu]), 1);
		if ((priv->frin_to_be_sent[cpu]) > MAC_FRIN_TO_BE_SENT_THRESHOLD) {
			mac_frin_replenish(NULL);
		}
		dbg_msg("gmac_%d: rx packet: phys_addr = %jx, length = %x\n",
		    priv->instance, (uintmax_t)phys_addr, length);
		mac_stats_add(priv->stats.rx_packets, 1);
		mac_stats_add(priv->stats.rx_bytes, length);
		xlr_inc_counter(NETIF_RX);
		xlr_set_counter(NETIF_RX_CYCLES,
		    (read_c0_count() - msgrng_msg_cycles));
		rge_rx(sc, phys_addr, length);
		xlr_rge_rx_done[vcpu]++;
	} else {
		printf("[%s]: unrecognized ctrl=%d!\n", __FUNCTION__, ctrl);
	}

}

/**********************************************************************
 **********************************************************************/
static int
rge_probe(dev)
	device_t dev;
{
	device_set_desc(dev, "RMI Gigabit Ethernet");

	/* Always return 0 */
	return 0;
}

volatile unsigned long xlr_debug_enabled;
struct callout rge_dbg_count;
static void
xlr_debug_count(void *addr)
{
	struct driver_data *priv = &dev_mac[0]->priv;

	/* uint32_t crdt; */
	if (xlr_debug_enabled) {
		printf("\nAvailRxIn %#x\n", xlr_read_reg(priv->mmio, 0x23e));
	}
	callout_reset(&rge_dbg_count, hz, xlr_debug_count, NULL);
}


static void
xlr_tx_q_wakeup(void *addr)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < xlr_board_info.gmacports; i++) {
		if (!dev_mac[i] || !dev_mac[i]->active)
			continue;
		if ((dev_mac[i]->rge_ifp->if_drv_flags) & IFF_DRV_OACTIVE) {
			for (j = 0; j < XLR_MAX_CORE; j++) {
				if (xlr_tot_avail_p2d[j]) {
					dev_mac[i]->rge_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
					break;
				}
			}
		}
	}
	if (atomic_cmpset_int(&g_tx_frm_tx_ok, 0, 1))
		rge_tx_bkp_func(NULL, 0);
	callout_reset(&xlr_tx_stop_bkp, 5 * hz, xlr_tx_q_wakeup, NULL);
}

static int
rge_attach(device_t dev)
{
	struct ifnet *ifp;
	struct rge_softc *sc;
	struct driver_data *priv = 0;
	int ret = 0;
	struct xlr_gmac_block_t *gmac_conf = device_get_ivars(dev);

	sc = device_get_softc(dev);
	sc->rge_dev = dev;

	/* Initialize mac's */
	sc->unit = device_get_unit(dev);

	if (sc->unit > XLR_MAX_MACS) {
		ret = ENXIO;
		goto out;
	}
	RGE_LOCK_INIT(sc, device_get_nameunit(dev));

	priv = &(sc->priv);
	priv->sc = sc;

	sc->flags = 0;		/* TODO : fix me up later */

	priv->id = sc->unit;
	if (gmac_conf->type == XLR_GMAC) {
		priv->instance = priv->id;
		priv->mmio = (xlr_reg_t *) (xlr_io_base + gmac_conf->baseaddr +
		    0x1000 * (sc->unit % 4));
		if ((ret = rmi_xlr_gmac_reset(priv)) == -1)
			goto out;
	} else if (gmac_conf->type == XLR_XGMAC) {
		priv->instance = priv->id - xlr_board_info.gmacports;
		priv->mmio = (xlr_reg_t *) (xlr_io_base + gmac_conf->baseaddr);
	}
	if (xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_VI ||
	    (xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XI &&
	     priv->instance >=4)) {
		dbg_msg("Arizona board - offset 4 \n");
		priv->mii_mmio = (xlr_reg_t *) (xlr_io_base + XLR_IO_GMAC_4_OFFSET);
	} else
		priv->mii_mmio = (xlr_reg_t *) (xlr_io_base + XLR_IO_GMAC_0_OFFSET);

	priv->pcs_mmio = (xlr_reg_t *) (xlr_io_base + gmac_conf->baseaddr);
	priv->serdes_mmio = (xlr_reg_t *) (xlr_io_base + XLR_IO_GMAC_0_OFFSET);

	sc->base_addr = (unsigned long)priv->mmio;
	sc->mem_end = (unsigned long)priv->mmio + XLR_IO_SIZE - 1;

	sc->xmit = rge_start;
	sc->stop = rge_stop;
	sc->get_stats = rmi_xlr_mac_get_stats;
	sc->ioctl = rge_ioctl;

	/* Initialize the device specific driver data */
	mtx_init(&priv->lock, "rge", NULL, MTX_SPIN);

	priv->type = gmac_conf->type;

	priv->mode = gmac_conf->mode;
	if (xlr_board_info.is_xls == 0) {
		/* TODO - check II and IIB boards */
		if (xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_II &&
		    xlr_boot1_info.board_minor_version != 1)
			priv->phy_addr = priv->instance - 2;
		else
			priv->phy_addr = priv->instance;
		priv->mode = XLR_RGMII;
	} else {
		if (gmac_conf->mode == XLR_PORT0_RGMII &&
		    priv->instance == 0) {
			priv->mode = XLR_PORT0_RGMII;
			priv->phy_addr = 0;
		} else {
			priv->mode = XLR_SGMII;
			/* Board 11 has SGMII daughter cards with the XLS chips, in this case
			   the phy number is 0-3 for both GMAC blocks */
			if (xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XI)
				priv->phy_addr = priv->instance % 4 + 16;
			else
				priv->phy_addr = priv->instance + 16;
		}
	}

	priv->txbucket = gmac_conf->station_txbase + priv->instance % 4;
	priv->rfrbucket = gmac_conf->station_rfr;
	priv->spill_configured = 0;

	dbg_msg("priv->mmio=%p\n", priv->mmio);

	/* Set up ifnet structure */
	ifp = sc->rge_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->rge_dev, "failed to if_alloc()\n");
		rge_release_resources(sc);
		ret = ENXIO;
		RGE_LOCK_DESTROY(sc);
		goto out;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rge_ioctl;
	ifp->if_start = rge_start;
	ifp->if_init = rge_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_drv_maxlen = RGE_TX_Q_SIZE;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);
	sc->active = 1;
	ifp->if_hwassist = 0;
	ifp->if_capabilities = IFCAP_TXCSUM | IFCAP_VLAN_HWTAGGING;
	ifp->if_capenable = ifp->if_capabilities;

	/* Initialize the rge_softc */
	sc->irq = gmac_conf->baseirq + priv->instance % 4;

	/* Set the IRQ into the rid field */
	/*
	 * note this is a hack to pass the irq to the iodi interrupt setup
	 * routines
	 */
	sc->rge_irq.__r_i = (struct resource_i *)(intptr_t)sc->irq;

	ret = bus_setup_intr(dev, &sc->rge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, rge_intr, sc, &sc->rge_intrhand);

	if (ret) {
		rge_detach(dev);
		device_printf(sc->rge_dev, "couldn't set up irq\n");
		RGE_LOCK_DESTROY(sc);
		goto out;
	}
	xlr_mac_get_hwaddr(sc);
	xlr_mac_setup_hwaddr(priv);

	dbg_msg("MMIO %08lx, MII %08lx, PCS %08lx, base %08lx PHY %d IRQ %d\n",
	    (u_long)priv->mmio, (u_long)priv->mii_mmio, (u_long)priv->pcs_mmio,
	    (u_long)sc->base_addr, priv->phy_addr, sc->irq);
	dbg_msg("HWADDR %02x:%02x tx %d rfr %d\n", (u_int)sc->dev_addr[4],
	    (u_int)sc->dev_addr[5], priv->txbucket, priv->rfrbucket);

	/*
	 * Set up ifmedia support.
	 */
	/*
	 * Initialize MII/media info.
	 */
	sc->rge_mii.mii_ifp = ifp;
	sc->rge_mii.mii_readreg = rge_mii_read;
	sc->rge_mii.mii_writereg = (mii_writereg_t) rge_mii_write;
	sc->rge_mii.mii_statchg = rmi_xlr_mac_mii_statchg;
	ifmedia_init(&sc->rge_mii.mii_media, 0, rmi_xlr_mac_mediachange,
	    rmi_xlr_mac_mediastatus);
	ifmedia_add(&sc->rge_mii.mii_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->rge_mii.mii_media, IFM_ETHER | IFM_AUTO);
	sc->rge_mii.mii_media.ifm_media = sc->rge_mii.mii_media.ifm_cur->ifm_media;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, sc->dev_addr);

	if (priv->type == XLR_GMAC) {
		rmi_xlr_gmac_init(priv);
	} else if (priv->type == XLR_XGMAC) {
		rmi_xlr_xgmac_init(priv);
	}
	dbg_msg("rge_%d: Phoenix Mac at 0x%p (mtu=%d)\n",
	    sc->unit, priv->mmio, sc->mtu);
	dev_mac[sc->unit] = sc;
	if (priv->type == XLR_XGMAC && priv->instance == 0)
		dev_mac_xgs0 = sc->unit;
	if (priv->type == XLR_GMAC && priv->instance == 0)
		dev_mac_gmac0 = sc->unit;

	if (!gmac_common_init_done) {
		mac_common_init();
		gmac_common_init_done = 1;
		callout_init(&xlr_tx_stop_bkp, 1);
		callout_reset(&xlr_tx_stop_bkp, hz, xlr_tx_q_wakeup, NULL);
		callout_init(&rge_dbg_count, 1);
		//callout_reset(&rge_dbg_count, hz, xlr_debug_count, NULL);
	}
	if ((ret = rmi_xlr_mac_open(sc)) == -1) {
		RGE_LOCK_DESTROY(sc);
		goto out;
	}
out:
	if (ret < 0) {
		device_printf(dev, "error - skipping\n");
	}
	return ret;
}

static void
rge_reset(struct rge_softc *sc)
{
}

static int
rge_detach(dev)
	device_t dev;
{
#ifdef FREEBSD_MAC_NOT_YET
	struct rge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->rge_ifp;

	RGE_LOCK(sc);
	rge_stop(sc);
	rge_reset(sc);
	RGE_UNLOCK(sc);

	ether_ifdetach(ifp);

	if (sc->rge_tbi) {
		ifmedia_removeall(&sc->rge_ifmedia);
	} else {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->rge_miibus);
	}

	rge_release_resources(sc);

#endif				/* FREEBSD_MAC_NOT_YET */
	return (0);
}
static int
rge_suspend(device_t dev)
{
	struct rge_softc *sc;

	sc = device_get_softc(dev);
	RGE_LOCK(sc);
	rge_stop(sc);
	RGE_UNLOCK(sc);

	return 0;
}

static int
rge_resume(device_t dev)
{
	panic("rge_resume(): unimplemented\n");
	return 0;
}

static void
rge_release_resources(struct rge_softc *sc)
{

	if (sc->rge_ifp != NULL)
		if_free(sc->rge_ifp);

	if (mtx_initialized(&sc->rge_mtx))	/* XXX */
		RGE_LOCK_DESTROY(sc);
}
uint32_t gmac_rx_fail[32];
uint32_t gmac_rx_pass[32];

static void
rge_rx(struct rge_softc *sc, vm_paddr_t paddr, int len)
{
	struct mbuf *m;
	struct ifnet *ifp = sc->rge_ifp;
	uint64_t mag;
	uint32_t sr;
	/*
	 * On 32 bit machines we use XKPHYS to get the values stores with
	 * the mbuf, need to explicitly enable KX. Disable interrupts while
	 * KX is enabled to prevent this setting leaking to other code.
	 */
	sr = xlr_enable_kx();
	m = (struct mbuf *)(intptr_t)xlr_paddr_ld(paddr - XLR_CACHELINE_SIZE);
	mag = xlr_paddr_ld(paddr - XLR_CACHELINE_SIZE + sizeof(uint64_t));
	xlr_restore_kx(sr);
	if (mag != 0xf00bad) {
		/* somebody else packet Error - FIXME in intialization */
		printf("cpu %d: *ERROR* Not my packet paddr %p\n",
		    xlr_cpu_id(), (void *)paddr);
		return;
	}
	/* align the data */
	m->m_data += BYTE_OFFSET;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = ifp;

#ifdef DUMP_PACKETS
	{
		int i = 0;
		unsigned char *buf = (char *)m->m_data;

		printf("Rx Packet: length=%d\n", len);
		for (i = 0; i < 64; i++) {
			if (i && (i % 16) == 0)
				printf("\n");
			printf("%02x ", buf[i]);
		}
		printf("\n");
	}
#endif
	ifp->if_ipackets++;
	(*ifp->if_input) (ifp, m);
}

static void
rge_intr(void *arg)
{
	struct rge_softc *sc = (struct rge_softc *)arg;
	struct driver_data *priv = &(sc->priv);
	xlr_reg_t *mmio = priv->mmio;
	uint32_t intreg = xlr_read_reg(mmio, R_INTREG);

	if (intreg & (1 << O_INTREG__MDInt)) {
		uint32_t phy_int_status = 0;
		int i = 0;

		for (i = 0; i < XLR_MAX_MACS; i++) {
			struct rge_softc *phy_dev = 0;
			struct driver_data *phy_priv = 0;

			phy_dev = dev_mac[i];
			if (phy_dev == NULL)
				continue;

			phy_priv = &phy_dev->priv;

			if (phy_priv->type == XLR_XGMAC)
				continue;

			phy_int_status = rge_mii_read_internal(phy_priv->mii_mmio,
			    phy_priv->phy_addr, 26);
			printf("rge%d: Phy addr %d, MII MMIO %lx status %x\n", phy_priv->instance,
			    (int)phy_priv->phy_addr, (u_long)phy_priv->mii_mmio, phy_int_status);
			rmi_xlr_gmac_config_speed(phy_priv);
		}
	} else {
		printf("[%s]: mac type = %d, instance %d error "
		    "interrupt: INTREG = 0x%08x\n",
		    __FUNCTION__, priv->type, priv->instance, intreg);
	}

	/* clear all interrupts and hope to make progress */
	xlr_write_reg(mmio, R_INTREG, 0xffffffff);

	/* (not yet) on A0 and B0, xgmac interrupts are routed only to xgs_1 irq */
	if ((xlr_revision() < 2) && (priv->type == XLR_XGMAC)) {
		struct rge_softc *xgs0_dev = dev_mac[dev_mac_xgs0];
		struct driver_data *xgs0_priv = &xgs0_dev->priv;
		xlr_reg_t *xgs0_mmio = xgs0_priv->mmio;
		uint32_t xgs0_intreg = xlr_read_reg(xgs0_mmio, R_INTREG);

		if (xgs0_intreg) {
			printf("[%s]: mac type = %d, instance %d error "
			    "interrupt: INTREG = 0x%08x\n",
			    __FUNCTION__, xgs0_priv->type, xgs0_priv->instance, xgs0_intreg);

			xlr_write_reg(xgs0_mmio, R_INTREG, 0xffffffff);
		}
	}
}

static void
rge_start_locked(struct ifnet *ifp, int threshold)
{
	struct rge_softc *sc = ifp->if_softc;
	struct mbuf *m = NULL;
	int prepend_pkt = 0;
	int i = 0;
	struct p2d_tx_desc *tx_desc = NULL;
	int cpu = xlr_core_id();
	uint32_t vcpu = xlr_cpu_id();

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	for (i = 0; i < xlr_tot_avail_p2d[cpu]; i++) {
		if (IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			return;
		tx_desc = get_p2d_desc();
		if (!tx_desc) {
			xlr_rge_get_p2d_failed[vcpu]++;
			return;
		}
		/* Grab a packet off the queue. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			free_p2d_desc(tx_desc);
			return;
		}
		prepend_pkt = rmi_xlr_mac_xmit(m, sc, 0, tx_desc);

		if (prepend_pkt) {
			xlr_rge_tx_prepend[vcpu]++;
			IF_PREPEND(&ifp->if_snd, m);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			return;
		} else {
			ifp->if_opackets++;
			xlr_rge_tx_done[vcpu]++;
		}
	}
}

static void
rge_start(struct ifnet *ifp)
{
	rge_start_locked(ifp, RGE_TX_Q_SIZE);
}

static int
rge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct rge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int mask, error = 0;

	/* struct mii_data *mii; */
	switch (command) {
	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		error = rmi_xlr_mac_change_mtu(sc, ifr->ifr_mtu);
		break;
	case SIOCSIFFLAGS:

		RGE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing a
			 * full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.  Similarly for ALLMULTI.
			 */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->flags & IFF_PROMISC)) {
				sc->flags |= IFF_PROMISC;
				xlr_mac_set_rx_mode(sc);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
				    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->flags & IFF_PROMISC) {
				sc->flags &= IFF_PROMISC;
				xlr_mac_set_rx_mode(sc);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    (ifp->if_flags ^ sc->flags) & IFF_ALLMULTI) {
				rmi_xlr_mac_set_multicast_list(sc);
			} else
				xlr_mac_set_rx_mode(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				xlr_mac_set_rx_mode(sc);
			}
		}
		sc->flags = ifp->if_flags;
		RGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			RGE_LOCK(sc);
			rmi_xlr_mac_set_multicast_list(sc);
			RGE_UNLOCK(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr,
		    &sc->rge_mii.mii_media, command);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		ifp->if_hwassist = 0;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
rge_init(void *addr)
{
	struct rge_softc *sc = (struct rge_softc *)addr;
	struct ifnet *ifp;
	struct driver_data *priv = &(sc->priv);

	ifp = sc->rge_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	rmi_xlr_mac_set_enable(priv, 1);
}

static void
rge_stop(struct rge_softc *sc)
{
	rmi_xlr_mac_close(sc);
}

static int
rge_shutdown(device_t dev)
{
	struct rge_softc *sc;

	sc = device_get_softc(dev);

	RGE_LOCK(sc);
	rge_stop(sc);
	rge_reset(sc);
	RGE_UNLOCK(sc);

	return (0);
}

static int
rmi_xlr_mac_open(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);
	int i;

	dbg_msg("IN\n");

	if (rmi_xlr_mac_fill_rxfr(sc)) {
		return -1;
	}
	mtx_lock_spin(&priv->lock);

	xlr_mac_set_rx_mode(sc);

	if (sc->unit == xlr_board_info.gmacports - 1) {
		printf("Enabling MDIO interrupts\n");
		struct rge_softc *tmp = NULL;

		for (i = 0; i < xlr_board_info.gmacports; i++) {
			tmp = dev_mac[i];
			if (tmp)
				xlr_write_reg(tmp->priv.mmio, R_INTMASK,
				    ((tmp->priv.instance == 0) << O_INTMASK__MDInt));
		}
	}
	/*
	 * Configure the speed, duplex, and flow control
	 */
	rmi_xlr_mac_set_speed(priv, priv->speed);
	rmi_xlr_mac_set_duplex(priv, priv->duplex, priv->flow_ctrl);
	rmi_xlr_mac_set_enable(priv, 0);

	mtx_unlock_spin(&priv->lock);

	for (i = 0; i < 8; i++) {
		priv->frin_to_be_sent[i] = 0;
	}

	return 0;
}

/**********************************************************************
 **********************************************************************/
static int
rmi_xlr_mac_close(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);

	mtx_lock_spin(&priv->lock);

	/*
	 * There may have left over mbufs in the ring as well as in free in
	 * they will be reused next time open is called
	 */

	rmi_xlr_mac_set_enable(priv, 0);

	xlr_inc_counter(NETIF_STOP_Q);
	port_inc_counter(priv->instance, PORT_STOPQ);

	mtx_unlock_spin(&priv->lock);

	return 0;
}

/**********************************************************************
 **********************************************************************/
static struct rge_softc_stats *
rmi_xlr_mac_get_stats(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);

	/* unsigned long flags; */

	mtx_lock_spin(&priv->lock);

	/* XXX update other stats here */

	mtx_unlock_spin(&priv->lock);

	return &priv->stats;
}

/**********************************************************************
 **********************************************************************/
static void
rmi_xlr_mac_set_multicast_list(struct rge_softc *sc)
{
}

/**********************************************************************
 **********************************************************************/
static int
rmi_xlr_mac_change_mtu(struct rge_softc *sc, int new_mtu)
{
	struct driver_data *priv = &(sc->priv);

	if ((new_mtu > 9500) || (new_mtu < 64)) {
		return -EINVAL;
	}
	mtx_lock_spin(&priv->lock);

	sc->mtu = new_mtu;

	/* Disable MAC TX/RX */
	rmi_xlr_mac_set_enable(priv, 0);

	/* Flush RX FR IN */
	/* Flush TX IN */
	rmi_xlr_mac_set_enable(priv, 1);

	mtx_unlock_spin(&priv->lock);
	return 0;
}

/**********************************************************************
 **********************************************************************/
static int
rmi_xlr_mac_fill_rxfr(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);
	int i;
	int ret = 0;
	void *ptr;

	dbg_msg("\n");
	if (!priv->init_frin_desc)
		return ret;
	priv->init_frin_desc = 0;

	dbg_msg("\n");
	for (i = 0; i < MAX_NUM_DESC; i++) {
		ptr = get_buf();
		if (!ptr) {
			ret = -ENOMEM;
			break;
		}
		/* Send the free Rx desc to the MAC */
		xlr_mac_send_fr(priv, vtophys(ptr), MAX_FRAME_SIZE);
	}

	return ret;
}

/**********************************************************************
 **********************************************************************/
static __inline__ void *
rmi_xlr_config_spill(xlr_reg_t * mmio,
    int reg_start_0, int reg_start_1,
    int reg_size, int size)
{
	uint32_t spill_size = size;
	void *spill = NULL;
	uint64_t phys_addr = 0;


	spill = contigmalloc((spill_size + XLR_CACHELINE_SIZE), M_DEVBUF,
	    M_NOWAIT | M_ZERO, 0, 0xffffffff, XLR_CACHELINE_SIZE, 0);
	if (!spill || ((vm_offset_t)spill & (XLR_CACHELINE_SIZE - 1))) {
		panic("Unable to allocate memory for spill area!\n");
	}
	phys_addr = vtophys(spill);
	dbg_msg("Allocate spill %d bytes at %jx\n", size, (uintmax_t)phys_addr);
	xlr_write_reg(mmio, reg_start_0, (phys_addr >> 5) & 0xffffffff);
	xlr_write_reg(mmio, reg_start_1, (phys_addr >> 37) & 0x07);
	xlr_write_reg(mmio, reg_size, spill_size);

	return spill;
}

static void
rmi_xlr_config_spill_area(struct driver_data *priv)
{
	/*
	 * if driver initialization is done parallely on multiple cpus
	 * spill_configured needs synchronization
	 */
	if (priv->spill_configured)
		return;

	if (priv->type == XLR_GMAC && priv->instance % 4 != 0) {
		priv->spill_configured = 1;
		return;
	}
	priv->spill_configured = 1;

	priv->frin_spill =
	    rmi_xlr_config_spill(priv->mmio,
	    R_REG_FRIN_SPILL_MEM_START_0,
	    R_REG_FRIN_SPILL_MEM_START_1,
	    R_REG_FRIN_SPILL_MEM_SIZE,
	    MAX_FRIN_SPILL *
	    sizeof(struct fr_desc));

	priv->class_0_spill =
	    rmi_xlr_config_spill(priv->mmio,
	    R_CLASS0_SPILL_MEM_START_0,
	    R_CLASS0_SPILL_MEM_START_1,
	    R_CLASS0_SPILL_MEM_SIZE,
	    MAX_CLASS_0_SPILL *
	    sizeof(union rx_tx_desc));
	priv->class_1_spill =
	    rmi_xlr_config_spill(priv->mmio,
	    R_CLASS1_SPILL_MEM_START_0,
	    R_CLASS1_SPILL_MEM_START_1,
	    R_CLASS1_SPILL_MEM_SIZE,
	    MAX_CLASS_1_SPILL *
	    sizeof(union rx_tx_desc));

	priv->frout_spill =
	    rmi_xlr_config_spill(priv->mmio, R_FROUT_SPILL_MEM_START_0,
	    R_FROUT_SPILL_MEM_START_1,
	    R_FROUT_SPILL_MEM_SIZE,
	    MAX_FROUT_SPILL *
	    sizeof(struct fr_desc));

	priv->class_2_spill =
	    rmi_xlr_config_spill(priv->mmio,
	    R_CLASS2_SPILL_MEM_START_0,
	    R_CLASS2_SPILL_MEM_START_1,
	    R_CLASS2_SPILL_MEM_SIZE,
	    MAX_CLASS_2_SPILL *
	    sizeof(union rx_tx_desc));
	priv->class_3_spill =
	    rmi_xlr_config_spill(priv->mmio,
	    R_CLASS3_SPILL_MEM_START_0,
	    R_CLASS3_SPILL_MEM_START_1,
	    R_CLASS3_SPILL_MEM_SIZE,
	    MAX_CLASS_3_SPILL *
	    sizeof(union rx_tx_desc));
	priv->spill_configured = 1;
}

/*****************************************************************
 * Write the MAC address to the XLR registers
 * All 4 addresses are the same for now
 *****************************************************************/
static void
xlr_mac_setup_hwaddr(struct driver_data *priv)
{
	struct rge_softc *sc = priv->sc;

	xlr_write_reg(priv->mmio, R_MAC_ADDR0,
	    ((sc->dev_addr[5] << 24) | (sc->dev_addr[4] << 16)
	    | (sc->dev_addr[3] << 8) | (sc->dev_addr[2]))
	    );

	xlr_write_reg(priv->mmio, R_MAC_ADDR0 + 1,
	    ((sc->dev_addr[1] << 24) | (sc->
	    dev_addr[0] << 16)));

	xlr_write_reg(priv->mmio, R_MAC_ADDR_MASK2, 0xffffffff);

	xlr_write_reg(priv->mmio, R_MAC_ADDR_MASK2 + 1, 0xffffffff);

	xlr_write_reg(priv->mmio, R_MAC_ADDR_MASK3, 0xffffffff);

	xlr_write_reg(priv->mmio, R_MAC_ADDR_MASK3 + 1, 0xffffffff);

	xlr_write_reg(priv->mmio, R_MAC_FILTER_CONFIG,
	    (1 << O_MAC_FILTER_CONFIG__BROADCAST_EN) |
	    (1 << O_MAC_FILTER_CONFIG__ALL_MCAST_EN) |
	    (1 << O_MAC_FILTER_CONFIG__MAC_ADDR0_VALID)
	    );
}

/*****************************************************************
 * Read the MAC address from the XLR registers
 * All 4 addresses are the same for now
 *****************************************************************/
static void
xlr_mac_get_hwaddr(struct rge_softc *sc)
{
	struct driver_data *priv = &(sc->priv);

	sc->dev_addr[0] = (xlr_boot1_info.mac_addr >> 40) & 0xff;
	sc->dev_addr[1] = (xlr_boot1_info.mac_addr >> 32) & 0xff;
	sc->dev_addr[2] = (xlr_boot1_info.mac_addr >> 24) & 0xff;
	sc->dev_addr[3] = (xlr_boot1_info.mac_addr >> 16) & 0xff;
	sc->dev_addr[4] = (xlr_boot1_info.mac_addr >> 8) & 0xff;
	sc->dev_addr[5] = ((xlr_boot1_info.mac_addr >> 0) & 0xff) + priv->instance;
}

/*****************************************************************
 * Mac Module Initialization
 *****************************************************************/
static void
mac_common_init(void)
{
	init_p2d_allocation();
	init_tx_ring();

	if (xlr_board_info.is_xls) {
		if (register_msgring_handler(MSGRNG_STNID_GMAC,
		   MSGRNG_STNID_GMAC + 1, rmi_xlr_mac_msgring_handler,
		   NULL)) {
			panic("Couldn't register msgring handler\n");
		}
		if (register_msgring_handler(MSGRNG_STNID_GMAC1,
		    MSGRNG_STNID_GMAC1 + 1, rmi_xlr_mac_msgring_handler,
		    NULL)) {
			panic("Couldn't register msgring handler\n");
		}
	} else {
		if (register_msgring_handler(MSGRNG_STNID_GMAC,
		   MSGRNG_STNID_GMAC + 1, rmi_xlr_mac_msgring_handler,
		   NULL)) {
			panic("Couldn't register msgring handler\n");
		}
	}

	/*
	 * Not yet if (xlr_board_atx_ii()) { if (register_msgring_handler
	 * (TX_STN_XGS_0, rmi_xlr_mac_msgring_handler, NULL)) {
	 * panic("Couldn't register msgring handler for TX_STN_XGS_0\n"); }
	 * if (register_msgring_handler (TX_STN_XGS_1,
	 * rmi_xlr_mac_msgring_handler, NULL)) { panic("Couldn't register
	 * msgring handler for TX_STN_XGS_1\n"); } }
	 */
}

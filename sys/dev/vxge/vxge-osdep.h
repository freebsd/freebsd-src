/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

/* LINTLIBRARY */

#ifndef	_VXGE_OSDEP_H_
#define	_VXGE_OSDEP_H_

#include <sys/param.h>
#include <sys/systm.h>

#if __FreeBSD_version >= 800000
#include <sys/buf_ring.h>
#endif

#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sockio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/stddef.h>
#include <sys/proc.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <sys/pcpu.h>
#include <sys/smp.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <machine/stdarg.h>
#include <machine/in_cksum.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>

#include <dev/vxge/include/vxge-defs.h>

/*
 * ------------------------- includes and defines -------------------------
 */

#if BYTE_ORDER == BIG_ENDIAN
#define	VXGE_OS_HOST_BIG_ENDIAN
#else
#define	VXGE_OS_HOST_LITTLE_ENDIAN
#endif

#if __LONG_BIT == 64
#define	VXGE_OS_PLATFORM_64BIT
#else
#define	VXGE_OS_PLATFORM_32BIT
#endif

#define	VXGE_OS_PCI_CONFIG_SIZE		256
#define	VXGE_OS_HOST_PAGE_SIZE		4096
#define	VXGE_LL_IP_FAST_CSUM(hdr, len)	0

#ifndef	__DECONST
#define	__DECONST(type, var)	((type)(uintrptr_t)(const void *)(var))
#endif

typedef struct ifnet *ifnet_t;
typedef struct mbuf *mbuf_t;
typedef struct mbuf *OS_NETSTACK_BUF;

typedef struct _vxge_bus_res_t {

	u_long			bus_res_len;
	bus_space_tag_t		bus_space_tag;		/* DMA Tag */
	bus_space_handle_t	bus_space_handle;	/* Bus handle */
	struct resource		*bar_start_addr;	/* BAR address */

} vxge_bus_res_t;

typedef struct _vxge_dma_alloc_t {

	bus_addr_t		dma_paddr;		/* Physical Address */
	caddr_t			dma_vaddr;		/* Virtual Address */
	bus_dma_tag_t		dma_tag;		/* DMA Tag */
	bus_dmamap_t		dma_map;		/* DMA Map */
	bus_dma_segment_t	dma_segment;		/* DMA Segment */
	bus_size_t		dma_size;		/* Size */
	int			dma_nseg;		/* scatter-gather */

} vxge_dma_alloc_t;

typedef struct _vxge_pci_info {

	device_t	ndev;		/* Device */
	void		*reg_map[3];	/* BAR Resource */
	struct resource	*bar_info[3];	/* BAR tag and handle */

} vxge_pci_info_t;

/*
 * ---------------------- fixed size primitive types -----------------------
 */
typedef size_t			ptr_t;
typedef int8_t			s8;
typedef uint8_t			u8;
typedef uint16_t		u16;
typedef int32_t			s32;
typedef uint32_t		u32;
typedef unsigned long long int	u64;
#ifndef __bool_true_false_are_defined
typedef boolean_t		bool;
#endif
typedef bus_addr_t		dma_addr_t;
typedef struct mtx		spinlock_t;
typedef struct resource		*pci_irq_h;
typedef vxge_pci_info_t		*pci_dev_h;
typedef vxge_pci_info_t		*pci_cfg_h;
typedef vxge_bus_res_t		*pci_reg_h;
typedef vxge_dma_alloc_t	pci_dma_h;
typedef vxge_dma_alloc_t	pci_dma_acc_h;

/*
 * -------------------------- "libc" functionality -------------------------
 */
#define	vxge_os_curr_time		systime
#define	vxge_os_strcpy			strcpy
#define	vxge_os_strlcpy			strlcpy
#define	vxge_os_strlen			strlen
#define	vxge_os_sprintf			sprintf
#define	vxge_os_snprintf		snprintf
#define	vxge_os_println(buf)		printf("%s\n", buf)
#define	vxge_os_memzero			bzero
#define	vxge_os_memcmp			memcmp
#define	vxge_os_memcpy(dst, src, size)	bcopy(src, dst, size)

#define	vxge_os_timestamp(buff) {			\
	struct timeval cur_time;			\
	gettimeofday(&cur_time, 0);			\
	snprintf(buff, sizeof(buff), "%08li.%08li: ",	\
		cur_time.tv_sec, cur_time.tv_usec);	\
}

#define	vxge_os_printf(fmt...) {			\
	printf(fmt);					\
	printf("\n");					\
}

#define	vxge_os_vaprintf(fmt...)			\
	vxge_os_printf(fmt);

#define	vxge_os_vasprintf(fmt...) {			\
	vxge_os_printf(fmt);				\
}

#define	vxge_trace(trace, fmt, args...)			\
	vxge_debug_uld(VXGE_COMPONENT_ULD,		\
		trace, hldev, vpid, fmt, ## args)

/*
 * -------------------- synchronization primitives -------------------------
 */
/* Initialize the spin lock */
#define	vxge_os_spin_lock_init(lockp, ctxh) {			\
	if (mtx_initialized(lockp) == 0)			\
		mtx_init((lockp), "vxge", NULL, MTX_DEF);	\
}

/* Initialize the spin lock (IRQ version) */
#define	vxge_os_spin_lock_init_irq(lockp, ctxh) {		\
	if (mtx_initialized(lockp) == 0)			\
		mtx_init((lockp), "vxge", NULL, MTX_DEF);	\
}

/* Destroy the lock */
#define	vxge_os_spin_lock_destroy(lockp, ctxh) {		\
	if (mtx_initialized(lockp) != 0)			\
		mtx_destroy(lockp);				\
}

/* Destroy the lock (IRQ version) */
#define	vxge_os_spin_lock_destroy_irq(lockp, ctxh) {		\
	if (mtx_initialized(lockp) != 0)			\
		mtx_destroy(lockp);				\
}

/* Acquire the lock */
#define	vxge_os_spin_lock(lockp) {				\
	if (mtx_owned(lockp) == 0)				\
		mtx_lock(lockp);				\
}

/* Release the lock */
#define	vxge_os_spin_unlock(lockp)	mtx_unlock(lockp)

/* Acquire the lock (IRQ version) */
#define	vxge_os_spin_lock_irq(lockp, flags) {			\
	flags = MTX_QUIET;					\
	if (mtx_owned(lockp) == 0)				\
		mtx_lock_flags(lockp, flags);			\
}

/* Release the lock (IRQ version) */
#define	vxge_os_spin_unlock_irq(lockp, flags) {			\
	flags = MTX_QUIET;					\
	mtx_unlock_flags(lockp, flags);				\
}

/* Write memory	barrier	*/
#if __FreeBSD_version <	800000
#if defined(__i386__) || defined(__amd64__)
#define	mb()  __asm volatile("mfence" ::: "memory")
#define	wmb() __asm volatile("sfence" ::: "memory")
#define	rmb() __asm volatile("lfence" ::: "memory")
#else
#define	mb()
#define	rmb()
#define	wmb()
#endif
#endif

#define	vxge_os_wmb()		wmb()
#define	vxge_os_udelay(x)	DELAY(x)
#define	vxge_os_stall(x)	DELAY(x)
#define	vxge_os_mdelay(x)	DELAY(x * 1000)
#define	vxge_os_xchg		(targetp, newval)

/*
 * ------------------------- misc primitives -------------------------------
 */
#define	vxge_os_be32		u32
#define	vxge_os_unlikely(x)	(x)
#define	vxge_os_prefetch(x)	(x = x)
#define	vxge_os_prefetchw(x)	(x = x)
#define	vxge_os_bug		vxge_os_printf

#define	vxge_os_ntohs		ntohs
#define	vxge_os_ntohl		ntohl
#define	vxge_os_ntohll		be64toh

#define	vxge_os_htons		htons
#define	vxge_os_htonl		htonl
#define	vxge_os_htonll		htobe64

#define	vxge_os_in_multicast		IN_MULTICAST
#define	VXGE_OS_INADDR_BROADCAST	INADDR_BROADCAST
/*
 * -------------------------- compiler stuff ------------------------------
 */
#define	__vxge_os_cacheline_size	CACHE_LINE_SIZE
#define	__vxge_os_attr_cacheline_aligned __aligned(__vxge_os_cacheline_size)

/*
 * ---------------------- memory primitives --------------------------------
 */
#if defined(VXGE_OS_MEMORY_CHECK)

typedef struct _vxge_os_malloc_t {

	u_long		line;
	u_long		size;
	void		*ptr;
	const char	*file;

} vxge_os_malloc_t;

#define	VXGE_OS_MALLOC_CNT_MAX	64*1024

extern u32 g_malloc_cnt;
extern vxge_os_malloc_t g_malloc_arr[VXGE_OS_MALLOC_CNT_MAX];

#define	VXGE_OS_MEMORY_CHECK_MALLOC(_vaddr, _size, _file, _line) {	\
	if (_vaddr) {							\
		u32 i;							\
		for (i = 0; i < g_malloc_cnt; i++) {			\
			if (g_malloc_arr[i].ptr == NULL)		\
				break;					\
		}							\
		if (i == g_malloc_cnt) {				\
			g_malloc_cnt++;					\
			if (g_malloc_cnt >= VXGE_OS_MALLOC_CNT_MAX) {	\
				vxge_os_bug("g_malloc_cnt exceed %d\n",	\
				    VXGE_OS_MALLOC_CNT_MAX);		\
			} else {					\
				g_malloc_arr[i].ptr = _vaddr;		\
				g_malloc_arr[i].size = _size;		\
				g_malloc_arr[i].file = _file;		\
				g_malloc_arr[i].line = _line;		\
			}						\
		}							\
	}								\
}

#define	VXGE_OS_MEMORY_CHECK_FREE(_vaddr, _size, _file, _line) {	\
	u32 i;								\
	for (i = 0; i < VXGE_OS_MALLOC_CNT_MAX; i++) {			\
		if (g_malloc_arr[i].ptr == _vaddr) {			\
			g_malloc_arr[i].ptr = NULL;			\
			if (_size && g_malloc_arr[i].size !=  _size) {	\
				vxge_os_printf("freeing wrong size "	\
				    "%lu allocated %s:%lu:"		\
				    VXGE_OS_LLXFMT":%lu\n",		\
				    _size,				\
				    g_malloc_arr[i].file,		\
				    g_malloc_arr[i].line,		\
				    (u64)(u_long) g_malloc_arr[i].ptr,	\
				    g_malloc_arr[i].size);		\
			}						\
			break;						\
		}							\
	}								\
}

#else
#define	VXGE_OS_MEMORY_CHECK_MALLOC(prt, size, file, line)
#define	VXGE_OS_MEMORY_CHECK_FREE(vaddr, size, file, line)
#endif

static inline void *
vxge_mem_alloc_ex(u_long size, const char *file, int line)
{
	void *vaddr = NULL;
	vaddr = malloc(size, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (NULL != vaddr) {
		VXGE_OS_MEMORY_CHECK_MALLOC((void *)vaddr, size, file, line)
		vxge_os_memzero(vaddr, size);
	}

	return (vaddr);
}

static inline void
vxge_mem_free_ex(const void *vaddr, u_long size, const char *file, int line)
{
	if (NULL != vaddr) {
		VXGE_OS_MEMORY_CHECK_FREE(vaddr, size, file, line)
		free(__DECONST(void *, vaddr), M_DEVBUF);
	}
}

#define	vxge_os_malloc(pdev, size)			\
	vxge_mem_alloc_ex(size, __FILE__, __LINE__)

#define	vxge_os_free(pdev, vaddr, size)			\
	vxge_mem_free_ex(vaddr, size, __FILE__, __LINE__)

#define	vxge_mem_alloc(size)				\
	vxge_mem_alloc_ex(size, __FILE__, __LINE__)

#define	vxge_mem_free(vaddr, size)			\
	vxge_mem_free_ex(vaddr, size, __FILE__, __LINE__)

#define	vxge_free_packet(x)				\
	if (NULL != x) { m_freem(x); x = NULL; }

/*
 * --------------------------- pci primitives ------------------------------
 */
#define	vxge_os_pci_read8(pdev, cfgh, where, val)	\
	(*(val) = pci_read_config(pdev->ndev, where, 1))

#define	vxge_os_pci_write8(pdev, cfgh, where, val)	\
	pci_write_config(pdev->ndev, where, val, 1)

#define	vxge_os_pci_read16(pdev, cfgh, where, val)	\
	(*(val) = pci_read_config(pdev->ndev, where, 2))

#define	vxge_os_pci_write16(pdev, cfgh, where, val)	\
	pci_write_config(pdev->ndev, where, val, 2)

#define	vxge_os_pci_read32(pdev, cfgh, where, val)	\
	(*(val) = pci_read_config(pdev->ndev, where, 4))

#define	vxge_os_pci_write32(pdev, cfgh, where, val)	\
	pci_write_config(pdev->ndev, where, val, 4)

static inline u32
vxge_os_pci_res_len(pci_dev_h pdev, pci_reg_h regh)
{
	return (((vxge_bus_res_t *) regh)->bus_res_len);
}

static inline u8
vxge_os_pio_mem_read8(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	return bus_space_read_1(((vxge_bus_res_t *) regh)->bus_space_tag,
				((vxge_bus_res_t *) regh)->bus_space_handle,
				(bus_size_t) ((caddr_t) (addr) - vaddr));
}

static inline u16
vxge_os_pio_mem_read16(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	return bus_space_read_2(((vxge_bus_res_t *) regh)->bus_space_tag,
				((vxge_bus_res_t *) regh)->bus_space_handle,
				(bus_size_t) ((caddr_t) (addr) - vaddr));
}

static inline u32
vxge_os_pio_mem_read32(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	return bus_space_read_4(((vxge_bus_res_t *) regh)->bus_space_tag,
				((vxge_bus_res_t *) regh)->bus_space_handle,
				(bus_size_t) ((caddr_t) (addr) - vaddr));
}

static inline u64
vxge_os_pio_mem_read64(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	u64 val, val_l, val_u;

	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	val_l = bus_space_read_4(((vxge_bus_res_t *) regh)->bus_space_tag,
				((vxge_bus_res_t *) regh)->bus_space_handle,
				(bus_size_t) (((caddr_t) addr) + 4 - vaddr));

	val_u = bus_space_read_4(((vxge_bus_res_t *) regh)->bus_space_tag,
				((vxge_bus_res_t *) regh)->bus_space_handle,
				(bus_size_t) ((caddr_t) (addr) - vaddr));

	val = ((val_l << 32) | val_u);
	return (val);
}

static inline void
vxge_os_pio_mem_write8(pci_dev_h pdev, pci_reg_h regh, u8 val, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) regh)->bar_start_addr);

	bus_space_write_1(((vxge_bus_res_t *) regh)->bus_space_tag,
			((vxge_bus_res_t *) regh)->bus_space_handle,
			(bus_size_t) ((caddr_t) (addr) - vaddr), val);
}

static inline void
vxge_os_pio_mem_write16(pci_dev_h pdev, pci_reg_h regh, u16 val, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	bus_space_write_2(((vxge_bus_res_t *) regh)->bus_space_tag,
			((vxge_bus_res_t *) regh)->bus_space_handle,
			(bus_size_t) ((caddr_t) (addr) - vaddr), val);
}

static inline void
vxge_os_pio_mem_write32(pci_dev_h pdev, pci_reg_h regh, u32 val, void *addr)
{
	caddr_t vaddr =
	    (caddr_t) (((vxge_bus_res_t *) (regh))->bar_start_addr);

	bus_space_write_4(((vxge_bus_res_t *) regh)->bus_space_tag,
			((vxge_bus_res_t *) regh)->bus_space_handle,
			(bus_size_t) ((caddr_t) (addr) - vaddr), val);
}

static inline void
vxge_os_pio_mem_write64(pci_dev_h pdev, pci_reg_h regh, u64 val, void *addr)
{
	u32 val_l = (u32) (val & 0xffffffff);
	u32 val_u = (u32) (val >> 32);

	vxge_os_pio_mem_write32(pdev, regh, val_l, addr);
	vxge_os_pio_mem_write32(pdev, regh, val_u, (caddr_t) addr + 4);
}

#define	vxge_os_flush_bridge	vxge_os_pio_mem_read64

/*
 * --------------------------- dma primitives -----------------------------
 */
#define	VXGE_OS_DMA_DIR_TODEVICE	0
#define	VXGE_OS_DMA_DIR_FROMDEVICE	1
#define	VXGE_OS_DMA_DIR_BIDIRECTIONAL	2
#define	VXGE_OS_INVALID_DMA_ADDR	((bus_addr_t)0)

static void
vxge_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error)
		return;

	*(bus_addr_t *) arg = segs->ds_addr;
}

static inline void *
vxge_os_dma_malloc(pci_dev_h pdev, u_long bytes, int dma_flags,
    pci_dma_h * p_dmah, pci_dma_acc_h * p_dma_acch)
{
	int error = 0;
	bus_addr_t bus_addr = BUS_SPACE_MAXADDR;
	bus_size_t boundary, max_size, alignment = PAGE_SIZE;

	if (bytes > PAGE_SIZE) {
		boundary = 0;
		max_size = bytes;
	} else {
		boundary = PAGE_SIZE;
		max_size = PAGE_SIZE;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(pdev->ndev),	/* Parent */
	    alignment,				/* Alignment */
	    boundary,				/* Bounds */
	    bus_addr,				/* Low Address */
	    bus_addr,				/* High Address */
	    NULL,				/* Filter Func */
	    NULL,				/* Filter Func Argument */
	    bytes,				/* Maximum Size */
	    1,					/* Number of Segments */
	    max_size,				/* Maximum Segment Size */
	    BUS_DMA_ALLOCNOW,			/* Flags */
	    NULL,				/* Lock Func */
	    NULL,				/* Lock Func Arguments */
	    &(p_dmah->dma_tag));		/* DMA Tag */

	if (error != 0) {
		device_printf(pdev->ndev, "bus_dma_tag_create failed\n");
		goto _exit0;
	}

	p_dmah->dma_size = bytes;
	error = bus_dmamem_alloc(p_dmah->dma_tag, (void **)&p_dmah->dma_vaddr,
	    (BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT),
	    &p_dmah->dma_map);
	if (error != 0) {
		device_printf(pdev->ndev, "bus_dmamem_alloc failed\n");
		goto _exit1;
	}

	VXGE_OS_MEMORY_CHECK_MALLOC(p_dmah->dma_vaddr, p_dmah->dma_size,
	    __FILE__, __LINE__);

	return (p_dmah->dma_vaddr);

_exit1:
	bus_dma_tag_destroy(p_dmah->dma_tag);
_exit0:
	return (NULL);
}

static inline void
vxge_dma_free(pci_dev_h pdev, const void *vaddr, u_long size,
    pci_dma_h *p_dmah, pci_dma_acc_h *p_dma_acch,
    const char *file, int line)
{
	VXGE_OS_MEMORY_CHECK_FREE(p_dmah->dma_vaddr, size, file, line)

	bus_dmamem_free(p_dmah->dma_tag, p_dmah->dma_vaddr, p_dmah->dma_map);
	bus_dma_tag_destroy(p_dmah->dma_tag);

	p_dmah->dma_tag = NULL;
	p_dmah->dma_vaddr = NULL;
}

extern void
vxge_hal_blockpool_block_add(void *, void *, u32, pci_dma_h *, pci_dma_acc_h *);

static inline void
vxge_os_dma_malloc_async(pci_dev_h pdev, void *devh,
    u_long size, int dma_flags)
{
	pci_dma_h dma_h;
	pci_dma_acc_h acc_handle;

	void *block_addr = NULL;

	block_addr = vxge_os_dma_malloc(pdev, size, dma_flags,
	    &dma_h, &acc_handle);

	vxge_hal_blockpool_block_add(devh, block_addr, size,
	    &dma_h, &acc_handle);
}

static inline void
vxge_os_dma_sync(pci_dev_h pdev, pci_dma_h dmah, dma_addr_t dma_paddr,
    u64 dma_offset, size_t length, int dir)
{
	bus_dmasync_op_t dmasync_op;

	switch (dir) {
	case VXGE_OS_DMA_DIR_TODEVICE:
		dmasync_op = BUS_DMASYNC_PREWRITE | BUS_DMASYNC_POSTWRITE;
		break;

	case VXGE_OS_DMA_DIR_FROMDEVICE:
		dmasync_op = BUS_DMASYNC_PREREAD | BUS_DMASYNC_POSTREAD;
		break;

	default:
	case VXGE_OS_DMA_DIR_BIDIRECTIONAL:
		dmasync_op = BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE;
		break;
	}

	bus_dmamap_sync(dmah.dma_tag, dmah.dma_map, dmasync_op);
}

static inline dma_addr_t
vxge_os_dma_map(pci_dev_h pdev, pci_dma_h dmah, void *vaddr, u_long size,
		int dir, int dma_flags)
{
	int error;

	error = bus_dmamap_load(dmah.dma_tag, dmah.dma_map, dmah.dma_vaddr,
	    dmah.dma_size, vxge_dmamap_cb, &(dmah.dma_paddr), BUS_DMA_NOWAIT);

	if (error != 0)
		return (VXGE_OS_INVALID_DMA_ADDR);

	dmah.dma_size = size;
	return (dmah.dma_paddr);
}

static inline void
vxge_os_dma_unmap(pci_dev_h pdev, pci_dma_h dmah, dma_addr_t dma_paddr,
    u32 size, int dir)
{
	bus_dmamap_unload(dmah.dma_tag, dmah.dma_map);
}

#define	vxge_os_dma_free(pdev, vaddr, size, dma_flags, p_dma_acch, p_dmah)  \
	vxge_dma_free(pdev, vaddr, size, p_dma_acch, p_dmah,		    \
		__FILE__, __LINE__)

static inline int
vxge_os_is_my_packet(void *pdev, unsigned long addr)
{
	return (0);
}

#endif /* _VXGE_OSDEP_H_ */

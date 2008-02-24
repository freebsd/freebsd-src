/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 * $FreeBSD: src/sys/dev/nxge/xge-osdep.h,v 1.1.2.1 2007/11/02 00:52:32 rwatson Exp $
 */

#ifndef XGE_OSDEP_H
#define XGE_OSDEP_H

/**
 * Includes and defines
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/stddef.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/bpf.h>
#include <net/if_types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define XGE_OS_PLATFORM_64BIT

#if BYTE_ORDER == BIG_ENDIAN
#define XGE_OS_HOST_BIG_ENDIAN
#elif BYTE_ORDER == LITTLE_ENDIAN
#define XGE_OS_HOST_LITTLE_ENDIAN
#endif

#define XGE_HAL_USE_5B_MODE

#ifdef XGE_TRACE_ASSERT
#undef XGE_TRACE_ASSERT
#endif

#define OS_NETSTACK_BUF struct mbuf *
#define XGE_LL_IP_FAST_CSUM(hdr, len)  0

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(uintrptr_t)(const void *)(var))
#endif

#define xge_os_ntohs                    ntohs
#define xge_os_ntohl                    ntohl
#define xge_os_htons                    htons
#define xge_os_htonl                    htonl

typedef struct xge_bus_resource_t {
	bus_space_tag_t       bus_tag;        /* DMA Tag                      */
	bus_space_handle_t    bus_handle;     /* Bus handle                   */
	struct resource       *bar_start_addr;/* BAR start address            */
} xge_bus_resource_t;

typedef struct xge_dma_alloc_t {
	bus_addr_t            dma_phyaddr;    /* Physical Address             */
	caddr_t               dma_viraddr;    /* Virtual Address              */
	bus_dma_tag_t         dma_tag;        /* DMA Tag                      */
	bus_dmamap_t          dma_map;        /* DMA Map                      */
	bus_dma_segment_t     dma_segment;    /* DMA Segment                  */
	bus_size_t            dma_size;       /* Size                         */
	int                   dma_nseg;       /* Maximum scatter-gather segs. */
} xge_dma_alloc_t;

typedef struct xge_dma_mbuf_t {
	bus_addr_t            dma_phyaddr;    /* Physical Address             */
	bus_dmamap_t          dma_map;        /* DMA Map                      */
}xge_dma_mbuf_t;

typedef struct xge_pci_info {
	device_t              device;         /* Device                       */
	struct resource       *regmap0;       /* Resource for BAR0            */
	struct resource       *regmap1;       /* Resource for BAR1            */
	void                  *bar0resource;  /* BAR0 tag and handle          */
	void                  *bar1resource;  /* BAR1 tag and handle          */
} xge_pci_info_t;


/**
 * Fixed size primitive types
 */
#define u8                         uint8_t
#define u16                        uint16_t
#define u32                        uint32_t
#define u64                        uint64_t
#define ulong_t                    unsigned long
#define uint                       unsigned int
#define ptrdiff_t                  ptrdiff_t
typedef bus_addr_t                 dma_addr_t;
typedef struct mtx                 spinlock_t;
typedef xge_pci_info_t             *pci_dev_h;
typedef xge_bus_resource_t         *pci_reg_h;
typedef xge_dma_alloc_t             pci_dma_h;
typedef xge_dma_alloc_t             pci_dma_acc_h;
typedef struct resource            *pci_irq_h;
typedef xge_pci_info_t             *pci_cfg_h;

/**
 * "libc" functionality
 */
#define xge_os_memzero(addr, size)    bzero(addr, size)
#define xge_os_memcpy(dst, src, size) bcopy(src, dst, size)
#define xge_os_memcmp                 memcmp
#define xge_os_strcpy                 strcpy
#define xge_os_strlen                 strlen
#define xge_os_snprintf               snprintf
#define xge_os_sprintf                sprintf
#define xge_os_printf(fmt...) {                                                \
	printf(fmt);                                                           \
	printf("\n");                                                          \
}

#define xge_os_vaprintf(fmt) {                                                 \
	sprintf(fmt, fmt, "\n");                                               \
	va_list va;                                                            \
	va_start(va, fmt);                                                     \
	vprintf(fmt, va);                                                      \
	va_end(va);                                                            \
}

#define xge_os_vasprintf(buf, fmt) {                                           \
	va_list va;                                                            \
	va_start(va, fmt);                                                     \
	(void) vaprintf(buf, fmt, va);                                         \
	va_end(va);                                                            \
}

#define xge_os_timestamp(buf) {                                                \
	struct timeval current_time;                                           \
	gettimeofday(&current_time, 0);                                        \
	sprintf(buf, "%08li.%08li: ", current_time.tv_sec,                     \
	    current_time.tv_usec);                                             \
}

#define xge_os_println            xge_os_printf

/**
 * Synchronization Primitives
 */
/* Initialize the spin lock */
#define xge_os_spin_lock_init(lockp, ctxh) {                                   \
	if(mtx_initialized(lockp) == 0) {                                      \
	    mtx_init((lockp), "xge", NULL, MTX_DEF);                           \
	}                                                                      \
}

/* Initialize the spin lock (IRQ version) */
#define xge_os_spin_lock_init_irq(lockp, ctxh) {                               \
	if(mtx_initialized(lockp) == 0) {                                      \
	    mtx_init((lockp), "xge", NULL, MTX_DEF);                           \
	}                                                                      \
}

/* Destroy the lock */
#define xge_os_spin_lock_destroy(lockp, ctxh) {                                \
	if(mtx_initialized(lockp) != 0) {                                      \
	    mtx_destroy(lockp);                                                \
	}                                                                      \
}

/* Destroy the lock (IRQ version) */
#define xge_os_spin_lock_destroy_irq(lockp, ctxh) {                            \
	if(mtx_initialized(lockp) != 0) {                                      \
	    mtx_destroy(lockp);                                                \
	}                                                                      \
}

/* Acquire the lock */
#define xge_os_spin_lock(lockp) {                                              \
	if(mtx_owned(lockp) == 0) mtx_lock(lockp);                             \
}

/* Release the lock */
#define xge_os_spin_unlock(lockp) {                                            \
	mtx_unlock(lockp);                                                     \
}

/* Acquire the lock (IRQ version) */
#define xge_os_spin_lock_irq(lockp, flags) {                                   \
	flags = MTX_QUIET;                                                     \
	if(mtx_owned(lockp) == 0) mtx_lock_flags(lockp, flags);                \
}

/* Release the lock (IRQ version) */
#define xge_os_spin_unlock_irq(lockp, flags) {                                 \
	flags = MTX_QUIET;                                                     \
	mtx_unlock_flags(lockp, flags);                                        \
}

/* Write memory barrier */
#define xge_os_wmb()

/* Delay (in micro seconds) */
#define xge_os_udelay(us)            DELAY(us)

/* Delay (in milli seconds) */
#define xge_os_mdelay(ms)            DELAY(ms * 1000)

/* Compare and exchange */
//#define xge_os_cmpxchg(targetp, cmd, newval)

/**
 * Misc primitives
 */
#define xge_os_unlikely(x)    (x)
#define xge_os_prefetch(x)    (x=x)
#define xge_os_prefetchw(x)   (x=x)
#define xge_os_bug(fmt...)    printf(fmt)
#define xge_os_htohs          ntohs
#define xge_os_ntohl          ntohl
#define xge_os_htons          htons
#define xge_os_htonl          htonl

/**
 * Compiler Stuffs
 */
#define __xge_os_attr_cacheline_aligned
#define __xge_os_cacheline_size        32

/**
 * Memory Primitives
 */
#define XGE_OS_INVALID_DMA_ADDR ((dma_addr_t)0)

/**
 * xge_os_malloc
 * Allocate non DMA-able memory.
 * @pdev: Device context.
 * @size: Size to allocate.
 *
 * Allocate @size bytes of memory. This allocation can sleep, and therefore,
 * and therefore it requires process context. In other words, xge_os_malloc()
 * cannot be called from the interrupt context. Use xge_os_free() to free the
 * allocated block.
 *
 * Returns: Pointer to allocated memory, NULL - on failure.
 *
 * See also: xge_os_free().
 */
static inline void *
xge_os_malloc(pci_dev_h pdev, unsigned long size) {
	void *vaddr = malloc((size), M_DEVBUF, M_NOWAIT | M_ZERO);
	if(vaddr != NULL) {
	    XGE_OS_MEMORY_CHECK_MALLOC(vaddr, size, __FILE__, __LINE__);
	    xge_os_memzero(vaddr, size);
	}
	return (vaddr);
}

/**
 * xge_os_free
 * Free non DMA-able memory.
 * @pdev: Device context.
 * @vaddr: Address of the allocated memory block.
 * @size: Some OS's require to provide size on free
 *
 * Free the memory area obtained via xge_os_malloc(). This call may also sleep,
 * and therefore it cannot be used inside interrupt.
 *
 * See also: xge_os_malloc().
 */
static inline void
xge_os_free(pci_dev_h pdev, const void *vaddr, unsigned long size) {
	XGE_OS_MEMORY_CHECK_FREE(vaddr, size);
	free(__DECONST(void *, vaddr), M_DEVBUF);
}

static void
xge_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error) {
	if(error) return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

/**
 * xge_os_dma_malloc
 * Allocate DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @size: Size (in bytes) to allocate.
 * @dma_flags: XGE_OS_DMA_CACHELINE_ALIGNED, XGE_OS_DMA_STREAMING,
 * XGE_OS_DMA_CONSISTENT (Note that the last two flags are mutually exclusive.)
 * @p_dmah: Handle used to map the memory onto the corresponding device memory
 * space. See xge_os_dma_map(). The handle is an out-parameter returned by the
 * function.
 * @p_dma_acch: One more DMA handle used subsequently to free the DMA object
 * (via xge_os_dma_free()).
 *
 * Allocate DMA-able contiguous memory block of the specified @size. This memory
 * can be subsequently freed using xge_os_dma_free().
 * Note: can be used inside interrupt context.
 *
 * Returns: Pointer to allocated memory(DMA-able), NULL on failure.
 */
static inline void *
xge_os_dma_malloc(pci_dev_h pdev, unsigned long size, int dma_flags,
	pci_dma_h *p_dmah, pci_dma_acc_h *p_dma_acch) {
	int retValue = bus_dma_tag_create(
	    bus_get_dma_tag(pdev->device), /* Parent                          */
	    PAGE_SIZE,                     /* Alignment no specific alignment */
	    0,                             /* Bounds                          */
	    BUS_SPACE_MAXADDR,             /* Low Address                     */
	    BUS_SPACE_MAXADDR,             /* High Address                    */
	    NULL,                          /* Filter                          */
	    NULL,                          /* Filter arg                      */
	    size,                          /* Max Size                        */
	    1,                             /* n segments                      */
	    size,                          /* max segment size                */
	    BUS_DMA_ALLOCNOW,              /* Flags                           */
	    NULL,                          /* lockfunction                    */
	    NULL,                          /* lock arg                        */
	    &p_dmah->dma_tag);             /* DMA tag                         */
	if(retValue != 0) {
	    xge_os_printf("bus_dma_tag_create failed\n")
	    goto fail_1;
	}
	p_dmah->dma_size = size;
	retValue = bus_dmamem_alloc(p_dmah->dma_tag,
	    (void **)&p_dmah->dma_viraddr, BUS_DMA_NOWAIT, &p_dmah->dma_map);
	if(retValue != 0) {
	    xge_os_printf("bus_dmamem_alloc failed\n")
	    goto fail_2;
	}
	XGE_OS_MEMORY_CHECK_MALLOC(p_dmah->dma_viraddr, p_dmah->dma_size,
	    __FILE__, __LINE__);
	return(p_dmah->dma_viraddr);

fail_2: bus_dma_tag_destroy(p_dmah->dma_tag);
fail_1: return(NULL);
}

/**
 * xge_os_dma_free
 * Free previously allocated DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @vaddr: Virtual address of the DMA-able memory.
 * @p_dma_acch: DMA handle used to free the resource.
 * @p_dmah: DMA handle used for mapping. See xge_os_dma_malloc().
 *
 * Free DMA-able memory originally allocated by xge_os_dma_malloc().
 * Note: can be used inside interrupt.
 * See also: xge_os_dma_malloc().
 */
static inline void
xge_os_dma_free(pci_dev_h pdev, const void *vaddr, int size,
	pci_dma_acc_h *p_dma_acch, pci_dma_h *p_dmah)
{
	XGE_OS_MEMORY_CHECK_FREE(p_dmah->dma_viraddr, size);
	bus_dmamem_free(p_dmah->dma_tag, p_dmah->dma_viraddr, p_dmah->dma_map);
	bus_dma_tag_destroy(p_dmah->dma_tag);
	p_dmah->dma_map = NULL;
	p_dmah->dma_tag = NULL;
	p_dmah->dma_viraddr = NULL;
	return;
}

/**
 * IO/PCI/DMA Primitives
 */
#define XGE_OS_DMA_DIR_TODEVICE        0
#define XGE_OS_DMA_DIR_FROMDEVICE      1
#define XGE_OS_DMA_DIR_BIDIRECTIONAL   2

/**
 * xge_os_pci_read8
 * Read one byte from device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform PIO and/or
 * config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of the result.
 *
 * Read byte value from the specified @regh PCI configuration space at the
 * specified offset = @where.
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_read8(pdev, cfgh, where, val)                               \
	(*(val) = pci_read_config(pdev->device, where, 1))

/**
 * xge_os_pci_write8
 * Write one byte into device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform PIO and/or
 * config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write byte value into the specified PCI configuration space
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_write8(pdev, cfgh, where, val)                              \
	pci_write_config(pdev->device, where, val, 1)

/**
 * xge_os_pci_read16
 * Read 16bit word from device PCI configuration.
 * @pdev: Device context.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of the 16bit result.
 *
 * Read 16bit value from the specified PCI configuration space at the
 * specified offset.
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_read16(pdev, cfgh, where, val)                              \
	(*(val) = pci_read_config(pdev->device, where, 2))

/**
 * xge_os_pci_write16
 * Write 16bit word into device PCI configuration.
 * @pdev: Device context.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write 16bit value into the specified @offset in PCI configuration space.
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_write16(pdev, cfgh, where, val)                             \
	pci_write_config(pdev->device, where, val, 2)

/**
 * xge_os_pci_read32
 * Read 32bit word from device PCI configuration.
 * @pdev: Device context.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of 32bit result.
 *
 * Read 32bit value from the specified PCI configuration space at the
 * specified offset.
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_read32(pdev, cfgh, where, val)                              \
	(*(val) = pci_read_config(pdev->device, where, 4))

/**
 * xge_os_pci_write32
 * Write 32bit word into device PCI configuration.
 * @pdev: Device context.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write 32bit value into the specified @offset in PCI configuration space.
 * Returns: 0 - success, non-zero - failure.
 */
#define xge_os_pci_write32(pdev, cfgh, where, val)                             \
	pci_write_config(pdev->device, where, val, 4)

/**
 * xge_os_pio_mem_read8
 * Read 1 byte from device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 1 byte value read from the specified (mapped) memory space address.
 */
static inline u8
xge_os_pio_mem_read8(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)
	    (((xge_bus_resource_t *)(regh))->bar_start_addr);

	return bus_space_read_1(tag, handle, (caddr_t)(addr) - addrss);
}

/**
 * xge_os_pio_mem_write8
 * Write 1 byte into device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write byte value into the specified (mapped) device memory space.
 */
static inline void
xge_os_pio_mem_write8(pci_dev_h pdev, pci_reg_h regh, u8 val, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)
	    (((xge_bus_resource_t *)(regh))->bar_start_addr);

	bus_space_write_1(tag, handle, (caddr_t)(addr) - addrss, val);
}

/**
 * xge_os_pio_mem_read16
 * Read 16bit from device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 16bit value read from the specified (mapped) memory space address.
 */
static inline u16
xge_os_pio_mem_read16(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)
	    (((xge_bus_resource_t *)(regh))->bar_start_addr);

	return bus_space_read_2(tag, handle, (caddr_t)(addr) - addrss);
}

/**
 * xge_os_pio_mem_write16
 * Write 16bit into device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 16bit value into the specified (mapped) device memory space.
 */
static inline void
xge_os_pio_mem_write16(pci_dev_h pdev, pci_reg_h regh, u16 val, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)(((xge_bus_resource_t *)(regh))->bar_start_addr);

	bus_space_write_2(tag, handle, (caddr_t)(addr) - addrss, val);
}

/**
 * xge_os_pio_mem_read32
 * Read 32bit from device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 32bit value read from the specified (mapped) memory space address.
 */
static inline u32
xge_os_pio_mem_read32(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)
	    (((xge_bus_resource_t *)(regh))->bar_start_addr);

	return bus_space_read_4(tag, handle, (caddr_t)(addr) - addrss);
}

/**
 * xge_os_pio_mem_write32
 * Write 32bit into device memory space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 32bit value into the specified (mapped) device memory space.
 */
static inline void
xge_os_pio_mem_write32(pci_dev_h pdev, pci_reg_h regh, u32 val, void *addr)
{
	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)(((xge_bus_resource_t *)(regh))->bar_start_addr);
	bus_space_write_4(tag, handle, (caddr_t)(addr) - addrss, val);
}

/**
 * xge_os_pio_mem_read64
 * Read 64bit from device memory mapped space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 64bit value read from the specified (mapped) memory space address.
 */
static inline u64
xge_os_pio_mem_read64(pci_dev_h pdev, pci_reg_h regh, void *addr)
{
	u64 value1, value2;

	bus_space_tag_t tag =
	    (bus_space_tag_t)(((xge_bus_resource_t *)regh)->bus_tag);
	bus_space_handle_t handle =
	    (bus_space_handle_t)(((xge_bus_resource_t *)regh)->bus_handle);
	caddr_t addrss = (caddr_t)
	    (((xge_bus_resource_t *)(regh))->bar_start_addr);

	value1 = bus_space_read_4(tag, handle, (caddr_t)(addr) + 4 - addrss);
	value1 <<= 32;
	value2 = bus_space_read_4(tag, handle, (caddr_t)(addr) - addrss);
	value1 |= value2;
	return value1;
}

/**
 * xge_os_pio_mem_write64
 * Write 32bit into device memory space.
 * @pdev: Device context.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 64bit value into the specified (mapped) device memory space.
 */
static inline void
xge_os_pio_mem_write64(pci_dev_h pdev, pci_reg_h regh, u64 val, void *addr)
{
	u32 vall = val & 0xffffffff;
	xge_os_pio_mem_write32(pdev, regh, vall, addr);
	xge_os_pio_mem_write32(pdev, regh, val >> 32, ((caddr_t)(addr) + 4));
}

/**
 * FIXME: document
 */
#define xge_os_flush_bridge    xge_os_pio_mem_read64

/**
 * xge_os_dma_map
 * Map DMA-able memory block to, or from, or to-and-from device.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @dmah: DMA handle used to map the memory block. Obtained via
 * xge_os_dma_malloc().
 * @vaddr: Virtual address of the DMA-able memory.
 * @size: Size (in bytes) to be mapped.
 * @dir: Direction of this operation (XGE_OS_DMA_DIR_TODEVICE, etc.)
 * @dma_flags: XGE_OS_DMA_CACHELINE_ALIGNED, XGE_OS_DMA_STREAMING,
 * XGE_OS_DMA_CONSISTENT (Note that the last two flags are mutually exclusive).
 *
 * Map a single memory block.
 *
 * Returns: DMA address of the memory block, XGE_OS_INVALID_DMA_ADDR on failure.
 *
 * See also: xge_os_dma_malloc(), xge_os_dma_unmap(), xge_os_dma_sync().
 */
static inline dma_addr_t
xge_os_dma_map(pci_dev_h pdev, pci_dma_h dmah, void *vaddr, size_t size,
	int dir, int dma_flags)
{
	int retValue =
	    bus_dmamap_load(dmah.dma_tag, dmah.dma_map, dmah.dma_viraddr,
	    dmah.dma_size, xge_dmamap_cb, &dmah.dma_phyaddr, BUS_DMA_NOWAIT);
	if(retValue != 0) {
	    xge_os_printf("bus_dmamap_load_ failed\n")
	    return XGE_OS_INVALID_DMA_ADDR;
	}
	dmah.dma_size = size;
	return dmah.dma_phyaddr;
}

/**
 * xge_os_dma_unmap - Unmap DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @dmah: DMA handle used to map the memory block. Obtained via
 * xge_os_dma_malloc().
 * @dma_addr: DMA address of the block. Obtained via xge_os_dma_map().
 * @size: Size (in bytes) to be unmapped.
 * @dir: Direction of this operation (XGE_OS_DMA_DIR_TODEVICE, etc.)
 *
 * Unmap a single DMA-able memory block that was previously mapped using
 * xge_os_dma_map().
 * See also: xge_os_dma_malloc(), xge_os_dma_map().
 */
static inline void
xge_os_dma_unmap(pci_dev_h pdev, pci_dma_h dmah, dma_addr_t dma_addr,
	size_t size, int dir)
{
	bus_dmamap_unload(dmah.dma_tag, dmah.dma_map);
	return;
}

/**
 * xge_os_dma_sync - Synchronize mapped memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @dmah: DMA handle used to map the memory block. Obtained via
 * xge_os_dma_malloc().
 * @dma_addr: DMA address of the block. Obtained via xge_os_dma_map().
 * @dma_offset: Offset from start of the blocke. Used by Solaris only.
 * @length: Size of the block.
 * @dir: Direction of this operation (XGE_OS_DMA_DIR_TODEVICE, etc.)
 *
 * Make physical and CPU memory consistent for a single streaming mode DMA
 * translation. This API compiles to NOP on cache-coherent platforms. On
 * non cache-coherent platforms, depending on the direction of the "sync"
 * operation, this API will effectively either invalidate CPU cache (that might
 * contain old data), or flush CPU cache to update physical memory.
 * See also: xge_os_dma_malloc(), xge_os_dma_map(),
 * xge_os_dma_unmap().
 */
static inline void
xge_os_dma_sync(pci_dev_h pdev, pci_dma_h dmah, dma_addr_t dma_addr,
	u64 dma_offset, size_t length, int dir)
{
	bus_dmasync_op_t syncop;
	switch(dir) {
	    case XGE_OS_DMA_DIR_TODEVICE:
	        syncop = BUS_DMASYNC_PREWRITE | BUS_DMASYNC_POSTWRITE;
	        break;

	    case XGE_OS_DMA_DIR_FROMDEVICE:
	        syncop = BUS_DMASYNC_PREREAD | BUS_DMASYNC_POSTREAD;
	        break;

	    default:
	        syncop = BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREWRITE;
	        break;
	}
	bus_dmamap_sync(dmah.dma_tag, dmah.dma_map, syncop);
	return;
}

#endif /* XGE_OSDEP_H */


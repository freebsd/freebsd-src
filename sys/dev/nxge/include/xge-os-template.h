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
 * $FreeBSD$
 */

/*
 *  FileName :    xge-os-template.h
 *
 *  Description:  Template for creating platform-dependent "glue" code.
 *
 *  Created:      6 May 2004
 */

#ifndef XGE_OS_TEMPLATE_H
#define XGE_OS_TEMPLATE_H

#ifndef TEMPLATE
#	error "should not be compiled for platforms other than TEMPLATE..."
#endif

/* ------------------------- includes and defines ------------------------- */

/*
 * Note:
 *
 *     - on some operating systems like Linux & FreeBSD, there is a macro
 *       by using which it is possible to determine endiennes automatically
 */
#define XGE_OS_HOST_BIG_ENDIAN	TEMPLATE

#define XGE_OS_HOST_PAGE_SIZE	TEMPLATE

/* ---------------------- fixed size primitive types -----------------------*/

/*
 * Note:
 *
 *	- u## - means ## bits unsigned int/long
 *      - all names must be preserved since HAL using them.
 *      - ulong_t is platform specific, i.e. for 64bit - 64bit size, for
 *        32bit - 32bit size
 */
#define TEMPLATE		u8
#define TEMPLATE		u16
#define TEMPLATE		u32
#define TEMPLATE		u64
#define TEMPLATE		ulong_t
#define TEMPLATE		ptrdiff_t
#define TEMPLATE		dma_addr_t
#define TEMPLATE		spinlock_t
typedef TEMPLATE		pci_dev_h;
typedef TEMPLATE		pci_reg_h;
typedef TEMPLATE		pci_dma_h;
typedef TEMPLATE		pci_irq_h;
typedef TEMPLATE		pci_cfg_h;
typedef TEMPLATE		pci_dma_acc_h;

/* -------------------------- "libc" functionality -------------------------*/

/*
 * Note:
 *
 *	- "libc" functionality maps one-to-one to be posix-like
 */
/* Note: use typedef: xge_os_memzero(void* mem, int size); */
#define xge_os_memzero			TEMPLATE

/* Note: the 1st argument MUST be destination, like in:
 * void *memcpy(void *dest, const void *src, size_t n);
 */
#define xge_os_memcpy			TEMPLATE

/* Note: should accept format (the 1st argument) and a variable
 * number of arguments thereafter.. */
#define xge_os_printf(fmt...)		TEMPLATE

#define xge_os_vasprintf(buf, fmt...)	TEMPLATE

#define xge_os_sprintf(buf, fmt, ...)	TEMPLATE

#define xge_os_timestamp(buf)		TEMPLATE

#define xge_os_println			TEMPLATE

/* -------------------- synchronization primitives -------------------------*/

/*
 * Note:
 *
 *	- use spin_lock in interrupts or in threads when there is no races
 *	  with interrupt
 *	- use spin_lock_irqsave in threads if there is a race with interrupt
 *	- use spin_lock_irqsave for nested locks
 */

/*
 * Initialize the spin lock.
 */
#define xge_os_spin_lock_init(lockp, ctxh)	TEMPLATE
/*
 * Initialize the spin lock (IRQ version).
 */
#define xge_os_spin_lock_init_irq(lockp, ctxh)	TEMPLATE
/*
 * Destroy the lock.
 */
#define xge_os_spin_lock_destroy(lockp, ctxh)	TEMPLATE

/*
 * Destroy the lock (IRQ version).
 */
#define xge_os_spin_lock_destroy_irq(lockp, ctxh)	TEMPLATE
/*
 * Acquire the lock.
 */
#define xge_os_spin_lock(lockp)			TEMPLATE
/*
 * Release the lock.
 */
#define xge_os_spin_unlock(lockp)		TEMPLATE
/*
 * Acquire the lock(IRQ version).
 */
#define xge_os_spin_lock_irq(lockp, flags)	TEMPLATE
/*
 * Release the lock(IRQ version).
 */
#define xge_os_spin_unlock_irq(lockp, flags)	TEMPLATE
/*
 * Write memory barrier.
 */
#define xge_os_wmb()				TEMPLATE
/*
 * Delay (in micro seconds).
 */
#define xge_os_udelay(us)			TEMPLATE
/*
 * Delay (in milli seconds).
 */
#define xge_os_mdelay(ms)			TEMPLATE
/*
 * Compare and exchange.
 */
#define xge_os_cmpxchg(targetp, cmp, newval)	TEMPLATE



/* ------------------------- misc primitives -------------------------------*/

#define xge_os_prefetch			TEMPLATE
#define xge_os_prefetchw		TEMPLATE
#define xge_os_bug(fmt...)		TEMPLATE

/* -------------------------- compiler stuffs ------------------------------*/

#define __xge_os_attr_cacheline_aligned	TEMPLATE

/* ---------------------- memory primitives --------------------------------*/

/**
 * xge_os_malloc - Allocate non DMA-able memory.
 * @pdev: Device context. Some OSs require device context to perform
 *        operations on memory.
 * @size: Size to allocate.
 *
 * Allocate @size bytes of memory. This allocation can sleep, and
 * therefore, and therefore it requires process context. In other words,
 * xge_os_malloc() cannot be called from the interrupt context.
 * Use xge_os_free() to free the allocated block.
 *
 * Returns: Pointer to allocated memory, NULL - on failure.
 *
 * See also: xge_os_free().
 */
static inline void *xge_os_malloc(IN  pci_dev_h pdev,
                                IN  unsigned long size)
{ TEMPLATE; }

/**
 * xge_os_free - Free non DMA-able memory.
 * @pdev: Device context. Some OSs require device context to perform
 *        operations on memory.
 * @vaddr: Address of the allocated memory block.
 * @size: Some OS's require to provide size on free
 *
 * Free the memory area obtained via xge_os_malloc().
 * This call may also sleep, and therefore it cannot be used inside
 * interrupt.
 *
 * See also: xge_os_malloc().
 */
static inline void xge_os_free(IN  pci_dev_h pdev,
                             IN  const void *vaddr,
			     IN unsigned long size)
{ TEMPLATE; }

/**
 * xge_os_vaddr - Get Virtual address for the given physical address.
 * @pdev: Device context. Some OSs require device context to perform
 *        operations on memory.
 * @vaddr: Physical Address of the memory block.
 * @size: Some OS's require to provide size
 *
 * Get the virtual address for physical address.
 * This call may also sleep, and therefore it cannot be used inside
 * interrupt.
 *
 * See also: xge_os_malloc().
 */
static inline void xge_os_vaddr(IN  pci_dev_h pdev,
                             IN  const void *vaddr,
			     IN unsigned long size)
{ TEMPLATE; }

/**
 * xge_os_dma_malloc  -  Allocate DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @size: Size (in bytes) to allocate.
 * @dma_flags: XGE_OS_DMA_CACHELINE_ALIGNED,
 *             XGE_OS_DMA_STREAMING,
 *             XGE_OS_DMA_CONSISTENT
 *	 Note that the last two flags are mutually exclusive.
 * @p_dmah: Handle used to map the memory onto the corresponding device memory
 *          space. See xge_os_dma_map(). The handle is an out-parameter
 *          returned by the function.
 * @p_dma_acch: One more DMA handle used subsequently to free the
 *              DMA object (via xge_os_dma_free()).
 *              Note that this and the previous handle have
 *              physical meaning for Solaris; on Windows and Linux the
 *              corresponding value will be simply a pointer to PCI device.
 *              The value is returned by this function.
 *
 * Allocate DMA-able contiguous memory block of the specified @size.
 * This memory can be subsequently freed using xge_os_dma_free().
 * Note: can be used inside interrupt context.
 *
 * Returns: Pointer to allocated memory(DMA-able), NULL on failure.
 *
 */
static inline void *xge_os_dma_malloc(IN  pci_dev_h pdev,
				    IN  unsigned long size,
				    IN  int dma_flags,
				    OUT pci_dma_h *p_dmah,
				    OUT pci_dma_acc_h *p_dma_acch)
{ TEMPLATE; }

/**
 * xge_os_dma_free - Free previously allocated DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @vaddr: Virtual address of the DMA-able memory.
 * @p_dma_acch: DMA handle used to free the resource.
 * @p_dmah: DMA handle used for mapping. See xge_os_dma_malloc().
 *
 * Free DMA-able memory originally allocated by xge_os_dma_malloc().
 * Note: can be used inside interrupt.
 * See also: xge_os_dma_malloc().
 */
static inline void xge_os_dma_free (IN  pci_dev_h pdev,
                                  IN  const void *vaddr,
				  IN  pci_dma_acc_h *p_dma_acch,
				  IN  pci_dma_h *p_dmah)
{ TEMPLATE; }

/* ----------------------- io/pci/dma primitives ---------------------------*/

#define XGE_OS_DMA_DIR_TODEVICE		TEMPLATE
#define XGE_OS_DMA_DIR_FROMDEVICE	TEMPLATE
#define XGE_OS_DMA_DIR_BIDIRECTIONAL	TEMPLATE

/**
 * xge_os_pci_read8 - Read one byte from device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of the result.
 *
 * Read byte value from the specified @regh PCI configuration space at the
 * specified offset = @where.
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_read8(IN  pci_dev_h pdev,
				 IN  pci_cfg_h cfgh,
				 IN  int where,
				 IN  u8 *val)
{ TEMPLATE; }

/**
 * xge_os_pci_write8 - Write one byte into device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write byte value into the specified PCI configuration space
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_write8(IN  pci_dev_h pdev,
				  IN  pci_cfg_h cfgh,
				  IN  int where,
				  IN  u8 val)
{ TEMPLATE; }

/**
 * xge_os_pci_read16 - Read 16bit word from device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of the 16bit result.
 *
 * Read 16bit value from the specified PCI configuration space at the
 * specified offset.
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_read16(IN  pci_dev_h pdev,
				  IN  pci_cfg_h cfgh,
				  IN  int where,
				  IN  u16 *val)
{ TEMPLATE; }

/**
 * xge_os_pci_write16 - Write 16bit word into device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write 16bit value into the specified @offset in PCI
 * configuration space.
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_write16(IN  pci_dev_h pdev,
				   IN  pci_cfg_h cfgh,
				   IN  int where,
				   IN  u16 val)
{ TEMPLATE; }

/**
 * xge_os_pci_read32 - Read 32bit word from device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Address of 32bit result.
 *
 * Read 32bit value from the specified PCI configuration space at the
 * specified offset.
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_read32(IN  pci_dev_h pdev,
				  IN  pci_cfg_h cfgh,
				  IN  int where,
				  IN  u32 *val)
{ TEMPLATE; }

/**
 * xge_os_pci_write32 - Write 32bit word into device PCI configuration.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO.
 * @cfgh: PCI configuration space handle.
 * @where: Offset in the PCI configuration space.
 * @val: Value to write.
 *
 * Write 32bit value into the specified @offset in PCI
 * configuration space.
 * Returns: 0 - success, non-zero - failure.
 */
static inline int xge_os_pci_write32(IN  pci_dev_h pdev,
				   IN  pci_cfg_h cfgh,
				   IN  int where,
				   IN  u32 val)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_read8 - Read 1 byte from device memory mapped space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO..
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 1 byte value read from the specified (mapped) memory space address.
 */
static inline u8 xge_os_pio_mem_read8(IN  pci_dev_h pdev,
                                    IN  pci_reg_h regh,
				    IN void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_write64 - Write 1 byte into device memory mapped
 * space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO and/or config space IO..
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write byte value into the specified (mapped) device memory space.
 */
static inline void xge_os_pio_mem_write8(IN  pci_dev_h pdev,
                                       IN  pci_reg_h regh,
				       IN  u8 val,
				       IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_read16 - Read 16bit from device memory mapped space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 16bit value read from the specified (mapped) memory space address.
 */
static inline u16 xge_os_pio_mem_read16(IN  pci_dev_h pdev,
                                      IN  pci_reg_h regh,
				      IN  void *addr)
{
TEMPLATE; }

/**
 * xge_os_pio_mem_write16 - Write 16bit into device memory mapped space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 16bit value into the specified (mapped) device memory space.
 */
static inline void xge_os_pio_mem_write16(IN  pci_dev_h pdev,
                                        IN  pci_reg_h regh,
					IN  u16 val,
					IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_read32 - Read 32bit from device memory mapped space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 32bit value read from the specified (mapped) memory space address.
 */
static inline u32 xge_os_pio_mem_read32(IN  pci_dev_h pdev,
                                      IN  pci_reg_h regh,
				      IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_write32 - Write 32bit into device memory space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 32bit value into the specified (mapped) device memory space.
 */
static inline void xge_os_pio_mem_write32(IN  pci_dev_h pdev,
                                        IN  pci_reg_h regh,
					IN  u32 val,
					IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_read64 - Read 64bit from device memory mapped space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Returns: 64bit value read from the specified (mapped) memory space address.
 */
static inline u64 xge_os_pio_mem_read64(IN  pci_dev_h pdev,
                                      IN  pci_reg_h regh,
				      IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_pio_mem_write64 - Write 64bit into device memory space.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @val: Value to write.
 * @addr: Address in device memory space.
 *
 * Write 64bit value into the specified (mapped) device memory space.
 */
static inline void xge_os_pio_mem_write64(IN  pci_dev_h pdev,
                                        IN  pci_reg_h regh,
					IN  u64 val,
					IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_flush_bridge - Flush the bridge.
 * @pdev: Device context. Some OSs require device context to perform
 *        PIO.
 * @regh: PCI configuration space handle.
 * @addr: Address in device memory space.
 *
 * Flush the bridge.
 */
static inline void xge_os_flush_bridge(IN  pci_dev_h pdev,
				       IN  pci_reg_h regh,
				       IN  void *addr)
{ TEMPLATE; }

/**
 * xge_os_dma_map - Map DMA-able memory block to, or from, or
 * to-and-from device.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @dmah: DMA handle used to map the memory block. Obtained via
 * xge_os_dma_malloc().
 * @vaddr: Virtual address of the DMA-able memory.
 * @size: Size (in bytes) to be mapped.
 * @dir: Direction of this operation (XGE_OS_DMA_DIR_TODEVICE, etc.)
 * @dma_flags: XGE_OS_DMA_CACHELINE_ALIGNED,
 *             XGE_OS_DMA_STREAMING,
 *             XGE_OS_DMA_CONSISTENT
 *	 Note that the last two flags are mutually exclusive.
 *
 * Map a single memory block.
 *
 * Returns: DMA address of the memory block,
 * XGE_OS_INVALID_DMA_ADDR on failure.
 *
 * See also: xge_os_dma_malloc(), xge_os_dma_unmap(),
 * xge_os_dma_sync().
 */
static inline dma_addr_t xge_os_dma_map(IN  pci_dev_h pdev,
				      IN  pci_dma_h dmah,
				      IN  void *vaddr,
				      IN  size_t size,
				      IN  int dir,
				      IN  int dma_flags)
{ TEMPLATE; }

/**
 * xge_os_dma_unmap - Unmap DMA-able memory.
 * @pdev: Device context. Used to allocate/pin/map/unmap DMA-able memory.
 * @dmah: DMA handle used to map the memory block. Obtained via
 * xge_os_dma_malloc().
 * @dma_addr: DMA address of the block. Obtained via xge_os_dma_map().
 * @size: Size (in bytes) to be unmapped.
 * @dir: Direction of this operation (XGE_OS_DMA_DIR_TODEVICE, etc.)
 *
 * Unmap a single DMA-able memory block that was previously mapped
 * using xge_os_dma_map().
 * See also: xge_os_dma_malloc(), xge_os_dma_map().
 */
static inline void xge_os_dma_unmap(IN  pci_dev_h pdev,
				  IN  pci_dma_h dmah,
				  IN  dma_addr_t dma_addr,
				  IN  size_t size,
				  IN  int dir)
{ TEMPLATE; }

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
 * Make physical and CPU memory consistent for a single
 * streaming mode DMA translation.
 * This API compiles to NOP on cache-coherent platforms.
 * On non cache-coherent platforms, depending on the direction
 * of the "sync" operation, this API will effectively
 * either invalidate CPU cache (that might contain old data),
 * or  flush CPU cache to update physical memory.
 * See also: xge_os_dma_malloc(), xge_os_dma_map(),
 * xge_os_dma_unmap().
 */
static inline void xge_os_dma_sync(IN  pci_dev_h pdev,
				 IN  pci_dma_h dmah,
				 IN  dma_addr_t dma_addr,
				 IN  u64 dma_offset,
				 IN  size_t length,
				 IN  int dir)
{ TEMPLATE; }

#endif /* XGE_OS_TEMPLATE_H */

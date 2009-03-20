/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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
 */

#ifndef _USB2_BUSDMA_H_
#define	_USB2_BUSDMA_H_

#include <sys/uio.h>
#include <sys/mbuf.h>

#include <machine/bus.h>

/* defines */

#define	USB_PAGE_SIZE PAGE_SIZE		/* use system PAGE_SIZE */

#ifdef __FreeBSD__
#if (__FreeBSD_version >= 700020)
#define	USB_GET_DMA_TAG(dev) bus_get_dma_tag(dev)
#else
#define	USB_GET_DMA_TAG(dev) NULL	/* XXX */
#endif
#endif

/* structure prototypes */

struct usb2_xfer_root;
struct usb2_dma_parent_tag;

/*
 * The following typedef defines the USB DMA load done callback.
 */

typedef void (usb2_dma_callback_t)(struct usb2_dma_parent_tag *udpt);

/*
 * The following structure defines physical and non kernel virtual
 * address of a memory page having size USB_PAGE_SIZE.
 */
struct usb2_page {
#if USB_HAVE_BUSDMA
	bus_size_t physaddr;
	void   *buffer;			/* non Kernel Virtual Address */
#endif
};

/*
 * The following structure is used when needing the kernel virtual
 * pointer and the physical address belonging to an offset in an USB
 * page cache.
 */
struct usb2_page_search {
	void   *buffer;
#if USB_HAVE_BUSDMA
	bus_size_t physaddr;
#endif
	usb2_size_t length;
};

/*
 * The following structure is used to keep information about a DMA
 * memory allocation.
 */
struct usb2_page_cache {

#if USB_HAVE_BUSDMA && defined(__FreeBSD__)
	bus_dma_tag_t tag;
	bus_dmamap_t map;
#endif
#if USB_HAVE_BUSDMA && defined(__NetBSD__)
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	bus_dma_segment_t *p_seg;
#endif
#if USB_HAVE_BUSDMA
	struct usb2_page *page_start;
#endif
	struct usb2_dma_parent_tag *tag_parent;	/* always set */
	void   *buffer;			/* virtual buffer pointer */
#if USB_HAVE_BUSDMA && defined(_NetBSD__)
	int	n_seg;
#endif
#if USB_HAVE_BUSDMA
	usb2_size_t page_offset_buf;
	usb2_size_t page_offset_end;
	uint8_t	isread:1;		/* set if we are currently reading
					 * from the memory. Else write. */
	uint8_t	ismultiseg:1;		/* set if we can have multiple
					 * segments */
#endif
};

/*
 * The following structure describes the parent USB DMA tag.
 */
struct usb2_dma_parent_tag {
#if USB_HAVE_BUSDMA && defined(__FreeBSD__)
	struct cv cv[1];		/* internal condition variable */
#endif
#if USB_HAVE_BUSDMA
	bus_dma_tag_t tag;		/* always set */

	struct mtx *mtx;		/* private mutex, always set */
	usb2_dma_callback_t *func;	/* load complete callback function */
	struct usb2_dma_tag *utag_first;/* pointer to first USB DMA tag */
	uint8_t	dma_error;		/* set if DMA load operation failed */
	uint8_t	dma_bits;		/* number of DMA address lines */
	uint8_t	utag_max;		/* number of USB DMA tags */
#endif
};

/*
 * The following structure describes an USB DMA tag.
 */
struct usb2_dma_tag {
#if USB_HAVE_BUSDMA && defined(__NetBSD__)
	bus_dma_segment_t *p_seg;
#endif
#if USB_HAVE_BUSDMA
	struct usb2_dma_parent_tag *tag_parent;
	bus_dma_tag_t tag;

	usb2_size_t align;
	usb2_size_t size;
#endif
#if USB_HAVE_BUSDMA && defined(__NetBSD__)
	usb2_size_t n_seg;
#endif
};

/* function prototypes */

int	usb2_uiomove(struct usb2_page_cache *pc, struct uio *uio,
	    usb2_frlength_t pc_offset, usb2_frlength_t len);
struct usb2_dma_tag *usb2_dma_tag_find(struct usb2_dma_parent_tag *udpt,
	    usb2_size_t size, usb2_size_t align);
uint8_t	usb2_pc_alloc_mem(struct usb2_page_cache *pc, struct usb2_page *pg,
	    usb2_size_t size, usb2_size_t align);
uint8_t	usb2_pc_dmamap_create(struct usb2_page_cache *pc, usb2_size_t size);
uint8_t	usb2_pc_load_mem(struct usb2_page_cache *pc, usb2_size_t size,
	    uint8_t sync);
void	usb2_bdma_done_event(struct usb2_dma_parent_tag *udpt);
void	usb2_bdma_post_sync(struct usb2_xfer *xfer);
void	usb2_bdma_pre_sync(struct usb2_xfer *xfer);
void	usb2_bdma_work_loop(struct usb2_xfer_queue *pq);
void	usb2_bzero(struct usb2_page_cache *cache, usb2_frlength_t offset,
	    usb2_frlength_t len);
void	usb2_copy_in(struct usb2_page_cache *cache, usb2_frlength_t offset,
	    const void *ptr, usb2_frlength_t len);
int	usb2_copy_in_user(struct usb2_page_cache *cache, usb2_frlength_t offset,
	    const void *ptr, usb2_frlength_t len);
void	usb2_copy_out(struct usb2_page_cache *cache, usb2_frlength_t offset,
	    void *ptr, usb2_frlength_t len);
int	usb2_copy_out_user(struct usb2_page_cache *cache, usb2_frlength_t offset,
	    void *ptr, usb2_frlength_t len);
void	usb2_dma_tag_setup(struct usb2_dma_parent_tag *udpt,
	    struct usb2_dma_tag *udt, bus_dma_tag_t dmat, struct mtx *mtx,
	    usb2_dma_callback_t *func, uint8_t ndmabits, uint8_t nudt);
void	usb2_dma_tag_unsetup(struct usb2_dma_parent_tag *udpt);
void	usb2_get_page(struct usb2_page_cache *pc, usb2_frlength_t offset,
	    struct usb2_page_search *res);
void	usb2_m_copy_in(struct usb2_page_cache *cache, usb2_frlength_t dst_offset,
	    struct mbuf *m, usb2_size_t src_offset, usb2_frlength_t src_len);
void	usb2_pc_cpu_flush(struct usb2_page_cache *pc);
void	usb2_pc_cpu_invalidate(struct usb2_page_cache *pc);
void	usb2_pc_dmamap_destroy(struct usb2_page_cache *pc);
void	usb2_pc_free_mem(struct usb2_page_cache *pc);

#endif					/* _USB2_BUSDMA_H_ */

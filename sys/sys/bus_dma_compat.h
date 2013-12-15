#ifndef _SYS_BUS_DMA_COMPAT_H_
#define	_SYS_BUS_DMA_COMPAT_H_

#include <sys/errno.h>
#include <sys/systm.h>

#include <machine/bus.h>

typedef struct bus_dma_segment {
	TAILQ_ENTRY(bus_dma_segment) ds_link;
	u_int		ds_idx;
	bus_addr_t	ds_addr;	/* bus address */
	vm_paddr_t	ds_physaddr;
	vm_offset_t	ds_virtaddr;
	vm_size_t	ds_len;
} bus_dma_segment_t;

typedef void bus_dmamap_callback_t(void *, bus_dma_segment_t *, int, int);
typedef void bus_dmamap_callback2_t(void *, bus_dma_segment_t *, int,
    bus_size_t, int);

#define	bus_dmamap_t		busdma_md_t

#ifdef CTASSERT
CTASSERT(__alignof(device_t) > 1);
CTASSERT(__alignof(bus_dma_tag_t) > 1);
#endif

static bus_dma_tag_t __inline
bus_get_dma_tag(device_t dev)
{
	bus_dma_tag_t tag;
	uintptr_t token;

	token = (uintptr_t)(void *)dev;
	token |= 1;
	tag = (bus_dma_tag_t)(void *)token;
	return (tag);
}

static int __inline
bus_dma_tag_create(bus_dma_tag_t dt, bus_size_t align, bus_addr_t bndry,
    bus_addr_t lowaddr, bus_addr_t highaddr, bus_dma_filter_t *filtfunc,
    void *filtfuncarg, bus_size_t maxsz, int nsegs, bus_size_t maxsegsz,
    int flags, bus_dma_lock_t *lockfunc, void *lockfuncarg,
    bus_dma_tag_t *tag_p)
{
	busdma_tag_t tag;
	device_t dev;
	uintptr_t token;
	int error;

	KASSERT(highaddr == BUS_SPACE_MAXADDR,
	    ("%s: bad highaddr %lx", __func__, (u_long)highaddr));

	token = (uintptr_t)(void *)dt;
	if (token & 1UL) {
		dev = (device_t)(void *)(token - 1UL);
		error = busdma_tag_create(dev, align, bndry, lowaddr, maxsz,
		    nsegs, maxsegsz, 0, flags, &tag);
	} else {
		tag = (busdma_tag_t)(void *)dt;
		error = busdma_tag_derive(tag, align, bndry, lowaddr, maxsz,
		    nsegs, maxsegsz, 0, flags, &tag);
	}
	*tag_p = (bus_dma_tag_t)(void *)tag;

	/* XXX lockfunc, lockfuncarg */
	/* XXX filtfunc, filtfuncarg */
	return (error);
}

static int __inline
bus_dma_tag_destroy(bus_dma_tag_t tag)
{
	uintptr_t token;

	token = (uintptr_t)(void *)tag;
	KASSERT((token & 1) == 0, ("%s: bad tag", __func__));
	return (ENOSYS);
}

static int __inline
bus_dmamap_create(bus_dma_tag_t tag, int flags, bus_dmamap_t *mapp)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t map)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback, void *callback_arg,
    int flags)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load_bio(bus_dma_tag_t tag, bus_dmamap_t map, struct bio *bio,
    bus_dmamap_callback_t *callback, void *callback_arg, int flags)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load_ccb(bus_dma_tag_t tag, bus_dmamap_t map, union ccb *ccb,
    bus_dmamap_callback_t *callback, void *callback_arg, int flags)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load_mbuf(bus_dma_tag_t tag, bus_dmamap_t map, struct mbuf *mbuf,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load_mbuf_sg(bus_dma_tag_t tag, bus_dmamap_t map, struct mbuf *mbuf,
    bus_dma_segment_t *segs, int *nsegs, int flags)
{

	return (ENOSYS);
}

static int __inline
bus_dmamap_load_uio(bus_dma_tag_t tag, bus_dmamap_t map, struct uio *ui,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{

	return (ENOSYS);
}

static void __inline
bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t map, bus_dmasync_op_t op)
{
	u_int nop;
	int error;

	nop = 0;
	if (op & BUS_DMASYNC_PREREAD)
		nop |= BUSDMA_SYNC_PREREAD;
	if (op & BUS_DMASYNC_POSTREAD)
		nop |= BUSDMA_SYNC_POSTREAD;
	if (op & BUS_DMASYNC_PREWRITE)
		nop |= BUSDMA_SYNC_PREWRITE;
	if (op & BUS_DMASYNC_POSTWRITE)
		nop |= BUSDMA_SYNC_POSTWRITE;
	KASSERT((nop & (BUSDMA_SYNC_BEFORE | BUSDMA_SYNC_AFTER)) !=
	    (BUSDMA_SYNC_BEFORE | BUSDMA_SYNC_AFTER),
	    ("%s: bad op", __func__));
	error = busdma_sync_range(map, op, 0UL, ~0UL);
	KASSERT(error == 0, ("%s: got error", __func__));
}

static void __inline
bus_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t map)
{
}

static int __inline
bus_dmamem_alloc(bus_dma_tag_t tag, void **vaddr, int flags,
    bus_dmamap_t *mapp)
{

	return (ENOSYS);
}

static void __inline
bus_dmamem_free(bus_dma_tag_t tag, void *vaddr, bus_dmamap_t map)
{
}

#endif /* _SYS_BUS_DMA_COMPAT_H_ */

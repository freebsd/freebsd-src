/*
 * iobuf.c
 *
 * Keep track of the general-purpose IO-buffer structures used to track
 * abstract kernel-space io buffers.
 * 
 */

#include <linux/iobuf.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>


static kmem_cache_t *kiobuf_cachep;

void end_kio_request(struct kiobuf *kiobuf, int uptodate)
{
	if ((!uptodate) && !kiobuf->errno)
		kiobuf->errno = -EIO;

	if (atomic_dec_and_test(&kiobuf->io_count)) {
		if (kiobuf->end_io)
			kiobuf->end_io(kiobuf);
		wake_up(&kiobuf->wait_queue);
	}
}

static int kiobuf_init(struct kiobuf *iobuf)
{
	init_waitqueue_head(&iobuf->wait_queue);
	iobuf->array_len = 0;
	iobuf->nr_pages = 0;
	iobuf->locked = 0;
	iobuf->bh = NULL;
	iobuf->blocks = NULL;
	atomic_set(&iobuf->io_count, 0);
	iobuf->end_io = NULL;
	return expand_kiobuf(iobuf, KIO_STATIC_PAGES);
}

int alloc_kiobuf_bhs(struct kiobuf * kiobuf)
{
	int i;

	kiobuf->blocks =
		kmalloc(sizeof(*kiobuf->blocks) * KIO_MAX_SECTORS, GFP_KERNEL);
	if (unlikely(!kiobuf->blocks))
		goto nomem;
	kiobuf->bh =
		kmalloc(sizeof(*kiobuf->bh) * KIO_MAX_SECTORS, GFP_KERNEL);
	if (unlikely(!kiobuf->bh))
		goto nomem;

	for (i = 0; i < KIO_MAX_SECTORS; i++) {
		kiobuf->bh[i] = kmem_cache_alloc(bh_cachep, GFP_KERNEL);
		if (unlikely(!kiobuf->bh[i]))
			goto nomem2;
	}

	return 0;

nomem2:
	while (i--) {
		kmem_cache_free(bh_cachep, kiobuf->bh[i]);
		kiobuf->bh[i] = NULL;
	}
	memset(kiobuf->bh, 0, sizeof(*kiobuf->bh) * KIO_MAX_SECTORS);

nomem:
	free_kiobuf_bhs(kiobuf);
	return -ENOMEM;
}

void free_kiobuf_bhs(struct kiobuf * kiobuf)
{
	int i;

	if (kiobuf->bh) {
		for (i = 0; i < KIO_MAX_SECTORS; i++)
			if (kiobuf->bh[i])
				kmem_cache_free(bh_cachep, kiobuf->bh[i]);
		kfree(kiobuf->bh);
		kiobuf->bh = NULL;
	}

	if (kiobuf->blocks) {
		kfree(kiobuf->blocks);
		kiobuf->blocks = NULL;
	}
}

int alloc_kiovec(int nr, struct kiobuf **bufp)
{
	int i;
	struct kiobuf *iobuf;
	
	for (i = 0; i < nr; i++) {
		iobuf = kmem_cache_alloc(kiobuf_cachep, GFP_KERNEL);
		if (unlikely(!iobuf))
			goto nomem;
		if (unlikely(kiobuf_init(iobuf)))
			goto nomem2;
 		if (unlikely(alloc_kiobuf_bhs(iobuf)))
			goto nomem2;
		bufp[i] = iobuf;
	}
	
	return 0;

nomem2:
	kmem_cache_free(kiobuf_cachep, iobuf);
nomem:
	free_kiovec(i, bufp);
	return -ENOMEM;
}

void free_kiovec(int nr, struct kiobuf **bufp) 
{
	int i;
	struct kiobuf *iobuf;
	
	for (i = 0; i < nr; i++) {
		iobuf = bufp[i];
		if (iobuf->locked)
			unlock_kiovec(1, &iobuf);
		kfree(iobuf->maplist);
		free_kiobuf_bhs(iobuf);
		kmem_cache_free(kiobuf_cachep, bufp[i]);
	}
}

int expand_kiobuf(struct kiobuf *iobuf, int wanted)
{
	struct page ** maplist;
	
	if (iobuf->array_len >= wanted)
		return 0;
	
	maplist = kmalloc(wanted * sizeof(struct page **), GFP_KERNEL);
	if (unlikely(!maplist))
		return -ENOMEM;

	/* Did it grow while we waited? */
	if (unlikely(iobuf->array_len >= wanted)) {
		kfree(maplist);
		return 0;
	}

	if (iobuf->array_len) {
		memcpy(maplist, iobuf->maplist, iobuf->array_len * sizeof(*maplist));
		kfree(iobuf->maplist);
	}
	
	iobuf->maplist   = maplist;
	iobuf->array_len = wanted;
	return 0;
}

void kiobuf_wait_for_io(struct kiobuf *kiobuf)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	if (atomic_read(&kiobuf->io_count) == 0)
		return;

	add_wait_queue(&kiobuf->wait_queue, &wait);
repeat:
	set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	if (atomic_read(&kiobuf->io_count) != 0) {
		run_task_queue(&tq_disk);
		schedule();
		if (atomic_read(&kiobuf->io_count) != 0)
			goto repeat;
	}
	tsk->state = TASK_RUNNING;
	remove_wait_queue(&kiobuf->wait_queue, &wait);
}

void __init iobuf_cache_init(void)
{
	kiobuf_cachep = kmem_cache_create("kiobuf", sizeof(struct kiobuf),
					  0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!kiobuf_cachep)
		panic("Cannot create kiobuf SLAB cache");
}

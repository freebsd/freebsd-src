/*
 * $Id: blkmtd.c,v 1.17 2003/01/24 13:00:24 dwmw2 Exp $
 *
 * blkmtd.c - use a block device as a fake MTD
 *
 * Author: Simon Evans <spse@secret.org.uk>
 *
 * Copyright (C) 2001,2002 Simon Evans
 *
 * Licence: GPL
 *
 * How it works:
 *	The driver uses raw/io to read/write the device and the page
 *	cache to cache access. Writes update the page cache with the
 *	new data and mark it dirty and add the page into a kiobuf.
 *	When the kiobuf becomes full or the next extry is to an earlier
 *	block in the kiobuf then it is flushed to disk. This allows
 *	writes to remained ordered and gives a small and simple outgoing
 *	write cache.
 *
 *	It can be loaded Read-Only to prevent erases and writes to the
 *	medium.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/iobuf.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/list.h>
#include <linux/mtd/mtd.h>

#ifdef CONFIG_MTD_DEBUG
#ifdef CONFIG_PROC_FS
#  include <linux/proc_fs.h>
#  define BLKMTD_PROC_DEBUG
   static struct proc_dir_entry *blkmtd_proc;
#endif
#endif


#define err(format, arg...) printk(KERN_ERR "blkmtd: " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO "blkmtd: " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "blkmtd: " format "\n" , ## arg)
#define crit(format, arg...) printk(KERN_CRIT "blkmtd: " format "\n" , ## arg)


/* Default erase size in KiB, always make it a multiple of PAGE_SIZE */
#define CONFIG_MTD_BLKDEV_ERASESIZE (128 << 10)	/* 128KiB */
#define VERSION "1.10"

/* Info for the block device */
struct blkmtd_dev {
	struct list_head list;
	struct block_device *binding;
	struct mtd_info mtd_info;
	struct kiobuf *rd_buf, *wr_buf;
	long iobuf_locks;
	struct semaphore wrbuf_mutex;
};


/* Static info about the MTD, used in cleanup_module */
static LIST_HEAD(blkmtd_device_list);


static void blkmtd_sync(struct mtd_info *mtd);

#define MAX_DEVICES 4

/* Module parameters passed by insmod/modprobe */
char *device[MAX_DEVICES];    /* the block device to use */
int erasesz[MAX_DEVICES];     /* optional default erase size */
int ro[MAX_DEVICES];          /* optional read only flag */
int sync;


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Evans <spse@secret.org.uk>");
MODULE_DESCRIPTION("Emulate an MTD using a block device");
MODULE_PARM(device, "1-4s");
MODULE_PARM_DESC(device, "block device to use");
MODULE_PARM(erasesz, "1-4i");
MODULE_PARM_DESC(erasesz, "optional erase size to use in KiB. eg 4=4KiB.");
MODULE_PARM(ro, "1-4i");
MODULE_PARM_DESC(ro, "1=Read only, writes and erases cause errors");
MODULE_PARM(sync, "i");
MODULE_PARM_DESC(sync, "1=Synchronous writes");


/**
 * read_pages - read in pages via the page cache
 * @dev: device to read from
 * @pagenrs: list of page numbers wanted
 * @pagelst: storage for struce page * pointers
 * @pages: count of pages wanted
 *
 * Read pages, getting them from the page cache if available
 * else reading them in from disk if not. pagelst must be preallocated
 * to hold the page count.
 */
static int read_pages(struct blkmtd_dev *dev, int pagenrs[], struct page **pagelst, int pages)
{
	kdev_t kdev;
	struct page *page;
	int cnt = 0;
	struct kiobuf *iobuf;
	int err = 0;

	if(!dev) {
		err("read_pages: PANIC dev == NULL");
		return -EIO;
	}
	kdev = to_kdev_t(dev->binding->bd_dev);

	DEBUG(2, "read_pages: reading %d pages\n", pages);
	if(test_and_set_bit(0, &dev->iobuf_locks)) {
		err = alloc_kiovec(1, &iobuf);
		if (err) {
			crit("cant allocate kiobuf");
			return -ENOMEM;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
		iobuf->blocks = kmalloc(KIO_MAX_SECTORS * sizeof(unsigned long), GFP_KERNEL);
		if(iobuf->blocks == NULL) {
			crit("cant allocate iobuf blocks");
			free_kiovec(1, &iobuf);
			return -ENOMEM;
		}
#endif
	} else {
		iobuf = dev->rd_buf;
	}

	iobuf->nr_pages = 0;
	iobuf->length = 0;
	iobuf->offset = 0;
	iobuf->locked = 1;
	
	for(cnt = 0; cnt < pages; cnt++) {
		page = grab_cache_page(dev->binding->bd_inode->i_mapping, pagenrs[cnt]);
		pagelst[cnt] = page;
		if(!PageUptodate(page)) {
				iobuf->blocks[iobuf->nr_pages] = pagenrs[cnt];
				iobuf->maplist[iobuf->nr_pages++] = page;
		}
	}

	if(iobuf->nr_pages) {
		iobuf->length = iobuf->nr_pages << PAGE_SHIFT;
		err = brw_kiovec(READ, 1, &iobuf, kdev, iobuf->blocks, PAGE_SIZE);
		DEBUG(3, "blkmtd: read_pages: finished, err = %d\n", err);
		if(err < 0) {
			while(pages--) {
				ClearPageUptodate(pagelst[pages]);
				unlock_page(pagelst[pages]);
				page_cache_release(pagelst[pages]);
			}
		} else {
			while(iobuf->nr_pages--) {
				SetPageUptodate(iobuf->maplist[iobuf->nr_pages]);
			}
			err = 0;
		}
	}


	if(iobuf != dev->rd_buf) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
		kfree(iobuf->blocks);
#endif
		free_kiovec(1, &iobuf);
	} else {
		clear_bit(0, &dev->iobuf_locks);
	}
	DEBUG(2, "read_pages: done, err = %d\n", err);
	return err;
}


/**
 * commit_pages - commit pages in the writeout kiobuf to disk
 * @dev: device to write to
 *
 * If the current dev has pages in the dev->wr_buf kiobuf,
 * they are written to disk using brw_kiovec()
 */
static int commit_pages(struct blkmtd_dev *dev)
{
	struct kiobuf *iobuf = dev->wr_buf;
	kdev_t kdev = to_kdev_t(dev->binding->bd_dev);
	int err = 0;

	iobuf->length = iobuf->nr_pages << PAGE_SHIFT;
	iobuf->locked = 1;
	if(iobuf->length) {
		int i;
		DEBUG(2, "blkmtd: commit_pages: nrpages = %d\n", iobuf->nr_pages);
		/* Check all the pages are dirty and lock them */
		for(i = 0; i < iobuf->nr_pages; i++) {
			struct page *page = iobuf->maplist[i];
			BUG_ON(!PageDirty(page));
			lock_page(page);
		}
		err = brw_kiovec(WRITE, 1, &iobuf, kdev, iobuf->blocks, PAGE_SIZE);
		DEBUG(3, "commit_write: committed %d pages err = %d\n", iobuf->nr_pages, err);
		while(iobuf->nr_pages) {
			struct page *page = iobuf->maplist[--iobuf->nr_pages];
			ClearPageDirty(page);
			SetPageUptodate(page);
			unlock_page(page);
			page_cache_release(page);
		}
	}

	DEBUG(2, "blkmtd: sync: end, err = %d\n", err);
	iobuf->offset = 0;
	iobuf->nr_pages = 0;
	iobuf->length = 0;
	return err;
}


/**
 * write_pages - write block of data to device via the page cache
 * @dev: device to write to
 * @buf: data source or NULL if erase (output is set to 0xff)
 * @to: offset into output device
 * @len: amount to data to write
 * @retlen: amount of data written
 *
 * Grab pages from the page cache and fill them with the source data.
 * Non page aligned start and end result in a readin of the page and
 * part of the page being modified. Pages are added to the wr_buf kiobuf
 * until this becomes full or the next page written to has a lower pagenr
 * then the current max pagenr in the kiobuf.
 */
static int write_pages(struct blkmtd_dev *dev, const u_char *buf, loff_t to,
		    size_t len, int *retlen)
{
	int pagenr, offset;
	size_t start_len = 0, end_len;
	int pagecnt = 0;
	struct kiobuf *iobuf = dev->wr_buf;
	int err = 0;
	struct page *pagelst[2];
	int pagenrs[2];
	int readpages = 0;
	int ignorepage = -1;

	pagenr = to >> PAGE_SHIFT;
	offset = to & ~PAGE_MASK;

	DEBUG(2, "blkmtd: write_pages: buf = %p to = %ld len = %d pagenr = %d offset = %d\n",
	      buf, (long)to, len, pagenr, offset);

	*retlen = 0;
	/* see if we have to do a partial write at the start */
	if(offset) {
		start_len = ((offset + len) > PAGE_SIZE) ? PAGE_SIZE - offset : len;
		len -= start_len;
	}

	/* calculate the length of the other two regions */
	end_len = len & ~PAGE_MASK;
	len -= end_len;

	if(start_len) {
		pagenrs[0] = pagenr;
		readpages++;
		pagecnt++;
	}
	if(len)
		pagecnt += len >> PAGE_SHIFT;
	if(end_len) {
		pagenrs[readpages] = pagenr + pagecnt;
		readpages++;
		pagecnt++;
	}

	DEBUG(3, "blkmtd: write: start_len = %d len = %d end_len = %d pagecnt = %d\n",
	      start_len, len, end_len, pagecnt);

	down(&dev->wrbuf_mutex);

	if(iobuf->nr_pages && ((pagenr <= iobuf->blocks[iobuf->nr_pages-1])
			       || (iobuf->nr_pages + pagecnt) >= KIO_STATIC_PAGES)) {

		if((pagenr == iobuf->blocks[iobuf->nr_pages-1])
		   && ((iobuf->nr_pages + pagecnt) < KIO_STATIC_PAGES)) {
			iobuf->nr_pages--;
			ignorepage = pagenr;
		} else {
			DEBUG(3, "blkmtd: doing writeout pagenr = %d max_pagenr = %ld pagecnt = %d idx = %d\n",
			      pagenr, iobuf->blocks[iobuf->nr_pages-1],
			      pagecnt, iobuf->nr_pages);
			commit_pages(dev);
		}
	}
	
	if(readpages) {
		err = read_pages(dev, pagenrs, pagelst, readpages);
		if(err < 0)
			goto readin_err;
	}

	if(start_len) {
		/* do partial start region */
		struct page *page;

		DEBUG(3, "blkmtd: write: doing partial start, page = %d len = %d offset = %d\n",
		      pagenr, start_len, offset);
		page = pagelst[0];
		BUG_ON(!buf);
		if(PageDirty(page) && pagenr != ignorepage) {
			err("to = %lld start_len = %d len = %d end_len = %d pagenr = %d ignorepage = %d\n",
			    to, start_len, len, end_len, pagenr, ignorepage);
			BUG();
		}
		memcpy(page_address(page)+offset, buf, start_len);
		SetPageDirty(page);
		SetPageUptodate(page);
		unlock_page(page);
		buf += start_len;
		*retlen = start_len;
		err = 0;
		iobuf->blocks[iobuf->nr_pages] = pagenr++;
		iobuf->maplist[iobuf->nr_pages] = page;
		iobuf->nr_pages++;
	}

	/* Now do the main loop to a page aligned, n page sized output */
	if(len) {
		int pagesc = len >> PAGE_SHIFT;
		DEBUG(3, "blkmtd: write: whole pages start = %d, count = %d\n",
		      pagenr, pagesc);
		while(pagesc) {
			struct page *page;

			/* see if page is in the page cache */
			DEBUG(3, "blkmtd: write: grabbing page %d from page cache\n", pagenr);
			page = grab_cache_page(dev->binding->bd_inode->i_mapping, pagenr);
			if(PageDirty(page) && pagenr != ignorepage) {
				BUG();
			}
			if(!page) {
				warn("write: cant grab cache page %d", pagenr);
				err = -ENOMEM;
				goto write_err;
			}
			if(!buf) {
				memset(page_address(page), 0xff, PAGE_SIZE);
			} else {
				memcpy(page_address(page), buf, PAGE_SIZE);
				buf += PAGE_SIZE;
			}
			iobuf->blocks[iobuf->nr_pages] = pagenr++;
			iobuf->maplist[iobuf->nr_pages] = page;
			iobuf->nr_pages++;
			SetPageDirty(page);
			SetPageUptodate(page);
			unlock_page(page);
			pagesc--;
			*retlen += PAGE_SIZE;
		}
	}

	if(end_len) {
		/* do the third region */
		struct page *page;
		DEBUG(3, "blkmtd: write: doing partial end, page = %d len = %d\n",
		      pagenr, end_len);
		page = pagelst[readpages-1];
		BUG_ON(!buf);
		if(PageDirty(page) && pagenr != ignorepage) {
			err("to = %lld start_len = %d len = %d end_len = %d pagenr = %d ignorepage = %d\n",
			    to, start_len, len, end_len, pagenr, ignorepage);
			BUG();
		}
		memcpy(page_address(page), buf, end_len);
		SetPageDirty(page);
		SetPageUptodate(page);
		unlock_page(page);
		DEBUG(3, "blkmtd: write: writing out partial end\n");
		*retlen += end_len;
		err = 0;
		iobuf->blocks[iobuf->nr_pages] = pagenr;
		iobuf->maplist[iobuf->nr_pages] = page;
		iobuf->nr_pages++;
	}

	DEBUG(2, "blkmtd: write: end, retlen = %d, err = %d\n", *retlen, err);

	if(sync) {
write_err:
		commit_pages(dev);
	}

readin_err:
	up(&dev->wrbuf_mutex);
	return err;
}


/* erase a specified part of the device */
static int blkmtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct blkmtd_dev *dev = mtd->priv;
	struct mtd_erase_region_info *einfo = mtd->eraseregions;
	int numregions = mtd->numeraseregions;
	size_t from;
	u_long len;
	int err = -EIO;
	int retlen;

	/* check readonly */
	if(!dev->wr_buf) {
		err("error: mtd%d trying to erase readonly device %s",
		    mtd->index, mtd->name);
		instr->state = MTD_ERASE_FAILED;
		goto erase_callback;
	}

	instr->state = MTD_ERASING;
	from = instr->addr;
	len = instr->len;

	/* check erase region has valid start and length */
	DEBUG(2, "blkmtd: erase: dev = `%s' from = 0x%x len = 0x%lx\n",
	      bdevname(dev->binding->bd_dev), from, len);
	while(numregions) {
		DEBUG(3, "blkmtd: checking erase region = 0x%08X size = 0x%X num = 0x%x\n",
		      einfo->offset, einfo->erasesize, einfo->numblocks);
		if(from >= einfo->offset
		   && from < einfo->offset + (einfo->erasesize * einfo->numblocks)) {
			if(len == einfo->erasesize
			   && ( (from - einfo->offset) % einfo->erasesize == 0))
				break;
		}
		numregions--;
		einfo++;
	}

	if(!numregions) {
		/* Not a valid erase block */
		err("erase: invalid erase request 0x%lX @ 0x%08X", len, from);
		instr->state = MTD_ERASE_FAILED;
		err = -EIO;
	}

	if(instr->state != MTD_ERASE_FAILED) {
		/* do the erase */
		DEBUG(3, "Doing erase from = %d len = %ld\n", from, len);
		err = write_pages(dev, NULL, from, len, &retlen);
		if(err < 0) {
			err("erase failed err = %d", err);
			instr->state = MTD_ERASE_FAILED;
		} else {
			instr->state = MTD_ERASE_DONE;
			err = 0;
		}
	}

	DEBUG(3, "blkmtd: erase: checking callback\n");
 erase_callback:
	if (instr->callback) {
		(*(instr->callback))(instr);
	}
	DEBUG(2, "blkmtd: erase: finished (err = %d)\n", err);
	return err;
}


/* read a range of the data via the page cache */
static int blkmtd_read(struct mtd_info *mtd, loff_t from, size_t len,
		       size_t *retlen, u_char *buf)
{
	struct blkmtd_dev *dev = mtd->priv;
	int err = 0;
	int offset;
	int pagenr, pages;
	struct page **pagelst;
	int *pagenrs;
	int i;

	*retlen = 0;

	DEBUG(2, "blkmtd: read: dev = `%s' from = %ld len = %d buf = %p\n",
	      bdevname(dev->binding->bd_dev), (long int)from, len, buf);

	pagenr = from >> PAGE_SHIFT;
	offset = from - (pagenr << PAGE_SHIFT);

	pages = (offset+len+PAGE_SIZE-1) >> PAGE_SHIFT;
	DEBUG(3, "blkmtd: read: pagenr = %d offset = %d, pages = %d\n",
	      pagenr, offset, pages);

	pagelst = kmalloc(sizeof(struct page *) * pages, GFP_KERNEL);
	if(!pagelst)
		return -ENOMEM;
	pagenrs = kmalloc(sizeof(int) * pages, GFP_KERNEL);
	if(!pagenrs) {
		kfree(pagelst);
		return -ENOMEM;
	}
	for(i = 0; i < pages; i++)
		pagenrs[i] = pagenr+i;

	err = read_pages(dev, pagenrs, pagelst, pages);
	if(err)
		goto readerr;

	pagenr = 0;
	while(pages) {
		struct page *page;
		int cpylen;

		DEBUG(3, "blkmtd: read: looking for page: %d\n", pagenr);
		page = pagelst[pagenr];

		cpylen = (PAGE_SIZE > len) ? len : PAGE_SIZE;
		if(offset+cpylen > PAGE_SIZE)
			cpylen = PAGE_SIZE-offset;

		memcpy(buf + *retlen, page_address(page) + offset, cpylen);
		offset = 0;
		len -= cpylen;
		*retlen += cpylen;
		pagenr++;
		pages--;
		unlock_page(page);
		if(!PageDirty(page))
			page_cache_release(page);
	}

 readerr:
	kfree(pagelst);
	kfree(pagenrs);
	DEBUG(2, "blkmtd: end read: retlen = %d, err = %d\n", *retlen, err);
	return err;
}


/* write data to the underlying device */
static int blkmtd_write(struct mtd_info *mtd, loff_t to, size_t len,
			size_t *retlen, const u_char *buf)
{
	struct blkmtd_dev *dev = mtd->priv;
	int err;

	*retlen = 0;
	if(!len)
		return 0;

	DEBUG(2, "blkmtd: write: dev = `%s' to = %ld len = %d buf = %p\n",
	      bdevname(dev->binding->bd_dev), (long int)to, len, buf);

	/* handle readonly and out of range numbers */

	if(!dev->wr_buf) {
		err("error: trying to write to a readonly device %s", mtd->name);
		return -EROFS;
	}

	if(to >= mtd->size) {
		return -ENOSPC;
	}

	if(to + len > mtd->size) {
		len = (mtd->size - to);
	}

	err = write_pages(dev, buf, to, len, retlen);
	if(err < 0)
		*retlen = 0;
	else
		err = 0;
	DEBUG(2, "blkmtd: write: end, err = %d\n", err);
	return err;
}


/* sync the device - wait until the write queue is empty */
static void blkmtd_sync(struct mtd_info *mtd)
{
	struct blkmtd_dev *dev = mtd->priv;
	struct kiobuf *iobuf = dev->wr_buf;

	DEBUG(2, "blkmtd: sync: called\n");
	if(iobuf == NULL)
		return;

	DEBUG(3, "blkmtd: kiovec: length = %d nr_pages = %d\n",
	      iobuf->length, iobuf->nr_pages);
	down(&dev->wrbuf_mutex);
	if(iobuf->nr_pages)
		commit_pages(dev);
	up(&dev->wrbuf_mutex);
}


#ifdef BLKMTD_PROC_DEBUG
/* procfs stuff */
static int blkmtd_proc_read(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	int len;
	struct list_head *temp1, *temp2;

	MOD_INC_USE_COUNT;

	/* Count the size of the page lists */

	len = sprintf(page, "dev\twr_idx\tmax_idx\tnrpages\tclean\tdirty\tlocked\tlru\n");
	list_for_each_safe(temp1, temp2, &blkmtd_device_list) {
		struct blkmtd_dev *dev = list_entry(temp1,  struct blkmtd_dev,
						    list);
		struct list_head *temp;
		struct page *pagei;

		int clean = 0, dirty = 0, locked = 0, lru = 0;
		/* Count the size of the page lists */
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->clean_pages) {
			pagei = list_entry(temp, struct page, list);
			clean++;
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->dirty_pages) {
			pagei = list_entry(temp, struct page, list);
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->locked_pages) {
			pagei = list_entry(temp, struct page, list);
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}

		len += sprintf(page+len, "mtd%d:\t%ld\t%d\t%ld\t%d\t%d\t%d\t%d\n",
			       dev->mtd_info.index,
			       (dev->wr_buf && dev->wr_buf->nr_pages) ?
			       dev->wr_buf->blocks[dev->wr_buf->nr_pages-1] : 0,
			       (dev->wr_buf) ? dev->wr_buf->nr_pages : 0,
			       dev->binding->bd_inode->i_mapping->nrpages,
			       clean, dirty, locked, lru);
	}

	if(len <= count)
		*eof = 1;

	MOD_DEC_USE_COUNT;
	return len;
}
#endif


static void free_device(struct blkmtd_dev *dev)
{
	DEBUG(2, "blkmtd: free_device() dev = %p\n", dev);
	if(dev) {
		del_mtd_device(&dev->mtd_info);
		info("mtd%d: [%s] removed", dev->mtd_info.index,
		     dev->mtd_info.name + strlen("blkmtd: "));
		if(dev->mtd_info.eraseregions)
			kfree(dev->mtd_info.eraseregions);
		if(dev->mtd_info.name)
			kfree(dev->mtd_info.name);

		if(dev->rd_buf) {
			dev->rd_buf->locked = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
			if(dev->rd_buf->blocks)
				kfree(dev->rd_buf->blocks);
#endif
			free_kiovec(1, &dev->rd_buf);
		}
		if(dev->wr_buf) {
			dev->wr_buf->locked = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)			
			if(dev->wr_buf->blocks)
				kfree(dev->rw_buf->blocks);
#endif
			free_kiovec(1, &dev->wr_buf);
		}

		if(dev->binding) {
			kdev_t kdev = to_kdev_t(dev->binding->bd_dev);
			invalidate_inode_pages(dev->binding->bd_inode);
			set_blocksize(kdev, 1 << 10);
			blkdev_put(dev->binding, BDEV_RAW);
		}
		kfree(dev);
	}
}


/* For a given size and initial erase size, calculate the number
 * and size of each erase region. Goes round the loop twice,
 * once to find out how many regions, then allocates space,
 * then round the loop again to fill it in.
 */
static struct mtd_erase_region_info *calc_erase_regions(
	size_t erase_size, size_t total_size, int *regions)
{
	struct mtd_erase_region_info *info = NULL;

	DEBUG(2, "calc_erase_regions, es = %d size = %d regions = %d\n",
	      erase_size, total_size, *regions);
	/* Make any user specified erasesize be a power of 2
	   and at least PAGE_SIZE */
	if(erase_size) {
		int es = erase_size;
		erase_size = 1;
		while(es != 1) {
			es >>= 1;
			erase_size <<= 1;
		}
		if(erase_size < PAGE_SIZE)
			erase_size = PAGE_SIZE;
	} else {
		erase_size = CONFIG_MTD_BLKDEV_ERASESIZE;
	}

	*regions = 0;

	do {
		int tot_size = total_size;
		int er_size = erase_size;
		int count = 0, offset = 0, regcnt = 0;

		while(tot_size) {
			count = tot_size / er_size;
			if(count) {
				tot_size = tot_size % er_size;
				if(info) {
					DEBUG(2, "adding to erase info off=%d er=%d cnt=%d\n",
					      offset, er_size, count);
					(info+regcnt)->offset = offset;
					(info+regcnt)->erasesize = er_size;
					(info+regcnt)->numblocks = count;
					(*regions)++;
				}
				regcnt++;
				offset += (count * er_size);
			}
			while(er_size > tot_size)
				er_size >>= 1;
		}
		if(info == NULL) {
			info = kmalloc(regcnt * sizeof(struct mtd_erase_region_info), GFP_KERNEL);
			if(!info)
				break;
		}
	} while(!(*regions));
	DEBUG(2, "calc_erase_regions done, es = %d size = %d regions = %d\n",
	      erase_size, total_size, *regions);
	return info;
}


extern kdev_t name_to_kdev_t(char *line) __init;


static struct blkmtd_dev *add_device(char *devname, int readonly, int erase_size)
{
	int maj, min;
	kdev_t kdev;
	int mode;
	struct blkmtd_dev *dev;

#ifdef MODULE
	struct file *file = NULL;
	struct inode *inode;
#endif

	if(!devname)
		return NULL;

	/* Get a handle on the device */
	mode = (readonly) ? O_RDONLY : O_RDWR;

#ifdef MODULE

	file = filp_open(devname, mode, 0);
	if(IS_ERR(file)) {
		err("error: cant open device %s", devname);
		DEBUG(2, "blkmtd: filp_open returned %ld\n", PTR_ERR(file));
		return NULL;
	}

	/* determine is this is a block device and
	 * if so get its major and minor numbers
	 */
	inode = file->f_dentry->d_inode;
	if(!S_ISBLK(inode->i_mode)) {
		err("%s not a block device", devname);
		filp_close(file, NULL);
		return NULL;
	}
	kdev = inode->i_rdev;
	filp_close(file, NULL);
#else
	kdev = name_to_kdev_t(devname);
#endif	/* MODULE */

	if(!kdev) {
		err("bad block device: `%s'", devname);
		return NULL;
	}

	maj = MAJOR(kdev);
	min = MINOR(kdev);
	DEBUG(1, "blkmtd: found a block device major = %d, minor = %d\n",
	      maj, min);

	if(maj == MTD_BLOCK_MAJOR) {
		err("attempting to use an MTD device as a block device");
		return NULL;
	}

	DEBUG(1, "blkmtd: devname = %s\n", bdevname(kdev));

	dev = kmalloc(sizeof(struct blkmtd_dev), GFP_KERNEL);
	if(dev == NULL)
		return NULL;

	memset(dev, 0, sizeof(struct blkmtd_dev));
	if(alloc_kiovec(1, &dev->rd_buf)) {
		err("cant allocate read iobuf");
		goto devinit_err;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
	dev->rd_buf->blocks = kmalloc(KIO_MAX_SECTORS * sizeof(unsigned long), GFP_KERNEL);
	if(dev->rd_buf->blocks == NULL) {
		crit("cant allocate rd_buf blocks");
		goto devinit_err;
	}
#endif
	
	if(!readonly) {
		if(alloc_kiovec(1, &dev->wr_buf)) {
			err("cant allocate kiobuf - readonly enabled");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
		} else {
			dev->wr_buf->blocks = kmalloc(KIO_MAX_SECTORS * sizeof(unsigned long), GFP_KERNEL);
			if(dev->wr_buf->blocks == NULL) {
				crit("cant allocate wr_buf blocks - readonly enabled");
				free_kiovec(1, &iobuf);
			}
#endif
		}
		if(dev->wr_buf)
			init_MUTEX(&dev->wrbuf_mutex);
	}

	/* get the block device */
	dev->binding = bdget(kdev_t_to_nr(MKDEV(maj, min)));
	if(blkdev_get(dev->binding, mode, 0, BDEV_RAW))
		goto devinit_err;

	if(set_blocksize(kdev, PAGE_SIZE)) {
		err("cant set block size to PAGE_SIZE on %s", bdevname(kdev));
		goto devinit_err;
	}

	dev->mtd_info.size = dev->binding->bd_inode->i_size & PAGE_MASK;

	/* Setup the MTD structure */
	/* make the name contain the block device in */
	dev->mtd_info.name = kmalloc(sizeof("blkmtd: ") + strlen(devname), GFP_KERNEL);
	if(dev->mtd_info.name == NULL)
		goto devinit_err;

	sprintf(dev->mtd_info.name, "blkmtd: %s", devname);
	dev->mtd_info.eraseregions = calc_erase_regions(erase_size, dev->mtd_info.size,
							&dev->mtd_info.numeraseregions);
	if(dev->mtd_info.eraseregions == NULL)
		goto devinit_err;

	dev->mtd_info.erasesize = dev->mtd_info.eraseregions->erasesize;
	DEBUG(1, "blkmtd: init: found %d erase regions\n",
	      dev->mtd_info.numeraseregions);

	if(readonly) {
		dev->mtd_info.type = MTD_ROM;
		dev->mtd_info.flags = MTD_CAP_ROM;
	} else {
		dev->mtd_info.type = MTD_RAM;
		dev->mtd_info.flags = MTD_CAP_RAM;
	}
	dev->mtd_info.erase = blkmtd_erase;
	dev->mtd_info.read = blkmtd_read;
	dev->mtd_info.write = blkmtd_write;
	dev->mtd_info.sync = blkmtd_sync;
	dev->mtd_info.point = 0;
	dev->mtd_info.unpoint = 0;
	dev->mtd_info.priv = dev;
	dev->mtd_info.module = THIS_MODULE;

	list_add(&dev->list, &blkmtd_device_list);
	if (add_mtd_device(&dev->mtd_info)) {
		/* Device didnt get added, so free the entry */
		list_del(&dev->list);
		free_device(dev);
		return NULL;
	} else {
		info("mtd%d: [%s] erase_size = %dKiB %s",
		     dev->mtd_info.index, dev->mtd_info.name + strlen("blkmtd: "),
		     dev->mtd_info.erasesize >> 10,
		     (dev->wr_buf) ? "" : "(read-only)");
	}
	
	return dev;

 devinit_err:
	free_device(dev);
	return NULL;
}


/* Cleanup and exit - sync the device and kill of the kernel thread */
static void __devexit cleanup_blkmtd(void)
{
	struct list_head *temp1, *temp2;
#ifdef BLKMTD_PROC_DEBUG
	if(blkmtd_proc) {
		remove_proc_entry("blkmtd_debug", NULL);
	}
#endif

	/* Remove the MTD devices */
	list_for_each_safe(temp1, temp2, &blkmtd_device_list) {
		struct blkmtd_dev *dev = list_entry(temp1, struct blkmtd_dev,
						    list);
		blkmtd_sync(&dev->mtd_info);
		free_device(dev);
	}
}

#ifndef MODULE

/* Handle kernel boot params */


static int __init param_blkmtd_device(char *str)
{
	int i;

	for(i = 0; i < MAX_DEVICES; i++) {
		device[i] = str;
		DEBUG(2, "blkmtd: device setup: %d = %s\n", i, device[i]);
		strsep(&str, ",");
	}
	return 1;
}


static int __init param_blkmtd_erasesz(char *str)
{
	int i;
	for(i = 0; i < MAX_DEVICES; i++) {
		char *val = strsep(&str, ",");
		if(val)
			erasesz[i] = simple_strtoul(val, NULL, 0);
		DEBUG(2, "blkmtd: erasesz setup: %d = %d\n", i, erasesz[i]);
	}

	return 1;
}


static int __init param_blkmtd_ro(char *str)
{
	int i;
	for(i = 0; i < MAX_DEVICES; i++) {
		char *val = strsep(&str, ",");
		if(val)
			ro[i] = simple_strtoul(val, NULL, 0);
		DEBUG(2, "blkmtd: ro setup: %d = %d\n", i, ro[i]);
	}

	return 1;
}


static int __init param_blkmtd_sync(char *str)
{
	if(str[0] == '1')
		sync = 1;
	return 1;
}

__setup("blkmtd_device=", param_blkmtd_device);
__setup("blkmtd_erasesz=", param_blkmtd_erasesz);
__setup("blkmtd_ro=", param_blkmtd_ro);
__setup("blkmtd_sync=", param_blkmtd_sync);

#endif


/* Startup */
static int __init init_blkmtd(void)
{
	int i;

	/* Check args - device[0] is the bare minimum*/
	if(!device[0]) {
		err("error: missing `device' name\n");
		return -EINVAL;
	}

	for(i = 0; i < MAX_DEVICES; i++)
		add_device(device[i], ro[i], erasesz[i] << 10);

	if(list_empty(&blkmtd_device_list))
		goto init_err;

	info("version " VERSION);

#ifdef BLKMTD_PROC_DEBUG
	/* create proc entry */
	DEBUG(2, "Creating /proc/blkmtd_debug\n");
	blkmtd_proc = create_proc_read_entry("blkmtd_debug", 0444,
					     NULL, blkmtd_proc_read, NULL);
	if(blkmtd_proc == NULL) {
		err("Cant create /proc/blkmtd_debug");
	} else {
		blkmtd_proc->owner = THIS_MODULE;
	}
#endif

	if(!list_empty(&blkmtd_device_list))
		/* Everything is ok if we got here */
		return 0;

 init_err:
	return -EINVAL;
}

module_init(init_blkmtd);
module_exit(cleanup_blkmtd);

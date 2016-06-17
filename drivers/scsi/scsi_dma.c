/*
 *  scsi_dma.c Copyright (C) 2000 Eric Youngdale
 *
 *  mid-level SCSI DMA bounce buffer allocator
 *
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/blk.h>


#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

/*
 * PAGE_SIZE must be a multiple of the sector size (512).  True
 * for all reasonably recent architectures (even the VAX...).
 */
#define SECTOR_SIZE		512
#define SECTORS_PER_PAGE	(PAGE_SIZE/SECTOR_SIZE)

#if SECTORS_PER_PAGE <= 8
typedef unsigned char FreeSectorBitmap;
#elif SECTORS_PER_PAGE <= 32
typedef unsigned int FreeSectorBitmap;
#else
#error You lose.
#endif

/*
 * Used for access to internal allocator used for DMA safe buffers.
 */
static spinlock_t allocator_request_lock = SPIN_LOCK_UNLOCKED;

static FreeSectorBitmap *dma_malloc_freelist = NULL;
static int need_isa_bounce_buffers;
static unsigned int dma_sectors = 0;
unsigned int scsi_dma_free_sectors = 0;
unsigned int scsi_need_isa_buffer = 0;
static unsigned char **dma_malloc_pages = NULL;

/*
 * Function:    scsi_malloc
 *
 * Purpose:     Allocate memory from the DMA-safe pool.
 *
 * Arguments:   len       - amount of memory we need.
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Pointer to memory block.
 *
 * Notes:       Prior to the new queue code, this function was not SMP-safe.
 *              This function can only allocate in units of sectors
 *              (i.e. 512 bytes).
 *
 *              We cannot use the normal system allocator becuase we need
 *              to be able to guarantee that we can process a complete disk
 *              I/O request without touching the system allocator.  Think
 *              about it - if the system were heavily swapping, and tried to
 *              write out a block of memory to disk, and the SCSI code needed
 *              to allocate more memory in order to be able to write the
 *              data to disk, you would wedge the system.
 */
void *scsi_malloc(unsigned int len)
{
	unsigned int nbits, mask;
	unsigned long flags;

	int i, j;
	if (len % SECTOR_SIZE != 0 || len > PAGE_SIZE)
		return NULL;

	nbits = len >> 9;
	mask = (1 << nbits) - 1;

	spin_lock_irqsave(&allocator_request_lock, flags);

	for (i = 0; i < dma_sectors / SECTORS_PER_PAGE; i++)
		for (j = 0; j <= SECTORS_PER_PAGE - nbits; j++) {
			if ((dma_malloc_freelist[i] & (mask << j)) == 0) {
				dma_malloc_freelist[i] |= (mask << j);
				scsi_dma_free_sectors -= nbits;
#ifdef DEBUG
				SCSI_LOG_MLQUEUE(3, printk("SMalloc: %d %p [From:%p]\n", len, dma_malloc_pages[i] + (j << 9)));
				printk("SMalloc: %d %p [From:%p]\n", len, dma_malloc_pages[i] + (j << 9));
#endif
				spin_unlock_irqrestore(&allocator_request_lock, flags);
				return (void *) ((unsigned long) dma_malloc_pages[i] + (j << 9));
			}
		}
	spin_unlock_irqrestore(&allocator_request_lock, flags);
	return NULL;		/* Nope.  No more */
}

/*
 * Function:    scsi_free
 *
 * Purpose:     Free memory into the DMA-safe pool.
 *
 * Arguments:   ptr       - data block we are freeing.
 *              len       - size of block we are freeing.
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Nothing
 *
 * Notes:       This function *must* only be used to free memory
 *              allocated from scsi_malloc().
 *
 *              Prior to the new queue code, this function was not SMP-safe.
 *              This function can only allocate in units of sectors
 *              (i.e. 512 bytes).
 */
int scsi_free(void *obj, unsigned int len)
{
	unsigned int page, sector, nbits, mask;
	unsigned long flags;

#ifdef DEBUG
	unsigned long ret = 0;

#ifdef __mips__
	__asm__ __volatile__("move\t%0,$31":"=r"(ret));
#else
	ret = __builtin_return_address(0);
#endif
	printk("scsi_free %p %d\n", obj, len);
	SCSI_LOG_MLQUEUE(3, printk("SFree: %p %d\n", obj, len));
#endif

	spin_lock_irqsave(&allocator_request_lock, flags);

	for (page = 0; page < dma_sectors / SECTORS_PER_PAGE; page++) {
		unsigned long page_addr = (unsigned long) dma_malloc_pages[page];
		if ((unsigned long) obj >= page_addr &&
		    (unsigned long) obj < page_addr + PAGE_SIZE) {
			sector = (((unsigned long) obj) - page_addr) >> 9;

			nbits = len >> 9;
			mask = (1 << nbits) - 1;

			if (sector + nbits > SECTORS_PER_PAGE)
				panic("scsi_free:Bad memory alignment");

			if ((dma_malloc_freelist[page] &
			     (mask << sector)) != (mask << sector)) {
#ifdef DEBUG
				printk("scsi_free(obj=%p, len=%d) called from %08lx\n",
				       obj, len, ret);
#endif
				panic("scsi_free:Trying to free unused memory");
			}
			scsi_dma_free_sectors += nbits;
			dma_malloc_freelist[page] &= ~(mask << sector);
			spin_unlock_irqrestore(&allocator_request_lock, flags);
			return 0;
		}
	}
	panic("scsi_free:Bad offset");
}


/*
 * Function:    scsi_resize_dma_pool
 *
 * Purpose:     Ensure that the DMA pool is sufficiently large to be
 *              able to guarantee that we can always process I/O requests
 *              without calling the system allocator.
 *
 * Arguments:   None.
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Nothing
 *
 * Notes:       Prior to the new queue code, this function was not SMP-safe.
 *              Go through the device list and recompute the most appropriate
 *              size for the dma pool.  Then grab more memory (as required).
 */
void scsi_resize_dma_pool(void)
{
	int i, k;
	unsigned long size;
	unsigned long flags;
	struct Scsi_Host *shpnt;
	struct Scsi_Host *host = NULL;
	Scsi_Device *SDpnt;
	FreeSectorBitmap *new_dma_malloc_freelist = NULL;
	unsigned int new_dma_sectors = 0;
	unsigned int new_need_isa_buffer = 0;
	unsigned char **new_dma_malloc_pages = NULL;
	int out_of_space = 0;

	spin_lock_irqsave(&allocator_request_lock, flags);

	if (!scsi_hostlist) {
		/*
		 * Free up the DMA pool.
		 */
		if (scsi_dma_free_sectors != dma_sectors)
			panic("SCSI DMA pool memory leak %d %d\n", scsi_dma_free_sectors, dma_sectors);

		for (i = 0; i < dma_sectors / SECTORS_PER_PAGE; i++)
			free_pages((unsigned long) dma_malloc_pages[i], 0);
		if (dma_malloc_pages)
			kfree((char *) dma_malloc_pages);
		dma_malloc_pages = NULL;
		if (dma_malloc_freelist)
			kfree((char *) dma_malloc_freelist);
		dma_malloc_freelist = NULL;
		dma_sectors = 0;
		scsi_dma_free_sectors = 0;
		spin_unlock_irqrestore(&allocator_request_lock, flags);
		return;
	}
	/* Next, check to see if we need to extend the DMA buffer pool */

	new_dma_sectors = 2 * SECTORS_PER_PAGE;		/* Base value we use */

	if (__pa(high_memory) - 1 > ISA_DMA_THRESHOLD)
		need_isa_bounce_buffers = 1;
	else
		need_isa_bounce_buffers = 0;

	if (scsi_devicelist)
		for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next)
			new_dma_sectors += SECTORS_PER_PAGE;	/* Increment for each host */

	for (host = scsi_hostlist; host; host = host->next) {
		for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
			/*
			 * sd and sr drivers allocate scatterlists.
			 * sr drivers may allocate for each command 1x2048 or 2x1024 extra
			 * buffers for 2k sector size and 1k fs.
			 * sg driver allocates buffers < 4k.
			 * st driver does not need buffers from the dma pool.
			 * estimate 4k buffer/command for devices of unknown type (should panic).
			 */
			if (SDpnt->type == TYPE_WORM || SDpnt->type == TYPE_ROM ||
			    SDpnt->type == TYPE_DISK || SDpnt->type == TYPE_MOD) {
				int nents = host->sg_tablesize;
#ifdef DMA_CHUNK_SIZE
				/* If the architecture does DMA sg merging, make sure
				   we count with at least 64 entries even for HBAs
				   which handle very few sg entries.  */
				if (nents < 64) nents = 64;
#endif
				new_dma_sectors += ((nents *
				sizeof(struct scatterlist) + 511) >> 9) *
				 SDpnt->queue_depth;
				if (SDpnt->type == TYPE_WORM || SDpnt->type == TYPE_ROM)
					new_dma_sectors += (2048 >> 9) * SDpnt->queue_depth;
			} else if (SDpnt->type == TYPE_SCANNER ||
				   SDpnt->type == TYPE_PRINTER ||
				   SDpnt->type == TYPE_PROCESSOR ||
				   SDpnt->type == TYPE_COMM ||
				   SDpnt->type == TYPE_MEDIUM_CHANGER ||
				   SDpnt->type == TYPE_ENCLOSURE) {
				new_dma_sectors += (4096 >> 9) * SDpnt->queue_depth;
			} else {
				if (SDpnt->type != TYPE_TAPE) {
					printk("resize_dma_pool: unknown device type %d\n", SDpnt->type);
					new_dma_sectors += (4096 >> 9) * SDpnt->queue_depth;
				}
			}

			if (host->unchecked_isa_dma &&
			    need_isa_bounce_buffers &&
			    SDpnt->type != TYPE_TAPE) {
				new_dma_sectors += (PAGE_SIZE >> 9) * host->sg_tablesize *
				    SDpnt->queue_depth;
				new_need_isa_buffer++;
			}
		}
	}

#ifdef DEBUG_INIT
	printk("resize_dma_pool: needed dma sectors = %d\n", new_dma_sectors);
#endif

	/* limit DMA memory to 32MB: */
	new_dma_sectors = (new_dma_sectors + 15) & 0xfff0;

	/*
	 * We never shrink the buffers - this leads to
	 * race conditions that I would rather not even think
	 * about right now.
	 */
#if 0				/* Why do this? No gain and risks out_of_space */
	if (new_dma_sectors < dma_sectors)
		new_dma_sectors = dma_sectors;
#endif
	if (new_dma_sectors <= dma_sectors) {
		spin_unlock_irqrestore(&allocator_request_lock, flags);
		return;		/* best to quit while we are in front */
        }

	for (k = 0; k < 20; ++k) {	/* just in case */
		out_of_space = 0;
		size = (new_dma_sectors / SECTORS_PER_PAGE) *
		    sizeof(FreeSectorBitmap);
		new_dma_malloc_freelist = (FreeSectorBitmap *)
		    kmalloc(size, GFP_ATOMIC);
		if (new_dma_malloc_freelist) {
                        memset(new_dma_malloc_freelist, 0, size);
			size = (new_dma_sectors / SECTORS_PER_PAGE) *
			    sizeof(*new_dma_malloc_pages);
			new_dma_malloc_pages = (unsigned char **)
			    kmalloc(size, GFP_ATOMIC);
			if (!new_dma_malloc_pages) {
				size = (new_dma_sectors / SECTORS_PER_PAGE) *
				    sizeof(FreeSectorBitmap);
				kfree((char *) new_dma_malloc_freelist);
				out_of_space = 1;
			} else {
                                memset(new_dma_malloc_pages, 0, size);
                        }
		} else
			out_of_space = 1;

		if ((!out_of_space) && (new_dma_sectors > dma_sectors)) {
			for (i = dma_sectors / SECTORS_PER_PAGE;
			   i < new_dma_sectors / SECTORS_PER_PAGE; i++) {
				new_dma_malloc_pages[i] = (unsigned char *)
				    __get_free_pages(GFP_ATOMIC | GFP_DMA, 0);
				if (!new_dma_malloc_pages[i])
					break;
			}
			if (i != new_dma_sectors / SECTORS_PER_PAGE) {	/* clean up */
				int k = i;

				out_of_space = 1;
				for (i = 0; i < k; ++i)
					free_pages((unsigned long) new_dma_malloc_pages[i], 0);
			}
		}
		if (out_of_space) {	/* try scaling down new_dma_sectors request */
			printk("scsi::resize_dma_pool: WARNING, dma_sectors=%u, "
			       "wanted=%u, scaling\n", dma_sectors, new_dma_sectors);
			if (new_dma_sectors < (8 * SECTORS_PER_PAGE))
				break;	/* pretty well hopeless ... */
			new_dma_sectors = (new_dma_sectors * 3) / 4;
			new_dma_sectors = (new_dma_sectors + 15) & 0xfff0;
			if (new_dma_sectors <= dma_sectors)
				break;	/* stick with what we have got */
		} else
			break;	/* found space ... */
	}			/* end of for loop */
	if (out_of_space) {
		spin_unlock_irqrestore(&allocator_request_lock, flags);
		scsi_need_isa_buffer = new_need_isa_buffer;	/* some useful info */
		printk("      WARNING, not enough memory, pool not expanded\n");
		return;
	}
	/* When we dick with the actual DMA list, we need to
	 * protect things
	 */
	if (dma_malloc_freelist) {
		size = (dma_sectors / SECTORS_PER_PAGE) * sizeof(FreeSectorBitmap);
		memcpy(new_dma_malloc_freelist, dma_malloc_freelist, size);
		kfree((char *) dma_malloc_freelist);
	}
	dma_malloc_freelist = new_dma_malloc_freelist;

	if (dma_malloc_pages) {
		size = (dma_sectors / SECTORS_PER_PAGE) * sizeof(*dma_malloc_pages);
		memcpy(new_dma_malloc_pages, dma_malloc_pages, size);
		kfree((char *) dma_malloc_pages);
	}
	scsi_dma_free_sectors += new_dma_sectors - dma_sectors;
	dma_malloc_pages = new_dma_malloc_pages;
	dma_sectors = new_dma_sectors;
	scsi_need_isa_buffer = new_need_isa_buffer;

	spin_unlock_irqrestore(&allocator_request_lock, flags);

#ifdef DEBUG_INIT
	printk("resize_dma_pool: dma free sectors   = %d\n", scsi_dma_free_sectors);
	printk("resize_dma_pool: dma sectors        = %d\n", dma_sectors);
	printk("resize_dma_pool: need isa buffers   = %d\n", scsi_need_isa_buffer);
#endif
}

/*
 * Function:    scsi_init_minimal_dma_pool
 *
 * Purpose:     Allocate a minimal (1-page) DMA pool.
 *
 * Arguments:   None.
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Nothing
 *
 * Notes:       
 */
int scsi_init_minimal_dma_pool(void)
{
	unsigned long size;
	unsigned long flags;
	int has_space = 0;

	spin_lock_irqsave(&allocator_request_lock, flags);

	dma_sectors = PAGE_SIZE / SECTOR_SIZE;
	scsi_dma_free_sectors = dma_sectors;
	/*
	 * Set up a minimal DMA buffer list - this will be used during scan_scsis
	 * in some cases.
	 */

	/* One bit per sector to indicate free/busy */
	size = (dma_sectors / SECTORS_PER_PAGE) * sizeof(FreeSectorBitmap);
	dma_malloc_freelist = (FreeSectorBitmap *)
	    kmalloc(size, GFP_ATOMIC);
	if (dma_malloc_freelist) {
                memset(dma_malloc_freelist, 0, size);
		/* One pointer per page for the page list */
		dma_malloc_pages = (unsigned char **) kmalloc(
                        (dma_sectors / SECTORS_PER_PAGE) * sizeof(*dma_malloc_pages),
							     GFP_ATOMIC);
		if (dma_malloc_pages) {
                        memset(dma_malloc_pages, 0, size);
			dma_malloc_pages[0] = (unsigned char *)
			    __get_free_pages(GFP_ATOMIC | GFP_DMA, 0);
			if (dma_malloc_pages[0])
				has_space = 1;
		}
	}
	if (!has_space) {
		if (dma_malloc_freelist) {
			kfree((char *) dma_malloc_freelist);
			if (dma_malloc_pages)
				kfree((char *) dma_malloc_pages);
		}
		spin_unlock_irqrestore(&allocator_request_lock, flags);
		printk("scsi::init_module: failed, out of memory\n");
		return 1;
	}

	spin_unlock_irqrestore(&allocator_request_lock, flags);
	return 0;
}

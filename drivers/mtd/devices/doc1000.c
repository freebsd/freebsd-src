/*======================================================================

  $Id: doc1000.c,v 1.17 2003/01/24 13:33:20 dwmw2 Exp $

======================================================================*/


#include <linux/config.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/iflash.h>

/* Parameters that can be set with 'insmod' */

static u_long base              = 0xe0000;
static int erase_timeout	= 10*HZ;	/* in ticks */
static int retry_limit		= 4;		/* write retries */
static u_long max_tries       	= 4096;		/* status polling */

MODULE_PARM(base,"l");
MODULE_PARM(erase_timeout, "i");
MODULE_PARM(retry_limit, "i");
MODULE_PARM(max_tries, "i");

#define WINDOW_SIZE 0x2000
#define WINDOW_MASK (WINDOW_SIZE - 1)
#define PAGEREG_LO (WINDOW_SIZE)
#define PAGEREG_HI (WINDOW_SIZE + 2)

static struct mtd_info *mymtd;
static struct timer_list flashcard_timer;

#define MAX_CELLS		32
#define MAX_FLASH_DEVICES       8

/* A flash region is composed of one or more "cells", where we allow
   simultaneous erases if they are in different cells */



struct mypriv {
	u_char *baseaddr;
	u_short curpage;
	u_char locked;
	u_short numdevices;
	u_char interleave;
	struct erase_info *cur_erases;
	wait_queue_head_t wq;
	u_char devstat[MAX_FLASH_DEVICES];
	u_long devshift;
};


static void flashcard_periodic(u_long data);
static int flashcard_erase (struct mtd_info *mtd, struct erase_info *instr);
static int flashcard_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf);
static int flashcard_write (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf);
static void flashcard_sync (struct mtd_info *mtd);

static inline void resume_erase(volatile u_char *addr);
static inline int suspend_erase(volatile u_char *addr);
static inline int byte_write (volatile u_char *addr, u_char byte);
static inline int word_write (volatile u_char *addr, __u16 word);
static inline int check_write(volatile u_char *addr);
static inline void block_erase (volatile u_char *addr);
static inline int check_erase(volatile u_char *addr);

#ifdef CONFIG_SMP
#warning This is definitely not SMP safe. Lock the paging mechanism.
#endif

static u_char *pagein(struct mtd_info *mtd, u_long addr)
{
  struct mypriv *priv=mtd->priv;
  u_short page = addr >> 13;

  priv->baseaddr[PAGEREG_LO] = page & 0xff;
  priv->baseaddr[PAGEREG_HI] = page >> 8;
  priv->curpage = page;
  
  return &priv->baseaddr[addr & WINDOW_MASK];
}


void flashcard_sync (struct mtd_info *mtd)
{
	struct mypriv *priv=mtd->priv;

	flashcard_periodic((u_long) mtd);
	printk("sync...");
	if (priv->cur_erases)
		interruptible_sleep_on(&priv->wq);
	printk("Done.\n");
}

int flashcard_erase (struct mtd_info *mtd, struct erase_info *instr)
{
	u_char *pageaddr;
	struct mypriv *priv=mtd->priv;
	struct erase_info **tmp=&priv->cur_erases;
	
	if (instr->len != mtd->erasesize)
		return -EINVAL;
	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

	pageaddr=pagein(mtd,instr->addr);
	instr->mtd = mtd;
	instr->dev = instr->addr >> priv->devshift;
	instr->cell = (instr->addr - (instr->dev << priv->devshift)) / mtd->erasesize;
	instr->next = NULL;
	instr->state = MTD_ERASE_PENDING;
	
	while (*tmp)
	{
		tmp = &((*tmp) -> next);
	}
	
	*tmp = instr;
	flashcard_periodic((u_long)mtd);
	return 0;
}


int flashcard_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
 	u_char *pageaddr=pagein(mtd,from);
	struct mypriv *priv=mtd->priv;
	u_char device = from >> priv->devshift;
	u_char cell = (int) (from - (device << priv->devshift)) / mtd->erasesize;
	int ret = 0, timeron = 0;

	if ((from & WINDOW_MASK) + len <= WINDOW_SIZE)
		*retlen = len;
	else
		*retlen = WINDOW_SIZE - (from & WINDOW_MASK);

	if (priv->devstat[device])
	{
		
		/* There is an erase in progress or pending for this device. Stop it */
		timeron = del_timer(&flashcard_timer);
		
		if (priv->cur_erases && priv->cur_erases->cell == cell) 
			
		{
			/* The erase is on the current cell. Just return all 0xff */ 
			add_timer(&flashcard_timer);
			
			
			printk("Cell %d currently erasing. Setting to all 0xff\n",cell);
			memset(buf, 0xff, *retlen);
			return 0;
		}
		if (priv->devstat[device] == MTD_ERASING)
		{
			ret = suspend_erase(pageaddr);
			priv->devstat[device] = MTD_ERASE_SUSPEND;
		       
			if (ret) 
			{
				printk("flashcard: failed to suspend erase\n");
				add_timer (&flashcard_timer);
				return ret;
			}
		}

	}

	writew(IF_READ_ARRAY, (u_long)pageaddr & ~1);
	
	ret = 0;
	memcpy (buf, pageaddr, *retlen);
	
	writew(IF_READ_CSR, (u_long)pageaddr & ~1);
	
	
	if (priv->devstat[device] & MTD_ERASE_SUSPEND)
	{
		resume_erase(pageaddr);
		priv->devstat[device]=MTD_ERASING;
	}


	if (timeron) add_timer (&flashcard_timer);
		
	return ret;
}


int flashcard_write (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct mypriv *priv = (struct mypriv *)mtd->priv;
 	u_char *endaddr, *startaddr;
	register u_char *pageaddr;
	u_char device = to >> priv->devshift;
/*	jiffies_t oldj=jiffies;*/
	int ret;

	while (priv->devstat[device])
	{
		flashcard_sync(mtd);
	}

	if ((to & WINDOW_MASK) + len <= WINDOW_SIZE)
		*retlen = len;
	else
		*retlen = WINDOW_SIZE - (to & WINDOW_MASK);
	
	pageaddr = pagein(mtd, to);
	startaddr = (u_char *)((u_long) pageaddr & ~1);
	endaddr = pageaddr+(*retlen);



	/* Set up to read */
	writew(IF_READ_CSR, startaddr);
	
	/* Make sure it's aligned by reading the first byte if necessary */
	if (to & 1)
	{
		/* Unaligned access */

		u_char cbuf;

		cbuf = *buf;

		if (!((u_long)pageaddr & 0xf))
			schedule();
			
		ret = byte_write(pageaddr, cbuf);
		if (ret) return ret;

		pageaddr++; buf++;
	}


	for ( ; pageaddr + 1 < endaddr; buf += 2, pageaddr += 2)
		{
			/* if ((u_long)pageaddr & 0xf) schedule();*/
			
			ret = word_write(pageaddr, *(__u16 *)buf);
			if (ret) 
				return ret;
		}
	
	if (pageaddr != endaddr)
	{
		/* One more byte to write at the end. */
		u_char cbuf;

		cbuf = *buf;

		ret = byte_write(pageaddr, cbuf);

		if (ret) return ret;
	}

	return check_write(startaddr);
/*	printk("Time taken in flashcard_write: %lx jiffies\n",jiffies - oldj);*/
}




/*====================================================================*/

static inline int byte_write (volatile u_char *addr, u_char byte)
{
	register u_char status;
	register u_short i = 0;

	do {
		status = readb(addr);
		if (status & CSR_WR_READY)
		{
			writeb(IF_WRITE & 0xff, addr);
			writeb(byte, addr);
			return 0;
		}
		i++;
	} while(i < max_tries);

		
	printk(KERN_NOTICE "flashcard: byte_write timed out, status 0x%x\n",status);
	return -EIO;
}

static inline int word_write (volatile u_char *addr, __u16 word)
{
	register u_short status;
	register u_short i = 0;
	
	do {
		status = readw(addr);
		if ((status & CSR_WR_READY) == CSR_WR_READY)
		{
			writew(IF_WRITE, addr);
			writew(word, addr);
			return 0;
		}
		i++;
	} while(i < max_tries);
		
	printk(KERN_NOTICE "flashcard: word_write timed out at %p, status 0x%x\n", addr, status);
	return -EIO;
}

static inline void block_erase (volatile u_char *addr)
{
	writew(IF_BLOCK_ERASE, addr);
	writew(IF_CONFIRM, addr);
}


static inline int check_erase(volatile u_char *addr)
{
	__u16 status;
	
/*	writew(IF_READ_CSR, addr);*/
	status = readw(addr);
	

	if ((status & CSR_WR_READY) != CSR_WR_READY)
		return -EBUSY;
	
	if (status & (CSR_ERA_ERR | CSR_VPP_LOW | CSR_WR_ERR)) 
	{
		printk(KERN_NOTICE "flashcard: erase failed, status 0x%x\n",
		       status);
		return -EIO;
	}
	
	return 0;
}

static inline int suspend_erase(volatile u_char *addr)
{
	__u16 status;
	u_long i = 0;
	
	writew(IF_ERASE_SUSPEND, addr);
	writew(IF_READ_CSR, addr);
	
	do {
		status = readw(addr);
		if ((status & CSR_WR_READY) == CSR_WR_READY)
			return 0;
		i++;
	} while(i < max_tries);

	printk(KERN_NOTICE "flashcard: suspend_erase timed out, status 0x%x\n", status);
	return -EIO;

}

static inline void resume_erase(volatile u_char *addr)
{
	__u16 status;
	
	writew(IF_READ_CSR, addr);
	status = readw(addr);
	
	/* Only give resume signal if the erase is really suspended */
	if (status & CSR_ERA_SUSPEND)
		writew(IF_CONFIRM, addr);
}

static inline void reset_block(volatile u_char *addr)
{
	u_short i;
	__u16 status;

	writew(IF_CLEAR_CSR, addr);

	for (i = 0; i < 100; i++) {
		writew(IF_READ_CSR, addr);
		status = readw(addr);
		if (status != 0xffff) break;
		udelay(1000);
	}

	writew(IF_READ_CSR, addr);
}

static inline int check_write(volatile u_char *addr)
{
	u_short status, i = 0;
	
	writew(IF_READ_CSR, addr);
	
	do {
		status = readw(addr);
		if (status & (CSR_WR_ERR | CSR_VPP_LOW))
		{
			printk(KERN_NOTICE "flashcard: write failure at %p, status 0x%x\n", addr, status);
			reset_block(addr);
			return -EIO;
		}
		if ((status & CSR_WR_READY) == CSR_WR_READY)
			return 0;
		i++;
	} while (i < max_tries);

	printk(KERN_NOTICE "flashcard: write timed out at %p, status 0x%x\n", addr, status);
	return -EIO;
}


/*====================================================================*/



static void flashcard_periodic(unsigned long data)
{
	register struct mtd_info *mtd = (struct mtd_info *)data;
	register struct mypriv *priv = mtd->priv;
	struct erase_info *erase = priv->cur_erases;
	u_char *pageaddr;

	del_timer (&flashcard_timer);

	if (!erase)
		return;

	pageaddr = pagein(mtd, erase->addr);
	
	if (erase->state == MTD_ERASE_PENDING)
	{
		block_erase(pageaddr);
		priv->devstat[erase->dev] = erase->state = MTD_ERASING;
		erase->time = jiffies;
		erase->retries = 0;
	}
	else if (erase->state == MTD_ERASING)
	{
		/* It's trying to erase. Check whether it's finished */

		int ret = check_erase(pageaddr);

		if (!ret)
		{
			/* It's finished OK */
			priv->devstat[erase->dev] = 0;
			priv->cur_erases = erase->next;
			erase->state = MTD_ERASE_DONE;
			if (erase->callback)
				(*(erase->callback))(erase);
			else
				kfree(erase);
		}
		else if (ret == -EIO)
		{
			if (++erase->retries > retry_limit)
			{
				printk("Failed too many times. Giving up\n");
				priv->cur_erases = erase->next;
				priv->devstat[erase->dev] = 0;
				erase->state = MTD_ERASE_FAILED;
				if (erase->callback)
					(*(erase->callback))(erase);
				else
					kfree(erase);
			}
			else
				priv->devstat[erase->dev] = erase->state = MTD_ERASE_PENDING;
		}
		else if (time_after(jiffies, erase->time + erase_timeout))
		{
			printk("Flash erase timed out. The world is broken.\n");

			/* Just ignore and hope it goes away. For a while, read ops will give the CSR
			   and writes won't work. */

			priv->cur_erases = erase->next;
			priv->devstat[erase->dev] = 0;
			erase->state = MTD_ERASE_FAILED;
			if (erase->callback)
					(*(erase->callback))(erase);
				else
					kfree(erase);
		}
	}

	if (priv->cur_erases)
	{
		flashcard_timer.expires = jiffies + HZ;
		add_timer (&flashcard_timer);
	}
	else 
		wake_up_interruptible(&priv->wq);

}

int __init init_doc1000(void)
{
	struct mypriv *priv;

	if (!base)
	{
		printk(KERN_NOTICE "flashcard: No start address for memory device.\n");
		return -EINVAL;
	}

	mymtd  = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);

	if (!mymtd)
	{
		printk(KERN_NOTICE "physmem: Cannot allocate memory for new MTD device.\n");
		return -ENOMEM;
	}

	memset(mymtd,0,sizeof(struct mtd_info));

	mymtd->priv = (void *) kmalloc (sizeof(struct mypriv), GFP_KERNEL);
	if (!mymtd->priv)
	  {
	    kfree(mymtd);
	    printk(KERN_NOTICE "physmem: Cannot allocate memory for new MTD device's private data.\n");
	    return -ENOMEM;
	  }
	



	priv=mymtd->priv;
	init_waitqueue_head(&priv->wq);

	memset (priv,0,sizeof(struct mypriv));

	priv->baseaddr = phys_to_virt(base);
	priv->numdevices = 4;
	
	mymtd->name = "M-Systems DiskOnChip 1000";

	mymtd->size = 0x100000;
	mymtd->flags = MTD_CLEAR_BITS | MTD_ERASEABLE;
        mymtd->erase = flashcard_erase;
	mymtd->point = NULL;
	mymtd->unpoint = NULL;
	mymtd->read = flashcard_read;
	mymtd->write = flashcard_write;

	mymtd->sync = flashcard_sync;
	mymtd->erasesize = 0x10000;
	//	mymtd->interleave = 2;
	priv->devshift =  24;
	mymtd->type = MTD_NORFLASH;
	
	if (add_mtd_device(mymtd))
	{
		printk(KERN_NOTICE "MTD device registration failed!\n");
		kfree(mymtd->priv);
		kfree(mymtd);
		return -EAGAIN;
	}
	
	init_timer(&flashcard_timer);
	flashcard_timer.function = flashcard_periodic;
	flashcard_timer.data = (u_long)mymtd;
	return 0;
}

static void __init cleanup_doc1000(void)
{
	kfree (mymtd->priv);
	del_mtd_device(mymtd);
	kfree(mymtd);
}

module_init(init_doc1000);
module_exit(cleanup_doc1000);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("MTD driver for DiskOnChip 1000");


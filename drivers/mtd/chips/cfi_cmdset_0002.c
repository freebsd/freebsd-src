/*
 * Common Flash Interface support:
 *   AMD & Fujitsu Standard Vendor Command Set (ID 0x0002)
 *
 * Copyright (C) 2000 Crossnet Co. <info@crossnet.co.jp>
 *
 * 2_by_8 routines added by Simon Munton
 *
 * This code is GPL
 *
 * $Id: cfi_cmdset_0002.c,v 1.62 2003/01/24 23:30:13 dwmw2 Exp $
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>

#define AMD_BOOTLOC_BUG

static int cfi_amdstd_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cfi_amdstd_write(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static int cfi_amdstd_erase_chip(struct mtd_info *, struct erase_info *);
static int cfi_amdstd_erase_onesize(struct mtd_info *, struct erase_info *);
static int cfi_amdstd_erase_varsize(struct mtd_info *, struct erase_info *);
static void cfi_amdstd_sync (struct mtd_info *);
static int cfi_amdstd_suspend (struct mtd_info *);
static void cfi_amdstd_resume (struct mtd_info *);
static int cfi_amdstd_secsi_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);

static void cfi_amdstd_destroy(struct mtd_info *);

struct mtd_info *cfi_cmdset_0002(struct map_info *, int);
static struct mtd_info *cfi_amdstd_setup (struct map_info *);


static struct mtd_chip_driver cfi_amdstd_chipdrv = {
	probe: NULL, /* Not usable directly */
	destroy: cfi_amdstd_destroy,
	name: "cfi_cmdset_0002",
	module: THIS_MODULE
};

struct mtd_info *cfi_cmdset_0002(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned char bootloc;
	int ofs_factor = cfi->interleave * cfi->device_type;
	int i;
	__u8 major, minor;
	__u32 base = cfi->chips[0].start;

	if (cfi->cfi_mode==CFI_MODE_CFI){
		__u16 adr = primary?cfi->cfiq->P_ADR:cfi->cfiq->A_ADR;

		cfi_send_gen_cmd(0x98, 0x55, base, map, cfi, cfi->device_type, NULL);
		
		major = cfi_read_query(map, base + (adr+3)*ofs_factor);
		minor = cfi_read_query(map, base + (adr+4)*ofs_factor);
		
		printk(KERN_NOTICE " Amd/Fujitsu Extended Query Table v%c.%c at 0x%4.4X\n",
		       major, minor, adr);
				cfi_send_gen_cmd(0xf0, 0x55, base, map, cfi, cfi->device_type, NULL);
		
		cfi_send_gen_cmd(0xaa, 0x555, base, map, cfi, cfi->device_type, NULL);
		cfi_send_gen_cmd(0x55, 0x2aa, base, map, cfi, cfi->device_type, NULL);
		cfi_send_gen_cmd(0x90, 0x555, base, map, cfi, cfi->device_type, NULL);
		cfi->mfr = cfi_read_query(map, base);
		cfi->id = cfi_read_query(map, base + ofs_factor);    

		/* Wheee. Bring me the head of someone at AMD. */
#ifdef AMD_BOOTLOC_BUG
		if (((major << 8) | minor) < 0x3131) {
			/* CFI version 1.0 => don't trust bootloc */
			if (cfi->id & 0x80) {
				printk(KERN_WARNING "%s: JEDEC Device ID is 0x%02X. Assuming broken CFI table.\n", map->name, cfi->id);
				bootloc = 3;	/* top boot */
			} else {
				bootloc = 2;	/* bottom boot */
			}
		} else
#endif
			{
				cfi_send_gen_cmd(0x98, 0x55, base, map, cfi, cfi->device_type, NULL);
				bootloc = cfi_read_query(map, base + (adr+15)*ofs_factor);
			}
		if (bootloc == 3 && cfi->cfiq->NumEraseRegions > 1) {
			printk(KERN_WARNING "%s: Swapping erase regions for broken CFI table.\n", map->name);
			
			for (i=0; i<cfi->cfiq->NumEraseRegions / 2; i++) {
				int j = (cfi->cfiq->NumEraseRegions-1)-i;
				__u32 swap;
				
				swap = cfi->cfiq->EraseRegionInfo[i];
				cfi->cfiq->EraseRegionInfo[i] = cfi->cfiq->EraseRegionInfo[j];
				cfi->cfiq->EraseRegionInfo[j] = swap;
			}
		}
		switch (cfi->device_type) {
		case CFI_DEVICETYPE_X8:
			cfi->addr_unlock1 = 0x555; 
			cfi->addr_unlock2 = 0x2aa; 
			break;
		case CFI_DEVICETYPE_X16:
			cfi->addr_unlock1 = 0xaaa;
			if (map->buswidth == cfi->interleave) {
				/* X16 chip(s) in X8 mode */
				cfi->addr_unlock2 = 0x555;
			} else {
				cfi->addr_unlock2 = 0x554;
			}
			break;
		case CFI_DEVICETYPE_X32:
			cfi->addr_unlock1 = 0x1555; 
			cfi->addr_unlock2 = 0xaaa; 
			break;
		default:
			printk(KERN_NOTICE "Eep. Unknown cfi_cmdset_0002 device type %d\n", cfi->device_type);
			return NULL;
		}
	} /* CFI mode */

	for (i=0; i< cfi->numchips; i++) {
		cfi->chips[i].word_write_time = 1<<cfi->cfiq->WordWriteTimeoutTyp;
		cfi->chips[i].buffer_write_time = 1<<cfi->cfiq->BufWriteTimeoutTyp;
		cfi->chips[i].erase_time = 1<<cfi->cfiq->BlockEraseTimeoutTyp;
	}		
	
	map->fldrv = &cfi_amdstd_chipdrv;

	cfi_send_gen_cmd(0xf0, 0x55, base, map, cfi, cfi->device_type, NULL);
	return cfi_amdstd_setup(map);
}

static struct mtd_info *cfi_amdstd_setup(struct map_info *map)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct mtd_info *mtd;
	unsigned long devsize = (1<<cfi->cfiq->DevSize) * cfi->interleave;

	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	printk(KERN_NOTICE "number of %s chips: %d\n", 
		(cfi->cfi_mode == CFI_MODE_CFI)?"CFI":"JEDEC",cfi->numchips);

	if (!mtd) {
	  printk(KERN_WARNING "Failed to allocate memory for MTD device\n");
	  goto setup_err;
	}

	memset(mtd, 0, sizeof(*mtd));
	mtd->priv = map;
	mtd->type = MTD_NORFLASH;
	/* Also select the correct geometry setup too */ 
	mtd->size = devsize * cfi->numchips;
	
	if (cfi->cfiq->NumEraseRegions == 1) {
		/* No need to muck about with multiple erase sizes */
		mtd->erasesize = ((cfi->cfiq->EraseRegionInfo[0] >> 8) & ~0xff) * cfi->interleave;
	} else {
		unsigned long offset = 0;
		int i,j;

		mtd->numeraseregions = cfi->cfiq->NumEraseRegions * cfi->numchips;
		mtd->eraseregions = kmalloc(sizeof(struct mtd_erase_region_info) * mtd->numeraseregions, GFP_KERNEL);
		if (!mtd->eraseregions) { 
			printk(KERN_WARNING "Failed to allocate memory for MTD erase region info\n");
			goto setup_err;
		}
			
		for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
			unsigned long ernum, ersize;
			ersize = ((cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xff) * cfi->interleave;
			ernum = (cfi->cfiq->EraseRegionInfo[i] & 0xffff) + 1;
			
			if (mtd->erasesize < ersize) {
				mtd->erasesize = ersize;
			}
			for (j=0; j<cfi->numchips; j++) {
				mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].offset = (j*devsize)+offset;
				mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].erasesize = ersize;
				mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].numblocks = ernum;
			}
			offset += (ersize * ernum);
		}
		if (offset != devsize) {
			/* Argh */
			printk(KERN_WARNING "Sum of regions (%lx) != total size of set of interleaved chips (%lx)\n", offset, devsize);
			goto setup_err;
		}
#if 0
		// debug
		for (i=0; i<mtd->numeraseregions;i++){
			printk("%d: offset=0x%x,size=0x%x,blocks=%d\n",
			       i,mtd->eraseregions[i].offset,
			       mtd->eraseregions[i].erasesize,
			       mtd->eraseregions[i].numblocks);
		}
#endif
	}

	switch (CFIDEV_BUSWIDTH)
	{
	case 1:
	case 2:
	case 4:
#if 1
		if (mtd->numeraseregions > 1)
			mtd->erase = cfi_amdstd_erase_varsize;
		else
#endif
		if (((cfi->cfiq->EraseRegionInfo[0] & 0xffff) + 1) == 1)
			mtd->erase = cfi_amdstd_erase_chip;
		else
			mtd->erase = cfi_amdstd_erase_onesize;
		mtd->read = cfi_amdstd_read;
		mtd->write = cfi_amdstd_write;
		break;

	default:
	        printk(KERN_WARNING "Unsupported buswidth\n");
		goto setup_err;
		break;
	}
	if (cfi->fast_prog) {
		/* In cfi_amdstd_write() we frob the protection stuff
		   without paying any attention to the state machine.
		   This upsets in-progress erases. So we turn this flag
		   off for now till the code gets fixed. */
		printk(KERN_NOTICE "cfi_cmdset_0002: Disabling fast programming due to code brokenness.\n");
		cfi->fast_prog = 0;
	}


        /* does this chip have a secsi area? */
	if(cfi->mfr==1){
		
		switch(cfi->id){
		case 0x50:
		case 0x53:
		case 0x55:
		case 0x56:
		case 0x5C:
		case 0x5F:
			/* Yes */
			mtd->read_user_prot_reg = cfi_amdstd_secsi_read;
			mtd->read_fact_prot_reg = cfi_amdstd_secsi_read;
		default:		       
			;
		}
	}
	
		
	mtd->sync = cfi_amdstd_sync;
	mtd->suspend = cfi_amdstd_suspend;
	mtd->resume = cfi_amdstd_resume;
	mtd->flags = MTD_CAP_NORFLASH;
	map->fldrv = &cfi_amdstd_chipdrv;
	mtd->name = map->name;
	MOD_INC_USE_COUNT;
	return mtd;

 setup_err:
	if(mtd) {
		if(mtd->eraseregions)
			kfree(mtd->eraseregions);
		kfree(mtd);
	}
	kfree(cfi->cmdset_priv);
	kfree(cfi->cfiq);
	return NULL;
}

static inline int do_read_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long timeo = jiffies + HZ;

 retry:
	cfi_spin_lock(chip->mutex);

	if (chip->state != FL_READY){
#if 0
	        printk(KERN_DEBUG "Waiting for chip to read, status = %d\n", chip->state);
#endif
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
                
		cfi_spin_unlock(chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
#if 0
		if(signal_pending(current))
			return -EINTR;
#endif
		timeo = jiffies + HZ;

		goto retry;
	}	

	adr += chip->start;

	chip->state = FL_READY;

	map->copy_from(map, buf, adr, len);

	wake_up(&chip->wq);
	cfi_spin_unlock(chip->mutex);

	return 0;
}

static int cfi_amdstd_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	/* ofs: offset within the first chip that the first read should start */

	chipnum = (from >> cfi->chipshift);
	ofs = from - (chipnum <<  cfi->chipshift);


	*retlen = 0;

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> cfi->chipshift)
			thislen = (1<<cfi->chipshift) - ofs;
		else
			thislen = len;

		ret = do_read_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

static inline int do_read_secsi_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long timeo = jiffies + HZ;
	struct cfi_private *cfi = map->fldrv_priv;

 retry:
	cfi_spin_lock(chip->mutex);

	if (chip->state != FL_READY){
#if 0
	        printk(KERN_DEBUG "Waiting for chip to read, status = %d\n", chip->state);
#endif
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
                
		cfi_spin_unlock(chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
#if 0
		if(signal_pending(current))
			return -EINTR;
#endif
		timeo = jiffies + HZ;

		goto retry;
	}	

	adr += chip->start;

	chip->state = FL_READY;
	
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x88, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	
	map->copy_from(map, buf, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x90, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x00, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	
	wake_up(&chip->wq);
	cfi_spin_unlock(chip->mutex);

	return 0;
}

static int cfi_amdstd_secsi_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;


	/* ofs: offset within the first chip that the first read should start */

	/* 8 secsi bytes per chip */
	chipnum=from>>3;
	ofs=from & 7;


	*retlen = 0;

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> 3)
			thislen = (1<<3) - ofs;
		else
			thislen = len;

		ret = do_read_secsi_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

static int do_write_oneword(struct map_info *map, struct flchip *chip, unsigned long adr, __u32 datum, int fast)
{
	unsigned long timeo = jiffies + HZ;
	unsigned int oldstatus, status;
	unsigned int dq6, dq5;	
	struct cfi_private *cfi = map->fldrv_priv;
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

 retry:
	cfi_spin_lock(chip->mutex);

	if (chip->state != FL_READY) {
#if 0
	        printk(KERN_DEBUG "Waiting for chip to write, status = %d\n", chip->state);
#endif
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
                
		cfi_spin_unlock(chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
#if 0
		printk(KERN_DEBUG "Wake up to write:\n");
		if(signal_pending(current))
			return -EINTR;
#endif
		timeo = jiffies + HZ;

		goto retry;
	}	

	chip->state = FL_WRITING;

	adr += chip->start;
	ENABLE_VPP(map);
	if (fast) { /* Unlock bypass */
		cfi_send_gen_cmd(0xA0, 0, chip->start, map, cfi, cfi->device_type, NULL);
	}
	else {
	        cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	        cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	        cfi_send_gen_cmd(0xA0, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	}

	cfi_write(map, datum, adr);

	cfi_spin_unlock(chip->mutex);
	cfi_udelay(chip->word_write_time);
	cfi_spin_lock(chip->mutex);

	/* Polling toggle bits instead of reading back many times
	   This ensures that write operation is really completed,
	   or tells us why it failed. */        
	dq6 = CMD(1<<6);
	dq5 = CMD(1<<5);
	timeo = jiffies + (HZ/1000); /* setting timeout to 1ms for now */
		
	oldstatus = cfi_read(map, adr);
	status = cfi_read(map, adr);

	while( (status & dq6) != (oldstatus & dq6) && 
	       (status & dq5) != dq5 &&
	       !time_after(jiffies, timeo) ) {

		if (need_resched()) {
			cfi_spin_unlock(chip->mutex);
			yield();
			cfi_spin_lock(chip->mutex);
		} else 
			udelay(1);

		oldstatus = cfi_read( map, adr );
		status = cfi_read( map, adr );
	}
	
	if( (status & dq6) != (oldstatus & dq6) ) {
		/* The erasing didn't stop?? */
		if( (status & dq5) == dq5 ) {
			/* When DQ5 raises, we must check once again
			   if DQ6 is toggling.  If not, the erase has been
			   completed OK.  If not, reset chip. */
			oldstatus = cfi_read(map, adr);
			status = cfi_read(map, adr);
		    
			if ( (oldstatus & 0x00FF) == (status & 0x00FF) ) {
				printk(KERN_WARNING "Warning: DQ5 raised while program operation was in progress, however operation completed OK\n" );
			} else { 
				/* DQ5 is active so we can do a reset and stop the erase */
				cfi_write(map, CMD(0xF0), chip->start);
				printk(KERN_WARNING "Internal flash device timeout occurred or write operation was performed while flash was programming.\n" );
			}
		} else {
			printk(KERN_WARNING "Waiting for write to complete timed out in do_write_oneword.");        
			
			chip->state = FL_READY;
			wake_up(&chip->wq);
			cfi_spin_unlock(chip->mutex);
			DISABLE_VPP(map);
			ret = -EIO;
		}
	}

	DISABLE_VPP(map);
	chip->state = FL_READY;
	wake_up(&chip->wq);
	cfi_spin_unlock(chip->mutex);

	return ret;
}

static int cfi_amdstd_write (struct mtd_info *mtd, loff_t to , size_t len, size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret = 0;
	int chipnum;
	unsigned long ofs, chipstart;

	*retlen = 0;
	if (!len)
		return 0;

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);
	chipstart = cfi->chips[chipnum].start;

	/* If it's not bus-aligned, do the first byte write */
	if (ofs & (CFIDEV_BUSWIDTH-1)) {
		unsigned long bus_ofs = ofs & ~(CFIDEV_BUSWIDTH-1);
		int i = ofs - bus_ofs;
		int n = 0;
		u_char tmp_buf[4];
		__u32 datum;

		map->copy_from(map, tmp_buf, bus_ofs + cfi->chips[chipnum].start, CFIDEV_BUSWIDTH);
		while (len && i < CFIDEV_BUSWIDTH)
			tmp_buf[i++] = buf[n++], len--;

		if (cfi_buswidth_is_2()) {
			datum = *(__u16*)tmp_buf;
		} else if (cfi_buswidth_is_4()) {
			datum = *(__u32*)tmp_buf;
		} else {
			return -EINVAL;  /* should never happen, but be safe */
		}

		ret = do_write_oneword(map, &cfi->chips[chipnum], 
				bus_ofs, datum, 0);
		if (ret) 
			return ret;
		
		ofs += n;
		buf += n;
		(*retlen) += n;

		if (ofs >> cfi->chipshift) {
			chipnum ++; 
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}
	
	if (cfi->fast_prog) {
		/* Go into unlock bypass mode */
		cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
		cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
		cfi_send_gen_cmd(0x20, cfi->addr_unlock1, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
	}

	/* We are now aligned, write as much as possible */
	while(len >= CFIDEV_BUSWIDTH) {
		__u32 datum;

		if (cfi_buswidth_is_1()) {
			datum = *(__u8*)buf;
		} else if (cfi_buswidth_is_2()) {
			datum = *(__u16*)buf;
		} else if (cfi_buswidth_is_4()) {
			datum = *(__u32*)buf;
		} else {
			return -EINVAL;
		}
		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       ofs, datum, cfi->fast_prog);
		if (ret) {
			if (cfi->fast_prog){
				/* Get out of unlock bypass mode */
				cfi_send_gen_cmd(0x90, 0, chipstart, map, cfi, cfi->device_type, NULL);
				cfi_send_gen_cmd(0x00, 0, chipstart, map, cfi, cfi->device_type, NULL);
			}
			return ret;
		}

		ofs += CFIDEV_BUSWIDTH;
		buf += CFIDEV_BUSWIDTH;
		(*retlen) += CFIDEV_BUSWIDTH;
		len -= CFIDEV_BUSWIDTH;

		if (ofs >> cfi->chipshift) {
			if (cfi->fast_prog){
				/* Get out of unlock bypass mode */
				cfi_send_gen_cmd(0x90, 0, chipstart, map, cfi, cfi->device_type, NULL);
				cfi_send_gen_cmd(0x00, 0, chipstart, map, cfi, cfi->device_type, NULL);
			}

			chipnum ++; 
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
			chipstart = cfi->chips[chipnum].start;
			if (cfi->fast_prog){
				/* Go into unlock bypass mode for next set of chips */
				cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
				cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
				cfi_send_gen_cmd(0x20, cfi->addr_unlock1, chipstart, map, cfi, CFI_DEVICETYPE_X8, NULL);
			}
		}
	}

	if (cfi->fast_prog){
		/* Get out of unlock bypass mode */
		cfi_send_gen_cmd(0x90, 0, chipstart, map, cfi, cfi->device_type, NULL);
		cfi_send_gen_cmd(0x00, 0, chipstart, map, cfi, cfi->device_type, NULL);
	}

	/* Write the trailing bytes if any */
	if (len & (CFIDEV_BUSWIDTH-1)) {
		int i = 0, n = 0;
		u_char tmp_buf[4];
		__u32 datum;

		map->copy_from(map, tmp_buf, ofs + cfi->chips[chipnum].start, CFIDEV_BUSWIDTH);
		while (len--)
			tmp_buf[i++] = buf[n++];

		if (cfi_buswidth_is_2()) {
			datum = *(__u16*)tmp_buf;
		} else if (cfi_buswidth_is_4()) {
			datum = *(__u32*)tmp_buf;
		} else {
			return -EINVAL;  /* should never happen, but be safe */
		}

		ret = do_write_oneword(map, &cfi->chips[chipnum], 
				ofs, datum, 0);
		if (ret) 
			return ret;
		
		(*retlen) += n;
	}

	return 0;
}

static inline int do_erase_chip(struct map_info *map, struct flchip *chip)
{
	unsigned int oldstatus, status;
	unsigned int dq6, dq5;
	unsigned long timeo = jiffies + HZ;
	unsigned int adr;
	struct cfi_private *cfi = map->fldrv_priv;
	DECLARE_WAITQUEUE(wait, current);

 retry:
	cfi_spin_lock(chip->mutex);

	if (chip->state != FL_READY){
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
                
		cfi_spin_unlock(chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
#if 0
		if(signal_pending(current))
			return -EINTR;
#endif
		timeo = jiffies + HZ;

		goto retry;
	}	

	chip->state = FL_ERASING;
	
	/* Handle devices with one erase region, that only implement
	 * the chip erase command.
	 */
	ENABLE_VPP(map);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x10, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	timeo = jiffies + (HZ*20);
	adr = cfi->addr_unlock1;

	/* Wait for the end of programing/erasure by using the toggle method.
	 * As long as there is a programming procedure going on, bit 6 of the last
	 * written byte is toggling it's state with each consectuve read.
	 * The toggling stops as soon as the procedure is completed.
	 *
	 * If the process has gone on for too long on the chip bit 5 gets.
	 * After bit5 is set you can kill the operation by sending a reset
	 * command to the chip.
	 */
	dq6 = CMD(1<<6);
	dq5 = CMD(1<<5);

	oldstatus = cfi_read(map, adr);
	status = cfi_read(map, adr);
	while( ((status & dq6) != (oldstatus & dq6)) && 
		((status & dq5) != dq5) &&
		!time_after(jiffies, timeo)) {
		int wait_reps;

		/* an initial short sleep */
		cfi_spin_unlock(chip->mutex);
		schedule_timeout(HZ/100);
		cfi_spin_lock(chip->mutex);
		
		if (chip->state != FL_ERASING) {
			/* Someone's suspended the erase. Sleep */
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			
			cfi_spin_unlock(chip->mutex);
			printk("erase suspended. Sleeping\n");
			
			schedule();
			remove_wait_queue(&chip->wq, &wait);
#if 0			
			if (signal_pending(current))
				return -EINTR;
#endif			
			timeo = jiffies + (HZ*2); /* FIXME */
			cfi_spin_lock(chip->mutex);
			continue;
		}

		/* Busy wait for 1/10 of a milisecond */
		for(wait_reps = 0;
		    	(wait_reps < 100) &&
			((status & dq6) != (oldstatus & dq6)) && 
			((status & dq5) != dq5);
			wait_reps++) {
			
			/* Latency issues. Drop the lock, wait a while and retry */
			cfi_spin_unlock(chip->mutex);
			
			cfi_udelay(1);
		
			cfi_spin_lock(chip->mutex);
			oldstatus = cfi_read(map, adr);
			status = cfi_read(map, adr);
		}
		oldstatus = cfi_read(map, adr);
		status = cfi_read(map, adr);
	}
	if ((status & dq6) != (oldstatus & dq6)) {
		/* The erasing didn't stop?? */
		if ((status & dq5) == dq5) {
			/* dq5 is active so we can do a reset and stop the erase */
			cfi_write(map, CMD(0xF0), chip->start);
		}
		chip->state = FL_READY;
		wake_up(&chip->wq);
		cfi_spin_unlock(chip->mutex);
		printk("waiting for erase to complete timed out.");
		DISABLE_VPP(map);
		return -EIO;
	}
	DISABLE_VPP(map);
	chip->state = FL_READY;
	wake_up(&chip->wq);
	cfi_spin_unlock(chip->mutex);

	return 0;
}

static inline int do_erase_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	unsigned int oldstatus, status;
	unsigned int dq6, dq5;
	unsigned long timeo = jiffies + HZ;
	struct cfi_private *cfi = map->fldrv_priv;
	DECLARE_WAITQUEUE(wait, current);

 retry:
	cfi_spin_lock(chip->mutex);

	if (chip->state != FL_READY){
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
                
		cfi_spin_unlock(chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
#if 0
		if(signal_pending(current))
			return -EINTR;
#endif
		timeo = jiffies + HZ;

		goto retry;
	}	

	chip->state = FL_ERASING;

	adr += chip->start;
	ENABLE_VPP(map);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, CFI_DEVICETYPE_X8, NULL);
	cfi_write(map, CMD(0x30), adr);
	
	timeo = jiffies + (HZ*20);

	/* Wait for the end of programing/erasure by using the toggle method.
	 * As long as there is a programming procedure going on, bit 6 of the last
	 * written byte is toggling it's state with each consectuve read.
	 * The toggling stops as soon as the procedure is completed.
	 *
	 * If the process has gone on for too long on the chip bit 5 gets.
	 * After bit5 is set you can kill the operation by sending a reset
	 * command to the chip.
	 */
	dq6 = CMD(1<<6);
	dq5 = CMD(1<<5);

	oldstatus = cfi_read(map, adr);
	status = cfi_read(map, adr);
	while( ((status & dq6) != (oldstatus & dq6)) && 
		((status & dq5) != dq5) &&
		!time_after(jiffies, timeo)) {
		int wait_reps;

		/* an initial short sleep */
		cfi_spin_unlock(chip->mutex);
		schedule_timeout(HZ/100);
		cfi_spin_lock(chip->mutex);
		
		if (chip->state != FL_ERASING) {
			/* Someone's suspended the erase. Sleep */
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			
			cfi_spin_unlock(chip->mutex);
			printk(KERN_DEBUG "erase suspended. Sleeping\n");
			
			schedule();
			remove_wait_queue(&chip->wq, &wait);
#if 0			
			if (signal_pending(current))
				return -EINTR;
#endif			
			timeo = jiffies + (HZ*2); /* FIXME */
			cfi_spin_lock(chip->mutex);
			continue;
		}

		/* Busy wait for 1/10 of a milisecond */
		for(wait_reps = 0;
		    	(wait_reps < 100) &&
			((status & dq6) != (oldstatus & dq6)) && 
			((status & dq5) != dq5);
			wait_reps++) {
			
			/* Latency issues. Drop the lock, wait a while and retry */
			cfi_spin_unlock(chip->mutex);
			
			cfi_udelay(1);
		
			cfi_spin_lock(chip->mutex);
			oldstatus = cfi_read(map, adr);
			status = cfi_read(map, adr);
		}
		oldstatus = cfi_read(map, adr);
		status = cfi_read(map, adr);
	}
	if( (status & dq6) != (oldstatus & dq6) ) 
	{                                       
		/* The erasing didn't stop?? */
		if( ( status & dq5 ) == dq5 ) 
		{   			
			/* When DQ5 raises, we must check once again if DQ6 is toggling.
               If not, the erase has been completed OK.  If not, reset chip. */
		    oldstatus   = cfi_read( map, adr );
		    status      = cfi_read( map, adr );
		    
		    if( ( oldstatus & 0x00FF ) == ( status & 0x00FF ) )
		    {
                printk( "Warning: DQ5 raised while erase operation was in progress, but erase completed OK\n" ); 		    
		    } 			
			else
            {
			    /* DQ5 is active so we can do a reset and stop the erase */
				cfi_write(map, CMD(0xF0), chip->start);
                printk( KERN_WARNING "Internal flash device timeout occured or write operation was performed while flash was erasing\n" );
			}
		}
        else
        {
		    printk( "Waiting for erase to complete timed out in do_erase_oneblock.");        
		    
		chip->state = FL_READY;
		wake_up(&chip->wq);
		cfi_spin_unlock(chip->mutex);
		DISABLE_VPP(map);
		return -EIO;
	}
	}

	DISABLE_VPP(map);
	chip->state = FL_READY;
	wake_up(&chip->wq);
	cfi_spin_unlock(chip->mutex);
	return 0;
}

static int cfi_amdstd_erase_varsize(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long adr, len;
	int chipnum, ret = 0;
	int i, first;
	struct mtd_erase_region_info *regions = mtd->eraseregions;

	if (instr->addr > mtd->size)
		return -EINVAL;

	if ((instr->len + instr->addr) > mtd->size)
		return -EINVAL;

	/* Check that both start and end of the requested erase are
	 * aligned with the erasesize at the appropriate addresses.
	 */

	i = 0;

	/* Skip all erase regions which are ended before the start of 
	   the requested erase. Actually, to save on the calculations,
	   we skip to the first erase region which starts after the
	   start of the requested erase, and then go back one.
	*/
	
	while (i < mtd->numeraseregions && instr->addr >= regions[i].offset)
	       i++;
	i--;

	/* OK, now i is pointing at the erase region in which this 
	   erase request starts. Check the start of the requested
	   erase range is aligned with the erase size which is in
	   effect here.
	*/

	if (instr->addr & (regions[i].erasesize-1))
		return -EINVAL;

	/* Remember the erase region we start on */
	first = i;

	/* Next, check that the end of the requested erase is aligned
	 * with the erase region at that address.
	 */

	while (i<mtd->numeraseregions && (instr->addr + instr->len) >= regions[i].offset)
		i++;

	/* As before, drop back one to point at the region in which
	   the address actually falls
	*/
	i--;
	
	if ((instr->addr + instr->len) & (regions[i].erasesize-1))
		return -EINVAL;
	
	chipnum = instr->addr >> cfi->chipshift;
	adr = instr->addr - (chipnum << cfi->chipshift);
	len = instr->len;

	i=first;

	while(len) {
		ret = do_erase_oneblock(map, &cfi->chips[chipnum], adr);

		if (ret)
			return ret;

		adr += regions[i].erasesize;
		len -= regions[i].erasesize;

		if (adr % (1<< cfi->chipshift) == ((regions[i].offset + (regions[i].erasesize * regions[i].numblocks)) %( 1<< cfi->chipshift)))
			i++;

		if (adr >> cfi->chipshift) {
			adr = 0;
			chipnum++;
			
			if (chipnum >= cfi->numchips)
			break;
		}
	}

	instr->state = MTD_ERASE_DONE;
	if (instr->callback)
		instr->callback(instr);
	
	return 0;
}

static int cfi_amdstd_erase_onesize(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long adr, len;
	int chipnum, ret = 0;

	if (instr->addr & (mtd->erasesize - 1))
		return -EINVAL;

	if (instr->len & (mtd->erasesize -1))
		return -EINVAL;

	if ((instr->len + instr->addr) > mtd->size)
		return -EINVAL;

	chipnum = instr->addr >> cfi->chipshift;
	adr = instr->addr - (chipnum << cfi->chipshift);
	len = instr->len;

	while(len) {
		ret = do_erase_oneblock(map, &cfi->chips[chipnum], adr);

		if (ret)
			return ret;

		adr += mtd->erasesize;
		len -= mtd->erasesize;

		if (adr >> cfi->chipshift) {
			adr = 0;
			chipnum++;
			
			if (chipnum >= cfi->numchips)
			break;
		}
	}
		
	instr->state = MTD_ERASE_DONE;
	if (instr->callback)
		instr->callback(instr);
	
	return 0;
}

static int cfi_amdstd_erase_chip(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret = 0;

	if (instr->addr != 0)
		return -EINVAL;

	if (instr->len != mtd->size)
		return -EINVAL;

	ret = do_erase_chip(map, &cfi->chips[0]);
	if (ret)
		return ret;

	instr->state = MTD_ERASE_DONE;
	if (instr->callback)
		instr->callback(instr);
	
	return 0;
}

static void cfi_amdstd_sync (struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current);

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

	retry:
		cfi_spin_lock(chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_SYNCING;
			/* No need to wake_up() on this state change - 
			 * as the whole point is that nobody can do anything
			 * with the chip now anyway.
			 */
		case FL_SYNCING:
			cfi_spin_unlock(chip->mutex);
			break;

		default:
			/* Not an idle state */
			add_wait_queue(&chip->wq, &wait);
			
			cfi_spin_unlock(chip->mutex);

			schedule();

		        remove_wait_queue(&chip->wq, &wait);
			
			goto retry;
		}
	}

	/* Unlock the chips again */

	for (i--; i >=0; i--) {
		chip = &cfi->chips[i];

		cfi_spin_lock(chip->mutex);
		
		if (chip->state == FL_SYNCING) {
			chip->state = chip->oldstate;
			wake_up(&chip->wq);
		}
		cfi_spin_unlock(chip->mutex);
	}
}


static int cfi_amdstd_suspend(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

		cfi_spin_lock(chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_PM_SUSPENDED;
			/* No need to wake_up() on this state change - 
			 * as the whole point is that nobody can do anything
			 * with the chip now anyway.
			 */
		case FL_PM_SUSPENDED:
			break;

		default:
			ret = -EAGAIN;
			break;
		}
		cfi_spin_unlock(chip->mutex);
	}

	/* Unlock the chips again */

	if (ret) {
    		for (i--; i >=0; i--) {
			chip = &cfi->chips[i];

			cfi_spin_lock(chip->mutex);
		
			if (chip->state == FL_PM_SUSPENDED) {
				chip->state = chip->oldstate;
				wake_up(&chip->wq);
			}
			cfi_spin_unlock(chip->mutex);
		}
	}
	
	return ret;
}

static void cfi_amdstd_resume(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;

	for (i=0; i<cfi->numchips; i++) {
	
		chip = &cfi->chips[i];

		cfi_spin_lock(chip->mutex);
		
		if (chip->state == FL_PM_SUSPENDED) {
			chip->state = FL_READY;
			cfi_write(map, CMD(0xF0), chip->start);
			wake_up(&chip->wq);
		}
		else
			printk(KERN_ERR "Argh. Chip not in PM_SUSPENDED state upon resume()\n");

		cfi_spin_unlock(chip->mutex);
	}
}

static void cfi_amdstd_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	kfree(cfi->cmdset_priv);
	kfree(cfi->cfiq);
	kfree(cfi);
	kfree(mtd->eraseregions);
}

static char im_name[]="cfi_cmdset_0002";

int __init cfi_amdstd_init(void)
{
	inter_module_register(im_name, THIS_MODULE, &cfi_cmdset_0002);
	return 0;
}

static void __exit cfi_amdstd_exit(void)
{
	inter_module_unregister(im_name);
}

module_init(cfi_amdstd_init);
module_exit(cfi_amdstd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Crossnet Co. <info@crossnet.co.jp> et al.");
MODULE_DESCRIPTION("MTD chip driver for AMD/Fujitsu flash chips");


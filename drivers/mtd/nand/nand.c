/*
 *  drivers/mtd/nand.c
 *
 *  Overview:
 *   This is the generic MTD driver for NAND flash devices. It should be
 *   capable of working with almost all NAND chips currently available.
 *   
 *	Additional technical information is available on
 *	http://www.linux-mtd.infradead.org/tech/nand.html
 *	
 *  Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 * 		  2002 Thomas Gleixner (tglx@linutronix.de)
 *
 *  10-29-2001  Thomas Gleixner (tglx@linutronix.de)
 * 		- Changed nand_chip structure for controlline function to
 *		support different hardware structures (Access to
 *		controllines ALE,CLE,NCE via hardware specific function. 
 *		- exit out of "failed erase block" changed, to avoid
 *		driver hangup
 *		- init_waitqueue_head added in function nand_scan !!
 *
 *  01-30-2002  Thomas Gleixner (tglx@linutronix.de)
 *		change in nand_writev to block invalid vecs entries
 *
 *  02-11-2002  Thomas Gleixner (tglx@linutronix.de)
 *		- major rewrite to avoid duplicated code
 *		  common nand_write_page function  
 *		  common get_chip function 
 *		- added oob_config structure for out of band layouts
 *		- write_oob changed for partial programming
 *		- read cache for faster access for subsequent reads
 *		from the same page.
 *		- support for different read/write address
 *		- support for device ready/busy line
 *		- read oob for more than one page enabled
 *
 *  02-27-2002	Thomas Gleixner (tglx@linutronix.de)
 *		- command-delay can be programmed
 *		- fixed exit from erase with callback-function enabled
 *
 *  03-21-2002  Thomas Gleixner (tglx@linutronix.de)
 *		- DEBUG improvements provided by Elizabeth Clarke 
 *		(eclarke@aminocom.com)
 *		- added zero check for this->chip_delay
 *
 *  04-03-2002  Thomas Gleixner (tglx@linutronix.de)
 *		- added added hw-driver supplied command and wait functions
 *		- changed blocking for erase (erase suspend enabled)
 *		- check pointers before accessing flash provided by
 *		John Hall (john.hall@optionexist.co.uk)
 *
 *  04-09-2002  Thomas Gleixner (tglx@linutronix.de)
 *		- nand_wait repaired
 *
 *  04-28-2002  Thomas Gleixner (tglx@linutronix.de)	
 *		- OOB config defines moved to nand.h 
 *
 *  08-01-2002  Thomas Gleixner (tglx@linutronix.de)	
 *		- changed my mailaddress, added pointer to tech/nand.html
 *
 *  08-07-2002 	Thomas Gleixner (tglx@linutronix.de)
 *		forced bad block location to byte 5 of OOB, even if
 *		CONFIG_MTD_NAND_ECC_JFFS2 is not set, to prevent
 *		erase /dev/mtdX from erasing bad blocks and destroying
 *		bad block info
 *
 *  08-10-2002 	Thomas Gleixner (tglx@linutronix.de)
 *		Fixed writing tail of data. Thanks to Alice Hennessy
 *		<ahennessy@mvista.com>.
 *
 *  08-10-2002 	Thomas Gleixner (tglx@linutronix.de)
 *		nand_read_ecc and nand_write_page restructured to support
 *		hardware ECC. Thanks to Steven Hein (ssh@sgi.com)
 *		for basic implementation and suggestions.
 *		3 new pointers in nand_chip structure:
 *		calculate_ecc, correct_data, enabled_hwecc 					 
 *		forcing all hw-drivers to support page cache
 *		eccvalid_pos is now mandatory
 *
 *  08-17-2002	tglx: fixed signed/unsigned missmatch in write.c
 *		Thanks to Ken Offer <koffer@arlut.utexas.edu> 	
 *
 *  08-29-2002  tglx: use buffered read/write only for non pagealigned 
 *		access, speed up the aligned path by using the fs-buffer
 *		reset chip removed from nand_select(), implicit done
 *		only, when erase is interrupted
 *		waitfuntion use yield, instead of schedule_timeout
 *		support for 6byte/512byte hardware ECC
 *		read_ecc, write_ecc extended for different oob-layout
 *		selections: Implemented NAND_NONE_OOB, NAND_JFFS2_OOB,
 *		NAND_YAFFS_OOB. fs-driver gives one of these constants
 *		to select the oob-layout fitting the filesystem.
 *		oobdata can be read together with the raw data, when
 *		the fs-driver supplies a big enough buffer.
 *		size = 12 * number of pages to read (256B pagesize)
 *		       24 * number of pages to read (512B pagesize)
 *		the buffer contains 8/16 byte oobdata and 4/8 byte
 *		returncode from calculate_ecc
 *		oobdata can be given from filesystem to program them
 *		in one go together with the raw data. ECC codes are
 *		filled in at the place selected by oobsel.
 *
 *  09-04-2002  tglx: fixed write_verify (John Hall (john.hall@optionexist.co.uk))
 *
 *  11-11-2002  tglx: fixed debug output in nand_write_page 
 *		(John Hall (john.hall@optionexist.co.uk))
 *
 *  11-25-2002  tglx: Moved device ID/ manufacturer ID from nand_ids.h
 *		Splitted device ID and manufacturer ID table. 
 *		Removed CONFIG_MTD_NAND_ECC, as it defaults to ECC_NONE for
 *		mtd->read / mtd->write and is controllable by the fs driver
 *		for mtd->read_ecc / mtd->write_ecc
 *		some minor cleanups
 *
 *  12-05-2000  tglx: Dave Ellis (DGE@sixnetio) provided the fix for
 *		WRITE_VERIFY long time ago. Thanks for remembering me.	
 *	
 * $Id: nand.c,v 1.36 2002/12/05 20:59:11 gleixner Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/interrupt.h>
#include <asm/io.h>

/*
 * Macros for low-level register control
 */
#define nand_select()	this->hwcontrol(NAND_CTL_SETNCE);

#define nand_deselect() this->hwcontrol(NAND_CTL_CLRNCE);

/*
 * out of band configuration for different filesystems
 */
static int oobconfigs[][6] = {
	{ 0,0,0,0,0,0},

	{ NAND_JFFS2_OOB_ECCPOS0, NAND_JFFS2_OOB_ECCPOS1, NAND_JFFS2_OOB_ECCPOS2,
	NAND_JFFS2_OOB_ECCPOS3, NAND_JFFS2_OOB_ECCPOS4, NAND_JFFS2_OOB_ECCPOS5 },

	{ NAND_YAFFS_OOB_ECCPOS0, NAND_YAFFS_OOB_ECCPOS1, NAND_YAFFS_OOB_ECCPOS2,
	NAND_YAFFS_OOB_ECCPOS3, NAND_YAFFS_OOB_ECCPOS4, NAND_YAFFS_OOB_ECCPOS5 }	
};

/*
 * NAND low-level MTD interface functions
 */
static int nand_read (struct mtd_info *mtd, loff_t from, size_t len, size_t * retlen, u_char * buf);
static int nand_read_ecc (struct mtd_info *mtd, loff_t from, size_t len,
			  size_t * retlen, u_char * buf, u_char * eccbuf, int oobsel);
static int nand_read_oob (struct mtd_info *mtd, loff_t from, size_t len, size_t * retlen, u_char * buf);
static int nand_write (struct mtd_info *mtd, loff_t to, size_t len, size_t * retlen, const u_char * buf);
static int nand_write_ecc (struct mtd_info *mtd, loff_t to, size_t len,
			   size_t * retlen, const u_char * buf, u_char * eccbuf, int oobsel);
static int nand_write_oob (struct mtd_info *mtd, loff_t to, size_t len, size_t * retlen, const u_char *buf);
static int nand_writev (struct mtd_info *mtd, const struct iovec *vecs,
			unsigned long count, loff_t to, size_t * retlen);
static int nand_writev_ecc (struct mtd_info *mtd, const struct iovec *vecs,
			unsigned long count, loff_t to, size_t * retlen, u_char *eccbuf, int oobsel);
static int nand_erase (struct mtd_info *mtd, struct erase_info *instr);
static void nand_sync (struct mtd_info *mtd);
static int nand_write_page (struct mtd_info *mtd, struct nand_chip *this, int page, int col, 
			int last, u_char *oob_buf, int oobsel);
/*
 * Send command to NAND device
 */
static void nand_command (struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
	register struct nand_chip *this = mtd->priv;
	register unsigned long NAND_IO_ADDR = this->IO_ADDR_W;

	/* Begin command latch cycle */
	this->hwcontrol (NAND_CTL_SETCLE);
	/*
	 * Write out the command to the device.
	 */
	if (command != NAND_CMD_SEQIN)
		writeb (command, NAND_IO_ADDR);
	else {
		if (mtd->oobblock == 256 && column >= 256) {
			column -= 256;
			writeb (NAND_CMD_READOOB, NAND_IO_ADDR);
			writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
		} else if (mtd->oobblock == 512 && column >= 256) {
			if (column < 512) {
				column -= 256;
				writeb (NAND_CMD_READ1, NAND_IO_ADDR);
				writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
			} else {
				column -= 512;
				writeb (NAND_CMD_READOOB, NAND_IO_ADDR);
				writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
			}
		} else {
			writeb (NAND_CMD_READ0, NAND_IO_ADDR);
			writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
		}
	}

	/* Set ALE and clear CLE to start address cycle */
	this->hwcontrol (NAND_CTL_CLRCLE);

	if (column != -1 || page_addr != -1) {
		this->hwcontrol (NAND_CTL_SETALE);

		/* Serially input address */
		if (column != -1)
			writeb (column, NAND_IO_ADDR);
		if (page_addr != -1) {
			writeb ((unsigned char) (page_addr & 0xff), NAND_IO_ADDR);
			writeb ((unsigned char) ((page_addr >> 8) & 0xff), NAND_IO_ADDR);
			/* One more address cycle for higher density devices */
			if (mtd->size & 0x0c000000) 
				writeb ((unsigned char) ((page_addr >> 16) & 0x0f), NAND_IO_ADDR);
		}
		/* Latch in address */
		this->hwcontrol (NAND_CTL_CLRALE);
	}
	
	/* 
	 * program and erase have their own busy handlers 
	 * status and sequential in needs no delay
	*/
	switch (command) {
			
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		return;

	case NAND_CMD_RESET:
		if (this->dev_ready)	
			break;
		this->hwcontrol (NAND_CTL_SETCLE);
		writeb (NAND_CMD_STATUS, NAND_IO_ADDR);
		this->hwcontrol (NAND_CTL_CLRCLE);
		while ( !(readb (this->IO_ADDR_R) & 0x40));
		return;

	/* This applies to read commands */	
	default:
		/* 
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		*/
		if (!this->dev_ready) {
			udelay (this->chip_delay);
			return;
		}	
	}
	
	/* wait until command is processed */
	while (!this->dev_ready());
}

/*
 *	Get chip for selected access
 */
static inline void nand_get_chip (struct nand_chip *this, struct mtd_info *mtd, int new_state, int *erase_state)
{

	DECLARE_WAITQUEUE (wait, current);

	/* 
	 * Grab the lock and see if the device is available 
	 * For erasing, we keep the spinlock until the
	 * erase command is written. 
	*/
retry:
	spin_lock_bh (&this->chip_lock);

	if (this->state == FL_READY) {
		this->state = new_state;
		if (new_state != FL_ERASING)
			spin_unlock_bh (&this->chip_lock);
		return;
	}

	if (this->state == FL_ERASING) {
		if (new_state != FL_ERASING) {
			this->state = new_state;
			spin_unlock_bh (&this->chip_lock);
			nand_select ();	/* select in any case */
			this->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
			return;
		}
	}

	set_current_state (TASK_UNINTERRUPTIBLE);
	add_wait_queue (&this->wq, &wait);
	spin_unlock_bh (&this->chip_lock);
	schedule ();
	remove_wait_queue (&this->wq, &wait);
	goto retry;
}

/*
 * Wait for command done. This applies to erase and program only
 * Erase can take up to 400ms and program up to 20ms according to 
 * general NAND and SmartMedia specs
 *
*/
static int nand_wait(struct mtd_info *mtd, struct nand_chip *this, int state)
{

	unsigned long	timeo = jiffies;
	int	status;
	
	if (state == FL_ERASING)
		 timeo += (HZ * 400) / 1000;
	else
		 timeo += (HZ * 20) / 1000;

	spin_lock_bh (&this->chip_lock);
	this->cmdfunc (mtd, NAND_CMD_STATUS, -1, -1);

	while (time_before(jiffies, timeo)) {		
		/* Check, if we were interrupted */
		if (this->state != state) {
			spin_unlock_bh (&this->chip_lock);
			return 0;
		}
		if (this->dev_ready) {
			if (this->dev_ready ())
				break;
		}
		if (readb (this->IO_ADDR_R) & 0x40)
			break;
						
		spin_unlock_bh (&this->chip_lock);
		yield ();
		spin_lock_bh (&this->chip_lock);
	}
	status = (int) readb (this->IO_ADDR_R);
	spin_unlock_bh (&this->chip_lock);

	return status;
}

/*
 *	Nand_page_program function is used for write and writev !
 */
static int nand_write_page (struct mtd_info *mtd, struct nand_chip *this,
			    int page, int col, int last, u_char *oob_buf, int oobsel)
{
	int 	i, status;
	u_char	ecc_code[6], *oob_data;
	int	eccmode = oobsel ? this->eccmode : NAND_ECC_NONE;
	int  	*oob_config = oobconfigs[oobsel];

	/* pad oob area, if we have no oob buffer from fs-driver */
	if (!oob_buf) {
		oob_data = &this->data_buf[mtd->oobblock];
		for (i = 0; i < mtd->oobsize; i++)
			oob_data[i] = 0xff;
	} else 
		oob_data = oob_buf;
	
	/* software ecc 3 Bytes ECC / 256 Byte Data ? */
	if (eccmode == NAND_ECC_SOFT) {
		/* Read back previous written data, if col > 0 */
		if (col) {
			this->cmdfunc (mtd, NAND_CMD_READ0, 0, page);
			for (i = 0; i < col; i++)
				this->data_poi[i] = readb (this->IO_ADDR_R);
		}
		if ((col < this->eccsize) && (last >= this->eccsize)) {
			this->calculate_ecc (&this->data_poi[0], &(ecc_code[0]));
			for (i = 0; i < 3; i++)
				oob_data[oob_config[i]] = ecc_code[i];
		}
		/* Calculate and write the second ECC if we have enough data */
		if ((mtd->oobblock == 512) && (last == 512)) {
			this->calculate_ecc (&this->data_poi[256], &(ecc_code[3]));
			for (i = 3; i < 6; i++)
				oob_data[oob_config[i]] = ecc_code[i];
		} 
	} else {
		/* For hardware ECC skip ECC, if we have no full page write */
		if (eccmode != NAND_ECC_NONE && (col || last != mtd->oobblock))
			eccmode = NAND_ECC_NONE;
	}			

	/* Prepad for partial page programming !!! */
	for (i = 0; i < col; i++)
		this->data_poi[i] = 0xff;

	/* Postpad for partial page programming !!! oob is already padded */
	for (i = last; i < mtd->oobblock; i++)
		this->data_poi[i] = 0xff;

	/* Send command to begin auto page programming */
	this->cmdfunc (mtd, NAND_CMD_SEQIN, 0x00, page);

	/* Write out complete page of data, take care of eccmode */
	switch (this->eccmode) {
	/* No ecc and software ecc 3/256, write all */
	case NAND_ECC_NONE:
	case NAND_ECC_SOFT:
		for (i = 0; i < mtd->oobblock; i++) 
			writeb ( this->data_poi[i] , this->IO_ADDR_W);
		break;
		
	/* Hardware ecc 3 byte / 256 data, write first half, get ecc, then second, if 512 byte pagesize */	
	case NAND_ECC_HW3_256:		
		this->enable_hwecc (NAND_ECC_WRITE);	/* enable hardware ecc logic for write */
		for (i = 0; i < mtd->eccsize; i++) 
			writeb ( this->data_poi[i] , this->IO_ADDR_W);
		
		this->calculate_ecc (NULL, &(ecc_code[0]));
		for (i = 0; i < 3; i++)
			oob_data[oob_config[i]] = ecc_code[i];
			
		if (mtd->oobblock == 512) {
			this->enable_hwecc (NAND_ECC_WRITE);	/* enable hardware ecc logic for write*/
			for (i = mtd->eccsize; i < mtd->oobblock; i++) 
				writeb ( this->data_poi[i] , this->IO_ADDR_W);
			this->calculate_ecc (NULL, &(ecc_code[3]));
			for (i = 3; i < 6; i++)
				oob_data[oob_config[i]] = ecc_code[i];
		}
		break;
				
	/* Hardware ecc 3 byte / 512 byte data, write full page */	
	case NAND_ECC_HW3_512:	
		this->enable_hwecc (NAND_ECC_WRITE);	/* enable hardware ecc logic */
		for (i = 0; i < mtd->oobblock; i++) 
			writeb ( this->data_poi[i] , this->IO_ADDR_W);
		this->calculate_ecc (NULL, &(ecc_code[0]));
		for (i = 0; i < 3; i++)
			oob_data[oob_config[i]] = ecc_code[i];
		break;

	/* Hardware ecc 6 byte / 512 byte data, write full page */	
	case NAND_ECC_HW6_512:	
		this->enable_hwecc (NAND_ECC_WRITE);	/* enable hardware ecc logic */
		for (i = 0; i < mtd->oobblock; i++) 
			writeb ( this->data_poi[i] , this->IO_ADDR_W);
		this->calculate_ecc (NULL, &(ecc_code[0]));
		for (i = 0; i < 6; i++)
			oob_data[oob_config[i]] = ecc_code[i];
		break;
		
	default:
		printk (KERN_WARNING "Invalid NAND_ECC_MODE %d\n", this->eccmode);
		BUG();	
	}	
	
	/* Write out OOB data */
	for (i = 0; i <  mtd->oobsize; i++)
		writeb ( oob_data[i] , this->IO_ADDR_W);

	/* Send command to actually program the data */
	this->cmdfunc (mtd, NAND_CMD_PAGEPROG, -1, -1);

	/* call wait ready function */
	status = this->waitfunc (mtd, this, FL_WRITING);

	/* See if device thinks it succeeded */
	if (status & 0x01) {
		DEBUG (MTD_DEBUG_LEVEL0, "%s: " "Failed write, page 0x%08x, ", __FUNCTION__, page);
		return -EIO;
	}

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	/*
	 * The NAND device assumes that it is always writing to
	 * a cleanly erased page. Hence, it performs its internal
	 * write verification only on bits that transitioned from
	 * 1 to 0. The device does NOT verify the whole page on a
	 * byte by byte basis. It is possible that the page was
	 * not completely erased or the page is becoming unusable
	 * due to wear. The read with ECC would catch the error
	 * later when the ECC page check fails, but we would rather
	 * catch it early in the page write stage. Better to write
	 * no data than invalid data.
	 */

	/* Send command to read back the page */
	this->cmdfunc (mtd, NAND_CMD_READ0, col, page);
	/* Loop through and verify the data */
	for (i = col; i < last; i++) {
		if (this->data_poi[i] != readb (this->IO_ADDR_R)) {
			DEBUG (MTD_DEBUG_LEVEL0, "%s: " "Failed write verify, page 0x%08x ", __FUNCTION__, page);
			return -EIO;
		}
	}

	/* check, if we have a fs-supplied oob-buffer */
	if (oob_buf) {
		for (i = 0; i < mtd->oobsize; i++) {
			if (oob_data[i] != readb (this->IO_ADDR_R)) {
				DEBUG (MTD_DEBUG_LEVEL0, "%s: " "Failed write verify, page 0x%08x ", __FUNCTION__, page);
				return -EIO;
			}
		}
	} else {
		if (eccmode != NAND_ECC_NONE && !col && last == mtd->oobblock) {
			int ecc_bytes = 0;

			switch (this->eccmode) {
			case NAND_ECC_SOFT:
			case NAND_ECC_HW3_256: ecc_bytes = (mtd->oobblock == 512) ? 6 : 3; break;
			case NAND_ECC_HW3_512: ecc_bytes = 3; break;
			case NAND_ECC_HW6_512: ecc_bytes = 6; break;
			}

			for (i = 0; i < mtd->oobsize; i++)
				oob_data[i] = readb (this->IO_ADDR_R);

			for (i = 0; i < ecc_bytes; i++) {
				if (oob_data[oob_config[i]] != ecc_code[i]) {
					DEBUG (MTD_DEBUG_LEVEL0,
					       "%s: Failed ECC write "
				       "verify, page 0x%08x, " "%6i bytes were succesful\n", __FUNCTION__, page, i);
				return -EIO;
				}
			}
		}
	}
#endif
	return 0;
}

/*
*	Use NAND read ECC
*/
static int nand_read (struct mtd_info *mtd, loff_t from, size_t len, size_t * retlen, u_char * buf)
{
	return (nand_read_ecc (mtd, from, len, retlen, buf, NULL, 0));
}			   


/*
 * NAND read with ECC
 */
static int nand_read_ecc (struct mtd_info *mtd, loff_t from, size_t len,
			  size_t * retlen, u_char * buf, u_char * oob_buf, int oobsel)
{
	int j, col, page, end, ecc;
	int erase_state = 0;
	int read = 0, oob = 0, ecc_status = 0, ecc_failed = 0;
	struct nand_chip *this = mtd->priv;
	u_char *data_poi, *oob_data = oob_buf;
	u_char ecc_calc[6];
	u_char ecc_code[6];
	int	eccmode = oobsel ? this->eccmode : NAND_ECC_NONE;

	int *oob_config = oobconfigs[oobsel];
	
	DEBUG (MTD_DEBUG_LEVEL3, "nand_read_ecc: from = 0x%08x, len = %i\n", (unsigned int) from, (int) len);

	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_read_ecc: Attempt read beyond end of device\n");
		*retlen = 0;
		return -EINVAL;
	}

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd ,FL_READING, &erase_state);

	/* Select the NAND device */
	nand_select ();

	/* First we calculate the starting page */
	page = from >> this->page_shift;

	/* Get raw starting column */
	col = from & (mtd->oobblock - 1);

	end = mtd->oobblock;
	ecc = mtd->eccsize;

	/* Send the read command */
	this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page);
	
	/* Loop until all data read */
	while (read < len) {
		
		/* If we have consequent page reads, apply delay or wait for ready/busy pin */
		if (read) {
			if (!this->dev_ready) 
				udelay (this->chip_delay);
			else
				while (!this->dev_ready());	
		}

		/* 
		 * If the read is not page aligned, we have to read into data buffer
		 * due to ecc, else we read into return buffer direct
		 */
		if (!col && (len - read) >= end)  
			data_poi = &buf[read];
		else 
			data_poi = this->data_buf;

		/* get oob area, if we have no oob buffer from fs-driver */
		if (!oob_buf) {
			oob_data = &this->data_buf[end];
			oob = 0;
		} 	
			
		j = 0;
		switch (eccmode) {
		case NAND_ECC_NONE:	/* No ECC, Read in a page */		
			while (j < end)
				data_poi[j++] = readb (this->IO_ADDR_R);
			break;
			
		case NAND_ECC_SOFT:	/* Software ECC 3/256: Read in a page + oob data */
			while (j < end)
				data_poi[j++] = readb (this->IO_ADDR_R);
			this->calculate_ecc (&data_poi[0], &ecc_calc[0]);
			if (mtd->oobblock == 512)
				this->calculate_ecc (&data_poi[256], &ecc_calc[3]);
			break;	
			
		case NAND_ECC_HW3_256: /* Hardware ECC 3 byte /256 byte data: Read in first 256 byte, get ecc, */
			this->enable_hwecc (NAND_ECC_READ);	
			while (j < ecc)
				data_poi[j++] = readb (this->IO_ADDR_R);
			this->calculate_ecc (&data_poi[0], &ecc_calc[0]);	/* read from hardware */
			
			if (mtd->oobblock == 512) { /* read second, if pagesize = 512 */
				this->enable_hwecc (NAND_ECC_READ);	
				while (j < end)
					data_poi[j++] = readb (this->IO_ADDR_R);
				this->calculate_ecc (&data_poi[256], &ecc_calc[3]); /* read from hardware */
			}					
			break;						
				
		case NAND_ECC_HW3_512:	
		case NAND_ECC_HW6_512: /* Hardware ECC 3/6 byte / 512 byte data : Read in a page  */
			this->enable_hwecc (NAND_ECC_READ);	
			while (j < end)
				data_poi[j++] = readb (this->IO_ADDR_R);
			this->calculate_ecc (&data_poi[0], &ecc_calc[0]);	/* read from hardware */
			break;

		default:
			printk (KERN_WARNING "Invalid NAND_ECC_MODE %d\n", this->eccmode);
			BUG();	
		}

		/* read oobdata */
		for (j = 0; j <  mtd->oobsize; j++) 
			oob_data[oob + j] = readb (this->IO_ADDR_R);
		
		/* Skip ECC, if not active */
		if (eccmode == NAND_ECC_NONE)
			goto readdata;	
		
		/* Pick the ECC bytes out of the oob data */
		for (j = 0; j < 6; j++)
			ecc_code[j] = oob_data[oob + oob_config[j]];

		/* correct data, if neccecary */
		ecc_status = this->correct_data (&data_poi[0], &ecc_code[0], &ecc_calc[0]);
		/* check, if we have a fs supplied oob-buffer */
		if (oob_buf) { 
			oob += mtd->oobsize;
			*((int *)&oob_data[oob]) = ecc_status;
			oob += sizeof(int);
		}
		if (ecc_status == -1) {	
			DEBUG (MTD_DEBUG_LEVEL0, "nand_read_ecc: " "Failed ECC read, page 0x%08x\n", page);
			ecc_failed++;
		}
		
		if (mtd->oobblock == 512 && eccmode != NAND_ECC_HW3_512) {
			ecc_status = this->correct_data (&data_poi[256], &ecc_code[3], &ecc_calc[3]);
			if (oob_buf) {
				*((int *)&oob_data[oob]) = ecc_status;
				oob += sizeof(int);
			}
			if (ecc_status == -1) {
				DEBUG (MTD_DEBUG_LEVEL0, "nand_read_ecc: " "Failed ECC read, page 0x%08x\n", page);
				ecc_failed++;
			}
		}
readdata:
		if (col || (len - read) < end) { 
			for (j = col; j < end && read < len; j++)
				buf[read++] = data_poi[j];
		} else		
			read += mtd->oobblock;
		/* For subsequent reads align to page boundary. */
		col = 0;
		/* Increment page address */
		page++;
	}

	/* De-select the NAND device */
	nand_deselect ();

	/* Wake up anyone waiting on the device */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	wake_up (&this->wq);
	spin_unlock_bh (&this->chip_lock);

	/*
	 * Return success, if no ECC failures, else -EIO
	 * fs driver will take care of that, because
	 * retlen == desired len and result == -EIO
	 */
	*retlen = read;
	return ecc_failed ? -EIO : 0;
}

/*
 * NAND read out-of-band
 */
static int nand_read_oob (struct mtd_info *mtd, loff_t from, size_t len, size_t * retlen, u_char * buf)
{
	int i, col, page;
	int erase_state = 0;
	struct nand_chip *this = mtd->priv;

	DEBUG (MTD_DEBUG_LEVEL3, "nand_read_oob: from = 0x%08x, len = %i\n", (unsigned int) from, (int) len);

	/* Shift to get page */
	page = ((int) from) >> this->page_shift;

	/* Mask to get column */
	col = from & 0x0f;

	/* Initialize return length value */
	*retlen = 0;

	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_read_oob: Attempt read beyond end of device\n");
		*retlen = 0;
		return -EINVAL;
	}

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd , FL_READING, &erase_state);

	/* Select the NAND device */
	nand_select ();

	/* Send the read command */
	this->cmdfunc (mtd, NAND_CMD_READOOB, col, page);
	/* 
	 * Read the data, if we read more than one page
	 * oob data, let the device transfer the data !
	 */
	for (i = 0; i < len; i++) {
		buf[i] = readb (this->IO_ADDR_R);
		if ((col++ & (mtd->oobsize - 1)) == (mtd->oobsize - 1))
			udelay (this->chip_delay);
	}
	/* De-select the NAND device */
	nand_deselect ();

	/* Wake up anyone waiting on the device */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	wake_up (&this->wq);
	spin_unlock_bh (&this->chip_lock);

	/* Return happy */
	*retlen = len;
	return 0;
}

/*
*	Use NAND write ECC
*/
static int nand_write (struct mtd_info *mtd, loff_t to, size_t len, size_t * retlen, const u_char * buf)
{
	return (nand_write_ecc (mtd, to, len, retlen, buf, NULL, 0));
}			   
/*
 * NAND write with ECC
 */
static int nand_write_ecc (struct mtd_info *mtd, loff_t to, size_t len,
			   size_t * retlen, const u_char * buf, u_char * eccbuf, int oobsel)
{
	int i, page, col, cnt, ret = 0, oob = 0, written = 0;
	struct nand_chip *this = mtd->priv;

	DEBUG (MTD_DEBUG_LEVEL3, "nand_write_ecc: to = 0x%08x, len = %i\n", (unsigned int) to, (int) len);

	/* Do not allow write past end of device */
	if ((to + len) > mtd->size) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_write_oob: Attempt to write past end of page\n");
		return -EINVAL;
	}

	/* Shift to get page */
	page = ((int) to) >> this->page_shift;

	/* Get the starting column */
	col = to & (mtd->oobblock - 1);

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd, FL_WRITING, NULL);

	/* Select the NAND device */
	nand_select ();

	/* Check the WP bit */
	this->cmdfunc (mtd, NAND_CMD_STATUS, -1, -1);
	if (!(readb (this->IO_ADDR_R) & 0x80)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_write_ecc: Device is write protected!!!\n");
		ret = -EIO;
		goto out;
	}

	/* Loop until all data is written */
	while (written < len) {
		/* 
		 * Check, if we have a full page write, then we can
		 * use the given buffer, else we have to copy
		 */
		if (!col && (len - written) >= mtd->oobblock) {   
			this->data_poi = (u_char*) &buf[written];
			cnt = mtd->oobblock;
		} else {
			cnt = 0;
			for (i = col; i < len && i < mtd->oobblock; i++) {
				this->data_buf[i] = buf[written + i];
				cnt++;
			}
			this->data_poi = this->data_buf;		
		}
		/* We use the same function for write and writev !) */
		if (eccbuf) {
			ret = nand_write_page (mtd, this, page, col, cnt ,&eccbuf[oob], oobsel);
			oob += mtd->oobsize;
		} else 
			ret = nand_write_page (mtd, this, page, col, cnt, NULL, oobsel);	
		
		if (ret)
			goto out;

		/* Update written bytes count */
		written += cnt;
		/* Next write is aligned */
		col = 0;
		/* Increment page address */
		page++;
	}

out:
	/* De-select the NAND device */
	nand_deselect ();

	/* Wake up anyone waiting on the device */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	wake_up (&this->wq);
	spin_unlock_bh (&this->chip_lock);

	*retlen = written;
	return ret;
}

/*
 * NAND write out-of-band
 */
static int nand_write_oob (struct mtd_info *mtd, loff_t to, size_t len, size_t * retlen, const u_char * buf)
{
	int i, column, page, status, ret = 0;
	struct nand_chip *this = mtd->priv;

	DEBUG (MTD_DEBUG_LEVEL3, "nand_write_oob: to = 0x%08x, len = %i\n", (unsigned int) to, (int) len);

	/* Shift to get page */
	page = ((int) to) >> this->page_shift;

	/* Mask to get column */
	column = to & 0x1f;

	/* Initialize return length value */
	*retlen = 0;

	/* Do not allow write past end of page */
	if ((column + len) > mtd->oobsize) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_write_oob: Attempt to write past end of page\n");
		return -EINVAL;
	}

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd, FL_WRITING, NULL);

	/* Select the NAND device */
	nand_select ();

	/* Check the WP bit */
	this->cmdfunc (mtd, NAND_CMD_STATUS, -1, -1);
	if (!(readb (this->IO_ADDR_R) & 0x80)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_write_oob: Device is write protected!!!\n");
		ret = -EIO;
		goto out;
	}

	/* Write out desired data */
	this->cmdfunc (mtd, NAND_CMD_SEQIN, mtd->oobblock, page);
	/* prepad 0xff for partial programming */
	for (i = 0; i < column; i++)
		writeb (0xff, this->IO_ADDR_W);
	/* write data */
	for (i = 0; i < len; i++)
		writeb (buf[i], this->IO_ADDR_W);	
	/* postpad 0xff for partial programming */
	for (i = len + column; i < mtd->oobsize; i++)
		writeb (0xff, this->IO_ADDR_W);

	/* Send command to program the OOB data */
	this->cmdfunc (mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = this->waitfunc (mtd, this, FL_WRITING);

	/* See if device thinks it succeeded */
	if (status & 0x01) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_write_oob: " "Failed write, page 0x%08x\n", page);
		ret = -EIO;
		goto out;
	}
	/* Return happy */
	*retlen = len;

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	/* Send command to read back the data */
	this->cmdfunc (mtd, NAND_CMD_READOOB, column, page);

	/* Loop through and verify the data */
	for (i = 0; i < len; i++) {
		if (buf[i] != readb (this->IO_ADDR_R)) {
			DEBUG (MTD_DEBUG_LEVEL0, "nand_write_oob: " "Failed write verify, page 0x%08x\n", page);
			ret = -EIO;
			goto out;
		}
	}
#endif

out:
	/* De-select the NAND device */
	nand_deselect ();

	/* Wake up anyone waiting on the device */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	wake_up (&this->wq);
	spin_unlock_bh (&this->chip_lock);

	return ret;
}


/*
 * NAND write with iovec
 */
static int nand_writev (struct mtd_info *mtd, const struct iovec *vecs, unsigned long count, 
		loff_t to, size_t * retlen)
{
	return (nand_writev_ecc (mtd, vecs, count, to, retlen, NULL, 0));	
}

static int nand_writev_ecc (struct mtd_info *mtd, const struct iovec *vecs, unsigned long count, 
		loff_t to, size_t * retlen, u_char *eccbuf, int oobsel)
{
	int i, page, col, cnt, len, total_len, ret = 0, written = 0;
	struct nand_chip *this = mtd->priv;

	/* Calculate total length of data */
	total_len = 0;
	for (i = 0; i < count; i++)
		total_len += (int) vecs[i].iov_len;

	DEBUG (MTD_DEBUG_LEVEL3,
	       "nand_writev: to = 0x%08x, len = %i, count = %ld\n", (unsigned int) to, (unsigned int) total_len, count);

	/* Do not allow write past end of page */
	if ((to + total_len) > mtd->size) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_writev: Attempted write past end of device\n");
		return -EINVAL;
	}

	/* Shift to get page */
	page = ((int) to) >> this->page_shift;

	/* Get the starting column */
	col = to & (mtd->oobblock - 1);

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd, FL_WRITING, NULL);

	/* Select the NAND device */
	nand_select ();

	/* Check the WP bit */
	this->cmdfunc (mtd, NAND_CMD_STATUS, -1, -1);
	if (!(readb (this->IO_ADDR_R) & 0x80)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_writev: Device is write protected!!!\n");
		ret = -EIO;
		goto out;
	}

	/* Loop until all iovecs' data has been written */
	cnt = col;
	len = 0;
	
	while (count) {
		/* 
		 *  Check, if we write from offset 0 and if the tuple
		 *  gives us not enough data for a full page write. Then we
		 *  can use the iov direct, else we have to copy into
		 *  data_buf.		
		 */
		if (!cnt && (vecs->iov_len - len) >= mtd->oobblock) {
			cnt = mtd->oobblock;
			this->data_poi = (u_char *) vecs->iov_base;
			this->data_poi += len;
			len += mtd->oobblock; 
			/* Check, if we have to switch to the next tuple */
			if (len >= (int) vecs->iov_len) {
				vecs++;
				len = 0;
				count--;
			}
		} else {
			/*
			 * Read data out of each tuple until we have a full page
			 * to write or we've read all the tuples.
		 	*/
			while ((cnt < mtd->oobblock) && count) {
				if (vecs->iov_base != NULL && vecs->iov_len) {
					this->data_buf[cnt++] = ((u_char *) vecs->iov_base)[len++];
				}
				/* Check, if we have to switch to the next tuple */
				if (len >= (int) vecs->iov_len) {
					vecs++;
					len = 0;
					count--;
				}
			}	
			this->data_poi = this->data_buf;	
		}
		
		/* We use the same function for write and writev !) */
		ret = nand_write_page (mtd, this, page, col, cnt, NULL, oobsel);
		if (ret)
			goto out;

		/* Update written bytes count */
		written += (cnt - col);

		/* Reset written byte counter and column */
		col = cnt = 0;

		/* Increment page address */
		page++;
	}

out:
	/* De-select the NAND device */
	nand_deselect ();

	/* Wake up anyone waiting on the device */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	wake_up (&this->wq);
	spin_unlock_bh (&this->chip_lock);

	*retlen = written;
	return ret;
}

/*
 * NAND erase a block
 */
static int nand_erase (struct mtd_info *mtd, struct erase_info *instr)
{
	int page, len, status, pages_per_block, ret;
	struct nand_chip *this = mtd->priv;
	DECLARE_WAITQUEUE (wait, current);

	DEBUG (MTD_DEBUG_LEVEL3,
	       "nand_erase: start = 0x%08x, len = %i\n", (unsigned int) instr->addr, (unsigned int) instr->len);

	/* Start address must align on block boundary */
	if (instr->addr & (mtd->erasesize - 1)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: Unaligned address\n");
		return -EINVAL;
	}

	/* Length must align on block boundary */
	if (instr->len & (mtd->erasesize - 1)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: Length not block aligned\n");
		return -EINVAL;
	}

	/* Do not allow erase past end of device */
	if ((instr->len + instr->addr) > mtd->size) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: Erase past end of device\n");
		return -EINVAL;
	}

	/* Grab the lock and see if the device is available */
	nand_get_chip (this, mtd, FL_ERASING, NULL);

	/* Shift to get first page */
	page = (int) (instr->addr >> this->page_shift);

	/* Calculate pages in each block */
	pages_per_block = mtd->erasesize / mtd->oobblock;

	/* Select the NAND device */
	nand_select ();

	/* Check the WP bit */
	this->cmdfunc (mtd, NAND_CMD_STATUS, -1, -1);
	if (!(readb (this->IO_ADDR_R) & 0x80)) {
		DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: Device is write protected!!!\n");
		instr->state = MTD_ERASE_FAILED;
		goto erase_exit;
	}

	/* Loop through the pages */
	len = instr->len;

	instr->state = MTD_ERASING;

	while (len) {
		/* Check if we have a bad block, we do not erase bad blocks ! */
		this->cmdfunc (mtd, NAND_CMD_READOOB, NAND_BADBLOCK_POS, page);
		if (readb (this->IO_ADDR_R) != 0xff) {
			printk (KERN_WARNING "nand_erase: attempt to erase a bad block at page 0x%08x\n", page);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		/* Send commands to erase a page */
		this->cmdfunc (mtd, NAND_CMD_ERASE1, -1, page);
		this->cmdfunc (mtd, NAND_CMD_ERASE2, -1, -1);

		spin_unlock_bh (&this->chip_lock);
		status = this->waitfunc (mtd, this, FL_ERASING);

		/* Get spinlock, in case we exit */
		spin_lock_bh (&this->chip_lock);
		/* See if block erase succeeded */
		if (status & 0x01) {
			DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: " "Failed erase, page 0x%08x\n", page);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}
		
		/* Check, if we were interupted */
		if (this->state == FL_ERASING) {
			/* Increment page address and decrement length */
			len -= mtd->erasesize;
			page += pages_per_block;
		}
		/* Release the spin lock */
		spin_unlock_bh (&this->chip_lock);
erase_retry:
		spin_lock_bh (&this->chip_lock);
		/* Check the state and sleep if it changed */
		if (this->state == FL_ERASING || this->state == FL_READY) {
			/* Select the NAND device again, if we were interrupted */
			this->state = FL_ERASING;
			nand_select ();
			continue;
		} else {
			set_current_state (TASK_UNINTERRUPTIBLE);
			add_wait_queue (&this->wq, &wait);
			spin_unlock_bh (&this->chip_lock);
			schedule ();
			remove_wait_queue (&this->wq, &wait);
			goto erase_retry;
		}
	}
	instr->state = MTD_ERASE_DONE;

erase_exit:
	/* De-select the NAND device */
	nand_deselect ();
	spin_unlock_bh (&this->chip_lock);

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;;
	/* Do call back function */
	if (!ret && instr->callback)
		instr->callback (instr);

	/* The device is ready */
	spin_lock_bh (&this->chip_lock);
	this->state = FL_READY;
	spin_unlock_bh (&this->chip_lock);

	/* Return more or less happy */
	return ret;
}

/*
 * NAND sync
 */
static void nand_sync (struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	DECLARE_WAITQUEUE (wait, current);

	DEBUG (MTD_DEBUG_LEVEL3, "nand_sync: called\n");

retry:
	/* Grab the spinlock */
	spin_lock_bh (&this->chip_lock);

	/* See what's going on */
	switch (this->state) {
	case FL_READY:
	case FL_SYNCING:
		this->state = FL_SYNCING;
		spin_unlock_bh (&this->chip_lock);
		break;

	default:
		/* Not an idle state */
		add_wait_queue (&this->wq, &wait);
		spin_unlock_bh (&this->chip_lock);
		schedule ();

		remove_wait_queue (&this->wq, &wait);
		goto retry;
	}

	/* Lock the device */
	spin_lock_bh (&this->chip_lock);

	/* Set the device to be ready again */
	if (this->state == FL_SYNCING) {
		this->state = FL_READY;
		wake_up (&this->wq);
	}

	/* Unlock the device */
	spin_unlock_bh (&this->chip_lock);
}

/*
 * Scan for the NAND device
 */
int nand_scan (struct mtd_info *mtd)
{
	int i, nand_maf_id, nand_dev_id;
	struct nand_chip *this = mtd->priv;

	/* check for proper chip_delay setup, set 20us if not */
	if (!this->chip_delay)
		this->chip_delay = 20;

	/* check, if a user supplied command function given */
	if (this->cmdfunc == NULL)
		this->cmdfunc = nand_command;

	/* check, if a user supplied wait function given */
	if (this->waitfunc == NULL)
		this->waitfunc = nand_wait;

	/* Select the device */
	nand_select ();

	/* Send the command for reading device ID */
	this->cmdfunc (mtd, NAND_CMD_READID, 0x00, -1);

	/* Read manufacturer and device IDs */
	nand_maf_id = readb (this->IO_ADDR_R);
	nand_dev_id = readb (this->IO_ADDR_R);

	/* Print and store flash device information */
	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if (nand_dev_id == nand_flash_ids[i].id && !mtd->size) {
			mtd->name = nand_flash_ids[i].name;
			mtd->erasesize = nand_flash_ids[i].erasesize;
			mtd->size = (1 << nand_flash_ids[i].chipshift);
			mtd->eccsize = 256;
			if (nand_flash_ids[i].page256) {
				mtd->oobblock = 256;
				mtd->oobsize = 8;
				this->page_shift = 8;
			} else {
				mtd->oobblock = 512;
				mtd->oobsize = 16;
				this->page_shift = 9;
			}
			/* Try to identify manufacturer */
			for (i = 0; nand_manuf_ids[i].id != 0x0; i++) {
				if (nand_manuf_ids[i].id == nand_maf_id)
					break;
			}	
			printk (KERN_INFO "NAND device: Manufacture ID:"
				" 0x%02x, Chip ID: 0x%02x (%s %s)\n", nand_maf_id, nand_dev_id, 
				nand_manuf_ids[i].name , mtd->name);
			break;
		}
	}

	/* 
	 * check ECC mode, default to software
	 * if 3byte/512byte hardware ECC is selected and we have 256 byte pagesize
	 * fallback to software ECC 
	*/
	this->eccsize = 256;	/* set default eccsize */	

	switch (this->eccmode) {

	case NAND_ECC_HW3_512: 
		if (mtd->oobblock == 256) {
			printk (KERN_WARNING "512 byte HW ECC not possible on 256 Byte pagesize, fallback to SW ECC \n");
			this->eccmode = NAND_ECC_SOFT;
			this->calculate_ecc = nand_calculate_ecc;
			this->correct_data = nand_correct_data;
			break;		
		} else 
			this->eccsize = 512; /* set eccsize to 512 and fall through for function check */

	case NAND_ECC_HW3_256:
		if (this->calculate_ecc && this->correct_data && this->enable_hwecc)
			break;
		printk (KERN_WARNING "No ECC functions supplied, Hardware ECC not possible\n");
		BUG();	

	case NAND_ECC_NONE: 
		this->eccmode = NAND_ECC_NONE;
		break;

	case NAND_ECC_SOFT:	
		this->calculate_ecc = nand_calculate_ecc;
		this->correct_data = nand_correct_data;
		break;

	default:
		printk (KERN_WARNING "Invalid NAND_ECC_MODE %d\n", this->eccmode);
		BUG();	
	}	

	/* Initialize state, waitqueue and spinlock */
	this->state = FL_READY;
	init_waitqueue_head (&this->wq);
	spin_lock_init (&this->chip_lock);

	/* De-select the device */
	nand_deselect ();

	/* Print warning message for no device */
	if (!mtd->size) {
		printk (KERN_WARNING "No NAND device found!!!\n");
		return 1;
	}

	/* Fill in remaining MTD driver data */
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH | MTD_ECC;
	mtd->module = THIS_MODULE;
	mtd->ecctype = MTD_ECC_SW;
	mtd->erase = nand_erase;
	mtd->point = NULL;
	mtd->unpoint = NULL;
	mtd->read = nand_read;
	mtd->write = nand_write;
	mtd->read_ecc = nand_read_ecc;
	mtd->write_ecc = nand_write_ecc;
	mtd->read_oob = nand_read_oob;
	mtd->write_oob = nand_write_oob;
	mtd->readv = NULL;
	mtd->writev = nand_writev;
	mtd->writev_ecc = nand_writev_ecc;
	mtd->sync = nand_sync;
	mtd->lock = NULL;
	mtd->unlock = NULL;
	mtd->suspend = NULL;
	mtd->resume = NULL;

	/* Return happy */
	return 0;
}

EXPORT_SYMBOL (nand_scan);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Steven J. Hill <sjhill@cotw.com>, Thomas Gleixner <tglx@linutronix.de>");
MODULE_DESCRIPTION ("Generic NAND flash driver code");

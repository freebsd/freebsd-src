/***************************************************************************
 *
 *  drivers/s390/char/tape34xx.c
 *    common tape device discipline for 34xx tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 ****************************************************************************
 */

#include "tapedefs.h"
#include <linux/config.h>
#include <linux/version.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <asm/ccwcache.h> 
#include <asm/idals.h>  
#ifdef CONFIG_S390_TAPE_DYNAMIC
#include <asm/s390dyn.h>
#endif
#include <asm/debug.h>
#include <linux/compatmac.h>
#include "tape.h"
#include "tape34xx.h"

#define PRINTK_HEADER "T34xx:"

tape_event_handler_t tape34xx_event_handler_table[TS_SIZE][TE_SIZE] =
{
    /* {START , DONE, FAILED, ERROR, OTHER } */
	{NULL, tape34xx_unused_done, NULL, NULL, NULL},	/* TS_UNUSED */
	{NULL, tape34xx_idle_done, NULL, NULL, NULL},	/* TS_IDLE */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_DONE */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_FAILED */
	{NULL, tape34xx_block_done, NULL, NULL, NULL},		/* TS_BLOCK_INIT */
	{NULL, tape34xx_bsb_init_done, NULL, NULL, NULL},	/* TS_BSB_INIT */
	{NULL, tape34xx_bsf_init_done, NULL, NULL, NULL},	/* TS_BSF_INIT */
	{NULL, tape34xx_dse_init_done, NULL, NULL, NULL},	/* TS_DSE_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_EGA_INIT */
	{NULL, tape34xx_fsb_init_done, NULL, NULL, NULL},	/* TS_FSB_INIT */
	{NULL, tape34xx_fsf_init_done, NULL, NULL, NULL},	/* TS_FSF_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_LDI_INIT */
	{NULL, tape34xx_lbl_init_done, NULL, NULL, NULL},	/* TS_LBL_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_MSE_INIT */
	{NULL, tape34xx_nop_init_done, NULL, NULL, NULL},	/* TS_NOP_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_RBA_INIT */
	{NULL, tape34xx_rbi_init_done, NULL, NULL, NULL},	/* TS_RBI_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_RBU_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_RBL_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_RDC_INIT */
	{NULL, tape34xx_rfo_init_done, NULL, NULL, NULL},	/* TS_RFO_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_RSD_INIT */
	{NULL, tape34xx_rew_init_done, NULL, NULL, NULL},	/* TS_REW_INIT */
	{NULL, tape34xx_rew_release_init_done, NULL, NULL, NULL},	/* TS_REW_RELEASE_IMIT */
	{NULL, tape34xx_run_init_done, NULL, NULL, NULL},	/* TS_RUN_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SEN_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SID_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SNP_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SPG_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SWI_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SMR_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_SYN_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_TIO_INIT */
	{NULL, NULL, NULL, NULL, NULL},		/* TS_UNA_INIT */
	{NULL, tape34xx_wri_init_done, NULL, NULL, NULL},	/* TS_WRI_INIT */
	{NULL, tape34xx_wtm_init_done, NULL, NULL, NULL},	/* TS_WTM_INIT */
	{NULL, NULL, NULL, NULL, NULL}};        /* TS_NOT_OPER */


int
tape34xx_ioctl_overload (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;		// no additional ioctls

}

ccw_req_t *
tape34xx_write_block (const char *data, size_t count, tape_info_t * ti)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	void *mem;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xwbl nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	mem = kmalloc (count, GFP_KERNEL);
	if (!mem) {
		tape_free_request (cqr);
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xwbl nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	if (copy_from_user (mem, data, count)) {
		kfree (mem);
		tape_free_request (cqr);
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xwbl segf.");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;

	ccw->cmd_code = WRITE_CMD;
	ccw->flags = 0;
	ccw->count = count;
	set_normalized_cda (ccw, (unsigned long) mem);
	if ((ccw->cda) == 0) {
		kfree (mem);
		tape_free_request (cqr);
		return NULL;
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = mem;
	ti->userbuf = (void *) data;
	tapestate_set (ti, TS_WRI_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xwbl ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

void 
tape34xx_free_write_block (ccw_req_t * cqr, tape_info_t * ti)
{
	unsigned long lockflags;
	ccw1_t *ccw;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ccw = cqr->cpaddr;
	ccw++;
	clear_normalized_cda (ccw);
	kfree (ti->kernbuf);
	tape_free_request (cqr);
	ti->kernbuf = ti->userbuf = NULL;
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xfwb free");
#endif /* TAPE_DEBUG */
}

ccw_req_t *
tape34xx_read_block (const char *data, size_t count, tape_info_t * ti)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	void *mem;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xrbl nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	mem = kmalloc (count, GFP_KERNEL);
	if (!mem) {
		tape_free_request (cqr);
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xrbl nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;

	ccw->cmd_code = READ_FORWARD;
	ccw->flags = 0;
	ccw->count = count;
	set_normalized_cda (ccw, (unsigned long) mem);
	if ((ccw->cda) == 0) {
		kfree (mem);
		tape_free_request (cqr);
		return NULL;
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = mem;
	ti->userbuf = (void *) data;
	tapestate_set (ti, TS_RFO_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xrbl ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

ccw_req_t *
tape34xx_read_opposite (tape_info_t * ti,int novalue)
{
	ccw_req_t *cqr;
	ccw1_t *ccw;
	size_t count;
	// first, retrieve the count from the old cqr.
	cqr = ti->cqr;
	ccw = cqr->cpaddr;
	ccw++;
	count=ccw->count;
	// free old cqr.
	clear_normalized_cda (ccw);
	tape_free_request (cqr);
	// build new cqr
	cqr = tape_alloc_ccw_req (ti, 3, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xrop nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;

	ccw->cmd_code = READ_BACKWARD;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = count;
	set_normalized_cda (ccw, (unsigned long) ti->kernbuf);
	if ((ccw->cda) == 0) {
		tape_free_request (cqr);
		return NULL;
	}
	ccw++;
	ccw->cmd_code = FORSPACEBLOCK;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	ccw->cda = (unsigned long)ccw;
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 1;
	ccw->cda = (unsigned long)ccw;
	tapestate_set (ti, TS_RBA_INIT);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xrop ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

void 
tape34xx_free_read_block (ccw_req_t * cqr, tape_info_t * ti)
{
	unsigned long lockflags;
	size_t cpysize;
	ccw1_t *ccw;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ccw = cqr->cpaddr;
	ccw++;
	cpysize = ccw->count - ti->devstat.rescnt;
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
	if (copy_to_user (ti->userbuf, ti->kernbuf, cpysize)) {
#ifdef TAPE_DEBUG
	    debug_text_exception (tape_debug_area,6,"xfrb segf.");
#endif /* TAPE_DEBUG */
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	clear_normalized_cda (ccw);
	kfree (ti->kernbuf);
	tape_free_request (cqr);
	ti->kernbuf = ti->userbuf = NULL;
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xfrb free");
#endif /* TAPE_DEBUG */
}

/*
 * The IOCTL interface is implemented in the following section,
 * excepted the MTRESET, MTSETBLK which are handled by tapechar.c
 */
/*
 * MTFSF: Forward space over 'count' file marks. The tape is positioned
 * at the EOT (End of Tape) side of the file mark.
 */
ccw_req_t *
tape34xx_mtfsf (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsf parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsf nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = FORSPACEFILE;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_FSF_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xfsf ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTBSF: Backward space over 'count' file marks. The tape is positioned at
 * the EOT (End of Tape) side of the last skipped file mark.
 */
ccw_req_t *
tape34xx_mtbsf (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsf parm");
#endif /* TAPE_DEBUG */
	        return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsf nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = BACKSPACEFILE;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_BSF_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xbsf ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTFSR: Forward space over 'count' tape blocks (blocksize is set
 * via MTSETBLK.
 */
ccw_req_t *
tape34xx_mtfsr (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsr parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsr nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = FORSPACEBLOCK;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_FSB_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xfsr ccwgen");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTBSR: Backward space over 'count' tape blocks.
 * (blocksize is set via MTSETBLK.
 */
ccw_req_t *
tape34xx_mtbsr (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsr parm");
#endif /* TAPE_DEBUG */   
	        return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsr nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = BACKSPACEBLOCK;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_BSB_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xbsr ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTWEOF: Write 'count' file marks at the current position.
 */
ccw_req_t *
tape34xx_mtweof (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xweo parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xweo nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = WRITETAPEMARK;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 1;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_WTM_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xweo ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTREW: Rewind the tape.
 */
ccw_req_t *
tape34xx_mtrew (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 3, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xrew nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = REWIND;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_REW_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xrew ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTOFFL: Rewind the tape and put the drive off-line.
 * Implement 'rewind unload'
 */
ccw_req_t *
tape34xx_mtoffl (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 3, 32);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xoff nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = REWIND_UNLOAD;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = SENSE;
	ccw->flags = 0;
	ccw->count = 32;
	ccw->cda = (unsigned long) cqr->cpaddr;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_RUN_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xoff ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTNOP: 'No operation'.
 */
ccw_req_t *
tape34xx_mtnop (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 1, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xnop nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) ccw->cmd_code;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xnop ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTBSFM: Backward space over 'count' file marks.
 * The tape is positioned at the BOT (Begin Of Tape) side of the
 * last skipped file mark.
 */
ccw_req_t *
tape34xx_mtbsfm (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsm parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbsm nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = BACKSPACEFILE;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_BSF_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xbsm ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTFSFM: Forward space over 'count' file marks.
 * The tape is positioned at the BOT (Begin Of Tape) side
 * of the last skipped file mark.
 */
ccw_req_t *
tape34xx_mtfsfm (tape_info_t * ti, int count)
{
	long lockflags;
	int i;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count == 0) || (count > 510)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsm parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	cqr = tape_alloc_ccw_req (ti, 2 + count, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xfsm nomem");
#endif /* TAPE_DEBUG */	    
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	for (i = 0; i < count; i++) {
		ccw->cmd_code = FORSPACEFILE;
		ccw->flags = CCW_FLAG_CC;
		ccw->count = 0;
		ccw->cda = (unsigned long) (&(ccw->cmd_code));
		ccw++;
	}
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_FSF_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xfsm ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTEOM: positions at the end of the portion of the tape already used
 * for recordind data. MTEOM positions after the last file mark, ready for
 * appending another file.
 * MTRETEN: Retension the tape, i.e. forward space to end of tape and rewind.
 */
ccw_req_t *
tape34xx_mteom (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 4, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xeom nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = FORSPACEFILE;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = CCW_CMD_TIC;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (cqr->cpaddr);
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_FSF_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xeom ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTERASE: erases the tape.
 */
ccw_req_t *
tape34xx_mterase (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 5, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xera nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = REWIND;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = ERASE_GAP;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = DATA_SEC_ERASE;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_DSE_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xera ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTSETDENSITY: set tape density.
 */
ccw_req_t *
tape34xx_mtsetdensity (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xden nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xden ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTSEEK: seek to the specified block.
 */
ccw_req_t *
tape34xx_mtseek (tape_info_t * ti, int count)
{
	long lockflags;
	__u8 *data;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((data = kmalloc (4 * sizeof (__u8), GFP_KERNEL)) == NULL) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xsee nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	data[0] = 0x01;
	data[1] = data[2] = data[3] = 0x00;
	if (count >= 4194304) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xsee parm");
#endif /* TAPE_DEBUG */
		kfree(data);
		return NULL;
	}
	if (((tape34xx_disc_data_t *) ti->discdata)->modeset_byte & 0x08)	// IDRC on

		data[1] = data[1] | 0x80;
	data[3] += count % 256;
	data[2] += (count / 256) % 256;
	data[1] += (count / 65536);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xsee id:");
	debug_int_event (tape_debug_area,6,count);
#endif /* TAPE_DEBUG */
	cqr = tape_alloc_ccw_req (ti, 3, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xsee nomem");
#endif /* TAPE_DEBUG */
		kfree (data);
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = LOCATE;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 4;
	set_normalized_cda (ccw, (unsigned long) data);
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = data;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_LBL_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xsee ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTTELL: Tell block. Return the number of block relative to current file.
 */
ccw_req_t *
tape34xx_mttell (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	void *mem;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xtel nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	mem = kmalloc (8, GFP_KERNEL);
	if (!mem) {
		tape_free_request (cqr);
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xtel nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;

	ccw->cmd_code = READ_BLOCK_ID;
	ccw->flags = 0;
	ccw->count = 8;
	set_normalized_cda (ccw, (unsigned long) mem);
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = mem;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_RBI_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xtel ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTSETDRVBUFFER: Set the tape drive buffer code to number.
 * Implement NOP.
 */
ccw_req_t *
tape34xx_mtsetdrvbuffer (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xbuf nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xbuf ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTLOCK: Locks the tape drive door.
 * Implement NOP CCW command.
 */
ccw_req_t *
tape34xx_mtlock (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xloc nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xloc ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTUNLOCK: Unlocks the tape drive door.
 * Implement the NOP CCW command.
 */
ccw_req_t *
tape34xx_mtunlock (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xulk nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xulk ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTLOAD: Loads the tape.
 * This function is not implemented and returns NULL, which causes the Frontend to wait for a medium being loaded.
 *  The 3480/3490 type Tapes do not support a load command
 */
ccw_req_t *
tape34xx_mtload (tape_info_t * ti, int count)
{
         return NULL;
}

/*
 * MTUNLOAD: Rewind the tape and unload it.
 */
ccw_req_t *
tape34xx_mtunload (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 3, 32);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xunl nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = REWIND_UNLOAD;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ccw++;
	ccw->cmd_code = SENSE;
	ccw->flags = 0;
	ccw->count = 32;
	ccw->cda = (unsigned long) cqr->cpaddr;
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_RUN_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xunl ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTCOMPRESSION: used to enable compression.
 * Sets the IDRC on/off.
 */
ccw_req_t *
tape34xx_mtcompression (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	if ((count < 0) || (count > 1)) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xcom parm");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	if (count == 0)
		((tape34xx_disc_data_t *) ti->discdata)->modeset_byte = 0x00;		// IDRC off

	else
		((tape34xx_disc_data_t *) ti->discdata)->modeset_byte = 0x08;		// IDRC on

	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xcom nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xcom ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTSTPART: Move the tape head at the partition with the number 'count'.
 * Implement the NOP CCW command.
 */
ccw_req_t *
tape34xx_mtsetpart (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xspa nomem");
#endif /* TAPE_DEBUG */	    
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xspa ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTMKPART: .... dummy .
 * Implement the NOP CCW command.
 */
ccw_req_t *
tape34xx_mtmkpart (tape_info_t * ti, int count)
{
	long lockflags;
	ccw_req_t *cqr;
	ccw1_t *ccw;
	cqr = tape_alloc_ccw_req (ti, 2, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xnpa nomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->kernbuf = NULL;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_NOP_INIT);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xnpa ccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}

/*
 * MTIOCGET: query the tape drive status.
 */
ccw_req_t *
tape34xx_mtiocget (tape_info_t * ti, int count)
{
	return NULL;
}

/*
 * MTIOCPOS: query the tape position.
 */
ccw_req_t *
tape34xx_mtiocpos (tape_info_t * ti, int count)
{
	return NULL;
}

ccw_req_t * tape34xx_bread (struct request *req,tape_info_t* ti,int tapeblock_major) {
	ccw_req_t *cqr;
	ccw1_t *ccw;
	__u8 *data;
	int s2b = blksize_size[tapeblock_major][ti->blk_minor]/hardsect_size[tapeblock_major][ti->blk_minor];
	int realcount;
	int size,bhct = 0;
	struct buffer_head* bh;
	for (bh = req->bh; bh; bh = bh->b_reqnext) {
		if (bh->b_size > blksize_size[tapeblock_major][ti->blk_minor])
			for (size = 0; size < bh->b_size; size += blksize_size[tapeblock_major][ti->blk_minor])
				bhct++;
		else
			bhct++;
	}
	if ((data = kmalloc (4 * sizeof (__u8), GFP_ATOMIC)) == NULL) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,3,"xBREDnomem");
#endif /* TAPE_DEBUG */
		return NULL;
	}
	data[0] = 0x01;
	data[1] = data[2] = data[3] = 0x00;
	realcount=req->sector/s2b;
	if (((tape34xx_disc_data_t *) ti->discdata)->modeset_byte & 0x08)	// IDRC on

		data[1] = data[1] | 0x80;
	data[3] += realcount % 256;
	data[2] += (realcount / 256) % 256;
	data[1] += (realcount / 65536);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xBREDid:");
	debug_int_event (tape_debug_area,6,realcount);
#endif /* TAPE_DEBUG */
	cqr = tape_alloc_ccw_req (ti, 2+bhct+1, 0);
	if (!cqr) {
#ifdef TAPE_DEBUG
	        debug_text_exception (tape_debug_area,6,"xBREDnomem");
#endif /* TAPE_DEBUG */
		kfree(data);
		return NULL;
	}
	ccw = cqr->cpaddr;
	ccw->cmd_code = MODE_SET_DB;
	ccw->flags = CCW_FLAG_CC;
	ccw->count = 1;
	set_normalized_cda (ccw, (unsigned long) (&(((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)));
	if (realcount!=ti->position) {
	    ccw++;
	    ccw->cmd_code = LOCATE;
	    ccw->flags = CCW_FLAG_CC;
	    ccw->count = 4;
	    set_normalized_cda (ccw, (unsigned long) data);
	}
	ti->position=realcount+req->nr_sectors/s2b;
	for (bh=req->bh;bh!=NULL;) {
	        ccw->flags = CCW_FLAG_CC;
		if (bh->b_size >= blksize_size[tapeblock_major][ti->blk_minor]) {
			for (size = 0; size < bh->b_size; size += blksize_size[tapeblock_major][ti->blk_minor]) {
			        ccw++;
				ccw->flags = CCW_FLAG_CC;
				ccw->cmd_code = READ_FORWARD;
				ccw->count = blksize_size[tapeblock_major][ti->blk_minor];
				set_normalized_cda (ccw, __pa (bh->b_data + size));
			}
			bh = bh->b_reqnext;
		} else {	/* group N bhs to fit into byt_per_blk */
			for (size = 0; bh != NULL && size < blksize_size[tapeblock_major][ti->blk_minor];) {
				ccw++;
				ccw->flags = CCW_FLAG_DC;
				ccw->cmd_code = READ_FORWARD;
				ccw->count = bh->b_size;
				set_normalized_cda (ccw, __pa (bh->b_data));
				size += bh->b_size;
				bh = bh->b_reqnext;
			}
			if (size != blksize_size[tapeblock_major][ti->blk_minor]) {
				PRINT_WARN ("Cannot fulfill small request %d vs. %d (%ld sects)\n",
					    size,
					    blksize_size[tapeblock_major][ti->blk_minor],
					    req->nr_sectors);
				kfree(data);
				tape_free_request (cqr);
				return NULL;
			}
		}
	}
	ccw -> flags &= ~(CCW_FLAG_DC);
	ccw -> flags |= (CCW_FLAG_CC);
	ccw++;
	ccw->cmd_code = NOP;
	ccw->flags = 0;
	ccw->count = 0;
	ccw->cda = (unsigned long) (&(ccw->cmd_code));
	ti->kernbuf = data;
	ti->userbuf = NULL;
	tapestate_set (ti, TS_BLOCK_INIT);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xBREDccwg");
#endif /* TAPE_DEBUG */
	return cqr;
}
void tape34xx_free_bread (ccw_req_t* cqr,struct _tape_info_t* ti) {
    ccw1_t* ccw;
    for (ccw=(ccw1_t*)cqr->cpaddr;(ccw->flags & CCW_FLAG_CC)||(ccw->flags & CCW_FLAG_DC);ccw++) 
	if ((ccw->cmd_code == MODE_SET_DB) ||
	    (ccw->cmd_code == LOCATE) ||
	    (ccw->cmd_code == READ_FORWARD))
	    clear_normalized_cda(ccw);
    tape_free_request(cqr);
    kfree(ti->kernbuf);
    ti->kernbuf=NULL;
}

/* event handlers */
void
tape34xx_default_handler (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
    debug_text_event (tape_debug_area,6,"xdefhandle");
#endif /* TAPE_DEBUG */
	PRINT_ERR ("TAPE34XX: An unexpected Unit Check occurred.\n");
	PRINT_ERR ("TAPE34XX: Please read Documentation/s390/TAPE and report it!\n");
	PRINT_ERR ("TAPE34XX: Current state is: %s",
		   (((tapestate_get (ti) < TS_SIZE) && (tapestate_get (ti) >= 0)) ?
		    state_verbose[tapestate_get (ti)] : "->UNKNOWN STATE<-"));
	tape_dump_sense (&ti->devstat);
	ti->rc = -EIO;
	ti->wanna_wakeup=1;
        switch (tapestate_get(ti)) {
        case TS_REW_RELEASE_INIT:
            tapestate_set(ti,TS_FAILED);
            wake_up (&ti->wq);
            break;
        case TS_BLOCK_INIT:
            tapestate_set(ti,TS_FAILED);
            schedule_tapeblock_exec_IO(ti);
            break;
        default:
            tapestate_set(ti,TS_FAILED);
            wake_up_interruptible (&ti->wq);
        }      
}

void
tape34xx_unexpect_uchk_handler (tape_info_t * ti)
{
	if ((ti->devstat.ii.sense.data[0] == 0x40) &&
	    (ti->devstat.ii.sense.data[1] == 0x40) &&
	    (ti->devstat.ii.sense.data[3] == 0x43)) {
		// no tape in the drive
	        PRINT_INFO ("Drive %d not ready. No volume loaded.\n", ti->rew_minor / 2);
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,3,"xuuh nomed");
#endif /* TAPE_DEBUG */
		tapestate_set (ti, TS_FAILED);
		ti->rc = -ENOMEDIUM;
		ti->wanna_wakeup=1;
		wake_up_interruptible (&ti->wq);
	} else if ((ti->devstat.ii.sense.data[0] == 0x42) &&
		   (ti->devstat.ii.sense.data[1] == 0x44) &&
		   (ti->devstat.ii.sense.data[3] == 0x3b)) {
       	        PRINT_INFO ("Media in drive %d was changed!\n",
			    ti->rew_minor / 2);
#ifdef TAPE_DEBUG
		debug_text_event (tape_debug_area,3,"xuuh medchg");
#endif
		/* nothing to do. chan end & dev end will be reported when io is finished */
	} else {
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,3,"xuuh unexp");
	        debug_text_event (tape_debug_area,3,"state:");
	        debug_text_event (tape_debug_area,3,((tapestate_get (ti) < TS_SIZE) && 
						     (tapestate_get (ti) >= 0)) ?
				  state_verbose[tapestate_get (ti)] : 
				  "TS UNKNOWN");
#endif /* TAPE_DEBUG */
		tape34xx_default_handler (ti);
	}
}

void
tape34xx_unused_done (tape_info_t * ti)
{
    if (ti->medium_is_unloaded) {
	// A medium was inserted in the drive!
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xuui med");
#endif /* TAPE_DEBUG */
	PRINT_WARN ("A medium was inserted into the tape.\n");
	ti->medium_is_unloaded=0;
    } else {
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"unsol.irq!");
        debug_text_event (tape_debug_area,3,"dev end");
        debug_int_exception (tape_debug_area,3,ti->devinfo.irq);
#endif /* TAPE_DEBUG */
	PRINT_WARN ("Unsolicited IRQ (Device End) caught in unused state.\n");
	tape_dump_sense (&ti->devstat);
    }
}


void
tape34xx_idle_done (tape_info_t * ti)
{
    if (ti->medium_is_unloaded) {
	// A medium was inserted in the drive!
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"xuud med");
#endif /* TAPE_DEBUG */
	PRINT_WARN ("A medium was inserted into the tape.\n");
	ti->medium_is_unloaded=0;
	wake_up_interruptible (&ti->wq);
    } else {
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"unsol.irq!");
        debug_text_event (tape_debug_area,3,"dev end");
        debug_int_exception (tape_debug_area,3,ti->devinfo.irq);
#endif /* TAPE_DEBUG */
	PRINT_WARN ("Unsolicited IRQ (Device End) caught in idle state.\n");
	tape_dump_sense (&ti->devstat);
    }
}

void
tape34xx_block_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"x:bREQdone");
#endif /* TAPE_DEBUG */
	tapestate_set(ti,TS_DONE);
	schedule_tapeblock_exec_IO(ti);
}

void
tape34xx_bsf_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"bsf done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_dse_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"dse done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_fsf_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"fsf done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_fsb_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"fsb done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_bsb_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"bsb done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up (&ti->wq);
}

void
tape34xx_lbl_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"lbl done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	//s390irq_spin_unlock(tape->devinfo.irq);
	ti->wanna_wakeup=1;
	wake_up (&ti->wq);
}

void
tape34xx_nop_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"nop done..");
        debug_text_exception (tape_debug_area,6,"or rew/rel");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	//s390irq_spin_unlock(tape->devinfo.irq);
	ti->wanna_wakeup=1;
	wake_up (&ti->wq);
}

void
tape34xx_rfo_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"rfo done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up (&ti->wq);
}

void
tape34xx_rbi_init_done (tape_info_t * ti)
{
	__u8 *data;
#ifdef TAPE_DEBUG
	int i;
#endif
	tapestate_set (ti, TS_FAILED);
	data = ti->kernbuf;
	ti->rc = data[3];
	ti->rc += 256 * data[2];
	ti->rc += 65536 * (data[1] & 0x3F);
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"rbi done");
        debug_text_event (tape_debug_area,6,"data:");
	for (i=0;i<8;i++)
	    debug_int_event (tape_debug_area,6,data[i]);
#endif
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_rew_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"rew done");
#endif
	//BH: use irqsave
	//s390irq_spin_lock(tape->devinfo.irq);
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	//s390irq_spin_unlock(tape->devinfo.irq);
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_rew_release_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"rewR done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	//s390irq_spin_unlock(tape->devinfo.irq);
	ti->wanna_wakeup=1;
	wake_up (&ti->wq);
}

void
tape34xx_run_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"rew done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_wri_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"wri done");
#endif
	//BH: use irqsave
	//s390irq_spin_lock(ti->devinfo.irq);
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	//s390irq_spin_unlock(ti->devinfo.irq);
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

void
tape34xx_wtm_init_done (tape_info_t * ti)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"wtm done");
#endif
	tapestate_set (ti, TS_DONE);
	ti->rc = 0;
	ti->wanna_wakeup=1;
	wake_up_interruptible (&ti->wq);
}

/* This function analyses the tape's sense-data in case of a unit-check. If possible,
   it tries to recover from the error. Else the user is informed about the problem. */
void
tape34xx_error_recovery (tape_info_t* ti)
{
    __u8* sense=ti->devstat.ii.sense.data;
    int inhibit_cu_recovery=0;
    int cu_type=ti->discipline->cu_type;
    if ((((tape34xx_disc_data_t *) ti->discdata)->modeset_byte)&0x80) inhibit_cu_recovery=1;
    if (tapestate_get(ti)==TS_BLOCK_INIT) {
	// no recovery for block device, bottom half will retry...
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    }
    if (sense[0]&SENSE_COMMAND_REJECT)
	switch (tapestate_get(ti)) {
	case TS_BLOCK_INIT:
	case TS_DSE_INIT:
	case TS_EGA_INIT:
	case TS_WRI_INIT:
	case TS_WTM_INIT:
	    if (sense[1]&SENSE_WRITE_PROTECT) {
		// trying to write, but medium is write protected
		tape34xx_error_recovery_has_failed(ti,EACCES);
		return;
	    }
	default:
	    tape34xx_error_recovery_HWBUG(ti,1);
	    return;
	}
    // special cases for various tape-states when reaching end of recorded area
    if (((sense[0]==0x08) || (sense[0]==0x10) || (sense[0]==0x12)) &&
	((sense[1]==0x40) || (sense[1]==0x0c)))
	switch (tapestate_get(ti)) {
	case TS_FSF_INIT:
	    // Trying to seek beyond end of recorded area
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	case TS_LBL_INIT:
	    // Block could not be located.
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	case TS_RFO_INIT:
	    // Try to read beyond end of recorded area -> 0 bytes read
	    tape34xx_error_recovery_has_failed(ti,0);
	    return;
	}
    // Sensing special bits
    if (sense[0]&SENSE_BUS_OUT_CHECK) {
	tape34xx_error_recovery_do_retry(ti);
	return;
    }
    if (sense[0]&SENSE_DATA_CHECK) {
	// hardware failure, damaged tape or improper operating conditions
	switch (sense[3]) {
	case 0x23:
	    // a read data check occurred
	    if ((sense[2]&SENSE_TAPE_SYNC_MODE) ||
		(inhibit_cu_recovery)) {
		// data check is not permanent, may be recovered. 
		// We always use async-mode with cu-recovery, so this should *never* happen.
		tape34xx_error_recovery_HWBUG(ti,2);
		return;
	    } else {
		// data check is permanent, CU recovery has failed
		PRINT_WARN("Permanent read error, recovery failed!\n");
		tape34xx_error_recovery_has_failed(ti,EIO);
		return;
	    }
	case 0x25:
	    // a write data check occurred
	    if ((sense[2]&SENSE_TAPE_SYNC_MODE) ||
		(inhibit_cu_recovery)) {
		// data check is not permanent, may be recovered.
		// We always use async-mode with cu-recovery, so this should *never* happen.
		tape34xx_error_recovery_HWBUG(ti,3);
		return;
	    } else {
		// data check is permanent, cu-recovery has failed
		PRINT_WARN("Permanent write error, recovery failed!\n");
		tape34xx_error_recovery_has_failed(ti,EIO);
		return;
	    }
	case 0x26:
	    // Data Check (read opposite) occurred. We'll recover this.
	    tape34xx_error_recovery_read_opposite(ti);
	    return;
	case 0x28:
	    // The ID-Mark at the beginning of the tape could not be written. This is fatal, we'll report and exit.
	    PRINT_WARN("ID-Mark could not be written. Check your hardware!\n");
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	case 0x31:
	    // Tape void. Tried to read beyond end of device. We'll report and exit.
	    PRINT_WARN("Try to read beyond end of recorded area!\n");
	    tape34xx_error_recovery_has_failed(ti,ENOSPC);
	    return;
	case 0x41:
	    // Record sequence error. cu detected incorrect block-id sequence on tape. We'll report and exit.
	    PRINT_WARN("Illegal block-id sequence found!\n");
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	    default:
	    // well, all data checks for 3480 should result in one of the above erpa-codes. if not -> bug
	    // On 3490, other data-check conditions do exist.
		if (cu_type==0x3480) {
		    tape34xx_error_recovery_HWBUG(ti,4);
		    return;
		}
	}
    }
    if (sense[0]&SENSE_OVERRUN) {
	// A data overrun between cu and drive occurred. The channel speed is to slow! We'll report this and exit!
	switch (sense[3]) {
	case 0x40: // overrun error
	    PRINT_WARN ("Data overrun error between control-unit and drive. Use a faster channel connection, if possible! \n");
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	default:
	    // Overrun bit is set, but erpa does not show overrun error. This is a bug.
	    tape34xx_error_recovery_HWBUG(ti,5);
	    return;
	}
    }
    if (sense[1]&SENSE_RECORD_SEQUENCE_ERR) {
	switch (sense[3]) {
	case 0x41:
	    // Record sequence error. cu detected incorrect block-id sequence on tape. We'll report and exit.
	    PRINT_WARN("Illegal block-id sequence found!\n");
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	default:
	    // Record sequence error bit is set, but erpa does not show record sequence error. This is a bug.
	    tape34xx_error_recovery_HWBUG(ti,6);
	    return;
	}
    }
    // Sensing erpa codes
    switch (sense[3]) {
    case 0x00:
	// Everything is fine, but we got a unit check. Report and ignore!
	PRINT_WARN ("Non-error sense was found. Unit-check will be ignored, expect errors...\n");
	return;
    case 0x21:
	// Data streaming not operational. Cu switches to interlock mode, we reissue the command.
	PRINT_WARN ("Data streaming not operational. Switching to interlock-mode! \n");
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x22:
	// Path equipment check. Might be drive adapter error, buffer error on the lower interface, internal path not useable, or error during cartridge load.
	// All of the above are not recoverable
	PRINT_WARN ("A path equipment check occurred. One of the following conditions occurred:\n");
	PRINT_WARN ("drive adapter error,buffer error on the lower interface, internal path not useable, error during cartridge load.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x23:
	// Read data check. Should have been be covered earlier -> Bug!
	tape34xx_error_recovery_HWBUG(ti,7);
	return;
    case 0x24:
	// Load display check. Load display was command was issued, but the drive is displaying a drive check message. Can be threated as "device end".
	tape34xx_error_recovery_succeded(ti);
	return;
    case 0x25:
	// Write data check. Should have been covered earlier -> Bug!
	tape34xx_error_recovery_HWBUG(ti,8);
	return;
    case 0x26:
	// Data check (read opposite). Should have been covered earlier -> Bug!
	tape34xx_error_recovery_HWBUG(ti,9);
	return;
    case 0x27:
	// Command reject. May indicate illegal channel program or buffer over/underrun. 
	// Since all channel programms are issued by this driver and ought be correct,
	// we assume a over/underrun situaltion and retry the channel program.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x28:
	// Write id mark check. Should have beed covered earlier -> bug!
	tape34xx_error_recovery_HWBUG(ti,10);
	return;
    case 0x29:
	// Function incompatible. Either idrc is on but hardware not capable doing idrc 
	// or a perform subsystem func is issued and the cu is not online. Anyway, this 
	// cannot be recovered and is an I/O error.
	PRINT_WARN ("Function incompatible. Try to switch off idrc! \n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x2a:
	// Unsolicited environmental data. An internal counter overflows, we can ignore
	// this and reissue the cmd.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x2b:
	// Environmental data present. Indicates either unload completed ok or read buffered 
	// log command completed ok. 
	if (tapestate_get(ti)==TS_RUN_INIT) {
	    // Rewind unload completed ok.
	    tape34xx_error_recovery_succeded(ti);
	    return;
	}
	// Since we do not issue read buffered log commands, this should never occur -> bug.
	tape34xx_error_recovery_HWBUG(ti,11);
	return;
    case 0x2c:
	// Permanent equipment check. cu has tried recovery, but did not succeed. This is an
	// I/O error.
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x2d:
	// Data security erase failure.
	if (tapestate_get(ti)==TS_DSE_INIT) {
	    // report an I/O error
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	}
	// Data security erase failure, but no such command issued. This is a bug.
	tape34xx_error_recovery_HWBUG(ti,12);
	return;
    case 0x2e:
	// Not capable. This indicates either that the drive fails reading the format id mark
	// or that that format specified is not supported by the drive. We write a message and
	// return an I/O error.
	PRINT_WARN("Drive not capable processing the tape format!");
	tape34xx_error_recovery_has_failed(ti,EMEDIUMTYPE);
	return;
    case 0x2f:
	// This erpa is reserved. This is a bug.
	tape34xx_error_recovery_HWBUG(ti,13);
	return;
    case 0x30:
	// The medium is write protected, while trying to write on it. We'll report this.
	PRINT_WARN("Medium is write protected!\n");
	tape34xx_error_recovery_has_failed(ti,EACCES);
	return;
    case 0x31:
	// Tape void. Should have beed covered ealier -> bug
	tape34xx_error_recovery_HWBUG(ti,14);
	return;
    case 0x32:
	// Tension loss. We cannot recover this, it's an I/O error.
	PRINT_WARN("The drive lost tape tension.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x33:
	// Load Failure. The catridge was not inserted correctly or the tape is not threaded
	// correctly. We cannot recover this, the user has to reload the catridge.
	PRINT_WARN("Cartridge load failure. Reload the cartridge and try again.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x34:
	// Unload failure. The drive cannot maintain tape tension and control tape movement 
	// during an unload operation. 
	PRINT_WARN("Failure during cartridge unload. Please try manually.\n");
	if (tapestate_get(ti)!=TS_RUN_INIT) {
	    tape34xx_error_recovery_HWBUG(ti,15);
	    return;
	}
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x35:
	// Drive equipment check. One of the following:
	// - cu cannot recover from a drive detected error
	// - a check code message is displayed on drive message/load displays
	// - the cartridge loader does not respond correctly
	// - a failure occurs during an index, load, or unload cycle
	PRINT_WARN("Equipment check! Please check the drive and the cartridge loader.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x36:
	switch (cu_type) {
	case 0x3480:
	    // This erpa is reserved for 3480 -> BUG
	    tape34xx_error_recovery_HWBUG(ti,16);
	    return;
	case 0x3490:
	    // End of data. This is a permanent I/O error, which cannot be recovered.
	    // A read-type command has reached the end-of-data mark.
	    tape34xx_error_recovery_has_failed(ti,EIO);
	    return;
	}
    case 0x37:
	// Tape length error. The tape is shorter than reported in the beginning-of-tape data.
	PRINT_WARN("Tape length error.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x38:
	// Physical end of tape. A read/write operation reached the physical end of tape.
	if (tapestate_get(ti)==TS_WRI_INIT ||
	    tapestate_get(ti)==TS_DSE_INIT ||
	    tapestate_get(ti)==TS_EGA_INIT ||
	    tapestate_get(ti)==TS_WTM_INIT){
 	    tape34xx_error_recovery_has_failed(ti,ENOSPC);
	} else {
	    tape34xx_error_recovery_has_failed(ti,EIO);
	}
	return;
    case 0x39:
	// Backward at BOT. The drive is at BOT and is requestet to move backward.
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x3a:
	// Drive switched not ready, but the command needs the drive to be ready.
	PRINT_WARN("Drive not ready. Turn the ready/not ready switch to ready position and try again.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x3b:
	// Manual rewind or unload. This causes an I/O error.
	PRINT_WARN("Medium was rewound or unloaded manually. Expect errors! Please do only use the mtoffl and mtrew ioctl to unload tapes or rewind tapes.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
	// These erpas are reserved -> BUG
	tape34xx_error_recovery_HWBUG(ti,17);
	return;
    case 0x40:
	// Overrun error. This should have been covered earlier -> bug.
	tape34xx_error_recovery_HWBUG(ti,18);
	return;
    case 0x41:
	// Record sequence error. This should have been covered earlier -> bug.
	tape34xx_error_recovery_HWBUG(ti,19);
	return;
    case 0x42:
	// Degraded mode. A condition that can cause degraded performace is detected.
	PRINT_WARN("Subsystem is running in degraded mode. This may compromise your performace.\n");
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x43:
	// Drive not ready. Probably swith the ready/not ready switch to ready?
	PRINT_WARN("The drive is not ready. Maybe no medium in?\n");
	tape34xx_error_recovery_has_failed(ti,ENOMEDIUM);
	return;
    case 0x44:
	// Locate Block unsuccessfull. We'll report this.
	if ((tapestate_get(ti)!=TS_BLOCK_INIT) &&
	    (tapestate_get(ti)!=TS_LBL_INIT)) {
	    tape34xx_error_recovery_HWBUG(ti,20); // No locate block was issued...
	    return;
	}
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x45:
	// The drive is assigned elsewhere [to a different channel path/computer].
	PRINT_WARN("The drive is assigned elsewhere.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x46:
	// Drive not online. Drive may be switched offline, the power supply may be switched off 
	// or the drive address may not be set correctly.
	PRINT_WARN("The drive is not online.");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x47:
	// Volume fenced. cu reports volume integrity is lost! 
	PRINT_WARN("Volume fenced. The volume integrity is lost! \n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x48:
	// Log sense data and retry request. We'll do so...
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x49:
	// Bus out check. A parity check error on the bus was found.	PRINT_WARN("Bus out check. A data transfer over the bus was corrupted.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x4a:
	// Control unit erp failed. We'll report this.
	PRINT_WARN("The control unit failed recovering an I/O error.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x4b:
	// Cu and drive incompatible. The drive requests micro-program patches, which are not available on the cu.
	PRINT_WARN("The drive needs microprogram patches from the control unit, which are not available.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x4c:
	// Recovered Check-One failure. Cu develops a hardware error, but is able to recover. We'll reissue the command.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x4d:
	switch (cu_type) {
	case 0x3480:
	    // This erpa is reserved for 3480 -> bug
      	    tape34xx_error_recovery_HWBUG(ti,21);
	    return;
	case 0x3490:
	    // Resetting event received. Since the driver does not support resetting event recovery
	    // (which has to be handled by the I/O Layer), we'll report and retry our command.
	    tape34xx_error_recovery_do_retry(ti);
	    return;
	}
    case 0x4e:
	switch (cu_type) {
	case 0x3480:
	    // This erpa is reserved for 3480 -> bug.
	    tape34xx_error_recovery_HWBUG(ti,22);
	    return;
	case 0x3490:
	    // Maximum block size exeeded. This indicates, that the block to be written is larger
	    // than allowed for buffered mode. We'll report this...
	    PRINT_WARN("Maximum block size for buffered mode exceeded.\n");
	    tape34xx_error_recovery_has_failed(ti,ENOBUFS);
	    return;
	}
    case 0x4f:
	// These erpas are reserved -> bug
	tape34xx_error_recovery_HWBUG(ti,23);
	return;
    case 0x50:
	// Read buffered log (Overflow). Cu is running in extended beffered log mode, and a counter overflows.
	// This should never happen, since we're never running in extended buffered log mode -> bug.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x51:
	// Read buffered log (EOV). EOF processing occurs while the cu is in extended buffered log mode.
	// This should never happen, since we're never running in extended buffered log mode -> bug.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x52:
	// End of Volume complete. Rewind unload completed ok. We'll report to the user...
	if (tapestate_get(ti)!=TS_RUN_INIT) {
	    tape34xx_error_recovery_HWBUG(ti,24);
	    return;
	}
	tape34xx_error_recovery_succeded(ti);
	return;
    case 0x53:
	// Global command intercept. We'll have to reissue our command.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x54:
	// Channel interface recovery (temporary). This can be recovered by reissuing the command.
	tape34xx_error_recovery_do_retry(ti);
	return;
    case 0x55:
	// Channel interface recovery (permanent). This cannot be recovered, we'll inform the user.
	PRINT_WARN("A permanent channel interface error occurred.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x56:
	// Channel protocol error. This cannot be recovered.
	PRINT_WARN("A channel protocol error occurred.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x57:
	switch (cu_type) {
	case 0x3480:
	    // Attention intercept. We have to reissue the command.
	    PRINT_WARN("An attention intercept occurred, which will be recovered.\n");
	    tape34xx_error_recovery_do_retry(ti);
	    return;
	case 0x3490:
	    // Global status intercept. We have to reissue the command.
	    PRINT_WARN("An global status intercept was received, which will be recovered.\n");
	    tape34xx_error_recovery_do_retry(ti);
	    return;
	}
    case 0x58:
    case 0x59:
	// These erpas are reserved -> bug.
	tape34xx_error_recovery_HWBUG(ti,25);
	return;
    case 0x5a:
	// Tape length incompatible. The tape inserted is too long, 
	// which could cause damage to the tape or the drive.
	PRINT_WARN("Tape length incompatible [should be IBM Cartridge System Tape]. May cause damage to drive or tape.n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x5b:
	// Format 3480 XF incompatible
	if (sense[1]&SENSE_BEGINNING_OF_TAPE) {
	    // Everything is fine. The tape will be overwritten in a different format.
	    tape34xx_error_recovery_do_retry(ti);
	    return;
	}
	PRINT_WARN("Tape format is incompatible to the drive, which writes 3480-2 XF.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x5c:
	// Format 3480-2 XF incompatible
	PRINT_WARN("Tape format is incompatible to the drive. The drive cannot access 3480-2 XF volumes.\n");
	tape34xx_error_recovery_has_failed(ti,EIO);
	return;
    case 0x5d:
	// Tape length violation. 
	PRINT_WARN("Tape length violation [should be IBM Enhanced Capacity Cartridge System Tape]. May cause damage to drive or tape.\n");
	tape34xx_error_recovery_has_failed(ti,EMEDIUMTYPE);
	return;
    case 0x5e:
	// Compaction algorithm incompatible.
	PRINT_WARN("The volume is recorded using an incompatible compaction algorith, which is not supported by the control unit.\n");
	tape34xx_error_recovery_has_failed(ti,EMEDIUMTYPE);
	return;
    default:
	// Reserved erpas -> bug
	tape34xx_error_recovery_HWBUG(ti,26);
	return;
    }
}

void tape34xx_error_recovery_has_failed (tape_info_t* ti,int error_id) {
#ifdef TAPE_DEBUG
    debug_text_event (tape_debug_area,3,"xerp fail");
    debug_text_event (tape_debug_area,3,(((tapestate_get (ti) < TS_SIZE) && 
		      (tapestate_get (ti) >= 0)) ?
	state_verbose[tapestate_get (ti)] : "UNKNOWN"));
#endif
    if ((tapestate_get(ti)!=TS_UNUSED) && (tapestate_get(ti)!=TS_IDLE)) {
	tape_dump_sense(&ti->devstat);
	ti->rc = -error_id;
	ti->wanna_wakeup=1;
	switch (tapestate_get(ti)) {
	case TS_REW_RELEASE_INIT:
	case TS_RFO_INIT:
	case TS_RBA_INIT:
	    tapestate_set(ti,TS_FAILED);
	    wake_up (&ti->wq);
	    break;
	case TS_BLOCK_INIT:
	    tapestate_set(ti,TS_FAILED);
	    schedule_tapeblock_exec_IO(ti);
	    break;
	default:
	    tapestate_set(ti,TS_FAILED);
	    wake_up_interruptible (&ti->wq);
	}
    } else {
	PRINT_WARN("Recieved an unsolicited IRQ.\n");
	tape_dump_sense(&ti->devstat);
    }
}    

void tape34xx_error_recovery_succeded(tape_info_t* ti) {
#ifdef TAPE_DEBUG
    debug_text_event (tape_debug_area,3,"xerp done");
    debug_text_event (tape_debug_area,3,(((tapestate_get (ti) < TS_SIZE) && 
		      (tapestate_get (ti) >= 0)) ?
	state_verbose[tapestate_get (ti)] : "UNKNOWN"));
#endif
    if ((tapestate_get(ti)!=TS_UNUSED) && (tapestate_get(ti)!=TS_DONE)) {
	tapestate_event (ti, TE_DONE);
    } else {
	PRINT_WARN("Recieved an unsolicited IRQ.\n");
	tape_dump_sense(&ti->devstat);
    }
}

void tape34xx_error_recovery_do_retry(tape_info_t* ti) {
#ifdef TAPE_DEBUG
    debug_text_event (tape_debug_area,3,"xerp retr");
    debug_text_event (tape_debug_area,3,(((tapestate_get (ti) < TS_SIZE) && 
					  (tapestate_get (ti) >= 0)) ?
					 state_verbose[tapestate_get (ti)] : "UNKNOWN"));
#endif
    if ((tapestate_get(ti)!=TS_UNUSED) && (tapestate_get(ti)!=TS_IDLE)) {
	tape_dump_sense(&ti->devstat);
	while (do_IO (ti->devinfo.irq, ti->cqr->cpaddr, (unsigned long) ti->cqr, 0x00, ti->cqr->options));
    } else {
	PRINT_WARN("Recieved an unsolicited IRQ.\n");
	tape_dump_sense(&ti->devstat);
    }
}
    
void 
tape34xx_error_recovery_read_opposite (tape_info_t* ti) {
    switch (tapestate_get(ti)) {
    case TS_RFO_INIT:
	// We did read forward, but the data could not be read *correctly*.
	// We will read backward and then skip forward again.
	ti->cqr=tape34xx_read_opposite(ti,0);
	if (ti->cqr==NULL)
	    tape34xx_error_recovery_has_failed(ti,EIO);
	else
	    tape34xx_error_recovery_do_retry(ti);
	break;
    case TS_RBA_INIT:
	// We tried to read forward and backward, but hat no success -> failed.
	tape34xx_error_recovery_has_failed(ti,EIO);
	break;
    case TS_BLOCK_INIT:
	tape34xx_error_recovery_do_retry(ti);
	break;
    default:
	PRINT_WARN("read_opposite_recovery_called_with_state:%s\n",
		   (((tapestate_get (ti) < TS_SIZE) && 
		     (tapestate_get (ti) >= 0)) ?
		    state_verbose[tapestate_get (ti)] : "UNKNOWN"));
    }
}

void 
tape34xx_error_recovery_HWBUG (tape_info_t* ti,int condno) {
    devstat_t* stat=&ti->devstat;
    PRINT_WARN("An unexpected condition #%d was caught in tape error recovery.\n",condno);
    PRINT_WARN("Please report this incident.\n");
    PRINT_WARN("State of the tape:%s\n",
	       (((tapestate_get (ti) < TS_SIZE) && 
		 (tapestate_get (ti) >= 0)) ?
		state_verbose[tapestate_get (ti)] : "UNKNOWN"));
    PRINT_INFO ("Sense data: %02X%02X%02X%02X %02X%02X%02X%02X "
		" %02X%02X%02X%02X %02X%02X%02X%02X \n",
		stat->ii.sense.data[0], stat->ii.sense.data[1],
		stat->ii.sense.data[2], stat->ii.sense.data[3],
		stat->ii.sense.data[4], stat->ii.sense.data[5],
		stat->ii.sense.data[6], stat->ii.sense.data[7],
		stat->ii.sense.data[8], stat->ii.sense.data[9],
		stat->ii.sense.data[10], stat->ii.sense.data[11],
		stat->ii.sense.data[12], stat->ii.sense.data[13],
		stat->ii.sense.data[14], stat->ii.sense.data[15]);
    PRINT_INFO ("Sense data: %02X%02X%02X%02X %02X%02X%02X%02X "
		" %02X%02X%02X%02X %02X%02X%02X%02X \n",
		stat->ii.sense.data[16], stat->ii.sense.data[17],
		stat->ii.sense.data[18], stat->ii.sense.data[19],
		stat->ii.sense.data[20], stat->ii.sense.data[21],
		stat->ii.sense.data[22], stat->ii.sense.data[23],
		stat->ii.sense.data[24], stat->ii.sense.data[25],
		stat->ii.sense.data[26], stat->ii.sense.data[27],
		stat->ii.sense.data[28], stat->ii.sense.data[29],
		stat->ii.sense.data[30], stat->ii.sense.data[31]);
    tape34xx_error_recovery_has_failed(ti,EIO);
}

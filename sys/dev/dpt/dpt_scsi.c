/**
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * dpt_scsi.c: SCSI dependant code for the DPT driver
 *
 * credits:	Assisted by Mike Neuffer in the early low level DPT code
 *		Thanx to Mark Salyzyn of DPT for his assistance.
 *		Special thanx to Justin Gibbs for invaluable help in
 *		making this driver look and work like a FreeBSD component.
 *		Last but not least, many thanx to UCB and the FreeBSD
 *		team for creating and maintaining such a wonderful O/S.
 *
 * TODO: * Add EISA and ISA probe code.
 *	     * Add driver-level RSID-0. This will allow interoperability with
 *	       NiceTry, M$-Doze, Win-Dog, Slowlaris, etc. in recognizing RAID
 *	       arrays that span controllers (Wow!).
 */

/**
 * IMPORTANT:
 *	There are two critical section "levels" used in this driver:
 *	splcam() and splsoftcam().  Splcam() protects us from re-entrancy
 *	from both our software and hardware interrupt handler.  Splsoftcam()
 *	protects us only from our software interrupt handler.  The two
 *	main data structures that need protection are the submitted and
 *	completed queue.
 *
 *	There are three places where the submitted queue is accessed:
 *
 *       1.  dpt_run_queue        inserts into the queue
 *       2.  dpt_intr             removes from the queue
 *       3   dpt_handle_timeouts  potentially removes from the queue.
 *
 *	There are three places where the the completed queue is accessed:
 *       1.  dpt_intr()            inserts into the queue
 *       2.  dpt_sintr()           removes from the queue
 *       3.  dpt_handle_timeouts   potentially inserts into the queue
 */

#ident "$Id: dpt_scsi.c,v 1.29 1998/01/21 04:32:08 ShimonR Exp $"
#define _DPT_C_

#include "opt_dpt.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/acct.h>
#include <sys/queue.h>

#include <machine/endian.h>
#include <machine/ipl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/reboot.h>

#include <sys/dpt.h>

#ifdef INLINE
#undef INLINE
#endif

#define INLINE	__inline
#define INLINE_Q

/* Function Prototypes */

static INLINE u_int32_t dpt_inl(dpt_softc_t * dpt, u_int32_t offset);
static INLINE u_int8_t dpt_inb(dpt_softc_t * dpt, u_int32_t offset);
static INLINE void 
dpt_outb(dpt_softc_t * dpt, u_int32_t offset,
	 u_int8_t value);
static INLINE void 
dpt_outl(dpt_softc_t * dpt, u_int32_t offset,
	 u_int32_t value);
static INLINE_Q void dpt_Qpush_free(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE_Q dpt_ccb_t *dpt_Qpop_free(dpt_softc_t * dpt);
static INLINE_Q void dpt_Qadd_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE_Q void dpt_Qpush_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE_Q void 
dpt_Qremove_waiting(dpt_softc_t * dpt,
		    dpt_ccb_t * ccb);
static INLINE_Q void 
dpt_Qadd_submitted(dpt_softc_t * dpt,
		   dpt_ccb_t * ccb);
static INLINE_Q void 
dpt_Qremove_submitted(dpt_softc_t * dpt,
		      dpt_ccb_t * ccb);
static INLINE_Q void 
dpt_Qadd_completed(dpt_softc_t * dpt,
		   dpt_ccb_t * ccb);
static INLINE_Q void 
dpt_Qremove_completed(dpt_softc_t * dpt,
		      dpt_ccb_t * ccb);
static int 
dpt_send_eata_command(dpt_softc_t * dpt,
		      eata_ccb_t * cmd_block,
		      u_int8_t command,
		      int32_t retries,
		      u_int8_t ifc, u_int8_t code,
		      u_int8_t code2);
static INLINE int 
dpt_send_immediate(dpt_softc_t * dpt,
		   eata_ccb_t * cmd_block,
		   u_int8_t ifc, u_int8_t code,
		   u_int8_t code2);
static INLINE int dpt_just_reset(dpt_softc_t * dpt);
static INLINE int dpt_raid_busy(dpt_softc_t * dpt);
static INLINE void dpt_sched_queue(dpt_softc_t * dpt);

#ifdef DPT_MEASURE_PERFORMANCE
static void 
dpt_IObySize(dpt_softc_t * dpt, dpt_ccb_t * ccb,
	     int op, int index);
#endif

static void     dpt_swi_register(void *);

#ifdef DPT_HANDLE_TIMEOUTS
static void     dpt_handle_timeouts(dpt_softc_t * dpt);
static void     dpt_timeout(void *dpt);
#endif

#ifdef DPT_LOST_IRQ
static void     dpt_irq_timeout(void *dpt);
#endif

typedef struct scsi_inquiry_data s_inq_data_t;


static int 
dpt_scatter_gather(dpt_softc_t * dpt, dpt_ccb_t * ccb,
		   u_int32_t data_length,
		   caddr_t data);
static int      dpt_alloc_freelist(dpt_softc_t * dpt);
static void     dpt_run_queue(dpt_softc_t * dpt, int requests);
static void     dpt_complete(dpt_softc_t * dpt);
static int 
dpt_process_completion(dpt_softc_t * dpt,
		       dpt_ccb_t * ccb);
static void 
dpt_set_target(int redo, dpt_softc_t * dpt,
	       u_int8_t bus, u_int8_t target, u_int8_t lun, int mode,
	       u_int16_t length, u_int16_t offset, dpt_ccb_t * ccb);
static void 
dpt_target_ccb(dpt_softc_t * dpt, int bus, u_int8_t target, u_int8_t lun,
	       dpt_ccb_t * ccb, int mode, u_int8_t command,
	       u_int16_t length, u_int16_t offset);
static void     dpt_target_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb);
static void     dpt_user_cmd_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb);



u_int8_t        dpt_blinking_led(dpt_softc_t * dpt);
int 
dpt_user_cmd(dpt_softc_t * dpt, eata_pt_t * user_cmd,
	     caddr_t cmdarg, int minor_no);
void            dpt_detect_cache(dpt_softc_t * dpt);
void            dpt_shutdown(int howto, void *dpt);
void            hex_dump(u_int8_t * data, int length, char *name, int no);
char           *i2bin(unsigned int no, int length);
dpt_conf_t     *
dpt_get_conf(dpt_softc_t * dpt, u_int8_t page, u_int8_t target,
	     u_int8_t size, int extent);
dpt_inq_t      *dpt_get_board_data(dpt_softc_t * dpt, u_int32_t target_id);
int             dpt_setup(dpt_softc_t * dpt, dpt_conf_t * conf);
int             dpt_attach(dpt_softc_t * dpt);
int32_t         dpt_scsi_cmd(struct scsi_xfer * xs);
void            dptminphys(struct buf * bp);
void            dpt_sintr(void);
void            dpt_intr(void *arg);
char           *scsi_cmd_name(u_int8_t cmd);
dpt_rb_t 
dpt_register_buffer(int unit,
		    u_int8_t channel,
		    u_int8_t target,
		    u_int8_t lun,
		    u_int8_t mode,
		    u_int16_t length,
		    u_int16_t offset,
		    dpt_rec_buff callback,
		    dpt_rb_op_t op);
int 
dpt_send_buffer(int unit,
		u_int8_t channel,
		u_int8_t target,
		u_int8_t lun,
		u_int8_t mode,
		u_int16_t length,
		u_int16_t offset,
		void *data,
		buff_wr_done callback);

extern void     (*ihandlers[32]) __P((void));

u_long          dpt_unit;	/* This one is kernel-related, do not touch! */

/* The linked list of softc structures */
TAILQ_HEAD(, dpt_softc) dpt_softc_list = TAILQ_HEAD_INITIALIZER(dpt_softc_list);

/*
 * These will have to be setup by parameters passed at boot/load time. For
 * perfromance reasons, we make them constants for the time being.
 */
#define	dpt_min_segs	DPT_MAX_SEGS
#define	dpt_max_segs	DPT_MAX_SEGS

static struct scsi_adapter dpt_switch =
{
	dpt_scsi_cmd,
	dptminphys,
	NULL,
	NULL,
	NULL,
	"dpt",
	{0, 0}
};

static struct scsi_device dpt_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"dpt",
	0,
	{0, 0}
};

/* Software Interrupt Vector */

static void
dpt_swi_register(void *unused)
{
	ihandlers[SWI_CAMBIO] = dpt_sintr;
}

SYSINIT(dpt_camswi, SI_SUB_DRIVERS, SI_ORDER_FIRST, dpt_swi_register, NULL)
/* These functions allows us to do memory mapped I/O, if hardware supported. */

	static INLINE   u_int8_t
	                dpt_inb(dpt_softc_t * dpt, u_int32_t offset)
{
	u_int8_t        result;

	if (dpt->v_membase != NULL) {
		result = dpt->v_membase[offset];
	} else {
		result = inb(dpt->io_base + offset);
	}
	return (result);
}

static INLINE   u_int32_t
dpt_inl(dpt_softc_t * dpt, u_int32_t offset)
{
	u_int32_t       result;

	if (dpt->v_membase != NULL) {
		result = *(volatile u_int32_t *) (&dpt->v_membase[offset]);
	} else {
		result = inl(dpt->io_base + offset);
	}
	return (result);
}

static INLINE void
dpt_outb(dpt_softc_t * dpt, u_int32_t offset, u_int8_t value)
{
	if (dpt->v_membase != NULL) {
		dpt->v_membase[offset] = value;
	} else {
		outb(dpt->io_base + offset, value);
	}
}

static INLINE void
dpt_outl(dpt_softc_t * dpt, u_int32_t offset, u_int32_t value)
{
	if (dpt->v_membase != NULL) {
		*(volatile u_int32_t *) (&dpt->v_membase[offset]) = value;
	} else {
		outl(dpt->io_base + offset, value);
	}
}

static INLINE void
dpt_sched_queue(dpt_softc_t * dpt)
{
	if (dpt->state & DPT_HA_QUIET) {
		printf("dpt%d: Under Quiet Busses Condition.  "
		       "No Commands are submitted\n", dpt->unit);
		return;
	}
	setsoftcambio();
}

static INLINE int
dpt_wait(dpt_softc_t * dpt, u_int8_t bits, u_int8_t state)
{
	int             i;
	u_int8_t        c;

	for (i = 0; i < 20000; i++) {	/* wait 20ms for not busy */
		c = dpt_inb(dpt, HA_RSTATUS) & bits;
		if (c == state)
			return (0);
		else
			DELAY(50);
	}
	return (-1);
}

static INLINE int
dpt_just_reset(dpt_softc_t * dpt)
{
	if ((dpt_inb(dpt, 2) == 'D')
	    && (dpt_inb(dpt, 3) == 'P')
	    && (dpt_inb(dpt, 4) == 'T')
	    && (dpt_inb(dpt, 5) == 'H'))
		return (1);
	else
		return (0);
}

static INLINE int
dpt_raid_busy(dpt_softc_t * dpt)
{
	if ((dpt_inb(dpt, 0) == 'D')
	    && (dpt_inb(dpt, 1) == 'P')
	    && (dpt_inb(dpt, 2) == 'T'))
		return (1);
	else
		return (0);
}

/**
 * Build a Command Block for target mode READ/WRITE BUFFER,
 * with the ``sync'' bit ON.
 *
 * Although the length and offset are 24 bit fields in the command, they cannot
 * exceed 8192 bytes, so we take them as short integers andcheck their range.
 *  If they are sensless, we round them to zero offset, maximum length and complain.
 */

static void
dpt_target_ccb(dpt_softc_t * dpt, int bus, u_int8_t target, u_int8_t lun,
	       dpt_ccb_t * ccb, int mode, u_int8_t command,
	       u_int16_t length, u_int16_t offset)
{
	eata_ccb_t     *cp;
	int             ospl;

	if ((length + offset) > DPT_MAX_TARGET_MODE_BUFFER_SIZE) {
		printf("dpt%d:  Length of %d, and offset of %d are wrong\n",
		       dpt->unit, length, offset);
		length = DPT_MAX_TARGET_MODE_BUFFER_SIZE;
		offset = 0;
	}
	ccb->xs = NULL;
	ccb->flags = 0;
	ccb->state = DPT_CCB_STATE_NEW;
	ccb->std_callback = (ccb_callback) dpt_target_done;
	ccb->wrbuff_callback = NULL;

	cp = &ccb->eata_ccb;
	cp->CP_OpCode = EATA_CMD_DMA_SEND_CP;
	cp->SCSI_Reset = 0;
	cp->HBA_Init = 0;
	cp->Auto_Req_Sen = 1;
	cp->cp_id = target;
	cp->DataIn = 1;
	cp->DataOut = 0;
	cp->Interpret = 0;
	cp->reqlen = htonl(sizeof(struct scsi_sense_data));
	cp->cp_statDMA = htonl(vtophys(&cp->cp_statDMA));
	cp->cp_reqDMA = htonl(vtophys(&cp->cp_reqDMA));
	cp->cp_viraddr = (u_int32_t) & ccb;

	cp->cp_msg[0] = HA_IDENTIFY_MSG | HA_DISCO_RECO;

	cp->cp_scsi_cmd = command;
	cp->cp_cdb[1] = (u_int8_t) (mode & SCSI_TM_MODE_MASK);
	cp->cp_lun = lun;	/* Order is important here! */
	cp->cp_cdb[2] = 0x00;	/* Buffer Id, only 1 :-( */
	cp->cp_cdb[3] = (length >> 16) & 0xFF;	/* Buffer offset MSB */
	cp->cp_cdb[4] = (length >> 8) & 0xFF;
	cp->cp_cdb[5] = length & 0xFF;
	cp->cp_cdb[6] = (length >> 16) & 0xFF;	/* Length MSB */
	cp->cp_cdb[7] = (length >> 8) & 0xFF;
	cp->cp_cdb[8] = length & 0xFF;	/* Length LSB */
	cp->cp_cdb[9] = 0;	/* No sync, no match bits */

	/**
	 * This could be optimized to live in dpt_register_buffer.
	 *  We keep it here, just in case the kernel decides to reallocate pages
	 */
	if (dpt_scatter_gather(dpt, ccb, DPT_RW_BUFFER_SIZE,
			       dpt->rw_buffer[bus][target][lun])) {
		printf("dpt%d: Failed to setup Scatter/Gather for Target-Mode buffer\n",
		       dpt->unit);
	}
}

/* Setup a target mode READ command */

#define cmd_ct dpt->performance.command_count[(int)ccb->eata_ccb.cp_scsi_cmd];

static void
dpt_set_target(int redo, dpt_softc_t * dpt,
	       u_int8_t bus, u_int8_t target, u_int8_t lun, int mode,
	       u_int16_t length, u_int16_t offset, dpt_ccb_t * ccb)
{
	int             ospl;

#ifdef DPT_MEASURE_PERFORMANCE
	struct timeval  now;
#endif

	if (dpt->target_mode_enabled) {
		ospl = splcam();

		if (!redo)
			dpt_target_ccb(dpt, bus, target, lun, ccb, mode,
				       SCSI_TM_READ_BUFFER, length, offset);

		ccb->transaction_id = ++dpt->commands_processed;

#ifdef DPT_MEASURE_PERFORMANCE
		++cmd_ct;
		microtime(&now);
		ccb->command_started = now;
#endif
		dpt_Qadd_waiting(dpt, ccb);
		dpt_sched_queue(dpt);

		splx(ospl);
	} else {
		printf("dpt%d:  Target Mode Request, but Target Mode is OFF\n",
		       dpt->unit);
	}
}

/**
 * Schedule a buffer to be sent to another target.
 * The work will be scheduled and the callback provided will be called when the work is
 * actually done.
 *
 * Please NOTE:  ``Anyone'' can send a buffer, but only registered clients get notified
                 of receipt of buffers.
 */

int
dpt_send_buffer(int unit,
		u_int8_t channel,
		u_int8_t target,
		u_int8_t lun,
		u_int8_t mode,
		u_int16_t length,
		u_int16_t offset,
		void *data,
		buff_wr_done callback)
{
	dpt_softc_t    *dpt;
	dpt_ccb_t      *ccb = NULL;
	int             ospl;
#ifdef DPT_MEASURE_PERFORMANCE
	struct timeval  now;
#endif

	/* This is an external call.  Be a bit paranoid */
	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     dpt != NULL;
	     dpt = TAILQ_NEXT(dpt, links)) {
		if (dpt->unit == unit)
			goto valid_unit;
	}

	return (INVALID_UNIT);

valid_unit:

	if (dpt->target_mode_enabled) {
		if ((channel >= dpt->channels) || (target > dpt->max_id) ||
		    (lun > dpt->max_lun)) {
			return (INVALID_SENDER);
		}
		if ((dpt->rw_buffer[channel][target][lun] == NULL) ||
		    (dpt->buffer_receiver[channel][target][lun] == NULL))
			return (NOT_REGISTERED);

		ospl = splsoftcam();
		/* Process the free list */
		if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
			printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
			       "             Please try later\n",
			       dpt->unit);
			splx(ospl);
			return (NO_RESOURCES);
		}
		/* Now grab the newest CCB */
		if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
			splx(ospl);
			panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
		}
		splx(ospl);

		bcopy(dpt->rw_buffer[channel][target][lun] + offset, data, length);
		dpt_target_ccb(dpt, channel, target, lun, ccb, mode, SCSI_TM_WRITE_BUFFER,
			       length, offset);
		ccb->std_callback = (ccb_callback) callback;	/* A hack.  Potential
								 * trouble */

		ospl = splcam();
		ccb->transaction_id = ++dpt->commands_processed;

#ifdef DPT_MEASURE_PERFORMANCE
		++cmd_ct;
		microtime(&now);
		ccb->command_started = now;
#endif
		dpt_Qadd_waiting(dpt, ccb);
		dpt_sched_queue(dpt);

		splx(ospl);
		return (0);
	}
	return (DRIVER_DOWN);
}

static void
dpt_target_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb)
{
	int             ospl = splsoftcam();
	eata_ccb_t     *cp;

	cp = &ccb->eata_ccb;

	/**
	 * Remove the CCB from the waiting queue.
	 *  We do NOT put it back on the free, etc., queues as it is a special
	 * ccb, owned by the dpt_softc of this unit.
	 */
	ospl = splsoftcam();
	dpt_Qremove_completed(dpt, ccb);
	splx(ospl);

#define br_channel           (ccb->eata_ccb.cp_channel)
#define br_target            (ccb->eata_ccb.cp_id)
#define br_lun               (ccb->eata_ccb.cp_LUN)
#define br_index	     [br_channel][br_target][br_lun]
#define read_buffer_callback (dpt->buffer_receiver br_index )
#define	read_buffer	     (dpt->rw_buffer[br_channel][br_target][br_lun])
#define cb(offset)           (ccb->eata_ccb.cp_cdb[offset])
#define br_offset            ((cb(3) << 16) | (cb(4) << 8) | cb(5))
#define br_length            ((cb(6) << 16) | (cb(7) << 8) | cb(8))

	/* Different reasons for being here, you know... */
	switch (ccb->eata_ccb.cp_scsi_cmd) {
	case SCSI_TM_READ_BUFFER:
		if (read_buffer_callback != NULL) {
			/* This is a buffer generated by a kernel process */
			read_buffer_callback(dpt->unit, br_channel,
					     br_target, br_lun,
					     read_buffer,
					     br_offset, br_length);
		} else {
			/*
			 * This is a buffer waited for by a user (sleeping)
			 * command
			 */
			wakeup(ccb);
		}

		/* We ALWAYS re-issue the same command; args are don't-care  */
		dpt_set_target(1, 0, 0, 0, 0, 0, 0, 0, 0);
		break;

	case SCSI_TM_WRITE_BUFFER:
		(ccb->wrbuff_callback) (dpt->unit, br_channel, br_target,
					br_offset, br_length,
					br_lun, ccb->status_packet.hba_stat);
		break;
	default:
		printf("dpt%d:  %s is an unsupported command for target mode\n",
		       dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd));
	}
	ospl = splsoftcam();
	dpt->target_ccb[br_channel][br_target][br_lun] = NULL;
	dpt_Qpush_free(dpt, ccb);
	splx(ospl);

}


/**
 * Use this function to register a client for a buffer read target operation.
 * The function you register will be called every time a buffer is received
 * by the target mode code.
 */

dpt_rb_t
dpt_register_buffer(int unit,
		    u_int8_t channel,
		    u_int8_t target,
		    u_int8_t lun,
		    u_int8_t mode,
		    u_int16_t length,
		    u_int16_t offset,
		    dpt_rec_buff callback,
		    dpt_rb_op_t op)
{
	dpt_softc_t    *dpt;
	dpt_ccb_t      *ccb = NULL;
	int             ospl;

	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     dpt != NULL;
	     dpt = TAILQ_NEXT(dpt, links)) {
		if (dpt->unit == unit)
			goto valid_unit;
	}

	return (INVALID_UNIT);

valid_unit:

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		return (DRIVER_DOWN);

	if ((channel > (dpt->channels - 1)) || (target > (dpt->max_id - 1)) ||
	    (lun > (dpt->max_lun - 1)))
		return (INVALID_SENDER);

	if (dpt->buffer_receiver[channel][target][lun] == NULL) {
		if (op == REGISTER_BUFFER) {
			/* Assign the requested callback */
			dpt->buffer_receiver[channel][target][lun] = callback;
			/* Get a CCB */
			ospl = splsoftcam();

			/* Process the free list */
			if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
				printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
				       "             Please try later\n",
				       dpt->unit);
				splx(ospl);
				return (NO_RESOURCES);
			}
			/* Now grab the newest CCB */
			if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
				splx(ospl);
				panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
			}
			splx(ospl);

			/* Clean up the leftover of the previous tenant */
			ccb->status = DPT_CCB_STATE_NEW;
			dpt->target_ccb[channel][target][lun] = ccb;

			dpt->rw_buffer[channel][target][lun] = malloc(DPT_RW_BUFFER_SIZE,
							M_DEVBUF, M_NOWAIT);
			if (dpt->rw_buffer[channel][target][lun] == NULL) {
				printf("dpt%d: Failed to allocate Target-Mode buffer\n",
				       dpt->unit);
				ospl = splsoftcam();
				dpt_Qpush_free(dpt, ccb);
				splx(ospl);
				return (NO_RESOURCES);
			}
			dpt_set_target(0, dpt, channel, target, lun, mode, length, offset, ccb);
			return (SUCCESSFULLY_REGISTERED);
		} else
			return (NOT_REGISTERED);
	} else {
		if (op == REGISTER_BUFFER) {
			if (dpt->buffer_receiver[channel][target][lun] == callback)
				return (ALREADY_REGISTERED);
			else
				return (REGISTERED_TO_ANOTHER);
		} else {
			if (dpt->buffer_receiver[channel][target][lun] == callback) {
				dpt->buffer_receiver[channel][target][lun] = NULL;
				ospl = splsoftcam();
				dpt_Qpush_free(dpt, ccb);
				splx(ospl);
				free(dpt->rw_buffer[channel][target][lun], M_DEVBUF);
				return (SUCCESSFULLY_REGISTERED);
			} else
				return (INVALID_CALLBACK);
		}

	}
}

/**
 * This routine will try to send an EATA command to the DPT HBA.
 * It will, by default, try AHZ times, waiting 10ms between tries.
 * It returns 0 on success and 1 on failure.
 * It assumes the caller protects it with splbio() or some such.
 *
 * IMPORTANT:  We do NOT protect the ports from multiple access in here.
 *             You are expected to do it in the calling routine.
 *             Here, we cannot have any clue as to the scope of your work.
 */

static int
dpt_send_eata_command(dpt_softc_t * dpt, eata_ccb_t * cmd_block,
		      u_int8_t command, int32_t retries,
		      u_int8_t ifc, u_int8_t code, u_int8_t code2)
{
	int32_t         loop;
	u_int8_t        result;
	u_int32_t       test;
	u_int32_t       swapped_cmdaddr;

	if (!retries)
		retries = 1000;

	/*
	 * I hate this polling nonsense. Wish there was a way to tell the DPT
	 * to go get commands at its own pace,  or to interrupt when ready.
	 * In the mean time we will measure how many itterations it really
	 * takes.
	 */
	for (loop = 0; loop < retries; loop++) {
		if ((dpt_inb(dpt, HA_RAUXSTAT) & HA_ABUSY) == 0)
			break;
		else
			DELAY(50);
	}

	if (loop < retries) {
#ifdef DPT_MEASURE_PERFORMANCE
		if (loop > dpt->performance.max_eata_tries)
			dpt->performance.max_eata_tries = loop;

		if (loop < dpt->performance.min_eata_tries)
			dpt->performance.min_eata_tries = loop;
#endif
	} else {
#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.command_too_busy;
#endif
		return (1);
	}

	if (cmd_block != NULL) {
		swapped_cmdaddr = vtophys(cmd_block);

#if (BYTE_ORDER == BIG_ENDIAN)
		swapped_cmdaddr = ((swapped_cmdaddr >> 24) & 0xFF)
			| ((swapped_cmdaddr >> 16) & 0xFF)
			| ((swapped_cmdaddr >> 8) & 0xFF)
			| (swapped_cmdaddr & 0xFF);
#endif
	} else {
		swapped_cmdaddr = 0;
	}
	/* And now the address */
	dpt_outl(dpt, HA_WDMAADDR, swapped_cmdaddr);

	if (command == EATA_CMD_IMMEDIATE) {
		if (cmd_block == NULL) {
			dpt_outb(dpt, HA_WCODE2, code2);
			dpt_outb(dpt, HA_WCODE, code);
		}
		dpt_outb(dpt, HA_WIFC, ifc);
	}
	dpt_outb(dpt, HA_WCOMMAND, command);

	return (0);
}

/**
 * Send a command for immediate execution by the DPT
 * See above function for IMPORTANT notes.
 */

static INLINE int
dpt_send_immediate(dpt_softc_t * dpt, eata_ccb_t * cmd_block,
		   u_int8_t ifc, u_int8_t code, u_int8_t code2)
{
	return (dpt_send_eata_command(dpt, cmd_block, EATA_CMD_IMMEDIATE,
				  /* retries */ 1000000, ifc, code, code2));
}

/* Return the state of the blinking DPT LED's */
u_int8_t
dpt_blinking_led(dpt_softc_t * dpt)
{
	int             ndx;
	int             ospl;
	u_int32_t       state;
	u_int32_t       previous;
	u_int8_t        result;

	ospl = splcam();

	result = 0;

	for (ndx = 0, state = 0, previous = 0;
	     (ndx < 10) && (state != previous);
	     ndx++) {
		previous = state;
		state = dpt_inl(dpt, 1);
	}

	if ((state == previous) && (state == DPT_BLINK_INDICATOR))
		result = dpt_inb(dpt, 5);

	splx(ospl);
	return (result);
}

/**
 * Execute a command which did not come from the kernel's SCSI layer.
 * The only way to map user commands to bus and target is to comply with the
 * standard DPT wire-down scheme:
 */

int
dpt_user_cmd(dpt_softc_t * dpt, eata_pt_t * user_cmd,
	     caddr_t cmdarg, int minor_no)
{
	int             channel, target, lun;
	int             huh;
	int             result;
	int             ospl;
	int             submitted;
	dpt_ccb_t      *ccb;
	void           *data;
	struct timeval  now;

	data = NULL;
	channel = minor2hba(minor_no);
	target = minor2target(minor_no);
	lun = minor2lun(minor_no);

	if ((channel > (dpt->channels - 1))
	    || (target > dpt->max_id)
	    || (lun > dpt->max_lun))
		return (ENXIO);

	if (target == dpt->sc_scsi_link[channel].adapter_targ) {
		/* This one is for the controller itself */
		if ((user_cmd->eataID[0] != 'E')
		    || (user_cmd->eataID[1] != 'A')
		    || (user_cmd->eataID[2] != 'T')
		    || (user_cmd->eataID[3] != 'A')) {
			return (ENXIO);
		}
	}
	/* Get a DPT CCB, so we can prepare a command */
	ospl = splsoftcam();

	/* Process the free list */
	if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
		printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
		       "             Please try later\n",
		       dpt->unit);
		splx(ospl);
		return (EFAULT);
	}
	/* Now grab the newest CCB */
	if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
		splx(ospl);
		panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
	} else {
		splx(ospl);
		/* Clean up the leftover of the previous tenant */
		ccb->status = DPT_CCB_STATE_NEW;
	}

	bcopy((caddr_t) & user_cmd->command_packet, (caddr_t) & ccb->eata_ccb,
	      sizeof(eata_ccb_t));

	/* We do not want to do user specified scatter/gather.  Why?? */
	if (ccb->eata_ccb.scatter == 1)
		return (EINVAL);

	ccb->eata_ccb.Auto_Req_Sen = 1;
	ccb->eata_ccb.reqlen = htonl(sizeof(struct scsi_sense_data));
	ccb->eata_ccb.cp_datalen = htonl(sizeof(ccb->eata_ccb.cp_datalen));
	ccb->eata_ccb.cp_dataDMA = htonl(vtophys(ccb->eata_ccb.cp_dataDMA));
	ccb->eata_ccb.cp_statDMA = htonl(vtophys(&ccb->eata_ccb.cp_statDMA));
	ccb->eata_ccb.cp_reqDMA = htonl(vtophys(&ccb->eata_ccb.cp_reqDMA));
	ccb->eata_ccb.cp_viraddr = (u_int32_t) & ccb;

	if (ccb->eata_ccb.DataIn || ccb->eata_ccb.DataOut) {
		/* Data I/O is involved in this command.  Alocate buffer */
		if (ccb->eata_ccb.cp_datalen > PAGE_SIZE) {
			data = contigmalloc(ccb->eata_ccb.cp_datalen,
					    M_TEMP, M_WAITOK, 0, ~0,
					    ccb->eata_ccb.cp_datalen,
					    0x10000);
		} else {
			data = malloc(ccb->eata_ccb.cp_datalen, M_TEMP,
				      M_WAITOK);
		}

		if (data == NULL) {
			printf("dpt%d: Cannot allocate %d bytes "
			       "for EATA command\n", dpt->unit,
			       ccb->eata_ccb.cp_datalen);
			return (EFAULT);
		}
#define usr_cmd_DMA (caddr_t)user_cmd->command_packet.cp_dataDMA
		if (ccb->eata_ccb.DataIn == 1) {
			if (copyin(usr_cmd_DMA,
				   data, ccb->eata_ccb.cp_datalen) == -1)
				return (EFAULT);
		}
	} else {
		/* No data I/O involved here.  Make sure the DPT knows that */
		ccb->eata_ccb.cp_datalen = 0;
		data = NULL;
	}

	if (ccb->eata_ccb.FWNEST == 1)
		ccb->eata_ccb.FWNEST = 0;

	if (ccb->eata_ccb.cp_datalen != 0) {
		if (dpt_scatter_gather(dpt, ccb, ccb->eata_ccb.cp_datalen,
				       data) != 0) {
			if (data != NULL)
				free(data, M_TEMP);
			return (EFAULT);
		}
	}
	/**
	 * We are required to quiet a SCSI bus.
	 * since we do not queue comands on a bus basis,
	 * we wait for ALL commands on a controller to complete.
	 * In the mean time, sched_queue() will not schedule new commands.
	 */
	if ((ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	    && (ccb->eata_ccb.cp_cdb[2] == BUS_QUIET)) {
		/* We wait for ALL traffic for this HBa to subside */
		ospl = splsoftcam();
		dpt->state |= DPT_HA_QUIET;
		splx(ospl);

		while ((submitted = dpt->submitted_ccbs_count) != 0) {
			huh = tsleep((void *) dpt, PCATCH | PRIBIO, "dptqt",
				     100 * hz);
			switch (huh) {
			case 0:
				/* Wakeup call received */
				break;
			case EWOULDBLOCK:
				/* Timer Expired */
				break;
			default:
				/* anything else */
				break;
			}
		}
	}
	/* Resume normal operation */
	if ((ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	    && (ccb->eata_ccb.cp_cdb[2] == BUS_UNQUIET)) {
		ospl = splsoftcam();
		dpt->state &= ~DPT_HA_QUIET;
		splx(ospl);
	}
	/**
	 * Schedule the command and submit it.
	 * We bypass dpt_sched_queue, as it will block on DPT_HA_QUIET
	 */
	ccb->xs = NULL;
	ccb->flags = 0;
	ccb->eata_ccb.Auto_Req_Sen = 1;	/* We always want this feature */

	ccb->transaction_id = ++dpt->commands_processed;
	ccb->std_callback = (ccb_callback) dpt_user_cmd_done;
	ccb->result = (u_int32_t) & cmdarg;
	ccb->data = data;

#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.command_count[(int) ccb->eata_ccb.cp_scsi_cmd];
	microtime(&now);
	ccb->command_started = now;
#endif
	ospl = splcam();
	dpt_Qadd_waiting(dpt, ccb);
	splx(ospl);

	dpt_sched_queue(dpt);

	/* Wait for the command to complete */
	(void) tsleep((void *) ccb, PCATCH | PRIBIO, "dptucw", 100 * hz);

	/* Free allocated memory */
	if (data != NULL)
		free(data, M_TEMP);

	return (0);
}

static void
dpt_user_cmd_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb)
{
	int             ospl = splsoftcam();
	u_int32_t       result;
	caddr_t         cmd_arg;

	/**
	 * If Auto Request Sense is on, copyout the sense struct
	 */
#define usr_pckt_DMA 	(caddr_t)ntohl(ccb->eata_ccb.cp_reqDMA)
#define usr_pckt_len	ntohl(ccb->eata_ccb.cp_datalen)
	if (ccb->eata_ccb.Auto_Req_Sen == 1) {
		if (copyout((caddr_t) & ccb->sense_data, usr_pckt_DMA,
			    sizeof(struct scsi_sense_data))) {
			ccb->result = EFAULT;
			dpt_Qpush_free(dpt, ccb);
			splx(ospl);
			wakeup(ccb);
			return;
		}
	}
	/* If DataIn is on, copyout the data */
	if ((ccb->eata_ccb.DataIn == 1)
	    && (ccb->status_packet.hba_stat == HA_NO_ERROR)) {
		if (copyout(ccb->data, usr_pckt_DMA, usr_pckt_len)) {
			dpt_Qpush_free(dpt, ccb);
			ccb->result = EFAULT;

			splx(ospl);
			wakeup(ccb);
			return;
		}
	}
	/* Copyout the status */
	result = ccb->status_packet.hba_stat;
	cmd_arg = (caddr_t) ccb->result;

	if (copyout((caddr_t) & result, cmd_arg, sizeof(result))) {
		dpt_Qpush_free(dpt, ccb);
		ccb->result = EFAULT;
		splx(ospl);
		wakeup(ccb);
		return;
	}
	/* Put the CCB back in the freelist */
	ccb->state |= DPT_CCB_STATE_COMPLETED;
	dpt_Qpush_free(dpt, ccb);

	/* Free allocated memory */
	splx(ospl);
	return;
}

/* Detect Cache parameters and size */

void
dpt_detect_cache(dpt_softc_t * dpt)
{
	int             size;
	int             bytes;
	int             result;
	int             ospl;
	int             ndx;
	u_int8_t        status;
	char            name[64];
	char           *param;
	char           *buff;
	eata_ccb_t      cp;

	dpt_sp_t        sp;
	struct scsi_sense_data snp;

	/**
	 * We lock out the hardware early, so that we can either complete the
	 * operation or bust out right away.
	 */

	sprintf(name, "FreeBSD DPT Driver, version %d.%d.%d",
		DPT_RELEASE, DPT_VERSION, DPT_PATCH);

	/**
	 * Default setting, for best perfromance..
	 * This is what virtually all cards default to..
	 */
	dpt->cache_type = DPT_CACHE_WRITEBACK;
	dpt->cache_size = 0;

	if ((buff = malloc(512, M_DEVBUF, M_NOWAIT)) == NULL) {
		printf("dpt%d: Failed to allocate %d bytes for a work "
		       "buffer\n",
		       dpt->unit, 512);
		return;
	}
	bzero(&cp, sizeof(eata_ccb_t));
	bzero((int8_t *) & sp, sizeof(dpt_sp_t));
	bzero((int8_t *) & snp, sizeof(struct scsi_sense_data));
	bzero(buff, 512);

	/* Setup the command structure */
	cp.Interpret = 1;
	cp.DataIn = 1;
	cp.Auto_Req_Sen = 1;
	cp.reqlen = (u_int8_t) sizeof(struct scsi_sense_data);

	cp.cp_id = 0;		/* who cares?  The HBA will interpret.. */
	cp.cp_LUN = 0;		/* In the EATA packet */
	cp.cp_lun = 0;		/* In the SCSI command */
	cp.cp_channel = 0;

	cp.cp_scsi_cmd = EATA_CMD_DMA_SEND_CP;
	cp.cp_len = 56;
	cp.cp_dataDMA = htonl(vtophys(buff));
	cp.cp_statDMA = htonl(vtophys(&sp));
	cp.cp_reqDMA = htonl(vtophys(&snp));

	cp.cp_identify = 1;
	cp.cp_dispri = 1;

	/**
	 * Build the EATA Command Packet structure
	 * for a Log Sense Command.
	 */

	cp.cp_cdb[0] = 0x4d;
	cp.cp_cdb[1] = 0x0;
	cp.cp_cdb[2] = 0x40 | 0x33;
	cp.cp_cdb[7] = 1;

	cp.cp_datalen = htonl(512);

	ospl = splcam();
	result = dpt_send_eata_command(dpt, &cp, EATA_CMD_DMA_SEND_CP,
				       10000, 0, 0, 0);
	if (result != 0) {
		printf("dpt%d WARNING: detect_cache() failed (%d) to send "
		       "EATA_CMD_DMA_SEND_CP\n", dpt->unit, result);
		free(buff, M_TEMP);
		splx(ospl);
		return;
	}
	/* Wait for two seconds for a response.  This can be slow... */
	for (ndx = 0;
	     (ndx < 20000) &&
	     !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	     ndx++) {
		DELAY(50);
	}

	/* Grab the status and clear interrupts */
	status = dpt_inb(dpt, HA_RSTATUS);
	splx(ospl);

	/**
	 * Sanity check
	 */
	if (buff[0] != 0x33) {
		return;
	}
	bytes = DPT_HCP_LENGTH(buff);
	param = DPT_HCP_FIRST(buff);

	if (DPT_HCP_CODE(param) != 1) {
		/**
		 * DPT Log Page layout error
		 */
		printf("dpt%d: NOTICE: Log Page (1) layout error\n",
		       dpt->unit);
		return;
	}
	if (!(param[4] & 0x4)) {
		dpt->cache_type = DPT_NO_CACHE;
		return;
	}
	while (DPT_HCP_CODE(param) != 6) {
		param = DPT_HCP_NEXT(param);
		if ((param < buff)
		    || (param >= &buff[bytes])) {
			return;
		}
	}

	if (param[4] & 0x2) {
		/**
		 * Cache disabled
		 */
		dpt->cache_type = DPT_NO_CACHE;
		return;
	}
	if (param[4] & 0x4) {
		dpt->cache_type = DPT_CACHE_WRITETHROUGH;
		return;
	}
	dpt->cache_size = param[5]
		| (param[6] < 8)
		| (param[7] << 16)
		| (param[8] << 24);

	return;
}

/**
 * Initializes the softc structure and allocate all sorts of storage.
 * Returns 0 on good luck, 1-n otherwise (error condition sensitive).
 */

int
dpt_setup(dpt_softc_t * dpt, dpt_conf_t * conf)
{
	dpt_inq_t      *board_data;
	u_long          rev;
	int             ndx;
	int             ospl;
	dpt_ccb_t      *ccb;

	board_data = dpt_get_board_data(dpt, conf->scsi_id0);
	if (board_data == NULL) {
		printf("dpt%d ERROR: Get_board_data() failure. "
		       "Setup ignored!\n", dpt->unit);
		return (1);
	}
	dpt->total_ccbs_count = 0;
	dpt->free_ccbs_count = 0;
	dpt->waiting_ccbs_count = 0;
	dpt->submitted_ccbs_count = 0;
	dpt->completed_ccbs_count = 0;

	switch (ntohl(conf->splen)) {
	case DPT_EATA_REVA:
		dpt->EATA_revision = 'a';
		break;
	case DPT_EATA_REVB:
		dpt->EATA_revision = 'b';
		break;
	case DPT_EATA_REVC:
		dpt->EATA_revision = 'c';
		break;
	case DPT_EATA_REVZ:
		dpt->EATA_revision = 'z';
		break;
	default:
		dpt->EATA_revision = '?';
	}

	(void) memcpy(&dpt->board_data, board_data, sizeof(dpt_inq_t));

	dpt->bustype = IS_PCI;	/* We only support and operate on PCI devices */
	dpt->channels = conf->MAX_CHAN + 1;
	dpt->max_id = conf->MAX_ID;
	dpt->max_lun = conf->MAX_LUN;
	dpt->state |= DPT_HA_OK;

	if (conf->SECOND)
		dpt->primary = FALSE;
	else
		dpt->primary = TRUE;

	dpt->more_support = conf->MORE_support;

	if (board_data == NULL) {
		rev = ('?' << 24)
			| ('-' << 16)
			| ('?' << 8)
			| '-';
	} else {
		/* Convert from network byte order to a "string" */
		rev = (dpt->board_data.firmware[0] << 24)
			| (dpt->board_data.firmware[1] << 16)
			| (dpt->board_data.firmware[2] << 8)
			| dpt->board_data.firmware[3];
	}

	if (rev >= (('0' << 24) + ('7' << 16) + ('G' << 8) + '0'))
		dpt->immediate_support = 1;
	else
		dpt->immediate_support = 0;

	dpt->broken_INQUIRY = FALSE;

	for (ndx = 0; ndx < MAX_CHANNELS; ndx++)
		dpt->resetlevel[ndx] = DPT_HA_OK;

	dpt->cplen = ntohl(conf->cplen);
	dpt->cppadlen = ntohs(conf->cppadlen);
	dpt->queuesize = ntohs(conf->queuesiz);

	dpt->hostid[0] = conf->scsi_id0;
	dpt->hostid[1] = conf->scsi_id1;
	dpt->hostid[2] = conf->scsi_id2;

	if (conf->SG_64K) {
		dpt->sgsize = SG_SIZE_BIG;
	} else if ((ntohs(conf->SGsiz) < 1)
		   || (ntohs(conf->SGsiz) > SG_SIZE)) {
		/* Just a sanity check */
		dpt->sgsize = SG_SIZE;
	} else {
		dpt->sgsize = ntohs(conf->SGsiz);
	}

	if (dpt->sgsize > dpt_max_segs)
		dpt->sgsize = dpt_max_segs;

	if (dpt_alloc_freelist(dpt) != 0) {
		return (2);
	}
	/* Prepare for Target Mode */
	ospl = splsoftcam();
	dpt->target_mode_enabled = 1;
	splx(ospl);

	return (0);
}

/**
 * The following function returns a pointer to a buffer which MUST be freed by
 * The caller, a la free(result, M_DEVBUF)
 *
 * This function (and its like) assumes it is only running during system
 * initialization!
 */
dpt_inq_t      *
dpt_get_board_data(dpt_softc_t * dpt, u_int32_t target_id)
{
	/* get_conf returns 512 bytes, most of which are zeros... */
	return ((dpt_inq_t *) dpt_get_conf(dpt, 0, target_id,
					   sizeof(dpt_inq_t), 0));
}

/**
 * The following function returns a pointer to a buffer which MUST be freed by
 * the caller, a la ``free(result, M_TEMP);''
 */
dpt_conf_t     *
dpt_get_conf(dpt_softc_t * dpt, u_int8_t page, u_int8_t target,
	     u_int8_t size, int extent)
{
	dpt_sp_t        sp;
	eata_ccb_t      cp;

	/* Get_conf returns 512 bytes, most of which are zeros... */
	dpt_conf_t     *config;

	u_short        *ip;
	u_int8_t        status, sig1, sig2, sig3;

	int             ndx;
	int             ospl;
	int             result;

	struct scsi_sense_data snp;
	if ((config = (dpt_conf_t *) malloc(512, M_TEMP, M_WAITOK)) == NULL)
		return (NULL);

	bzero(&cp, sizeof(eata_ccb_t));
	bzero((int8_t *) & sp, sizeof(dpt_sp_t));
	bzero(config, size);

	cp.Interpret = 1;
	cp.DataIn = 1;
	cp.Auto_Req_Sen = 1;
	cp.reqlen = sizeof(struct scsi_sense_data);

	cp.cp_id = target;
	cp.cp_LUN = 0;		/* In the EATA packet */
	cp.cp_lun = 0;		/* In the SCSI command */

	cp.cp_scsi_cmd = INQUIRY;
	cp.cp_len = size;

	cp.cp_extent = extent;

	cp.cp_page = page;
	cp.cp_channel = 0;	/* DNC, Interpret mode is set */
	cp.cp_identify = 1;
	cp.cp_datalen = htonl(size);
	cp.cp_dataDMA = htonl(vtophys(config));
	cp.cp_statDMA = htonl(vtophys(&sp));
	cp.cp_reqDMA = htonl(vtophys(&snp));
	cp.cp_viraddr = (u_int32_t) & cp;

	ospl = splcam();

#ifdef DPT_RESET_BOARD
	printf("dpt%d: get_conf() resetting HBA at %x.\n",
	       dpt->unit, BaseRegister(dpt));
	dpt_outb(dpt, HA_WCOMMAND, EATA_CMD_RESET);
	DELAY(750000);
#endif

	/**
	 * This could be a simple for loop, but we suspected the compiler To
	 * have optimized it a bit too much. Wait for the controller to
	 * become ready
	 */
	while ((((status = dpt_inb(dpt, HA_RSTATUS)) != (HA_SREADY | HA_SSC))
		&& (status != (HA_SREADY | HA_SSC | HA_SERROR))
		&&		/* This results from the `wd' probe at our
				 * addresses */
		(status != (HA_SDRDY | HA_SERROR | HA_SDRQ)))
	       || (dpt_wait(dpt, HA_SBUSY, 0))) {
		/**
		 * RAID Drives still Spinning up? (This should only occur if
		 * the DPT controller is in a NON PC (PCI?) platform).
		 */
		if (dpt_raid_busy(dpt)) {
			printf("dpt%d WARNING: Get_conf() RSUS failed for "
			       "HBA at %x\n", dpt->unit, BaseRegister(dpt));
			free(config, M_TEMP);
			splx(ospl);
			return (NULL);
		}
	}

	DptStat_Reset_BUSY(&sp);

	/**
	 * XXXX We might want to do something more clever than aborting at
	 * this point, like resetting (rebooting) the controller and trying
	 * again.
	 */
	if ((result = dpt_send_eata_command(dpt, &cp, EATA_CMD_DMA_SEND_CP,
					    10000, 0, 0, 0)) != 0) {
		printf("dpt%d WARNING: Get_conf() failed (%d) to send "
		       "EATA_CMD_DMA_READ_CONFIG\n",
		       dpt->unit, result);
		free(config, M_TEMP);
		splx(ospl);
		return (NULL);
	}
	/* Wait for two seconds for a response.  This can be slow  */
	for (ndx = 0;
	     (ndx < 20000)
	     && !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	     ndx++) {
		DELAY(50);
	}

	/* Grab the status and clear interrupts */
	status = dpt_inb(dpt, HA_RSTATUS);

	splx(ospl);

	/**
	 * Check the status carefully.  Return only if the
	 * command was successful.
	 */
	if (((status & HA_SERROR) == 0)
	    && (sp.hba_stat == 0)
	    && (sp.scsi_stat == 0)
	    && (sp.residue_len == 0)) {
		return (config);
	}
	free(config, M_TEMP);
	return (NULL);
}

/* This gets called once per SCSI bus defined in config! */

int
dpt_attach(dpt_softc_t * dpt)
{
	struct scsibus_data *scbus;

	int             ndx;
	int             idx;
	int             channel;
	int             target;
	int             lun;

	struct scsi_inquiry_data *inq;

	for (ndx = 0; ndx < dpt->channels; ndx++) {
		/**
		 * We do not setup target nor lun on the assumption that
		 * these are being set for individual devices that will be
		 * attached to the bus later.
		 */
		dpt->sc_scsi_link[ndx].adapter_unit = dpt->unit;
		dpt->sc_scsi_link[ndx].adapter_targ = dpt->hostid[ndx];
		dpt->sc_scsi_link[ndx].fordriver = 0;
		dpt->sc_scsi_link[ndx].adapter_softc = dpt;
		dpt->sc_scsi_link[ndx].adapter = &dpt_switch;

		/*
		 * These appear to be the # of openings per that  DEVICE, not
		 * the DPT!
		 */
		dpt->sc_scsi_link[ndx].opennings = dpt->queuesize;
		dpt->sc_scsi_link[ndx].device = &dpt_dev;
		dpt->sc_scsi_link[ndx].adapter_bus = ndx;

		/**
		 * Prepare the scsibus_data area for the upperlevel scsi
		 * code.
		 */
		if ((scbus = scsi_alloc_bus()) == NULL)
			return 0;

		dpt->sc_scsi_link[ndx].scsibus = ndx;
		scbus->maxtarg = dpt->max_id;
		scbus->adapter_link = &dpt->sc_scsi_link[ndx];

		/*
		 * Invite the SCSI control layer to probe the busses.
		 */

		dpt->handle_interrupts = 1;	/* Now we are ready to work */
		scsi_attachdevs(scbus);
		scbus = (struct scsibus_data *) NULL;
	}

	return (1);
}

/**
 * Allocate another chunk of CCB's. Return 0 on success, 1 otherwise.
 * If the free list is empty, we allocate a block of entries and add them
 * to the list.	We obtain, at most, DPT_FREE_LIST_INCREMENT CCB's at a time.
 * If we cannot, we will try fewer entries until we succeed.
 * For every CCB, we allocate a maximal Scatter/Gather list.
 * This routine also initializes all the static data that pertains to this CCB.
 */

/**
 * XXX JGibbs - How big are your SG lists?  Remeber that the kernel malloc
 *		uses buckets and mallocs in powers of two.  So, if your
 *		SG list is not a power of two (up to PAGESIZE), you might
 *		waste a lot of memory.  This was the reason the ahc driver
 *		allocats multiple SG lists at a time up to a PAGESIZE.
 *		Just something to keep in mind.
 * YYY Simon -  Up to 8192 entries, each entry is two ulongs, comes to 64K.
 *              In reality they are much smaller, so you are right.
 */
static int
dpt_alloc_freelist(dpt_softc_t * dpt)
{
	dpt_ccb_t      *nccbp;
	dpt_sg_t       *sg;
	u_int8_t       *buff;
	int             ospl;
	int             incr;
	int             ndx;
	int             ccb_count;

	ccb_count = DPT_FREE_LIST_INCREMENT;

#ifdef DPT_RESTRICTED_FREELIST
	if (dpt->total_ccbs_count != 0) {
		printf("dpt%d: Restricted FreeList, No more than %d entries "
		       "allowed\n", dpt->unit, dpt->total_ccbs_count);
		return (-1);
	}
#endif

	/**
	 * Allocate a group of dpt_ccb's. Work on the CCB's, one at a time
	 */
	ospl = splsoftcam();
	for (ndx = 0; ndx < ccb_count; ndx++) {
		size_t          alloc_size;
		dpt_sg_t       *sgbuff;

		alloc_size = sizeof(dpt_ccb_t);	/* About 200 bytes */

		if (alloc_size > PAGE_SIZE) {
			/*
			 * Does not fit in a page. we try to fit in a
			 * contigious block of memory. If not, we will, later
			 * try to allocate smaller, and smaller chunks. There
			 * is a tradeof between memory and performance here.
			 * We know.this (crude) algorithm works well on
			 * machines with plenty of memory. We have seen it
			 * allocate in excess of 8MB.
			 */
			nccbp = (dpt_ccb_t *) contigmalloc(alloc_size,
							 M_DEVBUF, M_NOWAIT,
							   0, ~0,
							   PAGE_SIZE,
							   0x10000);
		} else {
			/* fits all in one page */
			nccbp = (dpt_ccb_t *) malloc(alloc_size, M_DEVBUF,
						     M_NOWAIT);
		}

		if (nccbp == (dpt_ccb_t *) NULL) {
			printf("dpt%d ERROR: Alloc_free_list() failed to "
			       "allocate %d\n",
			       dpt->unit, ndx);
			splx(ospl);
			return (-1);
		}
		alloc_size = sizeof(dpt_sg_t) * dpt->sgsize;

		if (alloc_size > PAGE_SIZE) {
			/* Does not fit in a page */
			sgbuff = (dpt_sg_t *) contigmalloc(alloc_size,
							 M_DEVBUF, M_NOWAIT,
							   0, ~0,
							   PAGE_SIZE,
							   0x10000);
		} else {
			/* fits all in one page */
			sgbuff = (dpt_sg_t *) malloc(alloc_size, M_DEVBUF,
						     M_NOWAIT);
		}

		/**
		 * If we cannot allocate sg lists, we do not want the entire
		 * list
		 */
		if (sgbuff == (dpt_sg_t *) NULL) {
			free(nccbp, M_DEVBUF);
			--ndx;
			break;
		}
		/* Clean up the mailboxes */
		bzero(sgbuff, alloc_size);
		bzero(nccbp, sizeof(dpt_ccb_t));
		/*
		 * this line is nullified by the one below.
		 * nccbp->eata_ccb.cp_dataDMA = (u_int32_t) sgbuff; Thanx,
		 * Mike!
		 */
		nccbp->sg_list = sgbuff;

		/**
		 * Now that we have a new block of free CCB's, put them into
		 * the free list. We always add to the head of the list and
		 * always take form the head of the list (LIFO). Each ccb
		 * has its own Scatter/Gather list. They are all of the same
		 * size, Regardless of how much is used.
		 *
		 * While looping through all the new CCB's, we initialize them
		 * properly. These items NEVER change; They are mostly
		 * self-pointers, relative to the CCB itself.
		 */
		dpt_Qpush_free(dpt, nccbp);
		++dpt->total_ccbs_count;

		nccbp->eata_ccb.cp_dataDMA = htonl(vtophys(nccbp->sg_list));
		nccbp->eata_ccb.cp_viraddr = (u_int32_t) nccbp;	/* Unique */
		nccbp->eata_ccb.cp_statDMA = htonl(vtophys(&dpt->sp));

		/**
		 * See dpt_intr for why we make ALL CCB's ``have the same''
		 * Status Packet
		 */
		nccbp->eata_ccb.cp_reqDMA = htonl(vtophys(&nccbp->sense_data));
	}

	splx(ospl);

	return (0);
}

/**
 * Prepare the data area for DMA.
 */
static int
dpt_scatter_gather(dpt_softc_t * dpt, dpt_ccb_t * ccb, u_int32_t data_length,
		   caddr_t data)
{
	int             seg;
	int             thiskv;
	int             bytes_this_seg;
	int             bytes_this_page;
	u_int32_t       datalen;
	vm_offset_t     vaddr;
	u_int32_t       paddr;
	u_int32_t       nextpaddr;
	dpt_sg_t       *sg;

	/* we start with Scatter/Gather OFF */
	ccb->eata_ccb.scatter = 0;

	if (data_length) {
		if (ccb->flags & SCSI_DATA_IN) {
			ccb->eata_ccb.DataIn = 1;
		}
		if (ccb->flags & SCSI_DATA_OUT) {
			ccb->eata_ccb.DataOut = 1;
		}
		seg = 0;
		datalen = data_length;
		vaddr = (vm_offset_t) data;
		paddr = vtophys(vaddr);
		ccb->eata_ccb.cp_dataDMA = htonl(vtophys(ccb->sg_list));
		sg = ccb->sg_list;

		while ((datalen > 0) && (seg < dpt->sgsize)) {
			/* put in the base address and length */
			sg->seg_addr = paddr;
			sg->seg_len = 0;

			/* do it at least once */
			nextpaddr = paddr;

			while ((datalen > 0) && (paddr == nextpaddr)) {
				u_int32_t       size;

				/**
				 * This page is contiguous (physically) with
				 * the the last, just extend the length
				 */

				/* how far to the end of the page */
				nextpaddr = trunc_page(paddr) + PAGE_SIZE;

				/* Compute the maximum size */

				size = nextpaddr - paddr;
				if (size > datalen)
					size = datalen;

				sg->seg_len += size;
				vaddr += size;
				datalen -= size;
				if (datalen > 0)
					paddr = vtophys(vaddr);
			}

			/* Next page isn't contiguous, finish the seg */
			sg->seg_addr = htonl(sg->seg_addr);
			sg->seg_len = htonl(sg->seg_len);
			seg++;
			sg++;
		}

		if (datalen) {
			/* There's still data, must have run out of segs! */
			printf("dpt%d: scsi_cmd() Too Many (%d) DMA segs "
			       "(%d bytes left)\n",
			       dpt->unit, dpt->sgsize, datalen);
			return (1);
		}
		if (seg == 1) {
			/**
			 * After going through all this trouble, we
			 * still have only one segment. As an
			 * optimization measure, we will do the
			 * I/O as a single, non-S/G operation.
			 */
			ccb->eata_ccb.cp_dataDMA = ccb->sg_list[0].seg_addr;
			ccb->eata_ccb.cp_datalen = ccb->sg_list[0].seg_len;
		} else {
			/**
			 * There is more than one segment. Use S/G.
			 */
			ccb->eata_ccb.scatter = 1;
			ccb->eata_ccb.cp_datalen =
				htonl(seg * sizeof(dpt_sg_t));
		}
	} else {		/* datalen == 0 */
		/* No data xfer */
		ccb->eata_ccb.cp_datalen = 0;
		ccb->eata_ccb.cp_dataDMA = 0;
	}

	return (0);
}

/**
 * This function obtains a CCB for a command and attempts to queue it to the
 * Controller.
 *
 * CCB Obtaining: Is done by getting the first entry in the free list for the
 * HBA. If we fail to get an scb, we send a TRY_LATER to the caller.
 *
 * XXX - JGibbs: XS_DRIVER_STUFFUP is equivalent to failing the I/O in the
 *               current SCSI layer.
 *
 * Command Queuing: Is done by putting the command at the end of the waiting
 * queue. This assures fair chance for all commands to be processed.
 * If the queue was empty (has only this, current command in it, we try to
 * submit it to the HBA. Otherwise we return SUCCESSFULLY_QUEUED.
 */

int32_t
dpt_scsi_cmd(struct scsi_xfer * xs)
{
	dpt_softc_t    *dpt;
	int             incr;
	int             ndx;
	int             ospl;
	int             huh;

	u_int32_t       flags;
	dpt_ccb_t      *ccb;
	u_int8_t        status;
	u_int32_t       aux_status = 0;	/* Initialized to shut GCC up */
	int             result;

	int             channel, target, lun;

	struct scsi_inquiry_data *inq;

	dpt = (dpt_softc_t *) xs->sc_link->adapter_softc;

	flags = xs->flags;
	channel = xs->sc_link->adapter_bus;
	target = xs->sc_link->target;
	lun = xs->sc_link->lun;

#ifdef DPT_HANDLE_TIMEOUTS
	ospl = splsoftcam();
	if ((dpt->state & DPT_HA_TIMEOUTS_SET) == 0) {
		dpt->state |= DPT_HA_TIMEOUTS_SET;
		timeout(dpt_timeout, dpt, hz * 10);
	}
	splx(ospl);
#endif

#ifdef DPT_LOST_IRQ
	ospl = splcam();
	if ((dpt->state & DPT_LOST_IRQ_SET) == 0) {
		printf("dpt%d: Initializing Lost IRQ Timer\n", dpt->unit);
		dpt->state |= DPT_LOST_IRQ_SET;
		timeout(dpt_irq_timeout, dpt, hz);
	}
	splx(ospl);
#endif

	/**
	 * Examine the command flags and handle properly. XXXX We are not
	 * handling external resets right now.	Needs to be added. We do not
	 * care about the SCSI_NOSLEEP flag as we do not sleep here. We have
	 * to observe the SCSI_NOMASK flag, though.
	 */
	if (xs->flags & SCSI_RESET) {
		printf("dpt%d: Unsupported option...\n"
		       "      I refuse to Reset b%dt%du%d...!\n",
		       __FILE__, __LINE__, channel, target, lun);
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}
	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE) {
		printf("dpt%d ERROR: Command \"%s\" recieved for b%dt%du%d\n"
		    "	    but controller is shutdown; Aborting...\n",
		       dpt->unit,
		       scsi_cmd_name(xs->cmd->opcode),
		       channel, target, lun);
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}
	if (flags & ITSDONE) {
		printf("dpt%d WARNING: scsi_cmd(%s) already done on "
		       "b%dt%du%d?!\n",
		       dpt->unit, scsi_cmd_name(xs->cmd->opcode),
		       channel, target, lun);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("dpt%d WARNING: Unit not in use in scsi_cmd(%s) "
		       "on b%dt%du%d?!\n",
		       dpt->unit, scsi_cmd_name(xs->cmd->opcode), channel,
		       target, lun);
		xs->flags |= INUSE;
	}
	/**
	 * We do not want to be disrupted when doing this, or another caller
	 * may do the same thing.
	 */
	ospl = splsoftcam();

	/* Process the free list */
	if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
		printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
		       "            Will try later\n",
		       dpt->unit);
		xs->error = XS_DRIVER_STUFFUP;
		splx(ospl);
		return (COMPLETE);
	}
	/* Now grab the newest CCB */
	if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
		/*
		 * No need to panic here.  We can continue with only as many
		 * CCBs as we have.
		 */
		printf("dpt%d ERROR: Got a NULL CCB from pop_free()\n",
		       dpt->unit);
		xs->error = XS_DRIVER_STUFFUP;
		splx(ospl);
		return (COMPLETE);
	}
#ifdef DPT_HANDLE_TIMEOUTS
	ccb->status &= ~(DPT_CCB_STATE_ABORTED | DPT_CCB_STATE_MARKED_LOST);
#endif

	splx(ospl);
	bcopy(xs->cmd, ccb->eata_ccb.cp_cdb, xs->cmdlen);

	/* Put all the CCB population stuff below */
	ccb->xs = xs;
	ccb->flags = flags;
	/* We NEVER reset the bus from a command   */
	ccb->eata_ccb.SCSI_Reset = 0;
	/* We NEVER re-boot the HBA from a * command */
	ccb->eata_ccb.HBA_Init = 0;
	ccb->eata_ccb.Auto_Req_Sen = 1;	/* We always want this feature */
	ccb->eata_ccb.reqlen = htonl(sizeof(struct scsi_sense_data));
	ccb->std_callback = NULL;
	ccb->wrbuff_callback = NULL;

	if (xs->sc_link->target == xs->sc_link->adapter_targ) {
		ccb->eata_ccb.Interpret = 1;
	} else {
		ccb->eata_ccb.Interpret = 0;
	}

	ccb->eata_ccb.scatter = 0;	/* S/G is OFF now */
	ccb->eata_ccb.DataIn = 0;
	ccb->eata_ccb.DataOut = 0;

	/* At this time we do not deal with the RAID internals */
	ccb->eata_ccb.FWNEST = 0;
	ccb->eata_ccb.Phsunit = 0;
	/* We do not do SMARTROM kind of things */
	ccb->eata_ccb.I_AT = 0;
	/* We do not inhibit the cache at this time */
	ccb->eata_ccb.Disable_Cache = 0;
	ccb->eata_ccb.cp_channel = channel;
	ccb->eata_ccb.cp_id = target;
	ccb->eata_ccb.cp_LUN = lun;	/**
					 * In the EATA packet. We do not
					 * change the SCSI command yet
					 */
	/* We are currently dealing with target LUN's, not ROUTINEs */
	ccb->eata_ccb.cp_luntar = 0;

	/**
	 * XXXX - We grant the target disconnect prvileges, except in polled
	 * mode (????).
	 */
	if ((ccb->flags & SCSI_NOMASK) || !dpt->handle_interrupts) {
		ccb->eata_ccb.cp_dispri = 0;
	} else {
		ccb->eata_ccb.cp_dispri = 1;
	}

	/* we always ask for Identify */
	ccb->eata_ccb.cp_identify = 1;

	/**
	 * These three are used for command queues and tags. How do we use
	 * them?
	 *
	 * XXX - JGibbs: Most likely like so: ccb->eata_ccb.cp_msg[0] =
	 * MSG_SIMPLEQ_TAG; ccb->eata_ccb.cp_msg[1] = tagid;
	 * ccb->eata_ccb.cp_msg[2] = 0;
	 *
	 * YYY - Shimon: Thanx!	We still do not do that as the current
	 * firmware does it automatically, including on RAID arrays.
	 */

	ccb->eata_ccb.cp_msg[0] = 0;
	ccb->eata_ccb.cp_msg[1] = 0;
	ccb->eata_ccb.cp_msg[2] = 0;

	/* End of CCB population */

	if (dpt_scatter_gather(dpt, ccb, xs->datalen, xs->data) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		ospl = splsoftcam();
		dpt_Qpush_free(dpt, ccb);
		splx(ospl);
		return (COMPLETE);
	}
	xs->resid = 0;
	xs->status = 0;

	/**
	 * This is the polled mode section. If we are here to honor
	 * SCSI_NOMASK, during scsi_attachdevs(), please notice that
	 * interrupts are ENABLED in the system (2.2.1) and that the DPT
	 * WILL generate them, unless we turn them off!
	 */

	/**
	 * XXX - JGibbs: Polled mode was a botch at best. It's nice to
	 *               know that it goes completely away with the CAM code.
	 * YYY - Simon:  Take it out once the rest is stable. Be careful about
	 *               how you wait for commands to complete when you switch
	 *               to interrupt mode in the scanning code (initiated by
	 *               scsi_attachdevs).
	 *               Disabling it in 2.2 causes a hung system.
	 */

	if ((ccb->flags & SCSI_NOMASK) || !dpt->handle_interrupts) {
		/**
		 * This is an ``immediate'' command.	Poll it! We poll by
		 * partially bypassing the queues. We first submit the
		 * command by asking dpt_run_queue() to queue it. Then we
		 * poll its status packet, until it completes. Then we give
		 * it to dpt_process_completion() to analyze and then we
		 * return.
		 */

		/*
		 * Increase the number of commands queueable for a device. We
		 * force each device to the maximum allowed for its HBA. This
		 * appears wrong but all it will do is cause excessive
		 * commands to sit in our queue. On the other hand, we can
		 * burst as many commands as the DPT can take for a single
		 * device. We do it here, so only while in polled mode (early
		 * boot) do we waste time on it.	We have no clean way
		 * to overrule sdattach() zeal in depressing the opennings
		 * back to one if it is more than 1.
		 */
		if (xs->sc_link->opennings < dpt->queuesize) {
			xs->sc_link->opennings = dpt->queuesize;
		}
		/**
		 * This test only protects us from submitting polled
		 * commands during Non-polled times.  We assumed polled
		 * commands go in serially, one at a time. BTW, we have NOT
		 * checked, nor verified the scope of the disaster that WILL
		 * follow going into polled mode after being in interrupt
		 * mode for any length of time.
		 */
		if (dpt->submitted_ccbs_count < dpt->queuesize) {
			/**
			 * Submit the request to the DPT. Unfortunately, ALL
			 * this must be done as an atomic operation :-(
			 */
			ccb->eata_ccb.cp_viraddr = (u_int32_t) & ccb;
#define dpt_SP		htonl(vtophys(&ccb->status_packet))
#define dpt_sense	htonl(vtophys(&ccb->sense_data))
			ccb->eata_ccb.cp_statDMA = dpt_SP;
			ccb->eata_ccb.cp_reqDMA = dpt_sense;

			/* Try to queue a command */
			ospl = splcam();
			result = dpt_send_eata_command(dpt, &ccb->eata_ccb,
						       EATA_CMD_DMA_SEND_CP,
						       0, 0, 0, 0);

			if (result != 0) {
				dpt_Qpush_free(dpt, ccb);
				xs->error = XS_DRIVER_STUFFUP;
				splx(ospl);
				return (COMPLETE);
			}
		} else {
			xs->error = XS_DRIVER_STUFFUP;
			dpt_Qpush_free(dpt, ccb);
			splx(ospl);
			return (COMPLETE);
		}

		for (ndx = 0;
		     (ndx < xs->timeout)
		     && !((aux_status = dpt_inb(dpt, HA_RAUXSTAT))
			  & HA_AIRQ);
		     ndx++) {
			DELAY(50);
		}

		/**
		 * Get the status and clear the interrupt flag on the
		 * controller
		 */
		status = dpt_inb(dpt, HA_RSTATUS);
		splx(ospl);

		ccb->status_reg = status;
		ccb->aux_status_reg = aux_status;
		/* This will setup the xs flags */
		dpt_process_completion(dpt, ccb);

		if (status & HA_SERROR) {
			ospl = splsoftcam();
			dpt_Qpush_free(dpt, ccb);
			splx(ospl);
			return (COMPLETE);
		}
		ospl = splsoftcam();
		dpt_Qpush_free(dpt, ccb);
		splx(ospl);
		return (COMPLETE);
	} else {
		struct timeval  junk;

		/**
		 * Not a polled command.
		 * The command can be queued normally.
		 * We start a critical section PRIOR to submitting to the DPT,
		 * and end it AFTER it moves to the submitted queue.
		 * If not, we cal (and will!) be hit with a completion
		 * interrupt while the command is in suspense between states.
		 */

		ospl = splsoftcam();
		ccb->transaction_id = ++dpt->commands_processed;

#ifdef DPT_MEASURE_PERFORMANCE
#define cmd_ndx (int)ccb->eata_ccb.cp_scsi_cmd
		++dpt->performance.command_count[cmd_ndx];
		microtime(&junk);
		ccb->command_started = junk;
#endif
		dpt_Qadd_waiting(dpt, ccb);
		splx(ospl);

		dpt_sched_queue(dpt);
	}

	return (SUCCESSFULLY_QUEUED);
}

/**
 * This function returns the transfer size in bytes,
 * as a function of the maximum number of Scatter/Gather
 * segments.  It should do so for a given HBA, but right now it returns
 * dpt_min_segs, which is the SMALLEST number, from the ``weakest'' HBA found.
 */

void
dptminphys(struct buf * bp)
{
	/**
	 * This IS a performance sensitive routine.
	 * It gets called at least once per I/O. Sometimes more
	 */

	if (dpt_min_segs == 0) {
		panic("DPT:  Minphys without attach!\n");
	}
	if (bp->b_bcount > ((dpt_min_segs - 1) * PAGE_SIZE)) {
#ifdef DPT_DEBUG_MINPHYS
		printf("DPT: Block size of %x is larger than %x.  Truncating\n",
		       bp->b_bcount, ((dpt_min_segs - 1) * PAGE_SIZE));
#endif
		bp->b_bcount = ((dpt_min_segs - 1) * PAGE_SIZE);
	}
}

/*
 * This function goes to the waiting queue, peels off a request, gives it to
 * the DPT HBA and returns. It takes care of some housekeeping details first.
 * The requests argument tells us how many requests to try and send to the
 * DPT. A requests = 0 will attempt to send as many as the controller can
 * take.
 */

static void
dpt_run_queue(dpt_softc_t * dpt, int requests)
{
	int             req;
	int             ospl;
	int             ndx;
	int             result;

	u_int8_t        status, aux_status;

	eata_ccb_t     *ccb;
	dpt_ccb_t      *dccb;

	if (TAILQ_EMPTY(&dpt->waiting_ccbs)) {
		return;		/* Nothing to do if the list is empty */
	}
	if (!requests)
		requests = dpt->queuesize;

	/* Main work loop */
	for (req = 0; (req < requests) && dpt->waiting_ccbs_count
	     && (dpt->submitted_ccbs_count < dpt->queuesize); req++) {
		/**
		 * Move the request from the waiting list to the submitted
		 * list, and submit to the DPT.
		 * We enter a critical section BEFORE even looking at the
		 * queue, and exit it AFTER the ccb has moved to a
		 * destination queue.
		 * This is normally the submitted queue but can be the waiting
		 * queue again, if pushing the command into the DPT failed.
		 */

		ospl = splsoftcam();
		dccb = TAILQ_FIRST(&dpt->waiting_ccbs);

		if (dccb == NULL) {
			/* We have yet to see one report of this condition */
			panic("dpt%d ERROR: Race condition in run_queue "
			      "(w%ds%d)\n",
			      dpt->unit, dpt->waiting_ccbs_count,
			      dpt->submitted_ccbs_count);
			splx(ospl);
			return;
		}
		dpt_Qremove_waiting(dpt, dccb);
		splx(ospl);

		/**
		 * Assign exact values here. We manipulate these values
		 * indirectly elsewhere, so BE CAREFUL!
		 */
		dccb->eata_ccb.cp_viraddr = (u_int32_t) dccb;
		dccb->eata_ccb.cp_statDMA = htonl(vtophys(&dpt->sp));
		dccb->eata_ccb.cp_reqDMA = htonl(vtophys(&dccb->sense_data));

		if (dccb->xs != NULL)
			bzero(&dccb->xs->sense, sizeof(struct scsi_sense_data));

		/* Try to queue a command */
		ospl = splcam();

		if ((result = dpt_send_eata_command(dpt, &dccb->eata_ccb,
						    EATA_CMD_DMA_SEND_CP, 0,
						    0, 0, 0)) != 0) {
			dpt_Qpush_waiting(dpt, dccb);
			splx(ospl);
			return;
		}
		dpt_Qadd_submitted(dpt, dccb);
		splx(ospl);
	}
}

/**
 * This is the interrupt handler for the DPT driver.
 * This routine runs at splcam (or whatever was configured for this device).
 */

void
dpt_intr(void *arg)
{
	dpt_softc_t    *dpt;
	dpt_softc_t    *ldpt;

	u_int8_t        status, aux_status;

	dpt_ccb_t      *dccb;
	dpt_ccb_t      *tccb;
	eata_ccb_t     *ccb;

	dpt = (dpt_softc_t *) arg;

#ifdef DPT_INTR_DELAY
	DELAY(DPT_INTR_DELAY);
#endif

#ifdef DPT_MEASURE_PERFORMANCE
	{
		struct timeval  junk;

		microtime(&junk);
		dpt->performance.intr_started = junk;
	}
#endif

	/* First order of business is to check if this interrupt is for us */
	aux_status = dpt_inb(dpt, HA_RAUXSTAT);
	if (!(aux_status & HA_AIRQ)) {
#ifdef DPT_LOST_IRQ
		if (dpt->state & DPT_LOST_IRQ_ACTIVE) {
			dpt->state &= ~DPT_LOST_IRQ_ACTIVE;
			return;
		}
#endif
#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.spurious_interrupts;
#endif
		return;
	}
	if (!dpt->handle_interrupts) {
#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.aborted_interrupts;
#endif
		status = dpt_inb(dpt, HA_RSTATUS);	/* This CLEARS
							 * interrupts */
		return;
	}
	/**
	 * What we want to do now, is to capture the status, all of it, move
	 * it where it belongs, wake up whoever sleeps waiting to process
	 * this result, and get out of here.
	 */

	dccb = dpt->sp.ccb;	/**
				 * There is a very SERIOUS and dangerous
				 * assumption here. We assume that EVERY
				 * interrupt is in response to some request we
				 * put to the DPT. IOW, we assume that the
				 * Virtual Address of CP always has a valid
				 * pointer that we put in! How will the DPT
				 * behave if it is in Target mode? How does it
				 * (and our driver) know it switches from
				 * Initiator to target? What will the SP be
				 * when a target mode interrupt is received?
				 */

#ifdef DPT_VERIFY_HINTR
	dpt->sp.ccb = (dpt_ccb_t *) 0x55555555;
#else
	dpt->sp.ccb = (dpt_ccb_t *) NULL;
#endif

#ifdef DPT_HANDLE_TIMEOUTS
	if (dccb->state & DPT_CCB_STATE_MARKED_LOST) {
		struct timeval  now;
		u_int32_t       age;
		struct scsi_xfer *xs = dccb->xs;

		microtime(&now);
		age = dpt_time_delta(dccb->command_started, now);

		printf("dpt%d: Salvaging Tx %d from the jaws of destruction "
		       "(%d/%d)\n",
		       dpt->unit, dccb->transaction_id, xs->timeout, age);
		dccb->state |= DPT_CCB_STATE_MARKED_SALVAGED;
		dccb->state &= ~DPT_CCB_STATE_MARKED_LOST;
	}
#endif

	/* Ignore status packets with EOC not set */
	if (dpt->sp.EOC == 0) {
		printf("dpt%d ERROR: Request %d recieved with clear EOC.\n"
		       "     Marking as LOST.\n",
		       dpt->unit, dccb->transaction_id);
#ifdef DPT_VERIFY_HINTR
		dpt->sp.ccb = (dpt_sp_t *) 0x55555555;
#else
		dpt->sp.ccb = (dpt_sp_t *) NULL;
#endif

#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.aborted_interrupts;
#endif

#ifdef DPT_HANDLE_TIMEOUTS
		dccb->state |= DPT_CCB_STATE_MARKED_LOST;
#endif
		/* This CLEARS the interrupt! */
		status = dpt_inb(dpt, HA_RSTATUS);
		return;
	}
	dpt->sp.EOC = 0;

#ifdef DPT_VERIFY_HINTR
	/*
	 * Make SURE the next caller is legitimate. If they are not, we will
	 * find 0x55555555 here. We see 0x000000 or 0xffffffff when the PCi
	 * bus has DMA troubles (as when behing a PCI-PCI * bridge .
	 */
	if ((dccb == NULL)
	    || (dccb == (dpt_ccb_t *) ~ 0)
	    || (dccb == (dpt_ccb_t *) 0x55555555)) {
		printf("dpt%d: BAD (%x) CCB in SP (AUX status = %s).\n",
		       dpt->unit, dccb, i2bin((unsigned long) aux_status,
					      sizeof(aux_status) * 8));
#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.aborted_interrupts;
#endif
		/* This CLEARS the interrupt! */
		status = dpt_inb(dpt, HA_RSTATUS);
		return;
	}
	for (tccb = TAILQ_FIRST(&dpt->submitted_ccbs);
	     (tccb != NULL) && (tccb != dccb);
	     tccb = TAILQ_NEXT(tccb, links));
	if (tccb == NULL) {
		printf("dpt%d: %x is not in the SUBMITTED queue\n",
		       dpt->unit, dccb);

		for (tccb = TAILQ_FIRST(&dpt->completed_ccbs);
		     (tccb != NULL) && (tccb != dccb);
		     tccb = TAILQ_NEXT(tccb, links));
		if (tccb != NULL)
			printf("dpt%d: %x is in the COMPLETED queue\n",
			       dpt->unit, dccb);

		for (tccb = TAILQ_FIRST(&dpt->waiting_ccbs);
		     (tccb != NULL) && (tccb != dccb);
		     tccb = TAILQ_NEXT(tccb, links));
		if (tccb != NULL)
			printf("dpt%d: %x is in the WAITING queue\n",
			       dpt->unit, dccb);

		for (tccb = TAILQ_FIRST(&dpt->free_ccbs);
		     (tccb != NULL) && (tccb != dccb);
		     tccb = TAILQ_NEXT(tccb, links));
		if (tccb != NULL)
			printf("dpt%d: %x is in the FREE queue\n",
			       dpt->unit, dccb);

#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.aborted_interrupts;
#endif
		/* This CLEARS the interrupt! */
		status = dpt_inb(dpt, HA_RSTATUS);
		return;
	}
#endif				/* DPT_VERIFY_HINTR */

	/**
	 * Copy the status packet from the general area to the dpt_ccb.
	 * According to Mark Salyzyn, we only need few pieces of it.
	 * Originally we had:
	 * bcopy((void *) &dpt->sp, (void *) &dccb->status_packet,
	 *        sizeof(dpt_sp_t));
	 */
	dccb->status_packet.hba_stat = dpt->sp.hba_stat;
	dccb->status_packet.scsi_stat = dpt->sp.scsi_stat;
	dccb->status_packet.residue_len = dpt->sp.residue_len;

	/* Make sure the EOC bit is OFF! */
	dpt->sp.EOC = 0;

	/* Clear interrupts, check for error */
	if ((status = dpt_inb(dpt, HA_RSTATUS)) & HA_SERROR) {
		/**
		 * Error Condition. Check for magic cookie. Exit this test
		 * on earliest sign of non-reset condition
		 */

		/* Check that this is not a board reset interrupt */
		if (dpt_just_reset(dpt)) {
			printf("dpt%d: HBA rebooted.\n"
			       "      All transactions should be "
			       "resubmitted\n",
			       dpt->unit);

			printf("dpt%d: >>---->>  This is incomplete, fix me"
			       "....  <<----<<",
			       dpt->unit);
			printf("      Incomplete Code; Re-queue the lost "
			       "commands\n",
			       dpt->unit);
			Debugger("DPT Rebooted");

#ifdef DPT_MEASURE_PERFORMANCE
			++dpt->performance.aborted_interrupts;
#endif
			return;
		}
	}
	dccb->status_reg = status;
	dccb->aux_status_reg = aux_status;

	/* Mark BOTH queues as busy */
	dpt->queue_status |= (DPT_SUBMITTED_QUEUE_ACTIVE
			      | DPT_COMPLETED_QUEUE_ACTIVE);
	dpt_Qremove_submitted(dpt, dccb);
	dpt_Qadd_completed(dpt, dccb);
	dpt->queue_status &= ~(DPT_SUBMITTED_QUEUE_ACTIVE
			       | DPT_COMPLETED_QUEUE_ACTIVE);
	dpt_sched_queue(dpt);

#ifdef DPT_MEASURE_PERFORMANCE
	{
		u_int32_t       result;
		struct timeval  junk;

		microtime(&junk);

		result = dpt_time_delta(dpt->performance.intr_started, junk);

		if (result != ~0) {
			if (dpt->performance.max_intr_time < result)
				dpt->performance.max_intr_time = result;

			if (result < dpt->performance.min_intr_time) {
				dpt->performance.min_intr_time = result;
			}
		}
	}
#endif
}

/*
 * This function is the DPT_ISR Software Interrupt Service Routine. When the
 * DPT completes a SCSI command, it puts the results in a Status Packet, sets
 * up two 1-byte registers and generates an interrupt.  We catch this
 * interrupt in dpt_intr and copy the whole status to the proper CCB. Once
 * this is done, we generate a software interrupt that calls this routine.
 * The routine then scans ALL the complete queues of all the DPT HBA's and
 * processes ALL the commands that are in the queue.
 * 
 * XXXX REMEMBER:	We always scan ALL the queues of all the HBA's. Always
 * starting with the first controller registered (dpt0).	This creates
 * an ``unfair'' opportunity for the first controllers in being served.
 * Careful instrumentation may prove a need to change this policy.
 * 
 * This command rns at splSOFTcam.  Remember that.
 */

void
dpt_sintr(void)
{
	dpt_softc_t    *dpt;
	int             ospl;

	/* Find which DPT needs help */
	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     dpt != NULL;
	     dpt = TAILQ_NEXT(dpt, links)) {
		/*
		 * Drain the completed queue, to make room for new, " waiting
		 * requests. We change to splcam to block interrupts from
		 * mucking with " the completed queue
		 */
		ospl = splcam();
		if (dpt->queue_status & DPT_SINTR_ACTIVE) {
			splx(ospl);
			continue;
		}
		dpt->queue_status |= DPT_SINTR_ACTIVE;

		if (!TAILQ_EMPTY(&dpt->completed_ccbs)) {
			splx(ospl);
			dpt_complete(dpt);
			ospl = splcam();
		}
		/* Submit as many waiting requests as the DPT can take */
		if (!TAILQ_EMPTY(&dpt->waiting_ccbs)) {
			dpt_run_queue(dpt, 0);
		}
		dpt->queue_status &= ~DPT_SINTR_ACTIVE;
		splx(ospl);
	}
}

/**
 * Scan the complete queue for a given controller and process ALL the completed
 * commands in the queue.
 */

static void
dpt_complete(dpt_softc_t * dpt)
{
	dpt_ccb_t      *ccb;
	int             ospl;

	ospl = splcam();

	if (dpt->queue_status & DPT_COMPLETED_QUEUE_ACTIVE) {
		splx(ospl);
		return;
	}
	dpt->queue_status |= DPT_COMPLETED_QUEUE_ACTIVE;

	while ((ccb = TAILQ_FIRST(&dpt->completed_ccbs)) != NULL) {
		struct scsi_xfer *xs;

		dpt_Qremove_completed(dpt, ccb);
		splx(ospl);

		/* Process this completed request */
		if (dpt_process_completion(dpt, ccb) == 0) {
			xs = ccb->xs;

			if (ccb->std_callback != NULL) {
				(ccb->std_callback) (dpt, ccb->eata_ccb.cp_channel,
						     ccb);
			} else {
				ospl = splcam();
				dpt_Qpush_free(dpt, ccb);
				splx(ospl);

#ifdef DPT_MEASURE_PERFORMANCE
				{
					u_int32_t       result;
					struct timeval  junk;

					microtime(&junk);
					ccb->command_ended = junk;
#define time_delta dpt_time_delta(ccb->command_started,	ccb->command_ended)
					result = time_delta;
#define maxctime dpt->performance.max_command_time[ccb->eata_ccb.cp_scsi_cmd]
#define minctime dpt->performance.min_command_time[ccb->eata_ccb.cp_scsi_cmd]

					if (result != ~0) {
						if (maxctime < result) {
							maxctime = result;
						}
						if ((minctime == 0)
						    || (minctime > result))
							minctime = result;
					}
				}
#endif

				scsi_done(xs);
			}
			ospl = splcam();
		}
	}
	splx(ospl);

	/**
	 * As per Justin's suggestion, we now will call the run_queue for
	 * this HBA. This is done in case there are left-over requests that
	 * were not submitted yet.
	 */
	dpt_run_queue(dpt, 0);
	ospl = splsoftcam();
	dpt->queue_status &= ~DPT_COMPLETED_QUEUE_ACTIVE;
	splx(ospl);
}

#ifdef DPT_MEASURE_PERFORMANCE
/**
 * Given a dpt_ccb and a scsi_xfr structures,
 * this functions translates the result of a SCSI operation.
 * It returns values in the structures pointed by the arguments.
 * This function does NOT attempt to protect itself from bad influence!
 */

#define WRITE_OP 1
#define READ_OP 2
#define min_submitR	dpt->performance.read_by_size_min_time[index]
#define max_submitR	dpt->performance.read_by_size_max_time[index]
#define min_submitW	dpt->performance.write_by_size_min_time[index]
#define max_submitW	dpt->performance.write_by_size_max_time[index]

static void
dpt_IObySize(dpt_softc_t * dpt, dpt_ccb_t * ccb, int op, int index)
{
	if (op == READ_OP) {
		++dpt->performance.read_by_size_count[index];
		if (ccb->submitted_time < min_submitR)
			min_submitR = ccb->submitted_time;

		if (ccb->submitted_time > max_submitR)
			max_submitR = ccb->submitted_time;
	} else {		/* WRITE operation */
		++dpt->performance.write_by_size_count[index];
		if (ccb->submitted_time < min_submitW)
			min_submitW = ccb->submitted_time;

		if (ccb->submitted_time > max_submitW)
			max_submitW = ccb->submitted_time;
	}
}
#endif

static int
dpt_process_completion(dpt_softc_t * dpt,
		       dpt_ccb_t * ccb)
{
	int             ospl;
	struct scsi_xfer *xs;

	if (ccb == NULL) {
		panic("dpt%d: Improper argumet to process_completion (%p%p)\n",
		      dpt->unit, ccb);
	} else {
		xs = ccb->xs;
	}

#ifdef DPT_MEASURE_PERFORMANCE
	{
		u_int32_t       size;
		struct scsi_rw_big *cmd;
		int             op_type;

		cmd = (struct scsi_rw_big *) & ccb->eata_ccb.cp_scsi_cmd;

		switch (cmd->op_code) {
		case 0xa8:	/* 12-byte READ	 */
		case 0x08:	/* 6-byte READ	 */
		case 0x28:	/* 10-byte READ	 */
			op_type = READ_OP;
			break;
		case 0x0a:	/* 6-byte WRITE */
		case 0xaa:	/* 12-byte WRITE */
		case 0x2a:	/* 10-byte WRITE */
			op_type = WRITE_OP;
			break;
		default:
			op_type = 0;
			break;
		}

		if (op_type != 0) {

			size = (((u_int32_t) cmd->length2 << 8)
				| ((u_int32_t) cmd->length1)) << 9;

			switch (size) {
			case 512:
				dpt_IObySize(dpt, ccb, op_type, SIZE_512);
				break;
			case 1024:
				dpt_IObySize(dpt, ccb, op_type, SIZE_1K);
				break;
			case 2048:
				dpt_IObySize(dpt, ccb, op_type, SIZE_2K);
				break;
			case 4096:
				dpt_IObySize(dpt, ccb, op_type, SIZE_4K);
				break;
			case 8192:
				dpt_IObySize(dpt, ccb, op_type, SIZE_8K);
				break;
			case 16384:
				dpt_IObySize(dpt, ccb, op_type, SIZE_16K);
				break;
			case 32768:
				dpt_IObySize(dpt, ccb, op_type, SIZE_32K);
				break;
			case 65536:
				dpt_IObySize(dpt, ccb, op_type, SIZE_64K);
				break;
			default:
				if (size > (1 << 16))
					dpt_IObySize(dpt, ccb, op_type,
						     SIZE_BIGGER);

				else
					dpt_IObySize(dpt, ccb, op_type,
						     SIZE_OTHER);
				break;
			}
		}
	}
#endif				/* DPT_MEASURE_PERFORMANCE */


	switch ((int) ccb->status_packet.hba_stat) {
	case HA_NO_ERROR:
		if (xs != NULL) {
			xs->error = XS_NOERROR;
			xs->flags |= SCSI_ITSDONE;
		}
		break;
	case HA_ERR_SEL_TO:
	case HA_ERR_CMD_TO:
		if (xs != NULL) {
			xs->error |= XS_SELTIMEOUT;
			xs->flags |= SCSI_ITSDONE;
		}
		break;
	case HA_SCSIBUS_RESET:
	case HA_CP_ABORTED:
	case HA_CP_RESET:
	case HA_PCI_PARITY:
	case HA_PCI_MABORT:
	case HA_PCI_TABORT:
	case HA_PCI_STABORT:
	case HA_BUS_PARITY:
	case HA_UNX_MSGRJCT:
		if (ccb->retries++ > DPT_RETRIES) {
			if (xs != NULL) {
				xs->error |= XS_SENSE;
				xs->flags |= SCSI_ITSDONE;
			}
		} else {
			ospl = splsoftcam();
			dpt_Qpush_waiting(dpt, ccb);
			splx(ospl);
			dpt_sched_queue(dpt);
		}
		break;
	case HA_HBA_POWER_UP:
	case HA_UNX_BUSPHASE:
	case HA_UNX_BUS_FREE:
	case HA_SCSI_HUNG:
	case HA_RESET_STUCK:
		if (ccb->retries++ > DPT_RETRIES) {
			if (xs != NULL) {
				xs->error |= XS_SENSE;
				xs->flags |= SCSI_ITSDONE;
			}
		} else {
			ospl = splsoftcam();
			dpt_Qpush_waiting(dpt, ccb);
			splx(ospl);
			dpt_sched_queue(dpt);
			return (1);
		}
		break;
	case HA_RSENSE_FAIL:
		if (ccb->status_packet.EOC) {
			if (xs != NULL) {
				xs->error |= XS_SENSE;
				xs->flags |= SCSI_ITSDONE;
			}
		} else {
			if (ccb->retries++ > DPT_RETRIES) {
				if (xs != NULL) {
					xs->error |= XS_SENSE;
					xs->flags |= SCSI_ITSDONE;
				}
			} else {
				ospl = splsoftcam();
				dpt_Qpush_waiting(dpt, ccb);
				splx(ospl);
				dpt_sched_queue(dpt);
				return (1);
			}
		}
		break;
	case HA_PARITY_ERR:
	case HA_CP_ABORT_NA:
	case HA_CP_RESET_NA:
	case HA_ECC_ERR:
		if (xs != NULL) {
			xs->error |= XS_SENSE;
			xs->flags |= SCSI_ITSDONE;
		}
		break;
	default:
		printf("dpt%d: Undocumented Error %x",
		       dpt->unit, ccb->status_packet.hba_stat);
		if (xs != NULL) {
			xs->error |= XS_SENSE;
			xs->flags |= SCSI_ITSDONE;
		}
		Debugger("Please mail this message to shimon@i-connect.net");
		break;
	}

	if (xs != NULL) {
		if ((xs->error & XS_SENSE))
			bcopy(&ccb->sense_data, &xs->sense,
			      sizeof(struct scsi_sense_data));

		if (ccb->status_packet.residue_len != 0) {
			xs->flags |= SCSI_RESID_VALID;
			xs->resid = ccb->status_packet.residue_len;
		}
	}
	return (0);
}

#ifdef DPT_LOST_IRQ
/**
 * This functions handles the calling of the interrupt routine on a periodic
 * basis.
 * It is a completely ugly hack which purpose is to handle the problem of
 * missing interrupts on certain platforms..
 */

static void
dpt_irq_timeout(void *arg)
{
	dpt_softc_t    *dpt = (dpt_softc_t *) arg;
	int             ospl;


	if (!(dpt->state & DPT_LOST_IRQ_ACTIVE)) {
		ospl = splcam();
		dpt->state |= DPT_LOST_IRQ_ACTIVE;
		dpt_intr(dpt);
		splx(ospl);
		if (dpt->state & DPT_LOST_IRQ_ACTIVE) {
			printf("dpt %d: %d lost Interrupts Recovered\n",
			       dpt->unit, ++dpt->lost_interrupts);
		}
		dpt->state &= ~DPT_LOST_IRQ_ACTIVE;
	}
	timeout(dpt_irq_timeout, (caddr_t) dpt, hz * 1);
}

#endif				/* DPT_LOST_IRQ */

#ifdef DPT_HANDLE_TIMEOUTS
/**
 * This function walks down the SUBMITTED queue.
 * Every request that is too old gets aborted and marked.
 * Since the DPT will complete (interrupt) immediately (what does that mean?),
 * We just walk the list, aborting old commands and marking them as such.
 * The dpt_complete function will get rid of the that were interrupted in the
 * normal manner.
 *
 * This function needs to run at splcam(), as it interacts with the submitted
 * queue, as well as the completed and free queues.  Just like dpt_intr() does.
 * To run it at any ISPL other than that of dpt_intr(), will mean that dpt_intr
 * willbe able to pre-empt it, grab a transaction in progress (towards
 * destruction) and operate on it.  The state of this transaction will be not
 * very clear.
 * The only other option, is to lock it only as long as necessary but have
 * dpt_intr() spin-wait on it. In a UP environment this makes no sense and in
 * a SMP environment, the advantage is dubvious for a function that runs once
 * every ten seconds for few microseconds and, on systems with healthy
 * hardware, does not do anything anyway.
 */

static void
dpt_handle_timeouts(dpt_softc_t * dpt)
{
	dpt_ccb_t      *ccb;
	int             ospl;

	ospl = splcam();

	if (dpt->state & DPT_HA_TIMEOUTS_ACTIVE) {
		printf("dpt%d WARNING: Timeout Handling Collision\n",
		       dpt->unit);
		splx(ospl);
		return;
	}
	dpt->state |= DPT_HA_TIMEOUTS_ACTIVE;

	/* Loop through the entire submitted queue, looking for lost souls */
	for (ccb = TAILQ_FIRST(&dpt->submitted_ccbs);
	     ccb != NULL;
	     ccb = TAILQ_NEXT(ccb, links)) {
		struct scsi_xfer *xs;
		struct timeval  now;
		u_int32_t       age, max_age;

		xs = ccb->xs;

		microtime(&now);
		age = dpt_time_delta(ccb->command_started, now);

#define TenSec	10000000

		if (xs == NULL) {	/* Local, non-kernel call */
			max_age = TenSec;
		} else {
			max_age = (((xs->timeout * (dpt->submitted_ccbs_count
						    + DPT_TIMEOUT_FACTOR))
				    > TenSec)
				 ? (xs->timeout * (dpt->submitted_ccbs_count
						   + DPT_TIMEOUT_FACTOR))
				   : TenSec);
		}

		/*
		 * If a transaction is marked lost and is TWICE as old as we
		 * care, then, and only then do we destroy it!
		 */
		if (ccb->state & DPT_CCB_STATE_MARKED_LOST) {
			/* Remember who is next */
			if (age > (max_age * 2)) {
				dpt_Qremove_submitted(dpt, ccb);
				ccb->state &= ~DPT_CCB_STATE_MARKED_LOST;
				ccb->state |= DPT_CCB_STATE_ABORTED;
#define cmd_name scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd)
				if (ccb->retries++ > DPT_RETRIES) {
					printf("dpt%d ERROR: Destroying stale "
					       "%d (%s)\n"
					       "		on "
					       "c%db%dt%du%d (%d/%d)\n",
					     dpt->unit, ccb->transaction_id,
					       cmd_name,
					       dpt->unit,
					       ccb->eata_ccb.cp_channel,
					       ccb->eata_ccb.cp_id,
					       ccb->eata_ccb.cp_LUN, age,
					       ccb->retries);
#define send_ccb &ccb->eata_ccb
#define ESA	 EATA_SPECIFIC_ABORT
					(void) dpt_send_immediate(dpt,
								  send_ccb,
								  ESA,
								  0, 0);
					dpt_Qpush_free(dpt, ccb);

					/* The SCSI layer should re-try */
					xs->error |= XS_TIMEOUT;
					xs->flags |= SCSI_ITSDONE;
					scsi_done(xs);
				} else {
					printf("dpt%d ERROR: Stale %d (%s) on "
					       "c%db%dt%du%d (%d)\n"
					     "		gets another "
					       "chance(%d/%d)\n",
					     dpt->unit, ccb->transaction_id,
					       cmd_name,
					       dpt->unit,
					       ccb->eata_ccb.cp_channel,
					       ccb->eata_ccb.cp_id,
					       ccb->eata_ccb.cp_LUN,
					    age, ccb->retries, DPT_RETRIES);

					dpt_Qpush_waiting(dpt, ccb);
					dpt_sched_queue(dpt);
				}
			}
		} else {
			/*
			 * This is a transaction that is not to be destroyed
			 * (yet) But it is too old for our liking. We wait as
			 * long as the upper layer thinks. Not really, we
			 * multiply that by the number of commands in the
			 * submitted queue + 1.
			 */
			if (!(ccb->state & DPT_CCB_STATE_MARKED_LOST) &&
			    (age != ~0) && (age > max_age)) {
				printf("dpt%d ERROR: Marking %d (%s) on "
				       "c%db%dt%du%d \n"
				       "            as late after %dusec\n",
				       dpt->unit, ccb->transaction_id,
				       cmd_name,
				       dpt->unit, ccb->eata_ccb.cp_channel,
				       ccb->eata_ccb.cp_id,
				       ccb->eata_ccb.cp_LUN, age);
				ccb->state |= DPT_CCB_STATE_MARKED_LOST;
			}
		}
	}

	dpt->state &= ~DPT_HA_TIMEOUTS_ACTIVE;
	splx(ospl);
}

static void
dpt_timeout(void *arg)
{
	dpt_softc_t    *dpt = (dpt_softc_t *) arg;

	if (!(dpt->state & DPT_HA_TIMEOUTS_ACTIVE))
		dpt_handle_timeouts(dpt);

	timeout(dpt_timeout, (caddr_t) dpt, hz * 10);
}

#endif				/* DPT_HANDLE_TIMEOUTS */

/*
 * Remove a ccb from the completed queue
 */
static INLINE_Q void
dpt_Qremove_completed(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
	u_int32_t       complete_time;
	struct timeval  now;

	microtime(&now);
	complete_time = dpt_time_delta(ccb->command_ended, now);

	if (complete_time != ~0) {
		if (dpt->performance.max_complete_time < complete_time)
			dpt->performance.max_complete_time = complete_time;
		if (complete_time < dpt->performance.min_complete_time)
			dpt->performance.min_complete_time = complete_time;
	}
#endif

	TAILQ_REMOVE(&dpt->completed_ccbs, ccb, links);
	--dpt->completed_ccbs_count;	/* One less completed ccb in the
					 * queue */
	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 * Pop the most recently used ccb off the (HEAD of the) FREE ccb queue
 */
static INLINE_Q dpt_ccb_t *
dpt_Qpop_free(dpt_softc_t * dpt)
{
	dpt_ccb_t      *ccb;

	if ((ccb = TAILQ_FIRST(&dpt->free_ccbs)) == NULL) {
		if (dpt_alloc_freelist(dpt))
			return (ccb);
		else
			return (dpt_Qpop_free(dpt));
	} else {
		TAILQ_REMOVE(&dpt->free_ccbs, ccb, links);
		--dpt->free_ccbs_count;
	}

	return (ccb);
}

/**
 * Put a (now freed) ccb back into the HEAD of the FREE ccb queue
 */
static INLINE_Q void
dpt_Qpush_free(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_FREELIST_IS_STACK
	TAILQ_INSERT_HEAD(&dpt->free_ccbs, ccb, links)
#else
	TAILQ_INSERT_TAIL(&dpt->free_ccbs, ccb, links);
#endif

	++dpt->free_ccbs_count;
}

/**
 *	Add a request to the TAIL of the WAITING ccb queue
 */
static INLINE_Q void
dpt_Qadd_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
	struct timeval  junk;

	TAILQ_INSERT_TAIL(&dpt->waiting_ccbs, ccb, links);
	++dpt->waiting_ccbs_count;

#ifdef DPT_MEASURE_PERFORMANCE
	microtime(&junk);
	ccb->command_ended = junk;
	if (dpt->waiting_ccbs_count > dpt->performance.max_waiting_count)
		dpt->performance.max_waiting_count = dpt->waiting_ccbs_count;
#endif

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 *	Add a request to the HEAD of the WAITING ccb queue
 */
static INLINE_Q void
dpt_Qpush_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
	struct timeval  junk;

	TAILQ_INSERT_HEAD(&dpt->waiting_ccbs, ccb, links);
	++dpt->waiting_ccbs_count;

#ifdef DPT_MEASURE_PERFORMANCE
	microtime(&junk);
	ccb->command_ended = junk;

	if (dpt->performance.max_waiting_count < dpt->waiting_ccbs_count)
		dpt->performance.max_waiting_count = dpt->waiting_ccbs_count;

#endif

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 * Remove a ccb from the waiting queue
 */
static INLINE_Q void
dpt_Qremove_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
	struct timeval  now;
	u_int32_t       waiting_time;

	microtime(&now);
	waiting_time = dpt_time_delta(ccb->command_ended, now);

	if (waiting_time != ~0) {
		if (dpt->performance.max_waiting_time < waiting_time)
			dpt->performance.max_waiting_time = waiting_time;
		if (waiting_time < dpt->performance.min_waiting_time)
			dpt->performance.min_waiting_time = waiting_time;
	}
#endif

	TAILQ_REMOVE(&dpt->waiting_ccbs, ccb, links);
	--dpt->waiting_ccbs_count;	/* One less waiting ccb in the queue	 */

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 * Add a request to the TAIL of the SUBMITTED ccb queue
 */
static INLINE_Q void
dpt_Qadd_submitted(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
	struct timeval  junk;

	TAILQ_INSERT_TAIL(&dpt->submitted_ccbs, ccb, links);
	++dpt->submitted_ccbs_count;

#ifdef DPT_MEASURE_PERFORMANCE
	microtime(&junk);
	ccb->command_ended = junk;
	if (dpt->performance.max_submit_count < dpt->submitted_ccbs_count)
		dpt->performance.max_submit_count = dpt->submitted_ccbs_count;
#endif

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 * Add a request to the TAIL of the Completed ccb queue
 */
static INLINE_Q void
dpt_Qadd_completed(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
	struct timeval  junk;

	TAILQ_INSERT_TAIL(&dpt->completed_ccbs, ccb, links);
	++dpt->completed_ccbs_count;

#ifdef DPT_MEASURE_PERFORMANCE
	microtime(&junk);
	ccb->command_ended = junk;
	if (dpt->performance.max_complete_count < dpt->completed_ccbs_count)
		dpt->performance.max_complete_count =
			dpt->completed_ccbs_count;
#endif

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		wakeup(&dpt);
}

/**
 * Remove a ccb from the submitted queue
 */
static INLINE_Q void
dpt_Qremove_submitted(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
	struct timeval  now;
	u_int32_t       submit_time;

	microtime(&now);
	submit_time = dpt_time_delta(ccb->command_ended, now);

	if (submit_time != ~0) {
		ccb->submitted_time = submit_time;
		if (dpt->performance.max_submit_time < submit_time)
			dpt->performance.max_submit_time = submit_time;
		if (submit_time < dpt->performance.min_submit_time)
			dpt->performance.min_submit_time = submit_time;
	} else {
		ccb->submitted_time = 0;
	}

#endif

	TAILQ_REMOVE(&dpt->submitted_ccbs, ccb, links);
	--dpt->submitted_ccbs_count;	/* One less submitted ccb in the
					 * queue */

	if ((dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
	    || (dpt->state & DPT_HA_QUIET))
		wakeup(&dpt);
}

/**
 * Handle Shutdowns.
 * Gets registered by the dpt_pci.c registar and called AFTER the system did
 * all its sync work.
 */

void
dpt_shutdown(int howto, void *arg_dpt)
{
	dpt_softc_t    *ldpt;
	u_int8_t        channel;
	u_int32_t       target;
	u_int32_t       lun;
	int             waiting;
	int             submitted;
	int             completed;
	int             huh;
	int             wait_is_over;
	int             ospl;
	dpt_softc_t    *dpt;

	dpt = (dpt_softc_t *) arg_dpt;

	printf("dpt%d: Shutting down (mode %d) HBA.	Please wait...",
	       dpt->unit, howto);
	wait_is_over = 0;

	ospl = splcam();
	dpt->state |= DPT_HA_SHUTDOWN_ACTIVE;
	splx(ospl);

	while ((((waiting = dpt->waiting_ccbs_count) != 0)
		|| ((submitted = dpt->submitted_ccbs_count) != 0)
		|| ((completed = dpt->completed_ccbs_count) != 0))
	       && (wait_is_over == 0)) {
#ifdef DPT_DEBUG_SHUTDOWN
		printf("dpt%d: Waiting for queues w%ds%dc%d to deplete\n",
		       dpt->unit, dpt->waiting_ccbs_count,
		       dpt->submitted_ccbs_count,
		       dpt->completed_ccbs_count);
#endif
		huh = tsleep((void *) dpt, PCATCH | PRIBIO, "dptoff", 100 * hz);
		switch (huh) {
		case 0:
			/* Wakeup call received */
			goto checkit;
			break;
		case EWOULDBLOCK:
			/* Timer Expired */
			printf("dpt%d: Shutdown timer expired with queues at "
			       "w%ds%dc%d\n",
			       dpt->unit, dpt->waiting_ccbs_count,
			       dpt->submitted_ccbs_count,
			       dpt->completed_ccbs_count);
			++wait_is_over;
			break;
		default:
			/* anything else */
			printf("dpt%d: Shutdown UNKNOWN with qeueues at "
			       "w%ds%dc%d\n",
			       dpt->unit, dpt->waiting_ccbs_count,
			       dpt->submitted_ccbs_count,
			       dpt->completed_ccbs_count);
			++wait_is_over;
			break;
		}
checkit:

	}

	/**
	 * What we do for a shutdown, is give the DPT early power loss
	 * warning
       . */
	(void) dpt_send_immediate(dpt, NULL, EATA_POWER_OFF_WARN, 0, 0);
	printf("dpt%d: Controller was warned of shutdown and is now "
	       "disabled\n",
	       dpt->unit);

	return;
}

/* A primitive subset of isgraph.	Used by hex_dump below */
#define IsGraph(val)	((((val) >= ' ') && ((val) <= '~')))

/**
 * This function dumps bytes to the screen in hex format.
 */
void
hex_dump(u_int8_t * data, int length, char *name, int no)
{
	int             line, column, ndx;

	printf("Kernel Hex Dump for %s-%d at %p (%d bytes)\n",
	       name, no, data, length);

	/* Zero out all the counters and repeat for as many bytes as we have */
	for (ndx = 0, column = 0, line = 0; ndx < length; ndx++) {
		/* Print relative offset at the beginning of every line */
		if (column == 0)
			printf("%04x ", ndx);

		/* Print the byte as two hex digits, followed by a space */
		printf("%02x ", data[ndx]);

		/* Split the row of 16 bytes in half */
		if (++column == 8) {
			printf(" ");
		}
		/* St the end of each row of 16 bytes, put a space ... */
		if (column == 16) {
			printf("	");

			/* ... and then print the ASCII-visible on a line. */
			for (column = 0; column < 16; column++) {
				int             ascii_pos = ndx - 15 + column;

				/**
				 * Non-printable and non-ASCII are just a
				 * dot. ;-(
				 */
				if (IsGraph(data[ascii_pos]))
					printf("%c", data[ascii_pos]);
				else
					printf(".");
			}

			/* Each line ends with a new line */
			printf("\n");
			column = 0;

			/**
			 * Every 256 bytes (16 lines of 16 bytes each) have
			 * an empty line, separating them from the next
			 * ``page''. Yes, I programmed on a Z-80, where a
			 * page was 256 bytes :-)
			 */
			if (++line > 15) {
				printf("\n");
				line = 0;
			}
		}
	}

	/**
	 * We are basically done. We do want, however, to handle the ASCII
	 * translation of fractional lines.
	 */
	if ((ndx == length) && (column != 0)) {
		int             modulus = 16 - column, spaces = modulus * 3,
		                skip;

		/**
		 * Skip to the right, as many spaces as there are bytes
		 * ``missing'' ...
		 */
		for (skip = 0; skip < spaces; skip++)
			printf(" ");

		/* ... And the gap separating the hex dump from the ASCII */
		printf("  ");

		/**
		 * Do not forget the extra space that splits the hex dump
		 * vertically
		 */
		if (column < 8)
			printf(" ");

		for (column = 0; column < (16 - modulus); column++) {
			int             ascii_pos = ndx - (16 - modulus) + column;

			if (IsGraph(data[ascii_pos]))
				printf("%c", data[ascii_pos]);
			else
				printf(".");
		}
		printf("\n");
	}
}

/**
 * and this one presents an integer as ones and zeros
 */
static char     i2bin_bitmap[48];	/* Used for binary dump of registers */

char           *
i2bin(unsigned int no, int length)
{
	int             ndx, rind;

	for (ndx = 0, rind = 0; ndx < 32; ndx++, rind++) {
		i2bin_bitmap[rind] = (((no << ndx) & 0x80000000) ? '1' : '0');

		if (((ndx % 4) == 3))
			i2bin_bitmap[++rind] = ' ';
	}

	if ((ndx % 4) == 3)
		i2bin_bitmap[rind - 1] = '\0';
	else
		i2bin_bitmap[rind] = '\0';

	switch (length) {
	case 8:
		return (i2bin_bitmap + 30);
		break;
	case 16:
		return (i2bin_bitmap + 20);
		break;
	case 24:
		return (i2bin_bitmap + 10);
		break;
	case 32:
		return (i2bin_bitmap);
	default:
		return ("i2bin: Invalid length Specs");
		break;
	}
}

/**
 * This function translates a SCSI command numeric code to a human readable
 * string.
 * The string contains the class of devices, scope, description, (length),
 * and [SCSI III documentation section].
 */

char           *
scsi_cmd_name(u_int8_t cmd)
{
	switch (cmd) {
		case 0x40:
		return ("Change Definition [7.1]");
		break;
	case 0x39:
		return ("Compare [7,2]");
		break;
	case 0x18:
		return ("Copy [7.3]");
		break;
	case 0x3a:
		return ("Copy and Verify [7.4]");
		break;
	case 0x04:
		return ("Format Unit [6.1.1]");
		break;
	case 0x12:
		return ("Inquiry [7.5]");
		break;
	case 0x36:
		return ("lock/Unlock Cache [6.1.2]");
		break;
	case 0x4c:
		return ("Log Select [7.6]");
		break;
	case 0x4d:
		return ("Log Sense [7.7]");
		break;
	case 0x15:
		return ("Mode select (6) [7.8]");
		break;
	case 0x55:
		return ("Mode Select (10) [7.9]");
		break;
	case 0x1a:
		return ("Mode Sense (6) [7.10]");
		break;
	case 0x5a:
		return ("Mode Sense (10) [7.11]");
		break;
	case 0xa7:
		return ("Move Medium Attached [SMC]");
		break;
	case 0x5e:
		return ("Persistent Reserve In [7.12]");
		break;
	case 0x5f:
		return ("Persistent Reserve Out [7.13]");
		break;
	case 0x1e:
		return ("Prevent/Allow Medium Removal [7.14]");
		break;
	case 0x08:
		return ("Read, Receive (6) [6.1.5]");
		break;
	case 0x28:
		return ("Read (10) [6.1.5]");
		break;
	case 0xa8:
		return ("Read (12) [6.1.5]");
		break;
	case 0x3c:
		return ("Read Buffer [7.15]");
		break;
	case 0x25:
		return ("Read Capacity [6.1.6]");
		break;
	case 0x37:
		return ("Read Defect Data (10) [6.1.7]");
		break;
	case 0xb7:
		return ("Read Defect Data (12) [6.2.5]");
		break;
	case 0xb4:
		return ("Read Element Status Attached [SMC]");
		break;
	case 0x3e:
		return ("Read Long [6.1.8]");
		break;
	case 0x07:
		return ("Reassign Blocks [6.1.9]");
		break;
	case 0x81:
		return ("Rebuild [6.1.10]");
		break;
	case 0x1c:
		return ("Receive Diagnostics Result [7.16]");
		break;
	case 0x82:
		return ("Regenerate [6.1.11]");
		break;
	case 0x17:
		return ("Release(6) [7.17]");
		break;
	case 0x57:
		return ("Release(10) [7.18]");
		break;
	case 0xa0:
		return ("Report LUNs [7.19]");
		break;
	case 0x03:
		return ("Request Sense [7.20]");
		break;
	case 0x16:
		return ("Resereve (6) [7.21]");
		break;
	case 0x56:
		return ("Reserve(10) [7.22]");
		break;
	case 0x2b:
		return ("Reserve(10) [6.1.12]");
		break;
	case 0x1d:
		return ("Send Disagnostics [7.23]");
		break;
	case 0x33:
		return ("Set Limit (10) [6.1.13]");
		break;
	case 0xb3:
		return ("Set Limit (12) [6.2.8]");
		break;
	case 0x1b:
		return ("Start/Stop Unit [6.1.14]");
		break;
	case 0x35:
		return ("Synchronize Cache [6.1.15]");
		break;
	case 0x00:
		return ("Test Unit Ready [7.24]");
		break;
	case 0x3d:
		return ("Update Block (6.2.9");
		break;
	case 0x2f:
		return ("Verify (10) [6.1.16, 6.2.10]");
		break;
	case 0xaf:
		return ("Verify (12) [6.2.11]");
		break;
	case 0x0a:
		return ("Write, Send (6) [6.1.17, 9.2]");
		break;
	case 0x2a:
		return ("Write (10) [6.1.18]");
		break;
	case 0xaa:
		return ("Write (12) [6.2.13]");
		break;
	case 0x2e:
		return ("Write and Verify (10) [6.1.19, 6.2.14]");
		break;
	case 0xae:
		return ("Write and Verify (12) [6.1.19, 6.2.15]");
		break;
	case 0x03b:
		return ("Write Buffer [7.25]");
		break;
	case 0x03f:
		return ("Write Long [6.1.20]");
		break;
	case 0x041:
		return ("Write Same [6.1.21]");
		break;
	case 0x052:
		return ("XD Read [6.1.22]");
		break;
	case 0x050:
		return ("XD Write [6.1.22]");
		break;
	case 0x080:
		return ("XD Write Extended [6.1.22]");
		break;
	case 0x051:
		return ("XO Write [6.1.22]");
		break;
	default:
		return ("Unknown SCSI Command");
	}
}

/* End of the DPT driver */

/**
 * Hello emacs, these are the
 * Local Variables:
 *  c-indent-level:               8
 *  c-continued-statement-offset: 8
 *  c-continued-brace-offset:     0
 *  c-brace-offset:              -8
 *  c-brace-imaginary-offset:     0
 *  c-argdecl-indent:             8
 *  c-label-offset:              -8
 *  c++-hanging-braces:           1
 *  c++-access-specifier-offset: -8
 *  c++-empty-arglist-indent:     8
 *  c++-friend-offset:            0
 * End:
 */

/*
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

/*
 *
 * dpt_scsi.c: SCSI dependant code for the DPT driver
 *
 * credits:	Assisted by Mike Neuffer in the early low level DPT code
 *		Thanx to Mark Salyzyn of DPT for his assistance.
 *              Ron MacDaniels wrote the scsi software interrupts.
 *		Special thanx to Justin Gibbs for invaluable help in
 *		making this driver look and work like a FreeBSD component.
 *		Last but not least, many thanx for UCB and the FreeBSD
 *		team for creating and maintaining such a wonderful O/S.
 *
 * TODO:     * Add EISA and ISA probe code.
 *	     * Replace most splbio() critical sections with specific locks.
 *	       As it stands now, for example, while the completed queue is
 *	       scanned, it is in splbio, to make sure that two interrupts do
 *	       not try to complete the same request (happens!). As a result
 *	       nobody can do anything here while the completions queue is run
 *	     * Add IOCTL support to allow userland calls to the DPT.	Needed
 *   	       by the dptmgr.
 *	     * Add driver-level RSID-0. This will allow interoperability with
 *	       NiceTry, M$-Doze, Win-Dog, Slowlaris, etc. in recognizing RAID
 *	       arrays that span controllers (Wow!).
 *
 */

/*
 * IMPORTANT:
 *	There are two critical section "levels" available in this driver:
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
 *
 */


#ident "$Id: dpt_scsi.c,v 1.4.2.1 1998/03/06 23:44:13 julian Exp $"
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

#include <dev/dpt/softisr.h>

#include <sys/reboot.h>

#include <dev/dpt/dpt.h>

#ifdef INLINE
#undef INLINE
#endif

#define INLINE	__inline

#ifdef DPT_USE_DPT_SWI
#define splcam         splbio
#define splsoftcam     splbio
#endif

/* Function Prototypes */

static INLINE u_int32_t	 dpt_inl(dpt_softc_t *dpt, u_int32_t offset);
static INLINE u_int8_t	 dpt_inb(dpt_softc_t *dpt, u_int32_t offset);
static INLINE void	 dpt_outb(dpt_softc_t *dpt, u_int32_t offset,
				  u_int8_t value );
static INLINE void	 dpt_outl(dpt_softc_t *dpt, u_int32_t offset,
				  u_int32_t value );
static INLINE void	 dpt_Qpush_free(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE dpt_ccb_t *dpt_Qpop_free(dpt_softc_t * dpt);
static INLINE void	 dpt_Qadd_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE void	 dpt_Qpush_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb);
static INLINE void	 dpt_Qremove_waiting(dpt_softc_t * dpt,
					     dpt_ccb_t * ccb);
static INLINE void	 dpt_Qadd_submitted(dpt_softc_t * dpt,
					    dpt_ccb_t * ccb);
static INLINE void	 dpt_Qremove_submitted(dpt_softc_t *dpt,
					       dpt_ccb_t * ccb);
static INLINE void	 dpt_Qadd_completed(dpt_softc_t * dpt,
					    dpt_ccb_t * ccb);
static INLINE void	 dpt_Qremove_completed(dpt_softc_t * dpt,
					       dpt_ccb_t * ccb);
static INLINE int 	 dpt_send_eata_command(eata_ccb_t *addr,
					       dpt_softc_t *dpt,
					       u_int8_t command,
					       int32_t retries);
static INLINE int	 dpt_send_immediate(dpt_softc_t *dpt, u_int32_t addr,
					    u_int8_t ifc, u_int8_t code,
					    u_int8_t code2);
static INLINE int	 dpt_just_reset(dpt_softc_t *dpt);
static INLINE int	 dpt_raid_busy(dpt_softc_t *dpt);
static INLINE void	 dpt_sched_queue(dpt_softc_t *dpt);
static INLINE void	 dpt_IObySize(dpt_softc_t *dpt, dpt_ccb_t *ccb,
				      int op, int index);

#ifndef DPT_USE_DPT_SWI
static  void dpt_swi_register(void *);
#endif

#ifdef DPT_HANDLE_TIMEOUTS
static void dpt_handle_timeouts(dpt_softc_t *dpt);
static void dpt_timeout(void *dpt);
#endif

typedef struct scsi_inquiry_data s_inq_data_t;


static int		 dpt_scatter_gather(dpt_softc_t *dpt, dpt_ccb_t *ccb,
					    u_int32_t data_length,
					    caddr_t data);
static u_int32_t	 dpt_time_delta(struct timeval start,
					struct timeval end);
static int		 dpt_alloc_freelist(dpt_softc_t * dpt);
static void		 dpt_run_queue(dpt_softc_t *dpt, int requests);
static void		 dpt_complete(dpt_softc_t * dpt);
static int		 dpt_process_completion(dpt_softc_t * dpt,
						dpt_ccb_t * ccb);
static s_inq_data_t	*dpt_inquire_device(dpt_softc_t * dpt,
					    u_int8_t channel,
					    u_int32_t target_id,
					    u_int8_t lun);

u_int8_t	 dpt_blinking_led(dpt_softc_t *dpt);
int		 dpt_user_cmd(dpt_softc_t *dpt, eata_pt_t *user_cmd,
			      caddr_t cmdarg, int minor_no);
void		 dpt_detect_cache(dpt_softc_t *dpt);
void		 dpt_shutdown(int howto, void *dpt);
void		 hex_dump(u_int8_t * data, int length, char *name, int no);
char		*i2bin(unsigned int no, int length);
dpt_conf_t	*dpt_get_conf(dpt_softc_t *dpt, u_int8_t page, u_int8_t target,
			      u_int8_t size, int extent);
dpt_inq_t	*dpt_get_board_data(dpt_softc_t * dpt, u_int32_t target_id);
int		 dpt_setup(dpt_softc_t * dpt, dpt_conf_t * conf);
int		 dpt_attach(dpt_softc_t * dpt);
int32_t		 dpt_scsi_cmd(struct scsi_xfer * xs);
void		 dptminphys(struct buf * bp);
void		 dpt_sintr(void);
void		 dpt_intr(void *arg);
char		*scsi_cmd_name(u_int8_t cmd);

extern  void    (*ihandlers[32]) __P((void));

u_long	dpt_unit;	/* This one is kernel-related, do not touch! */

/* The linked list of softc structures */
TAILQ_HEAD(,dpt_softc) dpt_softc_list = TAILQ_HEAD_INITIALIZER(dpt_softc_list);

/* These will have to be setup by parameters passed at boot/load time.
 * For perfromance reasons, we make them constants for the time being. */
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
    NULL,		/* Use default error handler */
    NULL,		/* have a queue, served by this */
    NULL,		/* have no async handler */
    NULL,		/* Use default 'done' routine */
    "dpt",
    0,
    {0, 0}
};

/* Software Interrupt Vector */
DPTISR_SET(DPTISR_DPT, dpt_sintr)  /* You need at least one reference */
#ifndef DPT_USE_DPT_SWI
SYSINIT(dpt_camswi, SI_SUB_DRIVERS, SI_ORDER_FIRST, dpt_swi_register, NULL)

static void    
dpt_swi_register(void *unused)
{
    ihandlers[SWI_CAMBIO] = dpt_sintr;
}
#endif

/* These functions allows us to do memory mapped I/O, if hardware supported. */

static INLINE u_int8_t
dpt_inb(dpt_softc_t *dpt, u_int32_t offset)
{
    volatile u_int8_t result;

    if ( dpt->v_membase != NULL ) {
	result = dpt->v_membase[offset];
    } else {
	result = inb(dpt->io_base + offset);
    }
    return(result);
}

static INLINE u_int32_t
dpt_inl(dpt_softc_t *dpt, u_int32_t offset)
{
    volatile u_int32_t result;

    if ( dpt->v_membase != NULL ) {
	result = dpt->v_membase[offset];
    } else {
	result = inl(dpt->io_base + offset);
    }
    return(result);
}

static INLINE void
dpt_outb(dpt_softc_t *dpt, u_int32_t offset, volatile u_int8_t value)
{
    if ( dpt->v_membase != NULL ) {
	dpt->v_membase[offset] = value;
    } else {
	outb(dpt->io_base + offset, value);
    }
}

static INLINE void
dpt_outl(dpt_softc_t *dpt, u_int32_t offset, volatile u_int32_t value)
{
    if ( dpt->v_membase != NULL ) {
	dpt->v_membase[offset] = value;
    } else {
	outl(dpt->io_base + offset, value);
    }
}

/*
 * XXX JGibbs - The interrupt scheme now requries that you use the software
 *		interrupt at all times.  This is because only the software
 *		interrupt handler is registered in the bio_imask meaning
 *		that to the rest of the system blocking our software interrupt
 *		is the way to lock out the driver.  This has the added benefit
 *		of allowing our hardware interrupts to be processed at almost
 *		any time (anything but splhigh) gaining additional performance.
 *		Since I'm guessing that the "knob" to control the use of
 *		software interrupts was only for initial performance assesments
 *		I think losing this control is worth the added gains.
 */

/* Wrapper around queue processing.
 * If software interrupts are ON an sintr is scheduled.
 * Otherwise, the queue is walked.
 */

static INLINE void
dpt_sched_queue(dpt_softc_t *dpt)
{
    if ( dpt->state & DPT_HA_QUIET ) {
	printf("dpt%d: Under Quiet Busses Condition.  "
	       "No Commands are submitted\n", dpt->unit);
	return;
    }
#ifdef DPT_USE_DPT_SWI
    scheddptisr(DPTISR_DPT)
    
#else
    setsoftcambio();
#endif /* DPT_USE_DPT_SWI */
}

/*
 * This function waits for the status of the controller
 * to CHANGE to a certain state
 */

static INLINE int
dpt_wait(dpt_softc_t *dpt, u_int8_t bits, u_int8_t state)
{
    int		i;
    u_int8_t	c;

    for (i = 0; i < 400; i++ ) {	/* wait 20ms for not busy */
	c = dpt_inb(dpt, HA_RSTATUS) & bits;
	if (c == state)
	    return (0);
	DELAY(50);	/* 50 us */
    }
    return (-1);
}

static INLINE int
dpt_just_reset(dpt_softc_t *dpt)
{
    if ( (dpt_inb(dpt, 2) == 'D')
	 && (dpt_inb(dpt, 3) == 'P')
	 && (dpt_inb(dpt, 4) == 'T')
	 && (dpt_inb(dpt, 5) == 'H') )
	return (1);
    else
	return (0);
}

static INLINE int
dpt_raid_busy(dpt_softc_t *dpt)
{
    if ( (dpt_inb(dpt, 0) == 'D')
	 && (dpt_inb(dpt, 1) == 'P')
	 && (dpt_inb(dpt, 2) == 'T') )
	return (1);
    else
	return (0);
}

/*
 * This routine will try to send a eata command to the DPT HBA.
 * It will, by default, try AHZ times, waiting 10ms between tries.
 * It returns 0 on success and 1 on failure.
 * It assumes the caller protects it with splbio() or some such.
 *
 * IMPORTANT:  We do NOT protect the ports from multiple access in here.
 *             You are expected to do it in the calling routine.
 *             Here, we cannot have any clue as to the scope of your work.
 */

static INLINE int
dpt_send_eata_command(eata_ccb_t *cmd_block, dpt_softc_t *dpt,
		      u_int8_t command, int32_t retries)	
{
    int32_t	loop;
    u_int8_t	result;
    u_int32_t	test;

    if (!retries)
	retries = 100;

    for (loop = 0; loop < retries; loop++) {
	if (!(dpt_inb(dpt, HA_RAUXSTAT) & HA_ABUSY))
	    break;
	
	/* 
	 * I hate this polling nonsense.
	 * Wish there was a way to tell the DPT to go get commands at its
	 * own pace, 
	 * or to interrupt when ready.
	 * In the mean time we will measure how many itterations it really
	 * takes.
	 */
	DELAY(10);
    }
	
    if (loop < retries) {
#ifdef DPT_MEASURE_PERFORMANCE
	if ( loop > dpt->performance.max_eata_tries )
		dpt->performance.max_eata_tries = loop;
	
	if ( dpt->performance.min_eata_tries
	     && (loop < dpt->performance.min_eata_tries) )
	    dpt->performance.min_eata_tries = loop;
#endif
    } else {
	/*
	 * XXX - JGibbs: You'll never get here, you know.
	 * YYY - * Simon: Actually we do. Occasionally.
	 * Not that we want to :-)
	 */
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.command_too_busy;
#endif
	return(1);
    }

    if (cmd_block != NULL)
	cmd_block = (void *) vtophys(cmd_block);
	
    /* And now the address in nice little byte chunks */

#if (BYTE_ORDER == LITTLE_ENDIAN)
    dpt_outb(dpt, HA_WDMAADDR, (u_long) cmd_block);
    dpt_outb(dpt, HA_WDMAADDR + 1, (u_long) cmd_block >> 8);
    dpt_outb(dpt, HA_WDMAADDR + 2, (u_long) cmd_block >> 16);
    dpt_outb(dpt, HA_WDMAADDR + 3, (u_long) cmd_block >> 24);
#else
    dpt_outb(dpt, HA_WDMAADDR, (u_long) cmd_block >> 24);
    dpt_outb(dpt, HA_WDMAADDR + 1, (u_long) cmd_block >> 16);
    dpt_outb(dpt, HA_WDMAADDR + 2, (u_long) cmd_block >> 8);
    dpt_outb(dpt, HA_WDMAADDR + 3, (u_long) cmd_block);
#endif
    dpt_outb(dpt, HA_WCOMMAND, command);

    return (0);
}

/*
 * Seend a command for immediate execution by the DPT
 * See above function for IMPORTANT notes.
 */ 

static INLINE int
dpt_send_immediate(dpt_softc_t *dpt, u_int32_t cmd_block, u_int8_t ifc,
					u_int8_t code, u_int8_t code2)
{
    int loop;
    int ospl;

    for (loop = 0; loop < 1000000; loop++) {
	if (!(dpt_inb(dpt, HA_RAUXSTAT) & HA_ABUSY))
	    break;
	else
	    DELAY(10);
    }

    if (loop > 1000000) {
	printf("dpt%d WARNING: HBA at %x failed to become ready\n",
	       dpt->unit, BaseRegister(dpt));
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.command_too_busy;
#endif
	return(1);
    }
    if (cmd_block != (u_int32_t) NULL)
	cmd_block = vtophys((void *) cmd_block);
    
    dpt_outb(dpt, HA_WDMAADDR - 1, 0x0);
    if (cmd_block) {
#if (BYTE_ORDER == LITTLE_ENDIAN)
	dpt_outb(dpt, HA_WDMAADDR, cmd_block);
	dpt_outb(dpt, HA_WDMAADDR + 1, cmd_block >> 8);
	dpt_outb(dpt, HA_WDMAADDR + 2, cmd_block >> 16);
	dpt_outb(dpt, HA_WDMAADDR + 3, cmd_block >> 24);
#else
	dpt_outb(dpt, HA_WDMAADDR, cmd_block >> 24);
	dpt_outb(dpt, HA_WDMAADDR + 1, cmd_block >> 16);
	dpt_outb(dpt, HA_WDMAADDR + 2, cmd_block >> 8);
	dpt_outb(dpt, HA_WDMAADDR + 3, cmd_block);
#endif
    } else {
	dpt_outb(dpt, HA_WDMAADDR, 0x0);
	dpt_outb(dpt, HA_WDMAADDR + 1, 0x0);
	dpt_outb(dpt, HA_WCODE2, code2);
	dpt_outb(dpt, HA_WCODE, code);
    }
	
    dpt_outb(dpt, HA_WIFC, ifc);
    dpt_outb(dpt, HA_WCOMMAND, EATA_CMD_IMMEDIATE);
    return(0);
}


/* Return the state of the blinking DPT LED's */
u_int8_t
dpt_blinking_led(dpt_softc_t *dpt)
{
    int       ndx;
    int       ospl;
    u_int32_t state;
    u_int32_t previous;
    u_int8_t  result;

    ospl = splcam();

    for ( ndx = 0, state = 0, previous = 0;
	  (ndx < 10) && (state != previous);
	  ndx++) {
	previous = state;
	state = dpt_inl(dpt, 1);
    }

    if ( (state == previous) && (state == DPT_BLINK_INDICATOR) ) {
	result = dpt_inb(dpt, 5);
	splx(ospl);
	return(result & 0xff);
    }

    splx(ospl);
    return(0);
}

/*
 * Execute a command which did not come from the kernel's SCSI layer.
 * The only way to map user commands to bus and target is to comply with the
 * standard DPT wire-down scheme:
 */

int
dpt_user_cmd(dpt_softc_t *dpt, eata_pt_t *user_cmd,
	     caddr_t cmdarg, int minor_no)
{
    int		 channel, target, lun;
    int		 huh;
    int		 result;
    int		 ospl;
    int		 submitted;
    dpt_ccb_t	*ccb;
    void	*data;
    struct	 timeval now;

    data    = NULL;
    channel = minor2hba(minor_no);
    target  = minor2target(minor_no);
    lun     = minor2lun(minor_no);

    if ( (channel > (dpt->channels - 1))
	 || (target > dpt->max_id)
	 || (lun > dpt->max_lun) )
	return(ENXIO);

    if (target == dpt->sc_scsi_link[channel].adapter_targ) {
	/* This one is for the controller itself */
	if ( (user_cmd->eataID[0] != 'E')
	     || (user_cmd->eataID[1] != 'A')
	     || (user_cmd->eataID[2] != 'T')
	     || (user_cmd->eataID[3] != 'A')) {
	    return(ENXIO);
	}
    }
    
    /* Get a DPT CCB, so we can prepare a command */
    ospl = splsoftcam();
	
    /* Process the free list */
    if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
	printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
	       "			Please try later\n",
	       dpt->unit);
	splx(ospl);
	return (EFAULT);
    }
    
    /* Now grab the newest CCB */
    if ( (ccb = dpt_Qpop_free(dpt)) == NULL) {
	splx(ospl);
	panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
    }
#ifdef DPT_HANDLE_TIMEOUTS
    else {
	splx(ospl);
	/* Clean up the leftover of the previous tenant */
	ccb->status &= ~(DPT_CCB_STATE_ABORTED | DPT_CCB_STATE_MARKED_LOST);
    }
#endif
	
#ifdef DPT_TRACK_CCB_STATES
    ccb->state |= DPT_CCB_STATE_FREE2WAITING;
#endif
    bcopy((caddr_t)&user_cmd->command_packet, (caddr_t)&ccb->eata_ccb,
	  sizeof(eata_ccb_t));

    /* We do not want to do user specified scatter/gather.  Why?? */
    if ( ccb->eata_ccb.scatter == 1 )
	return(EINVAL);

    ccb->eata_ccb.Auto_Req_Sen = 1;
    ccb->eata_ccb.reqlen = htonl(sizeof(struct scsi_sense_data));
    ccb->eata_ccb.cp_datalen = htonl(sizeof(eata_ccb_t));
    ccb->eata_ccb.cp_dataDMA = htonl(vtophys(ccb->eata_ccb.cp_dataDMA));
    ccb->eata_ccb.cp_statDMA = htonl(vtophys(&ccb->eata_ccb.cp_statDMA));
    ccb->eata_ccb.cp_reqDMA = htonl(vtophys(&ccb->eata_ccb.cp_reqDMA));
    ccb->eata_ccb.cp_viraddr = (u_int32_t) &ccb;

    if ( ccb->eata_ccb.DataIn || ccb->eata_ccb.DataOut ) {
	if ( (data = contigmalloc(ccb->eata_ccb.cp_datalen,
				  M_TEMP, M_WAITOK, 0, 0xffffffff, PAGE_SIZE,
				  0x10000)) == NULL ) {
	    if ( (ccb->eata_ccb.cp_datalen > PAGE_SIZE)
		 || (data = malloc(ccb->eata_ccb.cp_datalen, M_TEMP,
				   M_WAITOK)) == NULL){
		printf("dpt%d: Cannot allocate %d bytes for EATA command\n",
		       dpt->unit, ccb->eata_ccb.cp_datalen);
		return(EFAULT);
	    }	
	}

	if ( ccb->eata_ccb.DataIn == 1 ) {
	    if ( copyin((caddr_t)user_cmd->command_packet.cp_dataDMA, data,
			ccb->eata_ccb.cp_datalen) == -1 )
		return(EFAULT);
	}
    } else {
	ccb->eata_ccb.cp_datalen = 0;
	data = NULL;
    }

    if ( ccb->eata_ccb.FWNEST == 1 )
	ccb->eata_ccb.FWNEST = 0;
    
    if ( ccb->eata_ccb.cp_datalen != 0 ) {
	if (dpt_scatter_gather(dpt, ccb, ccb->eata_ccb.cp_datalen,
			       data) != 0) {
	    if ( data != NULL )
		free(data, M_TEMP);
	    return(EFAULT);
	}
	
    }

    /*
     * We are required to quiet a SCSI bus.
     * since we do not queue comands on a bus basis,
     * we wait for ALL commands on a controller to complete.
     * In the mean time, sched_queue() will not schedule new commands.
     */
    
    if ( (ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	 && (ccb->eata_ccb.cp_cdb[2] == BUS_QUIET) ) {
	/* We wait for ALL traffic for this HBa to subside */
	ospl = splsoftcam();
	dpt->state |= DPT_HA_QUIET;
	splx(ospl);
	
	while ( (submitted = dpt->submitted_ccbs_count) != 0 ) {
#ifdef DPT_DEBUG_USER_CMD
	    printf("dpt%d: Waiting for the Submitted Queues (%d) to deplete\n",
		   dpt->unit, dpt->submitted_ccbs_count);
#endif
	    huh = tsleep((void *)dpt, PCATCH | PRIBIO, "dptqt", 100 * hz);
	    switch ( huh ) {
	    case 0:
		/* Wakeup call received */
		break;
	    case EWOULDBLOCK:
		/* Timer Expired */
#ifdef DPT_DEBUG_USER_CMD
		printf("dpt%d: Still Waiting to Quiet the SCSI Busses. "
		       "Submit Queue depth is now %d\n",
		       dpt->unit, dpt->submitted_ccbs_count);
#endif
		break;
	    default:
		/* anything else */
#ifdef DPT_DEBUG_USER_CMD
		printf("dpt%d: UNKNOWN wakeup with qeueues at w%ds%dc%d\n",
		       dpt->unit, dpt->waiting_ccbs_count,
		       dpt->submitted_ccbs_count,
		       dpt->completed_ccbs_count);
#endif
		break;
	    }
	}
    }

    /* Resume normal operation */
    if ( (ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	 && (ccb->eata_ccb.cp_cdb[2] == BUS_UNQUIET) ) {
	ospl = splsoftcam();
	dpt->state &= ~DPT_HA_QUIET;
	splx(ospl);
	dpt_sched_queue(dpt);
    }

    /*
     * Schedule the command and submit it.
     * We bypass dpt_sched_queue, as it will block on DPT_HA_QUIET
     */
    ccb->xs = NULL;
    ccb->flags = 0;	
    ccb->eata_ccb.Auto_Req_Sen = 1;	/* We always want this feature */
    
    ospl = splcam();
#ifdef DPT_DEBUG_USER_CMD
    printf("dpt%d: user_cmd() Queuing %s command on c%db%dt%du%d\n",
	   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
	   dpt->unit, ccb->eata_ccb.cp_channel,
	   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
#endif
    ccb->transaction_id = ++dpt->commands_processed;
#ifdef DPT_MEASURE_PERFORMANCE
    ++dpt->performance.command_count[(int)ccb->eata_ccb.cp_scsi_cmd];
    microtime(&now);
    ccb->command_started = now;
#endif
    dpt_Qadd_waiting(dpt, ccb);
    splx(ospl);

    /* We could consider spl protecting the whole loop, but it sleeps! */
    dpt_sched_queue(dpt);
    while ( !(ccb->state & DPT_CCB_STATE_COMPLETED) ) {
	huh = tsleep((void *)ccb, PCATCH | PRIBIO, "dptucw", 100 * hz);
	ospl = splsoftcam();
	switch ( huh ) {
	case 0: /* We woke up */
	    /* If Auto Request Sense is on, copyout the sense struct */
	    if ( ccb->eata_ccb.Auto_Req_Sen == 1 ) {
		if ( copyout((caddr_t)&ccb->sense_data,
			     (caddr_t)user_cmd->command_packet.cp_reqDMA,
			     sizeof(struct	scsi_sense_data)) ) {
		    if ( data != NULL )
			free(data, M_TEMP);
		    dpt_Qpush_free(dpt, ccb);
		    splx(ospl);
		    return(EFAULT);
		}	
	    }
		
	    /* If DataIn is on, copyout the data */
	    if ( (ccb->eata_ccb.DataIn == 1)
		 && (ccb->status_packet.hba_stat == HA_NO_ERROR) ) {
		if ( copyout(data,
			     (caddr_t)user_cmd->command_packet.cp_dataDMA,
			     user_cmd->command_packet.cp_datalen) ) {
		    if ( data != NULL )
			free(data, M_TEMP);
		    dpt_Qpush_free(dpt, ccb);
		    splx(ospl);
		    return(EFAULT);
		}	
	    }

	    /* Copyout the status */
	    result = ccb->status_packet.hba_stat;
		
	    if ( copyout((caddr_t)&result, cmdarg, sizeof(result)) ) {
		if ( data != NULL )
		    free(data, M_TEMP);
		dpt_Qpush_free(dpt, ccb);
		splx(ospl);
		return(EFAULT);
	    }

	    /* Put the CCB back in the freelist */
#ifdef DPT_TRACK_CCB_STATES
	    ccb->state |= DPT_CCB_STATE_DONE;
#endif		
	    dpt_Qpush_free(dpt, ccb);
		
	    /* Free allocated memory */
	    if ( data != NULL )
		free(data, M_TEMP);

	    splx(ospl);
	    return(0);
	    break;
	case EWOULDBLOCK: /* Timer Expired */
#ifdef DPT_DEBUG_USER_CMD
	    printf("dpt%d: timer expired in user_cmd and but not done\n",
		   dpt->unit);
#endif
	    break;
	default:
	    printf("dpt%d: Unknown reason %x to wakeup in user_cmd()\n",
		   dpt->unit, huh);

	    break;
	}
	splx(ospl);
    }

    /* Free allocated memory */
    if ( data != NULL )
	free(data, M_TEMP);
	
    return(0);
}

/* Detect Cache parameters and size */

void
dpt_detect_cache(dpt_softc_t *dpt)
{
    int		 size;
    int		 bytes;
    int		 result;
    int		 ospl;
    int		 ndx;
    u_int8_t	 status;
    char	 name[64];
    char	*param;
    char	*buff;
    eata_ccb_t	 cp;
    
    volatile dpt_sp_t      sp;
    struct scsi_sense_data snp;

    /*
     * We lock out the hardware early, so that we can either complete the
     * operation or bust out right away.
     */

    sprintf(name, "FreeBSD DPT Driver, version %d.%d.%d",
	    DPT_RELEASE, DPT_VERSION, DPT_PATCH);

    /*
     * Default setting, for best perfromance..
     * This is what virtually all cards default to..
     */
    dpt->cache_type = DPT_CACHE_WRITEBACK;
    dpt->cache_size = 0;

    if ((buff =	malloc(512, M_DEVBUF, M_NOWAIT)) == NULL) {
	printf("dpt%d: Failed to allocate %d bytes for a work buffer\n",
	       dpt->unit, 512);
	return;
    }

    bzero(&cp, sizeof(eata_ccb_t));
    bzero((int8_t *) &sp, sizeof(dpt_sp_t));
    bzero((int8_t *) &snp, sizeof(struct scsi_sense_data));
    bzero(buff, 512);

    /* Setup the command structure */
    cp.Interpret	= 1;
    cp.DataIn		= 1;
    cp.Auto_Req_Sen	= 1;
    cp.reqlen		= (u_int8_t) sizeof(struct scsi_sense_data);
	
    cp.cp_id	= 0;	/* who cares?  The HBA will interpret.. */
    cp.cp_LUN	= 0;	/* In the EATA packet */
    cp.cp_lun	= 0;	/* In the SCSI command */
    cp.cp_channel	= 0;
	
    cp.cp_scsi_cmd	= EATA_CMD_DMA_SEND_CP;
    cp.cp_len		= 56;
    cp.cp_dataDMA	= htonl(vtophys(buff));
    cp.cp_statDMA	= htonl(vtophys(&sp));
    cp.cp_reqDMA	= htonl(vtophys(&snp));

    cp.cp_identify	= 1;
    cp.cp_dispri	= 1;

    /*
     * Build the EATA Command Packet structure
     * for a Log Sense Command.
     */

    cp.cp_cdb[0]	= 0x4d;
    cp.cp_cdb[1]	= 0x0;
    cp.cp_cdb[2]	= 0x40 | 0x33;
    cp.cp_cdb[7]	= 1;

    cp.cp_datalen = htonl(512);

    ospl = splcam();
    
    if ((result = dpt_send_eata_command(&cp, dpt, EATA_CMD_DMA_SEND_CP, 10000))
	!= 0) {
	printf("dpt%d WARNING: detect_cache() failed (%d) to send "
	       "EATA_CMD_DMA_SEND_CP\n", dpt->unit, result);
	free(buff, M_TEMP);
	splx(ospl);
	return;
    }

    /* Wait for two seconds for a response */
    for (ndx = 0;
	 (ndx < 20000) && !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	 ndx++ ) {
	DELAY(10);
    }
	
    /* Grab the status and clear interrupts */
    status = dpt_inb(dpt, HA_RSTATUS);
    splx(ospl);
    
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d detect_cache() status = %s, ndx = %d\n",
	   dpt->unit, i2bin((unsigned long) status, sizeof(status) * 8), ndx);
#ifdef DPT_DEBUG_HEX_DUMPS
    hex_dump((u_int8_t *) buff, 512, "Detect Cache", __LINE__);
    hex_dump((u_int8_t *) & sp, sizeof(dpt_sp_t), "StatusPacket", __LINE__);
    hex_dump((u_int8_t *) & snp, sizeof(struct scsi_sense_data), "SenseData",
	     __LINE__);
#endif
#endif
	
    /*
     * Sanity check
     */
    if (buff[0] != 0x33) {
	return;
    }
    
    bytes = DPT_HCP_LENGTH(buff);
    param	= DPT_HCP_FIRST(buff);

    if (DPT_HCP_CODE(param) != 1 ) {
	/*
	 * DPT Log Page layout error
	 */
	printf("dpt%d: NOTICE: Log Page (1) layout error\n", dpt->unit);
	return;
    }

    if ( !(param[4] & 0x4) ) {
	dpt->cache_type = DPT_NO_CACHE;
	return;
    }

    while (DPT_HCP_CODE(param) != 6)	{
	if ( (param = DPT_HCP_NEXT(param)) < buff || param >= &buff[bytes]) {
	    return;
	}
    }

    if (param[4] & 0x2) {
	/*
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


#ifdef DPT_INQUIRE_DEVICES
/*
 * This function does an inquiry on the specified device and returns
 * zero if it failed, and DPT_DEV_ST_INQ_DONE if a device was there.
 * It will be used later for more advanced features.
 *
 * Curently, there is no caller for this function.
 * Enable it in dpt.h, in case you need it.
 */

static struct scsi_inquiry_data *
dpt_inquire_device(dpt_softc_t * dpt, u_int8_t channel, u_int32_t target_id,
					u_int8_t lun)
{
    volatile dpt_sp_t sp;
    eata_ccb_t		cp;
	
    /* get_conf returns 512 bytes, most of which are zeros... */
    struct scsi_inquiry_data *inq = (struct scsi_inquiry_data *) inqbuff;
	
    u_int32_t	i;
    u_int16_t	*ip;
    u_int8_t	status;
    u_int8_t	aux_status;
    u_int8_t	sig1;
    u_int8_t	sig2;
    u_int8_t	sig3;
	
    volatile int ndx;
    int	 	 result;
	
    struct scsi_sense_data snp;
	
#ifdef DPT_DEBUG_INQUIRE
    printf("dpt%d: dpt_inquire_device(%p, %d, %d, %d)\n",
	   dpt->unit, dpt, channel, target_id, lun);
#endif

    bzero(&cp, sizeof(eata_ccb_t));
    bzero((int8_t *) &sp, sizeof(dpt_sp_t));
    bzero(inq, sizeof(dpt_inq_t));
	
    cp.Interpret = 0;
    cp.DataIn = 1;
    cp.Auto_Req_Sen = 1;
    cp.reqlen = (u_int8_t) sizeof(struct scsi_sense_data);
	
    cp.cp_id = target_id;	/* who cares?	The HBA will interpret... */
    cp.cp_LUN = lun;	/* In the EATA packet */
    cp.cp_lun = lun;	/* In the SCSI command */
	
    cp.cp_scsi_cmd = INQUIRY;
    cp.cp_len = 56;
	
    cp.cp_len = sizeof(dpt_inq_t);
    cp.cp_id = target_id;	/* DNC, Interpret mode is set */
    cp.cp_channel = channel;/* DNC, Interpret mode is set */
    cp.cp_identify = 1;
    cp.cp_datalen = htonl(sizeof(dpt_inq_t));
    cp.cp_dataDMA = htonl(vtophys(inq));
    cp.cp_statDMA = htonl(vtophys(&sp));
    cp.cp_reqDMA = htonl(vtophys(&snp));
    cp.cp_viraddr = (u_int32_t) & cp;
	
    
    /*
     * This could be a simple for loop, but we suspected the compiler To
     * have optimized it a bit too much. Wait for the controller to
     * become ready.
     */
    ospl = splcam();
    while ((((status = dpt_inb(dpt, HA_RSTATUS)) != (HA_SREADY | HA_SSC)) &&
	    (status != (HA_SREADY | HA_SSC | HA_SERROR)) &&
	    /* This results from the `wd' probe at our addresses */
	    (status != (HA_SDRDY | HA_SERROR | HA_SDRQ))) ||
	   (dpt_wait(dpt, HA_SBUSY, 0)) ) {
	/*
	 * RAID Drives still Spinning up? (This should only occur if
	 * the DPT controller is in a NON PC (PCI?) platform).
	 */
	if (dpt_raid_busy(dpt)) {
	    printf("dpt%d WARNING: Inquire_device() RSUS failed.\n",
		   dpt->unit);
	    splx(ospl);
	    return ((struct scsi_inquiry_data *) NULL);
	}
    }
	
    DptStat_Reset_BUSY(&sp);
	
    /*
     * XXXX We might want to do something more clever than aborting at
     * this point, like resetting (rebooting) the controller and trying
     * again.
     */
    if ((result = dpt_send_eata_command(&cp, dpt->base,
					EATA_CMD_DMA_SEND_CP,
					10000)) != 0 ) {
	printf("dpt% dERROR: Inquire_device() failed (%x) to send "
	       "EATA_CMD_DMA_SEND_CP\n", dpt->unit, result);
	return ((struct scsi_inquiry_data *) NULL);
    }
    /* Wait for two seconds for a response */
    for (ndx = 0;
	 (ndx < 2000) &&
	     !((aux_status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	 ndx++
	) {
	DELAY(10);
    }
	
    status = dpt_inb(dpt, HA_RSTATUS);
    splx(ospl);
	
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d: inquire_device() inquiry status = %s after %d loops\n",
	   dpt->unit, i2bin((unsigned long) status, sizeof(status) * 8), ndx);
	
#ifdef DPT_DEBUG_HEX_DUMPS
    hex_dump((u_int8_t *) inq, (sizeof(dpt_inq_t) > 512) ?
	     sizeof(dpt_inq_t) : 512, "Inq", __LINE__);
    hex_dump((u_int8_t *) & sp, sizeof(dpt_sp_t), "StatusPacket", __LINE__);
    hex_dump((u_int8_t *) & snp, sizeof(struct scsi_sense_data),
	     "SenseData", __LINE__);
#endif
#endif	
    i = (!(sp.hba_stat)) &&
	(!(sp.scsi_stat)) &&
	(!(sp.residue_len)) &&
	(ndx < 20000);
	
    /* Get the status and clear the interrupt flag on the controller */
    if (!(status & HA_SERROR) && (i) /* && Other sanity tests */ ) {
#ifdef DPT_DEBUG_SETUP
	printf("dpt%d dpt_inquire_device() return(%p), status = %s\n",
	       dpt->unit, inq, i2bin((unsigned long) status,
				     sizeof(status) * 8));
#endif
	
	/*
	 * No need to normalize the data before returning; It is all
	 * byte fields
	 */
	
	return (inq);
    }
    
    return ((struct scsi_inquiry_data *) NULL);
}

#endif /* DPT_INQUIRE_DEVICES */

/*
 * Initializes the softc structure and allocate all sorts of storage.
 * Returns 0 on good luck, 1-n otherwise (error condition sensitive).
 */

int
dpt_setup(dpt_softc_t * dpt, dpt_conf_t * conf)
{
    dpt_inq_t *board_data;
	
    unsigned long rev, ndx;
	
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d: dpt_setup(%p, %p) uses port %x\n", dpt->unit, dpt,
	   conf, BaseRegister(dpt));
#endif
	
    board_data = dpt_get_board_data(dpt, conf->scsi_id0);
    if (board_data == NULL) {
	printf("dpt%d ERROR: Get_board_data() failure. Setup ignored!\n",
	       dpt->unit);
	return (1);
    }

    dpt->total_ccbs_count	= 0;
    dpt->free_ccbs_count	= 0;
    dpt->waiting_ccbs_count	= 0;
    dpt->submitted_ccbs_count	= 0;
    dpt->completed_ccbs_count	= 0;
	
    switch (ntohl(conf->splen)) {
	/*
	 * XXX JGibbs - you should enumerate or #define these
	 *              somewhere instead of using 'Magic Numbers'.
	 * YYY Simon - Good idea.  What are these?
	 *             They are magic numbers, appearing exactly once; Here.
	 */
    case 0x1c:
	dpt->EATA_revision = 'a';
	break;
    case 0x1e:
	dpt->EATA_revision = 'b';
	break;
    case 0x22:
	dpt->EATA_revision = 'c';
	break;
    case 0x24:
	dpt->EATA_revision = 'z';
	break;
    default:
	dpt->EATA_revision = '?';
    }
	
    (void) memcpy(&dpt->board_data, board_data, sizeof(dpt_inq_t));
	
    dpt->bustype = IS_PCI;  /* We only support and operate on PCI devices */
    dpt->channels = conf->MAX_CHAN + 1;
    dpt->max_id	  = conf->MAX_ID;
    dpt->max_lun  = conf->MAX_LUN;
    dpt->state	 |= DPT_HA_OK;
	
    if ( conf->SECOND )
	dpt->primary = FALSE;
    else
	dpt->primary = TRUE;
	
    dpt->more_support = conf->MORE_support;
	
    if (board_data == NULL) {
	rev = ('?' << 24)
	    |	('-' << 16)
	    |	('?' << 8)
	    |	'-';
    } else {
	rev = (dpt->board_data.firmware[0] << 24)
	    |	(dpt->board_data.firmware[1] << 16)
	    |	(dpt->board_data.firmware[2] <<	8)
	    |	dpt->board_data.firmware[3];
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
    dpt->hostid[1] = conf->scsi_id2;
    dpt->hostid[2] = conf->scsi_id2;
	
    if (conf->SG_64K) {
	dpt->sgsize = SG_SIZE_BIG;
    } else if ((ntohs(conf->SGsiz) < 1) || (ntohs(conf->SGsiz) > SG_SIZE) ) {
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
	
    return (0);
}

/*
 * The following function returns a pointer to a buffer which MUST be freed by
 * The caller, a la free(result, M_DEVBUF)
 *
 * This function (and its like) assumes it is only running during system
 * initialization!
 */

dpt_inq_t *
dpt_get_board_data(dpt_softc_t * dpt, u_int32_t target_id)
{
    /* get_conf returns 512 bytes, most of which are zeros... */
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d: dpt_get_board_data(base = %x, target_id = %d)\n",
	   dpt->unit, BaseRegister(dpt), target_id);
#endif
	
    return((dpt_inq_t*)dpt_get_conf(dpt, 0, target_id, sizeof(dpt_inq_t), 0));
}

/* The following function returns a pointer to a buffer which MUST be freed by
 * the caller, a la ``free(result, M_TEMP);''
 *
 */

dpt_conf_t *
dpt_get_conf(dpt_softc_t *dpt, u_int8_t page, u_int8_t target, 
	     u_int8_t size, int extent)
{
    volatile dpt_sp_t sp;
    eata_ccb_t cp;
	
    /* Get_conf returns 512 bytes, most of which are zeros... */
    dpt_conf_t *config;
	
    u_int32_t i;
    u_short *ip;
    u_int8_t	status, sig1, sig2, sig3;
	
    volatile int ndx;
    int ospl;
    int result;
	
    struct scsi_sense_data snp;	
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d: get_conf(%x)\n", dpt->unit, BaseRegister(dpt));
#endif
	
    if ( (config = (dpt_conf_t *)malloc(512, M_TEMP, M_WAITOK)) == NULL )
	return(NULL);

    bzero(&cp, sizeof(eata_ccb_t));
    bzero((int8_t *) &sp, sizeof(dpt_sp_t));
    bzero(config, size);
	
    cp.Interpret = 1;
    cp.DataIn = 1;
    cp.Auto_Req_Sen = 1;
    cp.reqlen = (u_int8_t) sizeof(struct scsi_sense_data);
	
    cp.cp_id = target;
    cp.cp_LUN = 0;	/* In the EATA packet */
    cp.cp_lun = 0;	/* In the SCSI command */
	
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
    cp.cp_viraddr = (u_int32_t) &cp;
	
    ospl = splcam();
    
#ifdef DPT_RESET_BOARD
    printf("dpt%d: get_conf() resetting HBA at %x.\n",
	   dpt->unit, BaseRegister(dpt));
    dpt_outb(dpt, HA_WCOMMAND, EATA_CMD_RESET);
    DELAY(750000);
#endif
	
    /*
     * This could be a simple for loop, but we suspected the compiler To
     * have optimized it a bit too much. Wait for the controller to
     * become ready
     */
    while ((((status = dpt_inb(dpt, HA_RSTATUS)) != (HA_SREADY | HA_SSC)) &&
	    (status != (HA_SREADY | HA_SSC | HA_SERROR)) &&
	    /* This results from the `wd' probe at our addresses */
	    (status != (HA_SDRDY | HA_SERROR | HA_SDRQ))) ||
	   (dpt_wait(dpt, HA_SBUSY, 0)) ) {
	/*
	 * RAID Drives still Spinning up? (This should only occur if
	 * the DPT controller is in a NON PC (PCI?) platform).
	 */
	if (dpt_raid_busy(dpt)) {
	    printf("dpt%d WARNING: Get_conf() RSUS failed for HBA at %x\n",
		   dpt->unit, BaseRegister(dpt));
	    free(config, M_TEMP);
	    splx(ospl);
	    return(NULL);
	}
    }
	
    DptStat_Reset_BUSY(&sp);
	
    /*
     * XXXX We might want to do something more clever than aborting at
     * this point, like resetting (rebooting) the controller and trying
     * again.
     */
    if ((result = dpt_send_eata_command(&cp, dpt, EATA_CMD_DMA_SEND_CP,
					10000)) != 0) {
	printf("dpt%d WARNING: Get_conf() failed (%d) to send "
	       "EATA_CMD_DMA_READ_CONFIG\n",
	       dpt->unit, result);
	free(config, M_TEMP);
	splx(ospl);
	return (NULL);
    }
    /* Wait for two seconds for a response */
    for (ndx = 0;
	 (ndx < 20000) && !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	 ndx++ ) {
	DELAY(10);
    }
	
    /* Grab the status and clear interrupts */
    status = dpt_inb(dpt, HA_RSTATUS);
	
    splx(ospl);
    
#ifdef DPT_DEBUG_SETUP
    printf("dpt%d get_conf() status = %s, ndx = %d\n",
	   dpt->unit, i2bin((unsigned long) status, sizeof(status) * 8), ndx);
#ifdef DPT_DEBUG_HEX_DUMPS
    hex_dump((u_int8_t *) config, (sizeof(dpt_conf_t) > 512) ?
	     sizeof(dpt_conf_t) : 512, "Config", __LINE__);
    hex_dump((u_int8_t *) & sp, sizeof(dpt_sp_t), "StatusPacket", __LINE__);
    hex_dump((u_int8_t *) & snp, sizeof(struct scsi_sense_data), "SenseData",
	     __LINE__);
#endif
#endif
	
    /* Check the status carefully.  Return only is command was successful. */
    if ( ((status & HA_SERROR) == 0)
	 && (sp.hba_stat == 0)
	 && (sp.scsi_stat == 0)
	 && (sp.residue_len == 0)) {
#ifdef DPT_DEBUG_SETUP
	printf("dpt%d: get_conf() returning config = %p, status = %s\n",
	       dpt->unit, config,
	       i2bin((unsigned long) status, sizeof(status) * 8));
#endif
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
	
    int ndx;
    int idx;
    int channel;
    int target;
    int lun;
	
    struct scsi_inquiry_data *inq;
	
#ifdef DPT_DEBUG_CONFIG
    printf("dpt%d: dpt_attach(%p)\n", dpt->unit, dpt);
#endif
	
    for (ndx = 0; ndx < dpt->channels; ndx++) {
#ifdef DPT_DEBUG_CONFIG
	printf("dpt%d: Channel %d of %d\n", dpt->unit, ndx + 1, dpt->channels);
#endif
	
	/*
	 * We do not setup target nor lun on the assumption that
	 * these are being set for individual devices that will be
	 * attached to the bus later.
	 */
	
	dpt->sc_scsi_link[ndx].adapter_unit = dpt->unit;
	dpt->sc_scsi_link[ndx].adapter_targ = dpt->hostid[ndx];
	dpt->sc_scsi_link[ndx].fordriver = (void *) dpt; /* Another way... */
	dpt->sc_scsi_link[ndx].adapter_softc = dpt;
	dpt->sc_scsi_link[ndx].adapter = &dpt_switch;
	dpt->sc_scsi_link[ndx].opennings = dpt->queuesize;/* 
							   * These appear to be
							   * the # of openings
							   * per that  DEVICE,
							   * not the DPT!
							   */
	dpt->sc_scsi_link[ndx].device = &dpt_dev;
	dpt->sc_scsi_link[ndx].adapter_bus = ndx;

	/*
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
	 * This is done in polled mode.	Otherwise things seem to hang (2.2.2)
	 */

#ifdef DPT_SUPPORT_POLLING
	scsi_attachdevs(scbus);
	dpt->handle_interrupts = 1;	/* Now we are ready to work */
#else
	dpt->handle_interrupts = 1;	/* Now we are ready to work */
	scsi_attachdevs(scbus);
#endif
	scbus = (struct scsibus_data *) NULL;
    }
	
    return (1);
}

/*
 * Allocate another chunk of CCB's. Return 0 on success, 1 otherwise.
 * If the free list is empty, we allocate a block of entries and add them
 * to the list.	We obtain, at most, DPT_FREE_LIST_INCREMENT CCB's at a time.
 * If we cannot, we will try fewer entries until we succeed.
 * For every CCB, we allocate a maximal Scatter/Gather list.
 * This routine also initializes all the static data that pertains to this CCB.
 */

static int
dpt_alloc_freelist(dpt_softc_t * dpt)
{
    dpt_ccb_t *nccbp;
    dpt_sg_t  *sg; 
    u_int8_t  *buff;
    int        ospl;
    int        incr;
    int        ndx;
    int        ccb_count;
	
    ccb_count = DPT_FREE_LIST_INCREMENT;
	
#ifdef DPT_RESTRICTED_FREELIST
    if ( dpt->total_ccbs_count != 0) {
	printf("dpt%d: Restricted FreeList, No more than %d entries allowed\n",
	       dpt->unit, dpt->total_ccbs_count);
	return(-1);
    }
#endif
	
    /*
     * Allocate a group of dpt_ccb's. Work on the CCB's, one at a time
     */
    ospl = splsoftcam();
    for (ndx = 0; ndx < ccb_count; ndx++) {
	size_t	alloc_size;
	dpt_sg_t *sgbuff;
	
	alloc_size = sizeof(dpt_ccb_t);  /* About 200 bytes */
	
	if (alloc_size > PAGE_SIZE) {
	    /* 
	     * Does not fit in a page.
	     * we try to fit in a contigious block of memory.
	     * If not, we will, later try to allocate smaller,
	     * and smaller chunks..
	     * There is a tradeof between memory and performance here.
	     * We know.this (crude) algorithm works well on machines with
	     * plenty of memory.
	     * We have seen it allocate in excess of 8MB.
	     */
	    nccbp = (dpt_ccb_t *) contigmalloc(alloc_size, M_DEVBUF, M_NOWAIT,
					       0, 0xffffffff, PAGE_SIZE,
					       0x10000);
	} else {
	    /* fits all in one page */
	    nccbp = (dpt_ccb_t *) malloc(alloc_size, M_DEVBUF, M_NOWAIT);
	}
	
	if (nccbp == (dpt_ccb_t *) NULL) {
	    printf("dpt%d ERROR: Alloc_free_list() failed to allocate %d\n",
		   dpt->unit, ndx);
	    splx(ospl);
	    return (-1);
	}

	alloc_size = sizeof(dpt_sg_t) * dpt->sgsize;
	
	if (alloc_size > PAGE_SIZE) {
	    /* Does not fit in a page */
	    sgbuff = (dpt_sg_t *) contigmalloc(alloc_size, M_DEVBUF, M_NOWAIT,
					       0, 0xffffffff, PAGE_SIZE,
					       0x10000);
	} else {
	    /* fits all in one page */
	    sgbuff = (dpt_sg_t *) malloc(alloc_size, M_DEVBUF, M_NOWAIT);
	}
	
	/*
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
	nccbp->eata_ccb.cp_dataDMA = (u_int32_t) sgbuff;
	nccbp->sg_list = sgbuff;
	
	/*
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
	nccbp->eata_ccb.cp_viraddr = (u_int32_t) nccbp; /* Unique, address */
	nccbp->eata_ccb.cp_statDMA = htonl(vtophys(&dpt->sp));
	/*
	 * See dpt_intr for why we make ALL CCB's ``have the same''
	 * Status Packet
	 */
	nccbp->eata_ccb.cp_reqDMA = htonl(vtophys(&nccbp->sense_data));
    }

    splx(ospl);
	
#ifdef DPT_DEBUG_QUEUES
    printf("dpt%d: Total CCB's increased by %d to %d in alloc_freelist()\n",
	   dpt->unit, ndx, dpt->total_ccbs_count);
#endif
	
    return (0);
}

/*
 * Prepare the data area for DMA.
 */
static int
dpt_scatter_gather(dpt_softc_t *dpt, dpt_ccb_t *ccb, u_int32_t data_length,
		   caddr_t data)
{
    int		seg;
    int		thiskv;
    int		bytes_this_seg;
    int		bytes_this_page;
    u_int32_t	datalen;
    vm_offset_t	vaddr;
    u_int32_t	paddr;
    u_int32_t	nextpaddr;
    dpt_sg_t   *sg;
	
#ifdef DPT_DEBUG_SG
    printf("dpt%d: scatter_gather(%p, %p, %d, %p)\n",
	   dpt->unit, dpt, ccb, data_length, data);
#endif
    if (data_length) {
	if (ccb->flags & SCSI_DATA_IN) {
	    ccb->eata_ccb.DataIn = 1;
	}
	if (ccb->flags & SCSI_DATA_OUT) {
	    ccb->eata_ccb.DataOut = 1;
	}
	
	seg = 0;
	datalen = data_length;
	vaddr = (vm_offset_t)data;
	paddr = vtophys(vaddr);
	ccb->eata_ccb.cp_dataDMA = htonl(vtophys(ccb->sg_list));
	sg = ccb->sg_list;
	
#ifdef DPT_DEBUG_SG
	printf("dpt%d: scatter_gather() DMA setting up %s %s S/G (%x/%d)\n",
	       dpt->unit,
	       (ccb->eata_ccb.DataIn == 1) ? "Data-In" : "",
	       (ccb->eata_ccb.DataOut == 1) ? " Data-Out" : "",
	       paddr, datalen);
#if defined(DPT_DEBUG_SG_SHOW_DATA) && defined(DPT_DEBUG_HEX_DUMPS)
	hex_dump((u_int8_t *) data, datalen, "data", __LINE__);
#endif
#endif
	
	while ((datalen > 0) && (seg < dpt->sgsize)) {
	    /* put in the base address and length */
	    sg->seg_addr = paddr;
	    sg->seg_len  = 0;
		
	    /* do it at least once */
	    nextpaddr = paddr;
		
	    while ((datalen > 0) && (paddr == nextpaddr)) {
		u_int32_t size;
	
		/*
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
	    seg++;
	    sg++;
	}
	
	if (datalen) {
	    /* There's still data, must have run out of segs! */
	    printf("dpt%d: scsi_cmd() Too Many (%d) DMA segs "
		   "(%d bytes left)\n",
		   dpt->unit, dpt->sgsize, datalen);
	    return(1);
	} else {
	    if (seg == 1) {
		/*
		 * After going through all this trouble, we
		 * still have only one segment. As an
		 * optimization measure, we cancel the S/G
		 * operation and will do the I/O as a
		 * single, non-S/G operation.
		 */
	
		ccb->eata_ccb.cp_dataDMA =
		    htonl((u_int32_t)vtophys((vm_offset_t)data));
		ccb->eata_ccb.cp_datalen = htonl(data_length);
	
#ifdef DPT_DEBUG_SG
		printf("dpt%d: scsi_cmd() DMA has one segment (%p/%d)\n",
		       dpt->unit, (u_int32_t) vtophys((vm_offset_t) data),
		       data_length);
#endif
	    } else {
		/*
		 * There is more than one segment. Convert
		 * all data to network order. Convert S/G
		 * length from # of elements to size of
		 * array in bytes. XXXX - This loop should
		 * be eliminated and the htonl() integrated
		 * into the above while loop.
		 */
	
		int	ndx;
	
		ccb->eata_ccb.scatter = 1; /* Now we want S/G to be ON */
	
		for (ndx = 0, sg = ccb->sg_list; ndx < seg; ndx++, sg++) {
#ifdef DPT_DEBUG_SG
		    printf("dpt%d: scsi_cmd() CPU S/G segment %d @ %p, "
			   "for %8.0x bytes\n",
			   dpt->unit, ndx, sg->seg_addr, sg->seg_len);
#endif
		    sg->seg_addr = htonl((u_int32_t) sg->seg_addr);
		    sg->seg_len = htonl(sg->seg_len);
		}
	
		ccb->eata_ccb.cp_datalen = htonl(seg * sizeof(dpt_sg_t));
	    }
#ifdef DPT_DEBUG_SG
#endif
	}
    } else {	/* datalen == 0 */
	/* No data xfer */
	ccb->eata_ccb.cp_datalen = 0;
	ccb->eata_ccb.cp_dataDMA = 0;
	ccb->eata_ccb.scatter = 0; /* Make sure S/G does not try ALL-0 xfr */
	
#ifdef DPT_DEBUG_SG
	printf("dpt%d: scatter_gather() No Data\n", dpt->unit);
#endif
    }

    return(0);
}

/*
 * This function obtains an SCB for a command and attempts to queue it to the
 * Controller.
 *
 * SCB Obtaining: Is done by getting the first entry in the free list for the
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
    dpt_softc_t	*dpt;
    int		 incr;
    int		 ndx;
    int		 ospl;
    int		 huh;
    
    u_int32_t	 flags;	
    dpt_ccb_t	*ccb;	
    u_int8_t	 status;
    u_int32_t	 aux_status;
    int		 result;
	
    int	channel, target, lun;
	
    struct scsi_inquiry_data *inq;
	
    dpt = (dpt_softc_t *) xs->sc_link->adapter_softc;

#ifdef DPT_HANDLE_TIMEOUTS
    ospl = splsoftcam();
    if ( (dpt->state & DPT_HA_TIMEOUTS_SET) == 0 ) {
	dpt->state |= DPT_HA_TIMEOUTS_SET;
	timeout((timeout_func_t)dpt_timeout, dpt, hz * 10);
    }
    splx(ospl);
#endif

    /*
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
	return(COMPLETE);
    }

    flags = xs->flags;
    channel = DptChannel(dpt->unit, xs->sc_link->adapter_bus);
    target = xs->sc_link->target;
    lun = xs->sc_link->lun;
	
    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE ) {
	printf("dpt%d ERROR: Command \"%s\" recieved for b%dt%du%d\n"
	       "	    but controller is shutdown; Aborting...\n",
	       dpt->unit, 
	       scsi_cmd_name(xs->cmd->opcode),
	       channel, target, lun);
	xs->error = XS_DRIVER_STUFFUP;
	return (COMPLETE);
    }

#if defined(DPT_DEBUG_SCSI_CMDS) && defined(DPT_DEBUG_SCSI_CMD_NAME)
    printf("dpt%d: dpt_scs_cmd(%s)\n"
	   "		for b%dt%du%d, flags = %s\n", dpt->unit,
	   scsi_cmd_name(xs->cmd->opcode),
	   channel, target, lun,
	   i2bin((unsigned long) flags, sizeof(flags) * 8));
#endif
	
    if (flags & ITSDONE) {
	printf("dpt%d WARNING: scsi_cmd(%s) already done on b%dt%du%d?!\n",
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

    /*
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
    if ( (ccb = dpt_Qpop_free(dpt)) == NULL) {
	/* No need to panic here.  We can continue with only as
	 * many CCBs as we have.
	 */
	printf("dpt%d ERROR: Got a NULL CCB from pop_free()\n", dpt->unit);
	xs->error = XS_DRIVER_STUFFUP;
	splx(ospl);
	return (COMPLETE);
    }
    
#ifdef DPT_HANDLE_TIMEOUTS
    ccb->status &= ~(DPT_CCB_STATE_ABORTED | DPT_CCB_STATE_MARKED_LOST);
#endif
	
#ifdef DPT_TRACK_CCB_STATES
    ccb->state |= DPT_CCB_STATE_FREE2WAITING;
#endif
    
    splx(ospl);
    bcopy(xs->cmd, ccb->eata_ccb.cp_cdb, xs->cmdlen);
	
    /* Put all the CCB population stuff below */
    ccb->xs = xs;
    ccb->flags = flags;	
    ccb->eata_ccb.SCSI_Reset = 0; /* We NEVER reset the bus from a command   */
    ccb->eata_ccb.HBA_Init = 0;	/* We NEVER re-boot the HBA from a * command */
    ccb->eata_ccb.Auto_Req_Sen = 1;	/* We always want this feature */
    ccb->eata_ccb.reqlen = htonl(sizeof(struct scsi_sense_data));
	
    if (xs->sc_link->target == xs->sc_link->adapter_targ) {
	ccb->eata_ccb.Interpret = 1;
    } else {
	ccb->eata_ccb.Interpret = 0;
    }
	
    ccb->eata_ccb.scatter = 0;	/* S/G is ON now */	
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
    ccb->eata_ccb.cp_LUN = lun;	/*
				 * In the EATA packet. We do not
				 * change the SCSI command yet
				 */
    /* We are currently dealing with target LUN's, not ROUTINEs */
    ccb->eata_ccb.cp_luntar = 0;
	
    /*
     * XXXX - We grant the target disconnect prvileges, except in polled
     * mode (????).
     */
    if ( (ccb->flags & SCSI_NOMASK) || !dpt->handle_interrupts ) {
	ccb->eata_ccb.cp_dispri = 0;
    } else {
	ccb->eata_ccb.cp_dispri = 1;
    }
	
    /* we always ask for Identify */
    ccb->eata_ccb.cp_identify = 1;
	
    /*
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

    if ( dpt_scatter_gather(dpt, ccb, xs->datalen, xs->data) != 0 ) {
	xs->error = XS_DRIVER_STUFFUP;
	ospl = splsoftcam();
	dpt_Qpush_free(dpt, ccb);
	splx(ospl);
	return (COMPLETE);
    }
    
    xs->resid  = 0;
    xs->status = 0;
	
    /*
     * This is the polled mode section. If we are here to honor
     * SCSI_NOMASK, during scsi_attachdevs(), please notice that
     * interrupts are ENABLED in the system (2.2.1) and that the DPT
     * WILL generate them, unless we turn them off!
     */
	
    /*
     * XXX - JGibbs: Polled mode was a botch at best. It's nice to
     *               know that it goes completely away with the CAM code.
     * YYY - Simon:  Take it out once the rest is stable. Be careful about
     *               how you wait for commands to complete when you switch to
     *               interrupt mode in the scanning code (initiated by
     *               scsi_attachdevs).
     *               Disabling it in 2.2 causes a hung system.
     */

    if ( (ccb->flags & SCSI_NOMASK) || !dpt->handle_interrupts ) {
	/*
	 * This is an ``immediate'' command.	Poll it! We poll by
	 * partially bypassing the queues. We first submit the
	 * command by asking dpt_run_queue() to queue it. Then we
	 * poll its status packet, until it completes. Then we give
	 * it to dpt_process_completion() to analyze and then we
	 * return.
	 */
	
#ifdef DPT_DEBUG_POLLING
	printf("dpt%d: scsi_cmd() polling %s command on c%db%dt%du%d\n",
	       dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
	       dpt->unit, ccb->eata_ccb.cp_channel,
	       ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
#endif
	
	/* Increase the number of commands queueable for a device.
	 * We force each device to the maximum allowed for its HBA.
	 * This appears wrong but all it will do is cause excessive commands
	 * to sit in our queue.
	 * On the other hand, we can burst as many commands as the DPT can take
	 * for a single device.
	 * We do it here, so only while in polled mode (early boot) do we waste
	 * time on it.	We have no clean way to overrule sdattach() zeal in 
	 * depressing the opennings back to one if it is more than 1.
	 */
	if ( xs->sc_link->opennings < dpt->queuesize ) {
	    xs->sc_link->opennings = dpt->queuesize;
	}

	/*
	 * This test only protects us from submitting polled
	 * commands during Non-polled times.	We assumed polled
	 * commands go in serially, one at a time. BTW, we have NOT
	 * checked, nor verified the scope of the disaster that WILL
	 * follow going into polled mode after being in interrupt
	 * mode for any length of time.
	 */
	if (dpt->submitted_ccbs_count < dpt->queuesize) {
	    /*
	     * Submit the request to the DPT. Unfortunately, ALL
	     * this must be done as an atomic operation :-(
	     */
	    ccb->eata_ccb.cp_viraddr = (u_int32_t) & ccb;
	    ccb->eata_ccb.cp_statDMA = htonl(vtophys(&ccb->status_packet));
	    ccb->eata_ccb.cp_reqDMA = htonl(vtophys(&ccb->sense_data));

	    /* Try to queue a command */
	    ospl = splcam();
	    result = dpt_send_eata_command(&ccb->eata_ccb, dpt,
					   EATA_CMD_DMA_SEND_CP, 0);
	    
	    if ( result != 0 ) {
#ifdef DPT_DEBUG_POLLING
		printf("dpt%d: scsi_cmd() failed to submit polled "
		       "EATA command (%p/%p)\n", dpt->unit, xs, ccb);
#endif
	
		dpt_Qpush_free(dpt, ccb);
		xs->error = XS_DRIVER_STUFFUP;
		splx(ospl);
		return (COMPLETE);
	    }
	} else {
#ifdef DPT_DEBUG_POLLING
	    printf("dpt%d: scsi_cmd() cannot submit polled command (%p/%p);\n"
		   "		Too many (%d) commands in the queue\n",
		   dpt->unit, xs, ccb, dpt->queuesize);
#endif	
	    xs->error = XS_DRIVER_STUFFUP;
	    dpt_Qpush_free(dpt, ccb);
	    splx(ospl);
	    return (COMPLETE);
	}	
	
	for (ndx = 0;
	     (ndx < xs->timeout)
		 && !((aux_status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	     ndx++ ) {
	    DELAY(1000);
	}
	
	/*
	 * Get the status and clear the interrupt flag on the
	 * controller
	 */
	status = dpt_inb(dpt, HA_RSTATUS);
	splx(ospl);
	
	ccb->status_reg = status;
	ccb->aux_status_reg = aux_status;
	dpt_process_completion(dpt, ccb); /* It will setup the xs * flags */
	
#ifdef DPT_DEBUG_HEX_DUMPS
	hex_dump((u_int8_t *) & ccb->eata_ccb, sizeof(eata_ccb_t),
		 "EATA CCB", __LINE__);
	hex_dump((u_int8_t *) & ccb->status_packet, sizeof(dpt_sp_t),
			"StatusPacket", __LINE__);
	hex_dump((u_int8_t *) & ccb->sense_data,
		 sizeof(struct scsi_sense_data),	
		 "SenseData", __LINE__);
#endif
	
	if (status & HA_SERROR) {
#ifdef DPT_DEBUG_SETUP
	    printf("dpt%d: scsi_cmd() Polled Command (%p/%p) Failed, (%s)\n",
		   dpt->unit, xs, ccb,
		   i2bin((unsigned long) status, sizeof(status) * 8));
#endif
		
	    ospl = splsoftcam();
	    dpt_Qpush_free(dpt, ccb);
	    splx(ospl);
	    return (COMPLETE);
	}
#ifdef DPT_DEBUG_SETUP
	printf("dpt%d: scsi_cmd() Polled Command (%p/%p) Completed, (%s)\n",
	       dpt->unit, xs, ccb,
	       i2bin((unsigned long) status, sizeof(status) * 8));
#endif
	
	ospl = splsoftcam();
	dpt_Qpush_free(dpt, ccb);
	splx(ospl);
	return (COMPLETE);
    } else {
	struct timeval junk;

	/*
	 * Not a polled command.
	 * The command can be queued normally.
	 * We start a critical section PRIOR to submitting to the DPT,
	 * and end it AFTER it moves to the submitted queue.
	 * If not, we cal (and will!) be hit with a completion interrupt
	 * while the command is in suspense between states.
	 */
	
	ospl = splsoftcam();
#ifdef DPT_DEBUG_SCSI_CMDS
	printf("dpt%d: scsi_cmd() Queuing %s command on c%db%dt%du%d\n",
	       dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
	       dpt->unit, ccb->eata_ccb.cp_channel,
	       ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
#endif
	ccb->transaction_id = ++dpt->commands_processed;
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.command_count[(int)ccb->eata_ccb.cp_scsi_cmd];
	microtime(&junk);
	ccb->command_started = junk;
#endif
	dpt_Qadd_waiting(dpt, ccb);
	splx(ospl);
	
	dpt_sched_queue(dpt);
    }
    
    return (SUCCESSFULLY_QUEUED);
}

/*
 * This function returns the transfer size in bytes,
 * as a function of the maximum number of Scatter/Gather
 * segments.  It should do so for a given HBA, but right now it returns
 * dpt_min_segs, which is the SMALLEST number, from the ``weakest'' HBA found.
 */

void
dptminphys(struct buf * bp)
{
    /*
     * This IS a performance sensitive routine.	It gets called at least
     * once per I/O.	sometimes more
     */
	
    if (dpt_min_segs == 0) {
	panic("DPT:  Minphys without attach!\n");
    }
    
    if (bp->b_bcount > ((dpt_min_segs - 1) * PAGE_SIZE)) {
	bp->b_bcount = ((dpt_min_segs - 1) * PAGE_SIZE);
    }
}

/* This function goes to the waiting queue,
 * peels off a request,
 * gives it to the DPT HBA and returns.
 * It takes care of some housekeeping details first.
 * The requests argument tells us how many requests to try and send to the DPT.
 * A requests = 0 will attempt to send as many as the controller can take.
 */

static void
dpt_run_queue(dpt_softc_t * dpt, int requests)
{
    int		req;
    int		ospl;
    int		ndx;
    int		result;

    u_int8_t status, aux_status;
	
    eata_ccb_t *ccb;
    dpt_ccb_t *dccb;

    if (TAILQ_EMPTY(&dpt->waiting_ccbs)) {
#ifdef DPT_DEBUG_QUEUES
	printf("dpt%d: dpt_run_queue: Empty Queue\n", dpt->unit);
#endif
	return;	/* Nothing to do if the list is empty */
    }

    if (!requests)
	requests = dpt->queuesize;
	
#ifdef DPT_DEBUG_SOFTINTR
    printf("dpt%d: dpt_run_queue(%p, %d)\n", dpt->unit, dpt, requests);
#endif
	
    /* Main work loop */
    for (req = 0; (req < requests) && dpt->waiting_ccbs_count
	     && (dpt->submitted_ccbs_count < dpt->queuesize); req++ ) {
	/*
	 * Move the request from the waiting list to the submitted
	 * list, and submit to the DPT.
	 * We enter a critical section BEFORE even looking at the witing queue,
	 * and exit it AFTER the ccb has moved to a destination queue.
	 * This is normally the submitted queue but can be the waiting queue
	 * again, if pushing the command into the DPT failed.
	 */
	
	ospl = splsoftcam();
	dccb = TAILQ_FIRST(&dpt->waiting_ccbs);
	
	if (dccb == NULL) {
	    /* We have yet to see one report of this condition */
	    panic("dpt%d ERROR: Race condition in run_queue (w%ds%d)\n",
		   dpt->unit, dpt->waiting_ccbs_count,
		   dpt->submitted_ccbs_count);
	    splx(ospl);
	    return;
	}

	dpt_Qremove_waiting(dpt, dccb);
	splx(ospl);
	
	/*
	 * Assign exact values here. We manipulate these values
	 * indirectly elsewhere, so BE CAREFUL!
	 */
	dccb->eata_ccb.cp_viraddr = (u_int32_t) dccb;
	dccb->eata_ccb.cp_statDMA = htonl(vtophys(&dpt->sp));
	dccb->eata_ccb.cp_reqDMA = htonl(vtophys(&dccb->sense_data));
	
	/*
	 * XXXX - This may not be necessary, but to be on the safe
	 *        side...
	 * YYYY - They make no difference as far as the
	*         ``Device Busy'' error is concerned. Leave them in for a
	*         while
	*/
	bzero(&dccb->sense_data, sizeof(struct scsi_sense_data));
	bzero(&dccb->xs->sense, sizeof(struct scsi_sense_data));
	
#ifdef DPT_DEBUG_SOFTINTR
	printf("dpt%d: dpt_run_queue() \"%s\" on c%db%dt%du%d\n",
	       dpt->unit, scsi_cmd_name(dccb->eata_ccb.cp_scsi_cmd),
	       dpt->unit, dccb->eata_ccb.cp_channel,
	       dccb->eata_ccb.cp_id, dccb->eata_ccb.cp_LUN);
#endif
	
	/* Try to queue a command */
	ospl = splcam();
	
	if ( (result = dpt_send_eata_command(&dccb->eata_ccb, dpt,
					     EATA_CMD_DMA_SEND_CP, 0))
	     != 0 ) {
#ifdef DPT_DEBUG_SOFTINTR
	    printf("dpt%d: run_queue() failed to submit EATA command\n",
		   dpt->unit);
#endif
	    dpt_Qpush_waiting(dpt, dccb);
	    splx(ospl);
	    return;
	}
	
#ifdef DPT_DEBUG_SOFTINTR
	printf("dpt%d: run_queue() Submitted EATA command\n", dpt->unit);
#endif
	dpt_Qadd_submitted(dpt, dccb);
	splx(ospl);
    }
}

/*
 * This is the interrupt handler for the DPT driver.
 * This routine runs at splcam (or whatever was configured for this device).
 */

void
dpt_intr(void *arg)
{
    dpt_softc_t *dpt;
    dpt_softc_t *ldpt;
	
    u_int8_t status, aux_status;
	
    dpt_ccb_t *dccb;
    dpt_ccb_t *tccb;
    eata_ccb_t *ccb;

    dpt = (dpt_softc_t *) arg;

#ifdef DPT_INTR_CHECK_SOFTC
    if ( dpt == (dpt_softc_t *) NULL ) {
	panic("DPT:	NULL argument to dpt_intr!");
    }

    /* We have yet to see one report where this actually happens */
    for (ldpt = TAILQ_FIRST(&dpt_softc_list);
	 (ldpt != dpt) && (ldpt != NULL);
	 ldpt = TAILQ_NEXT(ldpt, links)) {
    } 

    if (ldpt == NULL ) {
	printf("DPT ERROR: %p is not a valid dpt softc!\n", dpt);
	return;
    }
#endif /* DPT_INTR_CHECK_SOFTC */

#ifdef DPT_MEASURE_PERFORMANCE
    {
	struct timeval junk;
	
	microtime(&junk);
	dpt->performance.intr_started = junk;
    }
#endif

    /* First order of business is to check if this interrupt is for us */
    aux_status = dpt_inb(dpt, HA_RAUXSTAT);
#ifdef DPT_DEBUG_HARDINTR
    printf("dpt%d: intr() AUX status = %s\n", dpt->unit,
	   i2bin((unsigned long) aux_status, sizeof(aux_status) * 8));
#endif
	
    if (!(aux_status & HA_AIRQ)) { /* Nope, not for us */
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.spurious_interrupts;
#endif
	return;
    }
	
    if ( !dpt->handle_interrupts ) {
#ifdef DPT_DEBUG_HARDINTR
	printf("dpt%d, Not handling interrupts and AUX status = %s\n",
	       dpt->unit,
	       i2bin((unsigned long) aux_status, sizeof(aux_status) * 8));
#endif

#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.aborted_interrupts;
#endif
	status = dpt_inb(dpt, HA_RSTATUS);	/* This CLEARS interrupts */
	return;
    }

    /*
     * What we want to do now, is to capture the status, all of it, move
     * it where it belongs, wake up whoever sleeps waiting to process
     * this result, and get out of here.
     */
	
    dccb = dpt->sp.ccb;	/*
			 * There is a very SERIOUS and dangerous
			 * assumption here. We assume that EVERY
			 * interrupt is in response to some request we
			 * put to the DPT. IOW, we assume that the
			 * Virtual Address of CP always has a valid
			 * pointer that we put in! How will the DPT
			 * behave if it is in Target mode? How does it
			 * (and our driver) know it switches from
			 * Initiator to target? What will the SP be
			 * when a target mode interrupt is received? */

#ifdef DPT_VERIFY_HINTR
    dpt->sp.ccb = (dpt_ccb_t *)0x55555555;
#else
    dpt->sp.ccb = (dpt_ccb_t *)NULL;
#endif
    
#ifdef DPT_HANDLE_TIMEOUTS
    if ( dccb->state & DPT_CCB_STATE_MARKED_LOST ) {
	struct timeval	now;
	u_int32_t	age;
	struct scsi_xfer *xs = dccb->xs;

	microtime(&now);
	age = dpt_time_delta(dccb->command_started, now);

	printf("dpt%d: Salvaging Tx %d from the jaws of destruction (%d/%d)\n",
	       dpt->unit, dccb->transaction_id, xs->timeout, age);
	dccb->state |= DPT_CCB_STATE_MARKED_SALVAGED;
	dccb->state &= ~DPT_CCB_STATE_MARKED_LOST;
    }
#endif

#ifdef DPT_TRACK_CCB_STATES
    if ( ((dccb->state & (DPT_CCB_STATE_COMPLETED | DPT_CCB_STATE_DONE))
	  || !(dccb->state & DPT_CCB_STATE_SUBMITTED)) ) {
#ifdef DPT_HANDLE_TIMEOUTS
	if ( dccb->state & DPT_CCB_STATE_ABORTED ) {
	    struct timeval now;
	    u_int32_t	   age;

	    microtime(&now);
	    age = dpt_time_delta(dccb->command_started, now);

	    printf("dpt%d: At the age of %dus, %d is a stale transaction\n",
		   dpt->unit, age, dccb->transaction_id);
	} else
#endif
        {
	
	    printf("dpt%d: In %d, %s is a bad state.\n"
		   "      Should be SUBMITTED, not DONE\n",
		   dpt->unit, dccb->transaction_id,
		   i2bin((unsigned long) dccb->state,
			 sizeof(dccb->state) * 8));
#ifdef DPT_MEASURE_PERFORMANCE
	    ++dpt->performance.aborted_interrupts;
#endif
	}
	status = dpt_inb(dpt, HA_RSTATUS); /* This CLEARS the interrupt! */
	return;
    }
#endif
    /* Ignore status packets with EOC not set */
    if ( dpt->sp.EOC == 0 ) {
	printf("dpt%d ERROR: Request %d recieved with clear EOC.\n"
	       "     Marking as LOST.\n",
	       dpt->unit, dccb->transaction_id);
#ifdef DPT_VERIFY_HINTR
	dpt->sp.ccb = (dpt_sp_t *)0x55555555;
#else	
	dpt->sp.ccb = (dpt_sp_t *)NULL;
#endif
	
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.aborted_interrupts;
#endif

#ifdef DPT_HANDLE_TIMEOUTS
	dccb->state |= DPT_CCB_STATE_MARKED_LOST;
#endif
	status = dpt_inb(dpt, HA_RSTATUS); /* This CLEARS the interrupt! */
	return;
    }

    dpt->sp.EOC = 0;

#ifdef DPT_VERIFY_HINTR
    /* Make SURE the next caller is legitimate.
     * If they are not, we will find 0x55555555 here.
     * We see 0x000000 or 0xffffffff when the PCi bus has DMA 
     * troubles (as when behing a PCI-PCI * bridge .
     */
    if ( (dccb == NULL)
	 || (dccb == (dpt_ccb_t *)0xffffffff)
	 || (dccb == (dpt_ccb_t *)0x55555555) ) {
#ifdef DPT_DEBUG_HEX_DUMPS
	hex_dump((u_int8_t *) & dpt->sp, sizeof(dpt_sp_t),
		 "StatusPacket", __LINE__);
#endif
	printf("dpt%d: BAD (%x) CCB in SP (AUX status = %s).\n",
	       dpt->unit, dccb, i2bin((unsigned long) aux_status,
				      sizeof(aux_status) * 8));
#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.aborted_interrupts;
#endif

	status = dpt_inb(dpt, HA_RSTATUS); /* This CLEARS the interrupt! */
	return;
    }
    
    for ( tccb = TAILQ_FIRST(&dpt->submitted_ccbs);
	  (tccb != NULL) && (tccb != dccb);
	  tccb = TAILQ_NEXT(tccb, links) );
    if ( tccb == NULL ) {
	printf("dpt%d: %x is not in the SUBMITTED queue\n",
	       dpt->unit, dccb);

	for ( tccb = TAILQ_FIRST(&dpt->completed_ccbs);
	      (tccb != NULL) && (tccb != dccb);
	      tccb = TAILQ_NEXT(tccb, links));
	if ( tccb != NULL )
	    printf("dpt%d: %x is in the COMPLETED queue\n",
		   dpt->unit, dccb);
	    
	for ( tccb = TAILQ_FIRST(&dpt->waiting_ccbs);
	      (tccb != NULL) && (tccb != dccb);
	      tccb = TAILQ_NEXT(tccb, links));
	if ( tccb != NULL )
	    printf("dpt%d: %x is in the WAITING queue\n", dpt->unit, dccb);

	for ( tccb = TAILQ_FIRST(&dpt->free_ccbs);
	      (tccb != NULL) && (tccb != dccb);
	      tccb = TAILQ_NEXT(tccb, links));
	if ( tccb != NULL )
	    printf("dpt%d: %x is in the FREE queue\n", dpt->unit, dccb);

#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.aborted_interrupts;
#endif
	status = dpt_inb(dpt, HA_RSTATUS); /* This CLEARS the interrupt! */
	return;
    }
#endif /* DPT_VERIFY_HINTR */

    /*
     * Copy the status packet from the general area to the dpt_ccb.
     * According to Mark Salyzyn, we only need few pieces of it.
     * Originally we had:
     * bcopy( (void *) &dpt->sp, (void *) &dccb->status_packet,
     *        sizeof(dpt_sp_t) );
     */
    dccb->status_packet.hba_stat	= dpt->sp.hba_stat;
    dccb->status_packet.scsi_stat	= dpt->sp.scsi_stat;
    dccb->status_packet.residue_len = dpt->sp.residue_len;

    /* Make sure the EOC bit is OFF! */
    dpt->sp.EOC = 0;

    /* Clear interrupts, check for error */
    if ((status = dpt_inb(dpt, HA_RSTATUS)) & HA_SERROR) {
	/*
	 * Error Condition. Check for magic cookie. Exit this test
	 * on earliest sign of non-reset condition
	 */
	
	/* Check that this is not a board reset interrupt */
	if (dpt_just_reset(dpt)) {
	    printf("dpt%d: HBA rebooted.\n"
		   "      All transactions should be resubmitted\n",
		   dpt->unit);
		
	    printf("dpt%d: >>---->>  This is incomplete, fix me....  <<----<<",
		   dpt->unit);
	    printf("      Incomplete Code; Re-queue the lost commands\n",
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
	
#ifdef DPT_DEBUG_HARDINTR
    printf("dpt%d, intr() status = %s\n", dpt->unit,
	   i2bin((unsigned long) status, sizeof(status) * 8));
#endif

    /* Mark BOTH queues as busy */
    dpt->queue_status |= (DPT_SUBMITTED_QUEUE_ACTIVE
			  | DPT_COMPLETED_QUEUE_ACTIVE);
    dpt_Qremove_submitted(dpt, dccb);
#ifdef DPT_TRACK_CCB_STATES
    dccb->state |= DPT_CCB_STATE_SUBMITTED2COMPLETED;
#endif
    dpt_Qadd_completed(dpt, dccb);
    dpt->queue_status &= ~(DPT_SUBMITTED_QUEUE_ACTIVE
			   | DPT_COMPLETED_QUEUE_ACTIVE);
	
#ifdef DPT_TRACK_CCB_STATES
    dccb->state &= ~DPT_CCB_STATE_SUBMITTED2COMPLETED;
    dccb->state |= DPT_CCB_STATE_COMPLETED;
    
#endif
#ifdef DPT_DEBUG_HARDINTR
    printf("dpt%d: intr() Submitted = %d, Completed = %d\n",
	   dpt->unit, dpt->submitted_ccbs_count, dpt->completed_ccbs_count);
#endif

    dpt_sched_queue(dpt);

#ifdef DPT_MEASURE_PERFORMANCE
    {
	u_int32_t result;
	struct timeval junk;
	
	microtime(&junk);
	
	result = dpt_time_delta(dpt->performance.intr_started, junk);
	
	if (result != 0xffffffff) {
	    if ( dpt->performance.max_intr_time < result )
		dpt->performance.max_intr_time = result;

	    if ( (dpt->performance.min_intr_time == 0)
		 || (dpt->performance.min_intr_time > result) ) {
		dpt->performance.min_intr_time = result;
	    }
	}
    }
#endif
}

/* This function is the DPT_ISR Software Interrupt Service Routine.
 * When the DPT completes a SCSI command, it puts the results in a Status
 * Packet, sets up two 1-byte registers and generates an interrupt.  We catch
 * this interrupt in dpt_intr and copy the whole status to the proper CCB.
 * Once this is done, we generate a software interrupt that calls this routine.
 * The routine then scans ALL the complete queues of all the DPT HBA's and
 * processes ALL the commands that are in the queue.
 *
 * XXXX REMEMBER:	We always scan ALL the queues of all the HBA's. Always
 * starting with the first controller registered (dpt0).	This creates an
 * ``unfair'' opportunity for the first controllers in being served.
 * Careful instrumentation may prove a need to change this policy.
 *
 * This command rns at splSOFTcam.  Remember that.
 */

void
dpt_sintr(void)
{
    dpt_softc_t *dpt;
    int			ospl;
	
    /* Find which DPT needs help */
    for (dpt = TAILQ_FIRST(&dpt_softc_list);
	 dpt != NULL;
	 dpt = TAILQ_NEXT(dpt, links)) {
	/*
	 * Drain the completed queue, to make room for new, waiting requests.
	 * We change to splcam to block interrupts from mucking with the
	 * completed queue
	 */
	ospl = splcam();
	if ( dpt->queue_status & DPT_SINTR_ACTIVE ) {
	    splx(ospl);
#ifdef DPT_DEBUG_SOFTINTR
	    printf("dpt%d, intr() status = %s (SINTR_ACTIVE)\n", dpt->unit,
		   i2bin((unsigned long) dpt->queue_status,
			 sizeof(dpt->queue_status) * 8));
#endif
	    continue;
	} 

	dpt->queue_status |= DPT_SINTR_ACTIVE;

	if ( !TAILQ_EMPTY(&dpt->completed_ccbs) ) {
	    	splx(ospl);
		dpt_complete(dpt);
		ospl = splcam();
	}
	
	/* Submit as many waiting requests as the DPT can take */
	if ( !TAILQ_EMPTY(&dpt->waiting_ccbs) ) {
		dpt_run_queue(dpt, 0);
	}

	dpt->queue_status &= ~DPT_SINTR_ACTIVE;
	splx(ospl);
    }
}

/*
 * Scan the complete queue for a given controller and process ALL the completed
 * commands in the queue.
 */

static void
dpt_complete(dpt_softc_t *dpt)
{	
    dpt_ccb_t *ccb;
#ifdef DPT_DEBUG_SOFTINTR
    int	ndx = 0;
#endif
    int	ospl;
	
    ospl = splcam();
	
    if (dpt->queue_status & DPT_COMPLETED_QUEUE_ACTIVE) {
#ifdef DPT_DEBUG_SOFTINTR
	printf("dpt%d: complete() Queue is already active\n",
	       dpt->unit);
#endif
	splx(ospl);
	return;
    }

    dpt->queue_status |= DPT_COMPLETED_QUEUE_ACTIVE;

    while ( (ccb = TAILQ_FIRST(&dpt->completed_ccbs)) != NULL ) {
	struct scsi_xfer *xs;
	
#ifdef DPT_DEBUG_SOFTINTR
	printf("dpt%d: complete() Processing %d on t%df%dw%ds%dc%d\n",
	       dpt->unit, ccb->transaction_id, dpt->total_ccbs_count, 
	       dpt->free_ccbs_count,
	       dpt->waiting_ccbs_count, dpt->submitted_ccbs_count,
	       dpt->completed_ccbs_count);
	++ndx;
#endif

#ifdef DPT_TRACK_CCB_STATES
	if ( (!(ccb->state & DPT_CCB_STATE_COMPLETED)
	      || (ccb->state & DPT_CCB_STATE_COMPLETED2DONE))
	     && !(ccb->state & DPT_CCB_STATE_MARKED_SALVAGED) ) {
	    printf("dpt%d: In %d, %s is a bad state. Should be COMPLETED\n",
		   dpt->unit, ccb->transaction_id, 
		   i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));
	}

	ccb->state |= DPT_CCB_STATE_COMPLETED2DONE;
#endif
	dpt_Qremove_completed(dpt, ccb);
	splx(ospl);

	/* Process this completed request */
	if (dpt_process_completion(dpt, ccb) == 0) {	
	    if ( (xs = ccb->xs) == NULL ) {
		/* Must be a user command */
		wakeup(ccb);
	    } else {	
		ospl = splcam();
		dpt_Qpush_free(dpt, ccb);
		splx(ospl);
		
#ifdef DPT_MEASURE_PERFORMANCE
		{
		    u_int32_t result;
		    struct timeval junk;
	
		    microtime(&junk);
		    ccb->command_ended = junk;
	
		    result = dpt_time_delta(ccb->command_started,
					ccb->command_ended);
#define maxctime dpt->performance.max_command_time[ccb->eata_ccb.cp_scsi_cmd]
#define minctime dpt->performance.min_command_time[ccb->eata_ccb.cp_scsi_cmd]
		
		    if (result != 0xffffffff) {
			if ( maxctime < result ) {
			    maxctime = result;
			}

			if ( (minctime == 0) || (minctime > result) )
			    minctime = result;
		    }	
		}
#endif

#ifdef DPT_TRACK_CCB_STATES
		if ( ccb->state & DPT_CCB_STATE_DONE ){
		    printf("dpt%d: %s is bad status.\n"
			   "		Should NOT be DONE\n",
			   dpt->unit,
			   i2bin((unsigned int) ccb->state,
				 sizeof(ccb->state) * 8));
		}
#endif
		scsi_done(xs);
#ifdef DPT_TRACK_CCB_STATES
		ccb->state &= ~DPT_CCB_STATE_COMPLETED2DONE;
		ccb->state |= DPT_CCB_STATE_DONE;
#endif
	    }
	    ospl = splcam();
	}
    }
    splx(ospl);

    /*
     * As per Justin's suggestion, we now will call the run_queue for
     * this HBA. This is done in case there are left-over requests that
     * were not submitted yet.
     */
    dpt_run_queue(dpt, 0);	
    ospl = splsoftcam();
    dpt->queue_status &= ~DPT_COMPLETED_QUEUE_ACTIVE;
    splx(ospl);
}

/*
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

static INLINE void
dpt_IObySize(dpt_softc_t *dpt, dpt_ccb_t *ccb, int op, int index)
{
    if ( op == READ_OP ) {
	++dpt->performance.read_by_size_count[index];
	if ( ccb->submitted_time != 0 ) {
	    if ( (min_submitR == 0) || (ccb->submitted_time < min_submitR) ) {
		min_submitR = ccb->submitted_time;
	    }
	    
	    if ( ccb->submitted_time > max_submitR ) {
		max_submitR = ccb->submitted_time;
	    }
	}
    } else {
	++dpt->performance.write_by_size_count[index];	
	if ( ccb->submitted_time != 0 ) {
	    if ( (ccb->submitted_time < min_submitW) || (min_submitW == 0) ) {
		min_submitW = ccb->submitted_time;
	    }
	    
	    if ( ccb->submitted_time > max_submitW ) {
		max_submitW = ccb->submitted_time;
	    }
	}
    }		
}
    
static int
dpt_process_completion(dpt_softc_t * dpt,
		       dpt_ccb_t * ccb)
{
    int	ospl, told_it = 0;
	
    struct scsi_xfer *xs;
	
    if ( ccb == NULL ) {
	panic("dpt%d: Improper argumet to process_completion (%p%p)\n",
	      dpt->unit, ccb);
    } else {
	xs = ccb->xs;
    }
	
#ifdef DPT_MEASURE_PERFORMANCE
    {
	u_int32_t size;
	struct  scsi_rw_big *cmd;
	int	op_type;

	cmd = (struct scsi_rw_big *)&ccb->eata_ccb.cp_scsi_cmd;
		 
	switch ( cmd->op_code ) {
	case 0xa8:	/* 12-byte READ	*/
	case 0x08:	/* 6-byte READ	*/
	case 0x28:	/* 10-byte READ	*/
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

	if ( op_type != 0 ) {
	    
	    size = (((u_int32_t) cmd->length2 << 8)
		    | ((u_int32_t) cmd->length1)) << 9;
	
	    switch ( size ) {
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
		if ( size > (1 << 16) )
		    dpt_IObySize(dpt, ccb, op_type, SIZE_BIGGER);
		
		else
		    dpt_IObySize(dpt, ccb, op_type, SIZE_OTHER);
		break;
	    }
	}	
    }
#endif		/* DPT_MEASURE_PERFORMANCE */
	
#ifdef DPT_DEBUG_COMPLETION
    printf("dpt%d: Tx%d \"%s\" completed (%x) \n"
	   "      on c%db%dt%du%d\n",
	   dpt->unit, ccb->transaction_id,
	   scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
	   (int) ccb->status_packet.hba_stat,
	   dpt->unit, ccb->eata_ccb.cp_channel,
	   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
#endif

    switch ((int) ccb->status_packet.hba_stat) {
    case HA_NO_ERROR:
	if ( xs !=NULL ) {	    
	    xs->error = XS_NOERROR;
	    xs->flags |= SCSI_ITSDONE;
	}
	break;
    case HA_ERR_SEL_TO:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	    printf("dpt%d: Select Timeout error on %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
#endif
    case HA_ERR_CMD_TO:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it )
	    printf("dpt%d: Command Timeout error on %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
#endif
	if ( xs !=NULL ) {	    
	    xs->error |= XS_SELTIMEOUT;
	    xs->flags |= SCSI_ITSDONE;
	}
	break;
    case HA_SCSIBUS_RESET:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	printf("dpt%d: Command Timeout error on %s - %d\n"
	       "      for c%db%dt%du%d\n",
	       dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
	       ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
	       ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	++told_it;
#endif
    case HA_CP_ABORTED:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: Command Packet Abort error on %s/%d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_CP_RESET:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: Command Packet Reset error on %s/%d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_PCI_PARITY:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Parity error on %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_PCI_MABORT:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Master Abort error on %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_PCI_TABORT:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Target Abort error on %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_PCI_STABORT:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Slave Target Abort error on %s - %d\n"
		   "		for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_BUS_PARITY:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Bus Parity error on %s %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_UNX_MSGRJCT:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it) {
	    printf("dpt%d: PCI Bus Message Reject for %s - %d\n"
		   "      for c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	}
#endif
	if (ccb->retries++ > DPT_RETRIES) {
	    if ( xs != NULL ) {
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
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Rebooted controller for %s - %d\n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_UNX_BUSPHASE:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Unexpected BusPhase for %s - %d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_UNX_BUS_FREE:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Unexpected BusFree. for %s - -%d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_SCSI_HUNG:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: SCSI Hung. Needs Reset for %s - %d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_RESET_STUCK:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: SCSI Reset Stuck. for %s - %d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
	if (ccb->retries++ > DPT_RETRIES) {
	    if ( xs != NULL ) {
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
	    if ( xs != NULL ) {
		xs->error |= XS_SENSE;
		xs->flags |= SCSI_ITSDONE;
	    }
	} else {
	    if (ccb->retries++ > DPT_RETRIES) {
		if ( xs != NULL ) {
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
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Controller RAM Parity Error, for %s/%d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_CP_ABORT_NA:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Abort inactive command, for %s - %d \n"
		   "on c%db%dt%du%d!\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_CP_RESET_NA:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Reset inactive command, for %s - %d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    case HA_ECC_ERR:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Controller RAM ECC Error for %s - %d \n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	    ++told_it;
	}
#endif
    default:
#ifdef DPT_DEBUG_COMPLETION_ERRORS
	if ( !told_it ) {
	    printf("dpt%d: Undocumented Error %x, EOC=%s for %s - %d\n"
		   "on c%db%dt%du%d\n",
		   dpt->unit, ccb->status_packet.EOC ? "TRUE" : "False",
		   scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		   ccb->transaction_id, dpt->unit, ccb->eata_ccb.cp_channel,
		   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN);
	}
#endif
	Debugger("DPT:	Undocumented Error");
    }

    if ( xs != NULL ) 
    {
	if (xs->error & XS_SENSE)
	    bcopy(&ccb->sense_data, &xs->sense,
		  sizeof(struct scsi_sense_data));
	
	if (ccb->status_packet.residue_len != 0) {
	    xs->flags |= SCSI_RESID_VALID;
	    xs->resid = ccb->status_packet.residue_len;
	}
    }
    
    return (0);
}

#ifdef DPT_HANDLE_TIMEOUTS
/*
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
dpt_handle_timeouts(dpt_softc_t *dpt)
{
    dpt_ccb_t *ccb;
    int		ospl;

    ospl = splcam();

    if ( dpt->state & DPT_HA_TIMEOUTS_ACTIVE ) {
	printf("dpt%d WARNING: Timeout Handling Collision\n", dpt->unit);
	splx(ospl);
	return;
    }

    dpt->state |= DPT_HA_TIMEOUTS_ACTIVE;

    /* Loop through the entire submitted queue, looking for lost souls */
    for ( ccb = TAILQ_FIRST(&dpt->submitted_ccbs);
	  ccb != NULL;
	  ccb = TAILQ_NEXT(ccb, links)) {
	struct scsi_xfer *xs;
	struct timeval	now;
	u_int32_t		age;

	xs	= ccb->xs;

	microtime(&now);
	age = dpt_time_delta(ccb->command_started, now);

#define TenSec	10000000
#define max_age ( ((xs->timeout * (dpt->submitted_ccbs_count		\
				   + DPT_TIMEOUT_FACTOR)) > TenSec)	\
		  ? (xs->timeout * (dpt->submitted_ccbs_count		\
				    + DPT_TIMEOUT_FACTOR))		\
		  : TenSec )
	
        /* If a transaction is marked lost and is TWICE as old as we care,
	 * then, and only then do we destroy it!
	 */
	if ( ccb->state & DPT_CCB_STATE_MARKED_LOST ) {
	    /* Remember who is next */
	    if ( age > (max_age * 2) ) {
		dpt_Qremove_submitted(dpt, ccb);
		ccb->state &= ~DPT_CCB_STATE_MARKED_LOST;
		ccb->state |= DPT_CCB_STATE_ABORTED;

		if (ccb->retries++ > DPT_RETRIES) {
		    printf("dpt%d ERROR: Destroying stale %d (%s)\n"
			   "		on c%db%dt%du%d (%d/%d)\n",
			   dpt->unit, ccb->transaction_id,
			   scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
			   dpt->unit, ccb->eata_ccb.cp_channel,
			   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN, age,
			   ccb->retries);

		    (void) dpt_send_immediate(dpt, (u_int32_t)&ccb->eata_ccb,
					      EATA_SPECIFIC_ABORT, 0, 0);
		    dpt_Qpush_free(dpt, ccb);

		    xs->error |= XS_TIMEOUT; /* The SCSI layer should re-try */
		    xs->flags |= SCSI_ITSDONE;
		    scsi_done(xs);	
		} else {
		    printf("dpt%d ERROR: Stale %d (%s) on c%db%dt%du%d (%d)\n"
			   "		gets another chance(%d/%d)\n",
			   dpt->unit, ccb->transaction_id,
			   scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
			   dpt->unit, ccb->eata_ccb.cp_channel,
			   ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN,
			   age, ccb->retries, DPT_RETRIES );

		    dpt_Qpush_waiting(dpt, ccb);
		    dpt_sched_queue(dpt);
		}
	    }
	} else {
	    /* This is a transaction that is not to be destroyed (yet)
	     * But it is too old for our liking.
	     * We wait as long as the upper layer thinks.
	     * Not really, we multiply that by the number of commands in the 
	     * submitted queue + 1.
	     */
	    if ( !(ccb->state & DPT_CCB_STATE_MARKED_LOST) &&
		 (age != 0xffffffff) && (age > max_age) ) {
		printf("dpt%d ERROR: Marking %d (%s) on c%db%dt%du%d \n"
		       "            as late after %dusec\n",
		       dpt->unit, ccb->transaction_id,
		       scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		       dpt->unit, ccb->eata_ccb.cp_channel,
		       ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN, age);
		ccb->state |= DPT_CCB_STATE_MARKED_LOST;
	    } else {
#ifdef DPT_DEBUG_TIMEOUTS
		printf("dpt%d: Leaving %d \"%s\" alone on c%db%dt%du%d (%d)\n",
		       dpt->unit, ccb->transaction_id,
		       scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd),
		       dpt->unit, ccb->eata_ccb.cp_channel,
		       ccb->eata_ccb.cp_id, ccb->eata_ccb.cp_LUN, age);
#endif
	    }
	}
    }

    dpt->state &= ~DPT_HA_TIMEOUTS_ACTIVE;
    splx(ospl);
}

static void
dpt_timeout(void *arg)
{
    dpt_softc_t *dpt = (dpt_softc_t *)arg;

    if ( !(dpt->state & DPT_HA_TIMEOUTS_ACTIVE) )
	dpt_handle_timeouts(dpt);

    timeout((timeout_func_t)dpt_timeout, (caddr_t)dpt, hz * 10);
}

#endif /* DPT_HANDLE_TIMEOUTS */

/* 
 * Remove a ccb from the completed queue 
 */
static INLINE void
dpt_Qremove_completed(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
    u_int32_t		complete_time;
    struct timeval now;

    microtime(&now);
    complete_time = dpt_time_delta(ccb->command_ended, now);
	
    if (complete_time != 0xffffffff) {
	if ( dpt->performance.max_complete_time < complete_time )
	    dpt->performance.max_complete_time = complete_time;
	if ( (dpt->performance.min_complete_time == 0) ||
	     (dpt->performance.min_complete_time > complete_time) )
	    dpt->performance.min_complete_time = complete_time;
    }
#endif
	
    TAILQ_REMOVE(&dpt->completed_ccbs, ccb, links);
    --dpt->completed_ccbs_count;  /* One less completed ccb in the queue */
#ifdef DPT_TRACK_CCB_STATES
    if ( !(ccb->state & DPT_CCB_STATE_COMPLETED) )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should be COMPLETED\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));

    ccb->state &= ~DPT_CCB_STATE_COMPLETED;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 * Pop the most recently used ccb off the (HEAD of the) FREE ccb queue
 */
static INLINE dpt_ccb_t *
dpt_Qpop_free(dpt_softc_t * dpt)
{
    dpt_ccb_t *ccb;
	
    if ((ccb = TAILQ_FIRST(&dpt->free_ccbs)) != NULL)
	TAILQ_REMOVE(&dpt->free_ccbs, ccb, links);
	
    --dpt->free_ccbs_count;
#ifdef DPT_TRACK_CCB_STATES
    if ( !(ccb->state & DPT_CCB_STATE_FREE) )
	printf("dpt%d: in %d, %s is a bad state.\n"
	       "		Should be FREE\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));

    ccb->state &= ~(DPT_CCB_STATE_FREE | DPT_CCB_STATE_DONE);
#endif
    return (ccb);
}

/*
 * Put a (now freed) ccb back into the HEAD of the FREE ccb queue
 */
static INLINE void
dpt_Qpush_free(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_FREELIST_IS_STACK
    TAILQ_INSERT_HEAD(&dpt->free_ccbs, ccb, links)
#else
	TAILQ_INSERT_TAIL(&dpt->free_ccbs, ccb, links);
#endif

#ifdef DPT_TRACK_CCB_STATES
    if ( ccb->state & DPT_CCB_STATE_FREE )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should NOT be FREE\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));

    ccb->state |= DPT_CCB_STATE_FREE;
    ccb->state &= ~DPT_CCB_STATE_MARKED_SALVAGED;
#endif
    ++ dpt->free_ccbs_count;
}

/*
 *	Add a request to the TAIL of the WAITING ccb queue
 */
static INLINE void
dpt_Qadd_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
    struct timeval junk;

    TAILQ_INSERT_TAIL(&dpt->waiting_ccbs, ccb, links);
    ++dpt->waiting_ccbs_count;
	
#ifdef DPT_MEASURE_PERFORMANCE
    microtime(&junk);
    ccb->command_ended = junk;
    if (dpt->waiting_ccbs_count > dpt->performance.max_waiting_count)
	dpt->performance.max_waiting_count = dpt->waiting_ccbs_count;
#endif
#ifdef DPT_TRACK_CCB_STATES
    if ( ccb->state & DPT_CCB_STATE_WAITING )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "      Should NOT be WAITING\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));

    ccb->state |= DPT_CCB_STATE_WAITING;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 *	Add a request to the HEAD of the WAITING ccb queue
 */
static INLINE void
dpt_Qpush_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
    struct timeval junk;

    TAILQ_INSERT_HEAD(&dpt->waiting_ccbs, ccb, links);
    ++dpt->waiting_ccbs_count;
	
#ifdef DPT_MEASURE_PERFORMANCE
    microtime(&junk);
    ccb->command_ended = junk;

    if ( dpt->performance.max_waiting_count < dpt->waiting_ccbs_count )
	dpt->performance.max_waiting_count = dpt->waiting_ccbs_count;

#endif
#ifdef DPT_TRACK_CCB_STATES
    if ( ccb->state & DPT_CCB_STATE_WAITING )
	printf("dpt%d: In %d, %s is not a bad state.\n"
	       "		Should NOT be WAITING\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8));

    ccb->state |= DPT_CCB_STATE_WAITING;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 * Remove a ccb from the waiting queue
 */
static INLINE void
dpt_Qremove_waiting(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
    struct timeval now;
    u_int32_t		waiting_time;

    microtime(&now);
    waiting_time = dpt_time_delta(ccb->command_ended, now);
	
    if (waiting_time != 0xffffffff) {
	if ( dpt->performance.max_waiting_time < waiting_time )
	    dpt->performance.max_waiting_time = waiting_time;
	if ( (dpt->performance.min_waiting_time == 0) ||
	     (dpt->performance.min_waiting_time > waiting_time) )
	    dpt->performance.min_waiting_time = waiting_time;
    }
#endif
	
    TAILQ_REMOVE(&dpt->waiting_ccbs, ccb, links);
    --dpt->waiting_ccbs_count;	/* One less waiting ccb in the queue	*/

#ifdef DPT_TRACK_CCB_STATES
    if ( !(ccb->state & DPT_CCB_STATE_WAITING) )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should be WAITING\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8)); 

    ccb->state &= ~DPT_CCB_STATE_WAITING;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 * Add a request to the TAIL of the SUBMITTED ccb queue
 */
static INLINE void
dpt_Qadd_submitted(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
    struct timeval junk;

    TAILQ_INSERT_TAIL(&dpt->submitted_ccbs, ccb, links);
    ++dpt->submitted_ccbs_count;
	
#ifdef DPT_MEASURE_PERFORMANCE
    microtime(&junk);
    ccb->command_ended = junk;
    if (dpt->performance.max_submit_count < dpt->submitted_ccbs_count)
	dpt->performance.max_submit_count = dpt->submitted_ccbs_count;
#endif
#ifdef DPT_TRACK_CCB_STATES
    if ( ccb->state & DPT_CCB_STATE_SUBMITTED )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should NOT be SUBMITTED\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8)); 

    ccb->state |= DPT_CCB_STATE_SUBMITTED;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 * Add a request to the TAIL of the Completed ccb queue
 */
static INLINE void
dpt_Qadd_completed(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
    struct timeval junk;

    TAILQ_INSERT_TAIL(&dpt->completed_ccbs, ccb, links);
    ++dpt->completed_ccbs_count;
	
#ifdef DPT_MEASURE_PERFORMANCE
    microtime(&junk);
    ccb->command_ended = junk;
    if (dpt->performance.max_complete_count < dpt->completed_ccbs_count)
	dpt->performance.max_complete_count = dpt->completed_ccbs_count;
#endif

#ifdef DPT_TRACK_CCB_STATES
    if ( ccb->state & DPT_CCB_STATE_COMPLETED )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should NOT be COMPLETED\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8)); 

    ccb->state |= DPT_CCB_STATE_COMPLETED;
#endif

    if ( dpt->state & DPT_HA_SHUTDOWN_ACTIVE )
	wakeup(&dpt);
}

/*
 * Remove a ccb from the submitted queue
 */
static INLINE void
dpt_Qremove_submitted(dpt_softc_t * dpt, dpt_ccb_t * ccb)
{
#ifdef DPT_MEASURE_PERFORMANCE
    struct timeval now;
    u_int32_t	   submit_time;

    microtime(&now);
    submit_time = dpt_time_delta(ccb->command_ended, now);
	
    if (submit_time != 0xffffffff) {
	ccb->submitted_time = submit_time;
	if ( dpt->performance.max_submit_time < submit_time )
	    dpt->performance.max_submit_time = submit_time;
	if ( (dpt->performance.min_submit_time == 0)
	     || (dpt->performance.min_submit_time > submit_time) )
	    dpt->performance.min_submit_time = submit_time;
    } else {
	ccb->submitted_time = 0;
    }
    
#endif
	
    TAILQ_REMOVE(&dpt->submitted_ccbs, ccb, links);
    --dpt->submitted_ccbs_count; /* One less submitted ccb in the queue	*/
#ifdef DPT_TRACK_CCB_STATES
    if ( !(ccb->state & DPT_CCB_STATE_SUBMITTED) )
	printf("dpt%d: In %d, %s is a bad state.\n"
	       "		Should be SUBMITTED\n",
	       dpt->unit, ccb->transaction_id,
	       i2bin((unsigned long) ccb->state, sizeof(ccb->state) * 8)); 

    ccb->state &= ~DPT_CCB_STATE_SUBMITTED;	
#endif

    if ( (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
	 || (dpt->state & DPT_HA_QUIET) )
	wakeup(&dpt);
}

/*
 * Handle Shutdowns.
 * Gets registered by the dpt_pci.c regiustar and called AFTER the system did
 * all its sync work.
 */

void
dpt_shutdown(int howto, void *arg_dpt)
{
    dpt_softc_t *ldpt;
    u_int8_t	 channel;
    u_int32_t	 target;
    u_int32_t	 lun;
    int		 waiting;
    int		 submitted;
    int		 completed;
    int		 huh;
    int		 wait_is_over;
    int		 ospl;
    dpt_softc_t *dpt;

    dpt = (dpt_softc_t *)arg_dpt;

    printf("dpt%d: Shutting down (mode %d) HBA.	Please wait...",
	   dpt->unit, howto);
    wait_is_over = 0;
	
    ospl = splcam();
    dpt->state |= DPT_HA_SHUTDOWN_ACTIVE;
    splx(ospl);

    while ( (((waiting = dpt->waiting_ccbs_count) != 0)
	     || ((submitted = dpt->submitted_ccbs_count) != 0)
	     || ((completed = dpt->completed_ccbs_count) != 0))
	    && (wait_is_over == 0) ) {
#ifdef DPT_DEBUG_SHUTDOWN
	printf("dpt%d: Waiting for queues w%ds%dc%d to deplete\n",
	       dpt->unit, dpt->waiting_ccbs_count, dpt->submitted_ccbs_count,
	       dpt->completed_ccbs_count);
#endif
	huh = tsleep((void *)dpt, PCATCH | PRIBIO, "dptoff", 100 * hz);
	switch ( huh ) {
	case 0:
	    /* Wakeup call received */
	    goto checkit;
	    break;
	case EWOULDBLOCK:
	    /* Timer Expired */
	    printf("dpt%d: Shutdown timer expired with queues at w%ds%dc%d\n",
		   dpt->unit, dpt->waiting_ccbs_count,
		   dpt->submitted_ccbs_count,
		   dpt->completed_ccbs_count);
	    ++wait_is_over;
	    break;
	default:
	    /* anything else */
	    printf("dpt%d: Shutdown UNKNOWN with qeueues at w%ds%dc%d\n",
		   dpt->unit, dpt->waiting_ccbs_count,
		   dpt->submitted_ccbs_count,
		   dpt->completed_ccbs_count);
	    ++wait_is_over;
	    break;
	}
checkit:
	
    }

    /* What we do for a shutdown, is give the DPT early power loss warning. */
    (void) dpt_send_immediate(dpt, 0, EATA_POWER_OFF_WARN, 0, 0);
    printf("dpt%d: Controller was warned of shutdown and is now disabled\n",
	   dpt->unit);

    return;
}

/* A primitive subset of isgraph.	Used by hex_dump below */
#define IsGraph(val)	( (((val) >= ' ') && ((val) <= '~')) )

#ifdef DPT_DEBUG_HEX_DUMPS
/*
 * This function dumps bytes to the screen in hex format.
 */
void
hex_dump(u_int8_t * data, int length, char *name, int no)
{
    int	line, column, ndx;
	
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
		int	ascii_pos = ndx - 15 + column;
	
		/*
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
		
	    /*
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
	
    /*
     * We are basically done. We do want, however, to handle the ASCII
     * translation of fractional lines.
     */
    if ((ndx == length) && (column != 0)) {
	int	modulus = 16 - column, spaces = modulus * 3, skip;
	
	/*
	 * Skip to the right, as many spaces as there are bytes
	 * ``missing'' ...
	 */
	for (skip = 0; skip < spaces; skip++)
	    printf(" ");
	
	/* ... And the gap separating the hex dump from the ASCII */
	printf("  ");
	
	/*
	 * Do not forget the extra space that splits the hex dump
	 * vertically
	 */
	if (column < 8)
	    printf(" ");
	
	for (column = 0; column < (16 - modulus); column++) {
	    int	ascii_pos = ndx - (16 - modulus) + column;
		
	    if (IsGraph(data[ascii_pos]))
		printf("%c", data[ascii_pos]);
	    else
		printf(".");
	}
	printf("\n");
    }
}

#endif /* DPT_DEBUG_HEX_DUMPS */

/*
 * and this one presents an integer as ones and zeros
 */

static char i2bin_bitmap[48];	/* Used for binary dump of registers */

char	*
i2bin(unsigned int no, int length)
{
    int	ndx, rind;
	
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

/*
 * This function substracts one timval structure from another,
 * Returning the result in usec.
 * It assumes that less than 4 billion usecs passed form start to end.
 * If times are sensless, 0xffffffff is returned.
 */

static	u_int32_t
dpt_time_delta(struct timeval start,
	       struct timeval end)
{
    u_int32_t result;

    if (start.tv_sec > end.tv_sec) {
	result = 0xffffffff;
    }
    else {
	if (start.tv_sec == end.tv_sec) {
	    if (start.tv_usec > end.tv_usec) {
		result = 0xffffffff;
	    }
	    else
		return (end.tv_usec - start.tv_usec);
	} else {
	    return (end.tv_sec - start.tv_sec) * 1000000 +
				end.tv_usec + (1000000 - start.tv_usec);
	}
    }
    return(result);
}

/*
 * This function translates a SCSI command numeric code to a human readable
 * string.
 * The string contains the class of devices, scope, description, (length),
 * and [SCSI III documentation section].
 */

char	*
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


/*-
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000-2001 Adaptec Corporation
 * All rights reserved.
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Adaptec and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose, are disclaimed. In no
 * event shall Adaptec be liable for any direct, indirect, incidental, special,
 * exemplary or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruptions) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this driver software, even
 * if advised of the possibility of such damage.
 *
 * SCSI I2O host adapter driver
 *
 *	V1.10 2004/05/05 scottl@freebsd.org
 *		- Massive cleanup of the driver to remove dead code and
 *		  non-conformant style.
 *		- Removed most i386-specific code to make it more portable.
 *		- Converted to the bus_space API.
 *	V1.08 2001/08/21 Mark_Salyzyn@adaptec.com
 *		- The 2000S and 2005S do not initialize on some machines,
 *		  increased timeout to 255ms from 50ms for the StatusGet
 *		  command.
 *	V1.07 2001/05/22 Mark_Salyzyn@adaptec.com
 *		- I knew this one was too good to be true. The error return
 *		  on ioctl commands needs to be compared to CAM_REQ_CMP, not
 *		  to the bit masked status.
 *	V1.06 2001/05/08 Mark_Salyzyn@adaptec.com
 *		- The 2005S that was supported is affectionately called the
 *		  Conjoined BAR Firmware. In order to support RAID-5 in a
 *		  16MB low-cost configuration, Firmware was forced to go
 *		  to a Split BAR Firmware. This requires a separate IOP and
 *		  Messaging base address.
 *	V1.05 2001/04/25 Mark_Salyzyn@adaptec.com
 *		- Handle support for 2005S Zero Channel RAID solution.
 *		- System locked up if the Adapter locked up. Do not try
 *		  to send other commands if the resetIOP command fails. The
 *		  fail outstanding command discovery loop was flawed as the
 *		  removal of the command from the list prevented discovering
 *		  all the commands.
 *		- Comment changes to clarify driver.
 *		- SysInfo searched for an EATA SmartROM, not an I2O SmartROM.
 *		- We do not use the AC_FOUND_DEV event because of I2O.
 *		  Removed asr_async.
 *	V1.04 2000/09/22 Mark_Salyzyn@adaptec.com, msmith@freebsd.org,
 *			 lampa@fee.vutbr.cz and Scott_Long@adaptec.com.
 *		- Removed support for PM1554, PM2554 and PM2654 in Mode-0
 *		  mode as this is confused with competitor adapters in run
 *		  mode.
 *		- critical locking needed in ASR_ccbAdd and ASR_ccbRemove
 *		  to prevent operating system panic.
 *		- moved default major number to 154 from 97.
 *	V1.03 2000/07/12 Mark_Salyzyn@adaptec.com
 *		- The controller is not actually an ASR (Adaptec SCSI RAID)
 *		  series that is visible, it's more of an internal code name.
 *		  remove any visible references within reason for now.
 *		- bus_ptr->LUN was not correctly zeroed when initially
 *		  allocated causing a possible panic of the operating system
 *		  during boot.
 *	V1.02 2000/06/26 Mark_Salyzyn@adaptec.com
 *		- Code always fails for ASR_getTid affecting performance.
 *		- initiated a set of changes that resulted from a formal
 *		  code inspection by Mark_Salyzyn@adaptec.com,
 *		  George_Dake@adaptec.com, Jeff_Zeak@adaptec.com,
 *		  Martin_Wilson@adaptec.com and Vincent_Trandoan@adaptec.com.
 *		  Their findings were focussed on the LCT & TID handler, and
 *		  all resulting changes were to improve code readability,
 *		  consistency or have a positive effect on performance.
 *	V1.01 2000/06/14 Mark_Salyzyn@adaptec.com
 *		- Passthrough returned an incorrect error.
 *		- Passthrough did not migrate the intrinsic scsi layer wakeup
 *		  on command completion.
 *		- generate control device nodes using make_dev and delete_dev.
 *		- Performance affected by TID caching reallocing.
 *		- Made suggested changes by Justin_Gibbs@adaptec.com
 *			- use splcam instead of splbio.
 *			- use cam_imask instead of bio_imask.
 *			- use u_int8_t instead of u_char.
 *			- use u_int16_t instead of u_short.
 *			- use u_int32_t instead of u_long where appropriate.
 *			- use 64 bit context handler instead of 32 bit.
 *			- create_ccb should only allocate the worst case
 *			  requirements for the driver since CAM may evolve
 *			  making union ccb much larger than needed here.
 *			  renamed create_ccb to asr_alloc_ccb.
 *			- go nutz justifying all debug prints as macros
 *			  defined at the top and remove unsightly ifdefs.
 *			- INLINE STATIC viewed as confusing. Historically
 *			  utilized to affect code performance and debug
 *			  issues in OS, Compiler or OEM specific situations.
 *	V1.00 2000/05/31 Mark_Salyzyn@adaptec.com
 *		- Ported from FreeBSD 2.2.X DPT I2O driver.
 *			changed struct scsi_xfer to union ccb/struct ccb_hdr
 *			changed variable name xs to ccb
 *			changed struct scsi_link to struct cam_path
 *			changed struct scsibus_data to struct cam_sim
 *			stopped using fordriver for holding on to the TID
 *			use proprietary packet creation instead of scsi_inquire
 *			CAM layer sends synchronize commands.
 */

#include <sys/cdefs.h>
#include <sys/param.h>	/* TRUE=1 and FALSE=0 defined here */
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/stat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#if defined(__i386__)
#include "opt_asr.h"
#include <i386/include/cputypes.h>

#ifndef BURN_BRIDGES
#if defined(ASR_COMPAT)
#define ASR_IOCTL_COMPAT
#endif /* ASR_COMPAT */
#endif /* !BURN_BRIDGES */

#elif defined(__alpha__)
#include <alpha/include/pmap.h>
#endif
#include <machine/vmparam.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define	osdSwap4(x) ((u_long)ntohl((u_long)(x)))
#define	KVTOPHYS(x) vtophys(x)
#include	"dev/asr/dptalign.h"
#include	"dev/asr/i2oexec.h"
#include	"dev/asr/i2obscsi.h"
#include	"dev/asr/i2odpt.h"
#include	"dev/asr/i2oadptr.h"

#include	"dev/asr/sys_info.h"

__FBSDID("$FreeBSD$");

#define	ASR_VERSION	1
#define	ASR_REVISION	'1'
#define	ASR_SUBREVISION '0'
#define	ASR_MONTH	5
#define	ASR_DAY		5
#define	ASR_YEAR	(2004 - 1980)

/*
 *	Debug macros to reduce the unsightly ifdefs
 */
#if (defined(DEBUG_ASR) || defined(DEBUG_ASR_USR_CMD) || defined(DEBUG_ASR_CMD))
static __inline void
debug_asr_message(PI2O_MESSAGE_FRAME message)
{
	u_int32_t * pointer = (u_int32_t *)message;
	u_int32_t   length = I2O_MESSAGE_FRAME_getMessageSize(message);
	u_int32_t   counter = 0;

	while (length--) {
		printf("%08lx%c", (u_long)*(pointer++),
		  (((++counter & 7) == 0) || (length == 0)) ? '\n' : ' ');
	}
}
#endif /* DEBUG_ASR || DEBUG_ASR_USR_CMD || DEBUG_ASR_CMD */

#ifdef DEBUG_ASR
  /* Breaks on none STDC based compilers :-( */
#define debug_asr_printf(fmt,args...)	printf(fmt, ##args)
#define debug_asr_dump_message(message)	debug_asr_message(message)
#define debug_asr_print_path(ccb)	xpt_print_path(ccb->ccb_h.path);
#else /* DEBUG_ASR */
#define debug_asr_printf(fmt,args...)
#define debug_asr_dump_message(message)
#define debug_asr_print_path(ccb)
#endif /* DEBUG_ASR */

/*
 *	If DEBUG_ASR_CMD is defined:
 *		0 - Display incoming SCSI commands
 *		1 - add in a quick character before queueing.
 *		2 - add in outgoing message frames.
 */
#if (defined(DEBUG_ASR_CMD))
#define debug_asr_cmd_printf(fmt,args...)     printf(fmt,##args)
static __inline void
debug_asr_dump_ccb(union ccb *ccb)
{
	u_int8_t	*cp = (unsigned char *)&(ccb->csio.cdb_io);
	int		len = ccb->csio.cdb_len;

	while (len) {
		debug_asr_cmd_printf (" %02x", *(cp++));
		--len;
	}
}
#if (DEBUG_ASR_CMD > 0)
#define debug_asr_cmd1_printf		       debug_asr_cmd_printf
#else
#define debug_asr_cmd1_printf(fmt,args...)
#endif
#if (DEBUG_ASR_CMD > 1)
#define debug_asr_cmd2_printf			debug_asr_cmd_printf
#define debug_asr_cmd2_dump_message(message)	debug_asr_message(message)
#else
#define debug_asr_cmd2_printf(fmt,args...)
#define debug_asr_cmd2_dump_message(message)
#endif
#else /* DEBUG_ASR_CMD */
#define debug_asr_cmd_printf(fmt,args...)
#define debug_asr_dump_ccb(ccb)
#define debug_asr_cmd1_printf(fmt,args...)
#define debug_asr_cmd2_printf(fmt,args...)
#define debug_asr_cmd2_dump_message(message)
#endif /* DEBUG_ASR_CMD */

#if (defined(DEBUG_ASR_USR_CMD))
#define debug_usr_cmd_printf(fmt,args...)   printf(fmt,##args)
#define debug_usr_cmd_dump_message(message) debug_usr_message(message)
#else /* DEBUG_ASR_USR_CMD */
#define debug_usr_cmd_printf(fmt,args...)
#define debug_usr_cmd_dump_message(message)
#endif /* DEBUG_ASR_USR_CMD */

#ifdef ASR_IOCTL_COMPAT
#define	dsDescription_size 46	/* Snug as a bug in a rug */
#endif /* ASR_IOCTL_COMPAT */

#include "dev/asr/dptsig.h"

static dpt_sig_S ASR_sig = {
	{ 'd', 'P', 't', 'S', 'i', 'G'}, SIG_VERSION, PROC_INTEL,
	PROC_386 | PROC_486 | PROC_PENTIUM | PROC_SEXIUM, FT_HBADRVR, 0,
	OEM_DPT, OS_FREE_BSD, CAP_ABOVE16MB, DEV_ALL, ADF_ALL_SC5,
	0, 0, ASR_VERSION, ASR_REVISION, ASR_SUBREVISION,
	ASR_MONTH, ASR_DAY, ASR_YEAR,
/*	 01234567890123456789012345678901234567890123456789	< 50 chars */
	"Adaptec FreeBSD 4.0.0 Unix SCSI I2O HBA Driver"
	/*		 ^^^^^ asr_attach alters these to match OS */
};

/* Configuration Definitions */

#define	SG_SIZE		 58	/* Scatter Gather list Size		 */
#define	MAX_TARGET_ID	 126	/* Maximum Target ID supported		 */
#define	MAX_LUN		 255	/* Maximum LUN Supported		 */
#define	MAX_CHANNEL	 7	/* Maximum Channel # Supported by driver */
#define	MAX_INBOUND	 2000	/* Max CCBs, Also Max Queue Size	 */
#define	MAX_OUTBOUND	 256	/* Maximum outbound frames/adapter	 */
#define	MAX_INBOUND_SIZE 512	/* Maximum inbound frame size		 */
#define	MAX_MAP		 4194304L /* Maximum mapping size of IOP	 */
				/* Also serves as the minimum map for	 */
				/* the 2005S zero channel RAID product	 */

/* I2O register set */
#define	I2O_REG_STATUS		0x30
#define	I2O_REG_MASK		0x34
#define	I2O_REG_TOFIFO		0x40
#define	I2O_REG_FROMFIFO	0x44

#define	Mask_InterruptsDisabled	0x08

/*
 * A MIX of performance and space considerations for TID lookups
 */
typedef u_int16_t tid_t;

typedef struct {
	u_int32_t size;		/* up to MAX_LUN    */
	tid_t	  TID[1];
} lun2tid_t;

typedef struct {
	u_int32_t   size;	/* up to MAX_TARGET */
	lun2tid_t * LUN[1];
} target2lun_t;

/*
 *	To ensure that we only allocate and use the worst case ccb here, lets
 *	make our own local ccb union. If asr_alloc_ccb is utilized for another
 *	ccb type, ensure that you add the additional structures into our local
 *	ccb union. To ensure strict type checking, we will utilize the local
 *	ccb definition wherever possible.
 */
union asr_ccb {
	struct ccb_hdr	    ccb_h;  /* For convenience */
	struct ccb_scsiio   csio;
	struct ccb_setasync csa;
};

/**************************************************************************
** ASR Host Adapter structure - One Structure For Each Host Adapter That **
**  Is Configured Into The System.  The Structure Supplies Configuration **
**  Information, Status Info, Queue Info And An Active CCB List Pointer. **
***************************************************************************/

typedef struct Asr_softc {
	u_int16_t		ha_irq;
	u_long			ha_Base;       /* base port for each board */
	bus_size_t		ha_blinkLED;
	bus_space_handle_t	ha_i2o_bhandle;
	bus_space_tag_t		ha_i2o_btag;
	bus_space_handle_t	ha_frame_bhandle;
	bus_space_tag_t		ha_frame_btag;
	I2O_IOP_ENTRY		ha_SystemTable;
	LIST_HEAD(,ccb_hdr)	ha_ccb;	       /* ccbs in use		   */
	struct cam_path	      * ha_path[MAX_CHANNEL+1];
	struct cam_sim	      * ha_sim[MAX_CHANNEL+1];
	struct resource	      * ha_mem_res;
	struct resource	      * ha_mes_res;
	struct resource	      * ha_irq_res;
	void		      * ha_intr;
	PI2O_LCT		ha_LCT;	       /* Complete list of devices */
#define le_type	  IdentityTag[0]
#define I2O_BSA	    0x20
#define I2O_FCA	    0x40
#define I2O_SCSI    0x00
#define I2O_PORT    0x80
#define I2O_UNKNOWN 0x7F
#define le_bus	  IdentityTag[1]
#define le_target IdentityTag[2]
#define le_lun	  IdentityTag[3]
	target2lun_t	      * ha_targets[MAX_CHANNEL+1];
	PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME ha_Msgs;
	u_long			ha_Msgs_Phys;

	u_int8_t		ha_in_reset;
#define HA_OPERATIONAL	    0
#define HA_IN_RESET	    1
#define HA_OFF_LINE	    2
#define HA_OFF_LINE_RECOVERY 3
	/* Configuration information */
	/* The target id maximums we take */
	u_int8_t		ha_MaxBus;     /* Maximum bus */
	u_int8_t		ha_MaxId;      /* Maximum target ID */
	u_int8_t		ha_MaxLun;     /* Maximum target LUN */
	u_int8_t		ha_SgSize;     /* Max SG elements */
	u_int8_t		ha_pciBusNum;
	u_int8_t		ha_pciDeviceNum;
	u_int8_t		ha_adapter_target[MAX_CHANNEL+1];
	u_int16_t		ha_QueueSize;  /* Max outstanding commands */
	u_int16_t		ha_Msgs_Count;

	/* Links into other parents and HBAs */
	struct Asr_softc      * ha_next;       /* HBA list */
	struct cdev *ha_devt;
} Asr_softc_t;

static Asr_softc_t * Asr_softc;

/*
 *	Prototypes of the routines we have in this object.
 */

/* I2O HDM interface */
static int	asr_probe(device_t tag);
static int	asr_attach(device_t tag);

static int	asr_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
			  struct thread *td);
static int	asr_open(struct cdev *dev, int32_t flags, int32_t ifmt,
			 struct thread *td);
static int	asr_close(struct cdev *dev, int flags, int ifmt, struct thread *td);
static int	asr_intr(Asr_softc_t *sc);
static void	asr_timeout(void *arg);
static int	ASR_init(Asr_softc_t *sc);
static int	ASR_acquireLct(Asr_softc_t *sc);
static int	ASR_acquireHrt(Asr_softc_t *sc);
static void	asr_action(struct cam_sim *sim, union ccb *ccb);
static void	asr_poll(struct cam_sim *sim);
static int	ASR_queue(Asr_softc_t *sc, PI2O_MESSAGE_FRAME Message);

/*
 *	Here is the auto-probe structure used to nest our tests appropriately
 *	during the startup phase of the operating system.
 */
static device_method_t asr_methods[] = {
	DEVMETHOD(device_probe,	 asr_probe),
	DEVMETHOD(device_attach, asr_attach),
	{ 0, 0 }
};

static driver_t asr_driver = {
	"asr",
	asr_methods,
	sizeof(Asr_softc_t)
};

static devclass_t asr_devclass;
DRIVER_MODULE(asr, pci, asr_driver, asr_devclass, 0, 0);
MODULE_DEPEND(asr, pci, 1, 1, 1);
MODULE_DEPEND(asr, cam, 1, 1, 1);

/*
 * devsw for asr hba driver
 *
 * only ioctl is used. the sd driver provides all other access.
 */
static struct cdevsw asr_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	asr_open,
	.d_close =	asr_close,
	.d_ioctl =	asr_ioctl,
	.d_name =	"asr",
};

/* I2O support routines */

static __inline u_int32_t
asr_get_FromFIFO(Asr_softc_t *sc)
{
	return (bus_space_read_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle,
				 I2O_REG_FROMFIFO));
}

static __inline u_int32_t
asr_get_ToFIFO(Asr_softc_t *sc)
{
	return (bus_space_read_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle,
				 I2O_REG_TOFIFO));
}

static __inline u_int32_t
asr_get_intr(Asr_softc_t *sc)
{
	return (bus_space_read_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle,
				 I2O_REG_MASK));
}

static __inline u_int32_t
asr_get_status(Asr_softc_t *sc)
{
	return (bus_space_read_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle,
				 I2O_REG_STATUS));
}

static __inline void
asr_set_FromFIFO(Asr_softc_t *sc, u_int32_t val)
{
	bus_space_write_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle, I2O_REG_FROMFIFO,
			  val);
}

static __inline void
asr_set_ToFIFO(Asr_softc_t *sc, u_int32_t val)
{
	bus_space_write_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle, I2O_REG_TOFIFO,
			  val);
}

static __inline void
asr_set_intr(Asr_softc_t *sc, u_int32_t val)
{
	bus_space_write_4(sc->ha_i2o_btag, sc->ha_i2o_bhandle, I2O_REG_MASK,
			  val);
}

static __inline void
asr_set_frame(Asr_softc_t *sc, void *frame, u_int32_t offset, int len)
{
	bus_space_write_region_4(sc->ha_frame_btag, sc->ha_frame_bhandle,
				 offset, (u_int32_t *)frame, len);
}

/*
 *	Fill message with default.
 */
static PI2O_MESSAGE_FRAME
ASR_fillMessage(void *Message, u_int16_t size)
{
	PI2O_MESSAGE_FRAME Message_Ptr;

	Message_Ptr = (I2O_MESSAGE_FRAME *)Message;
	bzero(Message_Ptr, size);
	I2O_MESSAGE_FRAME_setVersionOffset(Message_Ptr, I2O_VERSION_11);
	I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
	  (size + sizeof(U32) - 1) >> 2);
	I2O_MESSAGE_FRAME_setInitiatorAddress (Message_Ptr, 1);
	KASSERT(Message_Ptr != NULL, ("Message_Ptr == NULL"));
	return (Message_Ptr);
} /* ASR_fillMessage */

#define	EMPTY_QUEUE (-1L)

static __inline U32
ASR_getMessage(Asr_softc_t *sc)
{
	U32	MessageOffset;

	MessageOffset = asr_get_ToFIFO(sc);
	if (MessageOffset == EMPTY_QUEUE)
		MessageOffset = asr_get_ToFIFO(sc);

	return (MessageOffset);
} /* ASR_getMessage */

/* Issue a polled command */
static U32
ASR_initiateCp(Asr_softc_t *sc, PI2O_MESSAGE_FRAME Message)
{
	U32	Mask = -1L;
	U32	MessageOffset;
	u_int	Delay = 1500;

	/*
	 * ASR_initiateCp is only used for synchronous commands and will
	 * be made more resiliant to adapter delays since commands like
	 * resetIOP can cause the adapter to be deaf for a little time.
	 */
	while (((MessageOffset = ASR_getMessage(sc)) == EMPTY_QUEUE)
	 && (--Delay != 0)) {
		DELAY (10000);
	}
	if (MessageOffset != EMPTY_QUEUE) {
		asr_set_frame(sc, Message, MessageOffset,
			      I2O_MESSAGE_FRAME_getMessageSize(Message));
		/*
		 *	Disable the Interrupts
		 */
		Mask = asr_get_intr(sc);
		asr_set_intr(sc, Mask | Mask_InterruptsDisabled);
		asr_set_ToFIFO(sc, MessageOffset);
	}
	return (Mask);
} /* ASR_initiateCp */

/*
 *	Reset the adapter.
 */
static U32
ASR_resetIOP(Asr_softc_t *sc)
{
	struct resetMessage {
		I2O_EXEC_IOP_RESET_MESSAGE M;
		U32			   R;
	} Message;
	PI2O_EXEC_IOP_RESET_MESSAGE	 Message_Ptr;
	U32		      * volatile Reply_Ptr;
	U32				 Old;

	/*
	 *  Build up our copy of the Message.
	 */
	Message_Ptr = (PI2O_EXEC_IOP_RESET_MESSAGE)ASR_fillMessage(&Message,
	  sizeof(I2O_EXEC_IOP_RESET_MESSAGE));
	I2O_EXEC_IOP_RESET_MESSAGE_setFunction(Message_Ptr, I2O_EXEC_IOP_RESET);
	/*
	 *  Reset the Reply Status
	 */
	*(Reply_Ptr = (U32 *)((char *)Message_Ptr
	  + sizeof(I2O_EXEC_IOP_RESET_MESSAGE))) = 0;
	I2O_EXEC_IOP_RESET_MESSAGE_setStatusWordLowAddress(Message_Ptr,
	  KVTOPHYS((void *)Reply_Ptr));
	/*
	 *	Send the Message out
	 */
	if ((Old = ASR_initiateCp(sc,
				  (PI2O_MESSAGE_FRAME)Message_Ptr)) != -1L) {
		/*
		 * Wait for a response (Poll), timeouts are dangerous if
		 * the card is truly responsive. We assume response in 2s.
		 */
		u_int8_t Delay = 200;

		while ((*Reply_Ptr == 0) && (--Delay != 0)) {
			DELAY (10000);
		}
		/*
		 *	Re-enable the interrupts.
		 */
		asr_set_intr(sc, Old);
		KASSERT(*Reply_Ptr != 0, ("*Reply_Ptr == 0"));
		return(*Reply_Ptr);
	}
	KASSERT(Old != -1L, ("Old == -1"));
	return (0);
} /* ASR_resetIOP */

/*
 *	Get the curent state of the adapter
 */
static PI2O_EXEC_STATUS_GET_REPLY
ASR_getStatus(Asr_softc_t *sc, PI2O_EXEC_STATUS_GET_REPLY buffer)
{
	I2O_EXEC_STATUS_GET_MESSAGE	Message;
	PI2O_EXEC_STATUS_GET_MESSAGE	Message_Ptr;
	U32				Old;

	/*
	 *  Build up our copy of the Message.
	 */
	Message_Ptr = (PI2O_EXEC_STATUS_GET_MESSAGE)ASR_fillMessage(&Message,
	    sizeof(I2O_EXEC_STATUS_GET_MESSAGE));
	I2O_EXEC_STATUS_GET_MESSAGE_setFunction(Message_Ptr,
	    I2O_EXEC_STATUS_GET);
	I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferAddressLow(Message_Ptr,
	    KVTOPHYS((void *)buffer));
	/* This one is a Byte Count */
	I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferLength(Message_Ptr,
	    sizeof(I2O_EXEC_STATUS_GET_REPLY));
	/*
	 *  Reset the Reply Status
	 */
	bzero(buffer, sizeof(I2O_EXEC_STATUS_GET_REPLY));
	/*
	 *	Send the Message out
	 */
	if ((Old = ASR_initiateCp(sc,
				  (PI2O_MESSAGE_FRAME)Message_Ptr)) != -1L) {
		/*
		 *	Wait for a response (Poll), timeouts are dangerous if
		 * the card is truly responsive. We assume response in 50ms.
		 */
		u_int8_t Delay = 255;

		while (*((U8 * volatile)&(buffer->SyncByte)) == 0) {
			if (--Delay == 0) {
				buffer = NULL;
				break;
			}
			DELAY (1000);
		}
		/*
		 *	Re-enable the interrupts.
		 */
		asr_set_intr(sc, Old);
		return (buffer);
	}
	return (NULL);
} /* ASR_getStatus */

/*
 *	Check if the device is a SCSI I2O HBA, and add it to the list.
 */

/*
 * Probe for ASR controller.  If we find it, we will use it.
 * virtual adapters.
 */
static int
asr_probe(device_t tag)
{
	u_int32_t id;

	id = (pci_get_device(tag) << 16) | pci_get_vendor(tag);
	if ((id == 0xA5011044) || (id == 0xA5111044)) {
		device_set_desc(tag, "Adaptec Caching SCSI RAID");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
} /* asr_probe */

static __inline union asr_ccb *
asr_alloc_ccb(Asr_softc_t *sc)
{
	union asr_ccb *new_ccb;

	if ((new_ccb = (union asr_ccb *)malloc(sizeof(*new_ccb),
	  M_DEVBUF, M_WAITOK | M_ZERO)) != NULL) {
		new_ccb->ccb_h.pinfo.priority = 1;
		new_ccb->ccb_h.pinfo.index = CAM_UNQUEUED_INDEX;
		new_ccb->ccb_h.spriv_ptr0 = sc;
	}
	return (new_ccb);
} /* asr_alloc_ccb */

static __inline void
asr_free_ccb(union asr_ccb *free_ccb)
{
	free(free_ccb, M_DEVBUF);
} /* asr_free_ccb */

/*
 *	Print inquiry data `carefully'
 */
static void
ASR_prstring(u_int8_t *s, int len)
{
	while ((--len >= 0) && (*s) && (*s != ' ') && (*s != '-')) {
		printf ("%c", *(s++));
	}
} /* ASR_prstring */

/*
 *	Send a message synchronously and without Interrupt to a ccb.
 */
static int
ASR_queue_s(union asr_ccb *ccb, PI2O_MESSAGE_FRAME Message)
{
	int		s;
	U32		Mask;
	Asr_softc_t	*sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);

	/*
	 * We do not need any (optional byteswapping) method access to
	 * the Initiator context field.
	 */
	I2O_MESSAGE_FRAME_setInitiatorContext64(Message, (long)ccb);

	/* Prevent interrupt service */
	s = splcam ();
	Mask = asr_get_intr(sc);
	asr_set_intr(sc, Mask | Mask_InterruptsDisabled);

	if (ASR_queue(sc, Message) == EMPTY_QUEUE) {
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_REQUEUE_REQ;
	}

	/*
	 * Wait for this board to report a finished instruction.
	 */
	while ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
		(void)asr_intr (sc);
	}

	/* Re-enable Interrupts */
	asr_set_intr(sc, Mask);
	splx(s);

	return (ccb->ccb_h.status);
} /* ASR_queue_s */

/*
 *	Send a message synchronously to an Asr_softc_t.
 */
static int
ASR_queue_c(Asr_softc_t *sc, PI2O_MESSAGE_FRAME Message)
{
	union asr_ccb	*ccb;
	int		status;

	if ((ccb = asr_alloc_ccb (sc)) == NULL) {
		return (CAM_REQUEUE_REQ);
	}

	status = ASR_queue_s (ccb, Message);

	asr_free_ccb(ccb);

	return (status);
} /* ASR_queue_c */

/*
 *	Add the specified ccb to the active queue
 */
static __inline void
ASR_ccbAdd(Asr_softc_t *sc, union asr_ccb *ccb)
{
	int s;

	s = splcam();
	LIST_INSERT_HEAD(&(sc->ha_ccb), &(ccb->ccb_h), sim_links.le);
	if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
		if (ccb->ccb_h.timeout == CAM_TIME_DEFAULT) {
			/*
			 * RAID systems can take considerable time to
			 * complete some commands given the large cache
			 * flashes switching from write back to write thru.
			 */
			ccb->ccb_h.timeout = 6 * 60 * 1000;
		}
		ccb->ccb_h.timeout_ch = timeout(asr_timeout, (caddr_t)ccb,
		  (ccb->ccb_h.timeout * hz) / 1000);
	}
	splx(s);
} /* ASR_ccbAdd */

/*
 *	Remove the specified ccb from the active queue.
 */
static __inline void
ASR_ccbRemove(Asr_softc_t *sc, union asr_ccb *ccb)
{
	int s;

	s = splcam();
	untimeout(asr_timeout, (caddr_t)ccb, ccb->ccb_h.timeout_ch);
	LIST_REMOVE(&(ccb->ccb_h), sim_links.le);
	splx(s);
} /* ASR_ccbRemove */

/*
 *	Fail all the active commands, so they get re-issued by the operating
 *	system.
 */
static void
ASR_failActiveCommands(Asr_softc_t *sc)
{
	struct ccb_hdr	*ccb;
	int		s;

	s = splcam();
	/*
	 *	We do not need to inform the CAM layer that we had a bus
	 * reset since we manage it on our own, this also prevents the
	 * SCSI_DELAY settling that would be required on other systems.
	 * The `SCSI_DELAY' has already been handled by the card via the
	 * acquisition of the LCT table while we are at CAM priority level.
	 *  for (int bus = 0; bus <= sc->ha_MaxBus; ++bus) {
	 *	xpt_async (AC_BUS_RESET, sc->ha_path[bus], NULL);
	 *  }
	 */
	while ((ccb = LIST_FIRST(&(sc->ha_ccb))) != NULL) {
		ASR_ccbRemove (sc, (union asr_ccb *)ccb);

		ccb->status &= ~CAM_STATUS_MASK;
		ccb->status |= CAM_REQUEUE_REQ;
		/* Nothing Transfered */
		((struct ccb_scsiio *)ccb)->resid
		  = ((struct ccb_scsiio *)ccb)->dxfer_len;

		if (ccb->path) {
			xpt_done ((union ccb *)ccb);
		} else {
			wakeup (ccb);
		}
	}
	splx(s);
} /* ASR_failActiveCommands */

/*
 *	The following command causes the HBA to reset the specific bus
 */
static void
ASR_resetBus(Asr_softc_t *sc, int bus)
{
	I2O_HBA_BUS_RESET_MESSAGE	Message;
	I2O_HBA_BUS_RESET_MESSAGE	*Message_Ptr;
	PI2O_LCT_ENTRY			Device;

	Message_Ptr = (I2O_HBA_BUS_RESET_MESSAGE *)ASR_fillMessage(&Message,
	  sizeof(I2O_HBA_BUS_RESET_MESSAGE));
	I2O_MESSAGE_FRAME_setFunction(&Message_Ptr->StdMessageFrame,
	  I2O_HBA_BUS_RESET);
	for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
	  (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
	  ++Device) {
		if (((Device->le_type & I2O_PORT) != 0)
		 && (Device->le_bus == bus)) {
			I2O_MESSAGE_FRAME_setTargetAddress(
			  &Message_Ptr->StdMessageFrame,
			  I2O_LCT_ENTRY_getLocalTID(Device));
			/* Asynchronous command, with no expectations */
			(void)ASR_queue(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
			break;
		}
	}
} /* ASR_resetBus */

static __inline int
ASR_getBlinkLedCode(Asr_softc_t *sc)
{
	U8	blink;

	if (sc == NULL)
		return (0);

	blink = bus_space_read_1(sc->ha_frame_btag,
				 sc->ha_frame_bhandle, sc->ha_blinkLED + 1);
	if (blink != 0xBC)
		return (0);

	blink = bus_space_read_1(sc->ha_frame_btag,
				 sc->ha_frame_bhandle, sc->ha_blinkLED);
	return (blink);
} /* ASR_getBlinkCode */

/*
 *	Determine the address of an TID lookup. Must be done at high priority
 *	since the address can be changed by other threads of execution.
 *
 *	Returns NULL pointer if not indexible (but will attempt to generate
 *	an index if `new_entry' flag is set to TRUE).
 *
 *	All addressible entries are to be guaranteed zero if never initialized.
 */
static tid_t *
ASR_getTidAddress(Asr_softc_t *sc, int bus, int target, int lun, int new_entry)
{
	target2lun_t	*bus_ptr;
	lun2tid_t	*target_ptr;
	unsigned	new_size;

	/*
	 *	Validity checking of incoming parameters. More of a bound
	 * expansion limit than an issue with the code dealing with the
	 * values.
	 *
	 *	sc must be valid before it gets here, so that check could be
	 * dropped if speed a critical issue.
	 */
	if ((sc == NULL)
	 || (bus > MAX_CHANNEL)
	 || (target > sc->ha_MaxId)
	 || (lun > sc->ha_MaxLun)) {
		debug_asr_printf("(%lx,%d,%d,%d) target out of range\n",
		  (u_long)sc, bus, target, lun);
		return (NULL);
	}
	/*
	 *	See if there is an associated bus list.
	 *
	 *	for performance, allocate in size of BUS_CHUNK chunks.
	 *	BUS_CHUNK must be a power of two. This is to reduce
	 *	fragmentation effects on the allocations.
	 */
#define BUS_CHUNK 8
	new_size = ((target + BUS_CHUNK - 1) & ~(BUS_CHUNK - 1));
	if ((bus_ptr = sc->ha_targets[bus]) == NULL) {
		/*
		 *	Allocate a new structure?
		 *		Since one element in structure, the +1
		 *		needed for size has been abstracted.
		 */
		if ((new_entry == FALSE)
		 || ((sc->ha_targets[bus] = bus_ptr = (target2lun_t *)malloc (
		    sizeof(*bus_ptr) + (sizeof(bus_ptr->LUN) * new_size),
		    M_TEMP, M_WAITOK | M_ZERO))
		   == NULL)) {
			debug_asr_printf("failed to allocate bus list\n");
			return (NULL);
		}
		bus_ptr->size = new_size + 1;
	} else if (bus_ptr->size <= new_size) {
		target2lun_t * new_bus_ptr;

		/*
		 *	Reallocate a new structure?
		 *		Since one element in structure, the +1
		 *		needed for size has been abstracted.
		 */
		if ((new_entry == FALSE)
		 || ((new_bus_ptr = (target2lun_t *)malloc (
		    sizeof(*bus_ptr) + (sizeof(bus_ptr->LUN) * new_size),
		    M_TEMP, M_WAITOK | M_ZERO)) == NULL)) {
			debug_asr_printf("failed to reallocate bus list\n");
			return (NULL);
		}
		/*
		 *	Copy the whole thing, safer, simpler coding
		 * and not really performance critical at this point.
		 */
		bcopy(bus_ptr, new_bus_ptr, sizeof(*bus_ptr)
		    + (sizeof(bus_ptr->LUN) * (bus_ptr->size - 1)));
		sc->ha_targets[bus] = new_bus_ptr;
		free(bus_ptr, M_TEMP);
		bus_ptr = new_bus_ptr;
		bus_ptr->size = new_size + 1;
	}
	/*
	 *	We now have the bus list, lets get to the target list.
	 *	Since most systems have only *one* lun, we do not allocate
	 *	in chunks as above, here we allow one, then in chunk sizes.
	 *	TARGET_CHUNK must be a power of two. This is to reduce
	 *	fragmentation effects on the allocations.
	 */
#define TARGET_CHUNK 8
	if ((new_size = lun) != 0) {
		new_size = ((lun + TARGET_CHUNK - 1) & ~(TARGET_CHUNK - 1));
	}
	if ((target_ptr = bus_ptr->LUN[target]) == NULL) {
		/*
		 *	Allocate a new structure?
		 *		Since one element in structure, the +1
		 *		needed for size has been abstracted.
		 */
		if ((new_entry == FALSE)
		 || ((bus_ptr->LUN[target] = target_ptr = (lun2tid_t *)malloc (
		    sizeof(*target_ptr) + (sizeof(target_ptr->TID) * new_size),
		    M_TEMP, M_WAITOK | M_ZERO)) == NULL)) {
			debug_asr_printf("failed to allocate target list\n");
			return (NULL);
		}
		target_ptr->size = new_size + 1;
	} else if (target_ptr->size <= new_size) {
		lun2tid_t * new_target_ptr;

		/*
		 *	Reallocate a new structure?
		 *		Since one element in structure, the +1
		 *		needed for size has been abstracted.
		 */
		if ((new_entry == FALSE)
		 || ((new_target_ptr = (lun2tid_t *)malloc (
		    sizeof(*target_ptr) + (sizeof(target_ptr->TID) * new_size),
		    M_TEMP, M_WAITOK | M_ZERO)) == NULL)) {
			debug_asr_printf("failed to reallocate target list\n");
			return (NULL);
		}
		/*
		 *	Copy the whole thing, safer, simpler coding
		 * and not really performance critical at this point.
		 */
		bcopy(target_ptr, new_target_ptr, sizeof(*target_ptr)
		    + (sizeof(target_ptr->TID) * (target_ptr->size - 1)));
		bus_ptr->LUN[target] = new_target_ptr;
		free(target_ptr, M_TEMP);
		target_ptr = new_target_ptr;
		target_ptr->size = new_size + 1;
	}
	/*
	 *	Now, acquire the TID address from the LUN indexed list.
	 */
	return (&(target_ptr->TID[lun]));
} /* ASR_getTidAddress */

/*
 *	Get a pre-existing TID relationship.
 *
 *	If the TID was never set, return (tid_t)-1.
 *
 *	should use mutex rather than spl.
 */
static __inline tid_t
ASR_getTid(Asr_softc_t *sc, int bus, int target, int lun)
{
	tid_t	*tid_ptr;
	int	s;
	tid_t	retval;

	s = splcam();
	if (((tid_ptr = ASR_getTidAddress(sc, bus, target, lun, FALSE)) == NULL)
	/* (tid_t)0 or (tid_t)-1 indicate no TID */
	 || (*tid_ptr == (tid_t)0)) {
		splx(s);
		return ((tid_t)-1);
	}
	retval = *tid_ptr;
	splx(s);
	return (retval);
} /* ASR_getTid */

/*
 *	Set a TID relationship.
 *
 *	If the TID was not set, return (tid_t)-1.
 *
 *	should use mutex rather than spl.
 */
static __inline tid_t
ASR_setTid(Asr_softc_t *sc, int bus, int target, int lun, tid_t	TID)
{
	tid_t	*tid_ptr;
	int	s;

	if (TID != (tid_t)-1) {
		if (TID == 0) {
			return ((tid_t)-1);
		}
		s = splcam();
		if ((tid_ptr = ASR_getTidAddress(sc, bus, target, lun, TRUE))
		 == NULL) {
			splx(s);
			return ((tid_t)-1);
		}
		*tid_ptr = TID;
		splx(s);
	}
	return (TID);
} /* ASR_setTid */

/*-------------------------------------------------------------------------*/
/*		      Function ASR_rescan				   */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :				   */
/*     Asr_softc_t *	 : HBA miniport driver's adapter data storage.	   */
/*									   */
/* This Function Will rescan the adapter and resynchronize any data	   */
/*									   */
/* Return : 0 For OK, Error Code Otherwise				   */
/*-------------------------------------------------------------------------*/

static int
ASR_rescan(Asr_softc_t *sc)
{
	int bus;
	int error;

	/*
	 * Re-acquire the LCT table and synchronize us to the adapter.
	 */
	if ((error = ASR_acquireLct(sc)) == 0) {
		error = ASR_acquireHrt(sc);
	}

	if (error != 0) {
		return error;
	}

	bus = sc->ha_MaxBus;
	/* Reset all existing cached TID lookups */
	do {
		int target, event = 0;

		/*
		 *	Scan for all targets on this bus to see if they
		 * got affected by the rescan.
		 */
		for (target = 0; target <= sc->ha_MaxId; ++target) {
			int lun;

			/* Stay away from the controller ID */
			if (target == sc->ha_adapter_target[bus]) {
				continue;
			}
			for (lun = 0; lun <= sc->ha_MaxLun; ++lun) {
				PI2O_LCT_ENTRY Device;
				tid_t	       TID = (tid_t)-1;
				tid_t	       LastTID;

				/*
				 * See if the cached TID changed. Search for
				 * the device in our new LCT.
				 */
				for (Device = sc->ha_LCT->LCTEntry;
				  Device < (PI2O_LCT_ENTRY)(((U32 *)sc->ha_LCT)
				   + I2O_LCT_getTableSize(sc->ha_LCT));
				  ++Device) {
					if ((Device->le_type != I2O_UNKNOWN)
					 && (Device->le_bus == bus)
					 && (Device->le_target == target)
					 && (Device->le_lun == lun)
					 && (I2O_LCT_ENTRY_getUserTID(Device)
					  == 0xFFF)) {
						TID = I2O_LCT_ENTRY_getLocalTID(
						  Device);
						break;
					}
				}
				/*
				 * Indicate to the OS that the label needs
				 * to be recalculated, or that the specific
				 * open device is no longer valid (Merde)
				 * because the cached TID changed.
				 */
				LastTID = ASR_getTid (sc, bus, target, lun);
				if (LastTID != TID) {
					struct cam_path * path;

					if (xpt_create_path(&path,
					  /*periph*/NULL,
					  cam_sim_path(sc->ha_sim[bus]),
					  target, lun) != CAM_REQ_CMP) {
						if (TID == (tid_t)-1) {
							event |= AC_LOST_DEVICE;
						} else {
							event |= AC_INQ_CHANGED
							       | AC_GETDEV_CHANGED;
						}
					} else {
						if (TID == (tid_t)-1) {
							xpt_async(
							  AC_LOST_DEVICE,
							  path, NULL);
						} else if (LastTID == (tid_t)-1) {
							struct ccb_getdev ccb;

							xpt_setup_ccb(
							  &(ccb.ccb_h),
							  path, /*priority*/5);
							xpt_async(
							  AC_FOUND_DEVICE,
							  path,
							  &ccb);
						} else {
							xpt_async(
							  AC_INQ_CHANGED,
							  path, NULL);
							xpt_async(
							  AC_GETDEV_CHANGED,
							  path, NULL);
						}
					}
				}
				/*
				 *	We have the option of clearing the
				 * cached TID for it to be rescanned, or to
				 * set it now even if the device never got
				 * accessed. We chose the later since we
				 * currently do not use the condition that
				 * the TID ever got cached.
				 */
				ASR_setTid (sc, bus, target, lun, TID);
			}
		}
		/*
		 *	The xpt layer can not handle multiple events at the
		 * same call.
		 */
		if (event & AC_LOST_DEVICE) {
			xpt_async(AC_LOST_DEVICE, sc->ha_path[bus], NULL);
		}
		if (event & AC_INQ_CHANGED) {
			xpt_async(AC_INQ_CHANGED, sc->ha_path[bus], NULL);
		}
		if (event & AC_GETDEV_CHANGED) {
			xpt_async(AC_GETDEV_CHANGED, sc->ha_path[bus], NULL);
		}
	} while (--bus >= 0);
	return (error);
} /* ASR_rescan */

/*-------------------------------------------------------------------------*/
/*		      Function ASR_reset				   */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :				   */
/*     Asr_softc_t *	  : HBA miniport driver's adapter data storage.	   */
/*									   */
/* This Function Will reset the adapter and resynchronize any data	   */
/*									   */
/* Return : None							   */
/*-------------------------------------------------------------------------*/

static int
ASR_reset(Asr_softc_t *sc)
{
	int s, retVal;

	s = splcam();
	if ((sc->ha_in_reset == HA_IN_RESET)
	 || (sc->ha_in_reset == HA_OFF_LINE_RECOVERY)) {
		splx (s);
		return (EBUSY);
	}
	/*
	 *	Promotes HA_OPERATIONAL to HA_IN_RESET,
	 * or HA_OFF_LINE to HA_OFF_LINE_RECOVERY.
	 */
	++(sc->ha_in_reset);
	if (ASR_resetIOP(sc) == 0) {
		debug_asr_printf ("ASR_resetIOP failed\n");
		/*
		 *	We really need to take this card off-line, easier said
		 * than make sense. Better to keep retrying for now since if a
		 * UART cable is connected the blinkLEDs the adapter is now in
		 * a hard state requiring action from the monitor commands to
		 * the HBA to continue. For debugging waiting forever is a
		 * good thing. In a production system, however, one may wish
		 * to instead take the card off-line ...
		 */
		/* Wait Forever */
		while (ASR_resetIOP(sc) == 0);
	}
	retVal = ASR_init (sc);
	splx (s);
	if (retVal != 0) {
		debug_asr_printf ("ASR_init failed\n");
		sc->ha_in_reset = HA_OFF_LINE;
		return (ENXIO);
	}
	if (ASR_rescan (sc) != 0) {
		debug_asr_printf ("ASR_rescan failed\n");
	}
	ASR_failActiveCommands (sc);
	if (sc->ha_in_reset == HA_OFF_LINE_RECOVERY) {
		printf ("asr%d: Brining adapter back on-line\n",
		  sc->ha_path[0]
		    ? cam_sim_unit(xpt_path_sim(sc->ha_path[0]))
		    : 0);
	}
	sc->ha_in_reset = HA_OPERATIONAL;
	return (0);
} /* ASR_reset */

/*
 *	Device timeout handler.
 */
static void
asr_timeout(void *arg)
{
	union asr_ccb	*ccb = (union asr_ccb *)arg;
	Asr_softc_t	*sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);
	int		s;

	debug_asr_print_path(ccb);
	debug_asr_printf("timed out");

	/*
	 *	Check if the adapter has locked up?
	 */
	if ((s = ASR_getBlinkLedCode(sc)) != 0) {
		/* Reset Adapter */
		printf ("asr%d: Blink LED 0x%x resetting adapter\n",
		  cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)), s);
		if (ASR_reset (sc) == ENXIO) {
			/* Try again later */
			ccb->ccb_h.timeout_ch = timeout(asr_timeout,
			  (caddr_t)ccb,
			  (ccb->ccb_h.timeout * hz) / 1000);
		}
		return;
	}
	/*
	 *	Abort does not function on the ASR card!!! Walking away from
	 * the SCSI command is also *very* dangerous. A SCSI BUS reset is
	 * our best bet, followed by a complete adapter reset if that fails.
	 */
	s = splcam();
	/* Check if we already timed out once to raise the issue */
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_CMD_TIMEOUT) {
		debug_asr_printf (" AGAIN\nreinitializing adapter\n");
		if (ASR_reset (sc) == ENXIO) {
			ccb->ccb_h.timeout_ch = timeout(asr_timeout,
			  (caddr_t)ccb,
			  (ccb->ccb_h.timeout * hz) / 1000);
		}
		splx(s);
		return;
	}
	debug_asr_printf ("\nresetting bus\n");
	/* If the BUS reset does not take, then an adapter reset is next! */
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
	ccb->ccb_h.timeout_ch = timeout(asr_timeout, (caddr_t)ccb,
	  (ccb->ccb_h.timeout * hz) / 1000);
	ASR_resetBus (sc, cam_sim_bus(xpt_path_sim(ccb->ccb_h.path)));
	xpt_async (AC_BUS_RESET, ccb->ccb_h.path, NULL);
	splx(s);
} /* asr_timeout */

/*
 * send a message asynchronously
 */
static int
ASR_queue(Asr_softc_t *sc, PI2O_MESSAGE_FRAME Message)
{
	U32		MessageOffset;
	union asr_ccb	*ccb;

	debug_asr_printf("Host Command Dump:\n");
	debug_asr_dump_message(Message);

	ccb = (union asr_ccb *)(long)
	  I2O_MESSAGE_FRAME_getInitiatorContext64(Message);

	if ((MessageOffset = ASR_getMessage(sc)) != EMPTY_QUEUE) {
		asr_set_frame(sc, Message, MessageOffset,
			      I2O_MESSAGE_FRAME_getMessageSize(Message));
		if (ccb) {
			ASR_ccbAdd (sc, ccb);
		}
		/* Post the command */
		asr_set_ToFIFO(sc, MessageOffset);
	} else {
		if (ASR_getBlinkLedCode(sc)) {
			/*
			 *	Unlikely we can do anything if we can't grab a
			 * message frame :-(, but lets give it a try.
			 */
			(void)ASR_reset(sc);
		}
	}
	return (MessageOffset);
} /* ASR_queue */


/* Simple Scatter Gather elements */
#define	SG(SGL,Index,Flags,Buffer,Size)				   \
	I2O_FLAGS_COUNT_setCount(				   \
	  &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index].FlagsCount), \
	  Size);						   \
	I2O_FLAGS_COUNT_setFlags(				   \
	  &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index].FlagsCount), \
	  I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT | (Flags));	   \
	I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(		   \
	  &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index]),		   \
	  (Buffer == NULL) ? 0 : KVTOPHYS(Buffer))

/*
 *	Retrieve Parameter Group.
 */
static void *
ASR_getParams(Asr_softc_t *sc, tid_t TID, int Group, void *Buffer,
	      unsigned BufferSize)
{
	struct paramGetMessage {
		I2O_UTIL_PARAMS_GET_MESSAGE M;
		char
		   F[sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT)];
		struct Operations {
			I2O_PARAM_OPERATIONS_LIST_HEADER Header;
			I2O_PARAM_OPERATION_ALL_TEMPLATE Template[1];
		}			     O;
	}				Message;
	struct Operations		*Operations_Ptr;
	I2O_UTIL_PARAMS_GET_MESSAGE	*Message_Ptr;
	struct ParamBuffer {
		I2O_PARAM_RESULTS_LIST_HEADER	    Header;
		I2O_PARAM_READ_OPERATION_RESULT	    Read;
		char				    Info[1];
	}				*Buffer_Ptr;

	Message_Ptr = (I2O_UTIL_PARAMS_GET_MESSAGE *)ASR_fillMessage(&Message,
	  sizeof(I2O_UTIL_PARAMS_GET_MESSAGE)
	    + sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT));
	Operations_Ptr = (struct Operations *)((char *)Message_Ptr
	  + sizeof(I2O_UTIL_PARAMS_GET_MESSAGE)
	  + sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT));
	bzero(Operations_Ptr, sizeof(struct Operations));
	I2O_PARAM_OPERATIONS_LIST_HEADER_setOperationCount(
	  &(Operations_Ptr->Header), 1);
	I2O_PARAM_OPERATION_ALL_TEMPLATE_setOperation(
	  &(Operations_Ptr->Template[0]), I2O_PARAMS_OPERATION_FIELD_GET);
	I2O_PARAM_OPERATION_ALL_TEMPLATE_setFieldCount(
	  &(Operations_Ptr->Template[0]), 0xFFFF);
	I2O_PARAM_OPERATION_ALL_TEMPLATE_setGroupNumber(
	  &(Operations_Ptr->Template[0]), Group);
	Buffer_Ptr = (struct ParamBuffer *)Buffer;
	bzero(Buffer_Ptr, BufferSize);

	I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
	  I2O_VERSION_11
	  + (((sizeof(I2O_UTIL_PARAMS_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
	    / sizeof(U32)) << 4));
	I2O_MESSAGE_FRAME_setTargetAddress (&(Message_Ptr->StdMessageFrame),
	  TID);
	I2O_MESSAGE_FRAME_setFunction (&(Message_Ptr->StdMessageFrame),
	  I2O_UTIL_PARAMS_GET);
	/*
	 *  Set up the buffers as scatter gather elements.
	 */
	SG(&(Message_Ptr->SGL), 0,
	  I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER,
	  Operations_Ptr, sizeof(struct Operations));
	SG(&(Message_Ptr->SGL), 1,
	  I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
	  Buffer_Ptr, BufferSize);

	if ((ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) == CAM_REQ_CMP)
	 && (Buffer_Ptr->Header.ResultCount)) {
		return ((void *)(Buffer_Ptr->Info));
	}
	return (NULL);
} /* ASR_getParams */

/*
 *	Acquire the LCT information.
 */
static int
ASR_acquireLct(Asr_softc_t *sc)
{
	PI2O_EXEC_LCT_NOTIFY_MESSAGE	Message_Ptr;
	PI2O_SGE_SIMPLE_ELEMENT		sg;
	int				MessageSizeInBytes;
	caddr_t				v;
	int				len;
	I2O_LCT				Table;
	PI2O_LCT_ENTRY			Entry;

	/*
	 *	sc value assumed valid
	 */
	MessageSizeInBytes = sizeof(I2O_EXEC_LCT_NOTIFY_MESSAGE) -
	    sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT);
	if ((Message_Ptr = (PI2O_EXEC_LCT_NOTIFY_MESSAGE)malloc(
	    MessageSizeInBytes, M_TEMP, M_WAITOK)) == NULL) {
		return (ENOMEM);
	}
	(void)ASR_fillMessage((void *)Message_Ptr, MessageSizeInBytes);
	I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
	    (I2O_VERSION_11 + (((sizeof(I2O_EXEC_LCT_NOTIFY_MESSAGE) -
	    sizeof(I2O_SG_ELEMENT)) / sizeof(U32)) << 4)));
	I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
	    I2O_EXEC_LCT_NOTIFY);
	I2O_EXEC_LCT_NOTIFY_MESSAGE_setClassIdentifier(Message_Ptr,
	    I2O_CLASS_MATCH_ANYCLASS);
	/*
	 *	Call the LCT table to determine the number of device entries
	 * to reserve space for.
	 */
	SG(&(Message_Ptr->SGL), 0,
	  I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER, &Table,
	  sizeof(I2O_LCT));
	/*
	 *	since this code is reused in several systems, code efficiency
	 * is greater by using a shift operation rather than a divide by
	 * sizeof(u_int32_t).
	 */
	I2O_LCT_setTableSize(&Table,
	  (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)) >> 2);
	(void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
	/*
	 *	Determine the size of the LCT table.
	 */
	if (sc->ha_LCT) {
		free(sc->ha_LCT, M_TEMP);
	}
	/*
	 *	malloc only generates contiguous memory when less than a
	 * page is expected. We must break the request up into an SG list ...
	 */
	if (((len = (I2O_LCT_getTableSize(&Table) << 2)) <=
	  (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)))
	 || (len > (128 * 1024))) {	/* Arbitrary */
		free(Message_Ptr, M_TEMP);
		return (EINVAL);
	}
	if ((sc->ha_LCT = (PI2O_LCT)malloc (len, M_TEMP, M_WAITOK)) == NULL) {
		free(Message_Ptr, M_TEMP);
		return (ENOMEM);
	}
	/*
	 *	since this code is reused in several systems, code efficiency
	 * is greater by using a shift operation rather than a divide by
	 * sizeof(u_int32_t).
	 */
	I2O_LCT_setTableSize(sc->ha_LCT,
	  (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)) >> 2);
	/*
	 *	Convert the access to the LCT table into a SG list.
	 */
	sg = Message_Ptr->SGL.u.Simple;
	v = (caddr_t)(sc->ha_LCT);
	for (;;) {
		int next, base, span;

		span = 0;
		next = base = KVTOPHYS(v);
		I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg, base);

		/* How far can we go contiguously */
		while ((len > 0) && (base == next)) {
			int size;

			next = trunc_page(base) + PAGE_SIZE;
			size = next - base;
			if (size > len) {
				size = len;
			}
			span += size;
			v += size;
			len -= size;
			base = KVTOPHYS(v);
		}

		/* Construct the Flags */
		I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount), span);
		{
			int rw = I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT;
			if (len <= 0) {
				rw = (I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT
				    | I2O_SGL_FLAGS_LAST_ELEMENT
				    | I2O_SGL_FLAGS_END_OF_BUFFER);
			}
			I2O_FLAGS_COUNT_setFlags(&(sg->FlagsCount), rw);
		}

		if (len <= 0) {
			break;
		}

		/*
		 * Incrementing requires resizing of the packet.
		 */
		++sg;
		MessageSizeInBytes += sizeof(*sg);
		I2O_MESSAGE_FRAME_setMessageSize(
		  &(Message_Ptr->StdMessageFrame),
		  I2O_MESSAGE_FRAME_getMessageSize(
		    &(Message_Ptr->StdMessageFrame))
		  + (sizeof(*sg) / sizeof(U32)));
		{
			PI2O_EXEC_LCT_NOTIFY_MESSAGE NewMessage_Ptr;

			if ((NewMessage_Ptr = (PI2O_EXEC_LCT_NOTIFY_MESSAGE)
			    malloc(MessageSizeInBytes, M_TEMP, M_WAITOK))
			    == NULL) {
				free(sc->ha_LCT, M_TEMP);
				sc->ha_LCT = NULL;
				free(Message_Ptr, M_TEMP);
				return (ENOMEM);
			}
			span = ((caddr_t)sg) - (caddr_t)Message_Ptr;
			bcopy(Message_Ptr, NewMessage_Ptr, span);
			free(Message_Ptr, M_TEMP);
			sg = (PI2O_SGE_SIMPLE_ELEMENT)
			  (((caddr_t)NewMessage_Ptr) + span);
			Message_Ptr = NewMessage_Ptr;
		}
	}
	{	int retval;

		retval = ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
		free(Message_Ptr, M_TEMP);
		if (retval != CAM_REQ_CMP) {
			return (ENODEV);
		}
	}
	/* If the LCT table grew, lets truncate accesses */
	if (I2O_LCT_getTableSize(&Table) < I2O_LCT_getTableSize(sc->ha_LCT)) {
		I2O_LCT_setTableSize(sc->ha_LCT, I2O_LCT_getTableSize(&Table));
	}
	for (Entry = sc->ha_LCT->LCTEntry; Entry < (PI2O_LCT_ENTRY)
	  (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
	  ++Entry) {
		Entry->le_type = I2O_UNKNOWN;
		switch (I2O_CLASS_ID_getClass(&(Entry->ClassID))) {

		case I2O_CLASS_RANDOM_BLOCK_STORAGE:
			Entry->le_type = I2O_BSA;
			break;

		case I2O_CLASS_SCSI_PERIPHERAL:
			Entry->le_type = I2O_SCSI;
			break;

		case I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL:
			Entry->le_type = I2O_FCA;
			break;

		case I2O_CLASS_BUS_ADAPTER_PORT:
			Entry->le_type = I2O_PORT | I2O_SCSI;
			/* FALLTHRU */
		case I2O_CLASS_FIBRE_CHANNEL_PORT:
			if (I2O_CLASS_ID_getClass(&(Entry->ClassID)) ==
			  I2O_CLASS_FIBRE_CHANNEL_PORT) {
				Entry->le_type = I2O_PORT | I2O_FCA;
			}
		{	struct ControllerInfo {
				I2O_PARAM_RESULTS_LIST_HEADER	    Header;
				I2O_PARAM_READ_OPERATION_RESULT	    Read;
				I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR Info;
			} Buffer;
			PI2O_HBA_SCSI_CONTROLLER_INFO_SCALAR Info;

			Entry->le_bus = 0xff;
			Entry->le_target = 0xff;
			Entry->le_lun = 0xff;

			if ((Info = (PI2O_HBA_SCSI_CONTROLLER_INFO_SCALAR)
			  ASR_getParams(sc,
			    I2O_LCT_ENTRY_getLocalTID(Entry),
			    I2O_HBA_SCSI_CONTROLLER_INFO_GROUP_NO,
			    &Buffer, sizeof(struct ControllerInfo))) == NULL) {
				continue;
			}
			Entry->le_target
			  = I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getInitiatorID(
			    Info);
			Entry->le_lun = 0;
		}	/* FALLTHRU */
		default:
			continue;
		}
		{	struct DeviceInfo {
				I2O_PARAM_RESULTS_LIST_HEADER	Header;
				I2O_PARAM_READ_OPERATION_RESULT Read;
				I2O_DPT_DEVICE_INFO_SCALAR	Info;
			} Buffer;
			PI2O_DPT_DEVICE_INFO_SCALAR	 Info;

			Entry->le_bus = 0xff;
			Entry->le_target = 0xff;
			Entry->le_lun = 0xff;

			if ((Info = (PI2O_DPT_DEVICE_INFO_SCALAR)
			  ASR_getParams(sc,
			    I2O_LCT_ENTRY_getLocalTID(Entry),
			    I2O_DPT_DEVICE_INFO_GROUP_NO,
			    &Buffer, sizeof(struct DeviceInfo))) == NULL) {
				continue;
			}
			Entry->le_type
			  |= I2O_DPT_DEVICE_INFO_SCALAR_getDeviceType(Info);
			Entry->le_bus
			  = I2O_DPT_DEVICE_INFO_SCALAR_getBus(Info);
			if ((Entry->le_bus > sc->ha_MaxBus)
			 && (Entry->le_bus <= MAX_CHANNEL)) {
				sc->ha_MaxBus = Entry->le_bus;
			}
			Entry->le_target
			  = I2O_DPT_DEVICE_INFO_SCALAR_getIdentifier(Info);
			Entry->le_lun
			  = I2O_DPT_DEVICE_INFO_SCALAR_getLunInfo(Info);
		}
	}
	/*
	 *	A zero return value indicates success.
	 */
	return (0);
} /* ASR_acquireLct */

/*
 * Initialize a message frame.
 * We assume that the CDB has already been set up, so all we do here is
 * generate the Scatter Gather list.
 */
static PI2O_MESSAGE_FRAME
ASR_init_message(union asr_ccb *ccb, PI2O_MESSAGE_FRAME	Message)
{
	PI2O_MESSAGE_FRAME	Message_Ptr;
	PI2O_SGE_SIMPLE_ELEMENT sg;
	Asr_softc_t		*sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);
	vm_size_t		size, len;
	caddr_t			v;
	U32			MessageSize;
	int			next, span, base, rw;
	int			target = ccb->ccb_h.target_id;
	int			lun = ccb->ccb_h.target_lun;
	int			bus =cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
	tid_t			TID;

	/* We only need to zero out the PRIVATE_SCSI_SCB_EXECUTE_MESSAGE */
	Message_Ptr = (I2O_MESSAGE_FRAME *)Message;
	bzero(Message_Ptr, (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) -
	      sizeof(I2O_SG_ELEMENT)));

	if ((TID = ASR_getTid (sc, bus, target, lun)) == (tid_t)-1) {
		PI2O_LCT_ENTRY Device;

		TID = 0;
		for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
		    (((U32 *)sc->ha_LCT) + I2O_LCT_getTableSize(sc->ha_LCT));
		    ++Device) {
			if ((Device->le_type != I2O_UNKNOWN)
			 && (Device->le_bus == bus)
			 && (Device->le_target == target)
			 && (Device->le_lun == lun)
			 && (I2O_LCT_ENTRY_getUserTID(Device) == 0xFFF)) {
				TID = I2O_LCT_ENTRY_getLocalTID(Device);
				ASR_setTid(sc, Device->le_bus,
					   Device->le_target, Device->le_lun,
					   TID);
				break;
			}
		}
	}
	if (TID == (tid_t)0) {
		return (NULL);
	}
	I2O_MESSAGE_FRAME_setTargetAddress(Message_Ptr, TID);
	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setTID(
	    (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, TID);
	I2O_MESSAGE_FRAME_setVersionOffset(Message_Ptr, I2O_VERSION_11 |
	  (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) - sizeof(I2O_SG_ELEMENT))
		/ sizeof(U32)) << 4));
	I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
	  (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
	  - sizeof(I2O_SG_ELEMENT)) / sizeof(U32));
	I2O_MESSAGE_FRAME_setInitiatorAddress (Message_Ptr, 1);
	I2O_MESSAGE_FRAME_setFunction(Message_Ptr, I2O_PRIVATE_MESSAGE);
	I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode (
	  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr, I2O_SCSI_SCB_EXEC);
	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (
	  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
	    I2O_SCB_FLAG_ENABLE_DISCONNECT
	  | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
	  | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
	/*
	 * We do not need any (optional byteswapping) method access to
	 * the Initiator & Transaction context field.
	 */
	I2O_MESSAGE_FRAME_setInitiatorContext64(Message, (long)ccb);

	I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
	  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr, DPT_ORGANIZATION_ID);
	/*
	 * copy the cdb over
	 */
	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(
	    (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, ccb->csio.cdb_len);
	bcopy(&(ccb->csio.cdb_io),
	    ((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr)->CDB,
	    ccb->csio.cdb_len);

	/*
	 * Given a buffer describing a transfer, set up a scatter/gather map
	 * in a ccb to map that SCSI transfer.
	 */

	rw = (ccb->ccb_h.flags & CAM_DIR_IN) ? 0 : I2O_SGL_FLAGS_DIR;

	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (
	  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
	  (ccb->csio.dxfer_len)
	    ? ((rw) ? (I2O_SCB_FLAG_XFER_TO_DEVICE
		     | I2O_SCB_FLAG_ENABLE_DISCONNECT
		     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER)
		    : (I2O_SCB_FLAG_XFER_FROM_DEVICE
		     | I2O_SCB_FLAG_ENABLE_DISCONNECT
		     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER))
	    :	      (I2O_SCB_FLAG_ENABLE_DISCONNECT
		     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

	/*
	 * Given a transfer described by a `data', fill in the SG list.
	 */
	sg = &((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr)->SGL.u.Simple[0];

	len = ccb->csio.dxfer_len;
	v = ccb->csio.data_ptr;
	KASSERT(ccb->csio.dxfer_len >= 0, ("csio.dxfer_len < 0"));
	MessageSize = I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr);
	PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(
	  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, len);
	while ((len > 0) && (sg < &((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
	  Message_Ptr)->SGL.u.Simple[SG_SIZE])) {
		span = 0;
		next = base = KVTOPHYS(v);
		I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg, base);

		/* How far can we go contiguously */
		while ((len > 0) && (base == next)) {
			next = trunc_page(base) + PAGE_SIZE;
			size = next - base;
			if (size > len) {
				size = len;
			}
			span += size;
			v += size;
			len -= size;
			base = KVTOPHYS(v);
		}

		I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount), span);
		if (len == 0) {
			rw |= I2O_SGL_FLAGS_LAST_ELEMENT;
		}
		I2O_FLAGS_COUNT_setFlags(&(sg->FlagsCount),
		  I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT | rw);
		++sg;
		MessageSize += sizeof(*sg) / sizeof(U32);
	}
	/* We always do the request sense ... */
	if ((span = ccb->csio.sense_len) == 0) {
		span = sizeof(ccb->csio.sense_data);
	}
	SG(sg, 0, I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
	  &(ccb->csio.sense_data), span);
	I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
	  MessageSize + (sizeof(*sg) / sizeof(U32)));
	return (Message_Ptr);
} /* ASR_init_message */

/*
 *	Reset the adapter.
 */
static U32
ASR_initOutBound(Asr_softc_t *sc)
{
	struct initOutBoundMessage {
		I2O_EXEC_OUTBOUND_INIT_MESSAGE M;
		U32			       R;
	}				Message;
	PI2O_EXEC_OUTBOUND_INIT_MESSAGE	Message_Ptr;
	U32				*volatile Reply_Ptr;
	U32				Old;

	/*
	 *  Build up our copy of the Message.
	 */
	Message_Ptr = (PI2O_EXEC_OUTBOUND_INIT_MESSAGE)ASR_fillMessage(&Message,
	  sizeof(I2O_EXEC_OUTBOUND_INIT_MESSAGE));
	I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
	  I2O_EXEC_OUTBOUND_INIT);
	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setHostPageFrameSize(Message_Ptr, PAGE_SIZE);
	I2O_EXEC_OUTBOUND_INIT_MESSAGE_setOutboundMFrameSize(Message_Ptr,
	  sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME));
	/*
	 *  Reset the Reply Status
	 */
	*(Reply_Ptr = (U32 *)((char *)Message_Ptr
	  + sizeof(I2O_EXEC_OUTBOUND_INIT_MESSAGE))) = 0;
	SG (&(Message_Ptr->SGL), 0, I2O_SGL_FLAGS_LAST_ELEMENT, Reply_Ptr,
	  sizeof(U32));
	/*
	 *	Send the Message out
	 */
	if ((Old = ASR_initiateCp(sc,
				  (PI2O_MESSAGE_FRAME)Message_Ptr)) != -1L) {
		u_long size, addr;

		/*
		 *	Wait for a response (Poll).
		 */
		while (*Reply_Ptr < I2O_EXEC_OUTBOUND_INIT_REJECTED);
		/*
		 *	Re-enable the interrupts.
		 */
		asr_set_intr(sc, Old);
		/*
		 *	Populate the outbound table.
		 */
		if (sc->ha_Msgs == NULL) {

			/* Allocate the reply frames */
			size = sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
			  * sc->ha_Msgs_Count;

			/*
			 *	contigmalloc only works reliably at
			 * initialization time.
			 */
			if ((sc->ha_Msgs = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
			  contigmalloc (size, M_DEVBUF, M_WAITOK, 0ul,
			    0xFFFFFFFFul, (u_long)sizeof(U32), 0ul)) != NULL) {
				bzero(sc->ha_Msgs, size);
				sc->ha_Msgs_Phys = KVTOPHYS(sc->ha_Msgs);
			}
		}

		/* Initialize the outbound FIFO */
		if (sc->ha_Msgs != NULL)
		for(size = sc->ha_Msgs_Count, addr = sc->ha_Msgs_Phys;
		    size; --size) {
			asr_set_FromFIFO(sc, addr);
			addr += sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME);
		}
		return (*Reply_Ptr);
	}
	return (0);
} /* ASR_initOutBound */

/*
 *	Set the system table
 */
static int
ASR_setSysTab(Asr_softc_t *sc)
{
	PI2O_EXEC_SYS_TAB_SET_MESSAGE Message_Ptr;
	PI2O_SET_SYSTAB_HEADER	      SystemTable;
	Asr_softc_t		    * ha;
	PI2O_SGE_SIMPLE_ELEMENT	      sg;
	int			      retVal;

	if ((SystemTable = (PI2O_SET_SYSTAB_HEADER)malloc (
	  sizeof(I2O_SET_SYSTAB_HEADER), M_TEMP, M_WAITOK | M_ZERO)) == NULL) {
		return (ENOMEM);
	}
	for (ha = Asr_softc; ha; ha = ha->ha_next) {
		++SystemTable->NumberEntries;
	}
	if ((Message_Ptr = (PI2O_EXEC_SYS_TAB_SET_MESSAGE)malloc (
	  sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
	   + ((3+SystemTable->NumberEntries) * sizeof(I2O_SGE_SIMPLE_ELEMENT)),
	  M_TEMP, M_WAITOK)) == NULL) {
		free(SystemTable, M_TEMP);
		return (ENOMEM);
	}
	(void)ASR_fillMessage((void *)Message_Ptr,
	  sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
	   + ((3+SystemTable->NumberEntries) * sizeof(I2O_SGE_SIMPLE_ELEMENT)));
	I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
	  (I2O_VERSION_11 +
	  (((sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
			/ sizeof(U32)) << 4)));
	I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
	  I2O_EXEC_SYS_TAB_SET);
	/*
	 *	Call the LCT table to determine the number of device entries
	 * to reserve space for.
	 *	since this code is reused in several systems, code efficiency
	 * is greater by using a shift operation rather than a divide by
	 * sizeof(u_int32_t).
	 */
	sg = (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
	  + ((I2O_MESSAGE_FRAME_getVersionOffset(
	      &(Message_Ptr->StdMessageFrame)) & 0xF0) >> 2));
	SG(sg, 0, I2O_SGL_FLAGS_DIR, SystemTable, sizeof(I2O_SET_SYSTAB_HEADER));
	++sg;
	for (ha = Asr_softc; ha; ha = ha->ha_next) {
		SG(sg, 0,
		  ((ha->ha_next)
		    ? (I2O_SGL_FLAGS_DIR)
		    : (I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER)),
		  &(ha->ha_SystemTable), sizeof(ha->ha_SystemTable));
		++sg;
	}
	SG(sg, 0, I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER, NULL, 0);
	SG(sg, 1, I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_LAST_ELEMENT
	    | I2O_SGL_FLAGS_END_OF_BUFFER, NULL, 0);
	retVal = ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
	free(Message_Ptr, M_TEMP);
	free(SystemTable, M_TEMP);
	return (retVal);
} /* ASR_setSysTab */

static int
ASR_acquireHrt(Asr_softc_t *sc)
{
	I2O_EXEC_HRT_GET_MESSAGE	Message;
	I2O_EXEC_HRT_GET_MESSAGE	*Message_Ptr;
	struct {
		I2O_HRT	      Header;
		I2O_HRT_ENTRY Entry[MAX_CHANNEL];
	}				Hrt;
	u_int8_t			NumberOfEntries;
	PI2O_HRT_ENTRY			Entry;

	bzero(&Hrt, sizeof (Hrt));
	Message_Ptr = (I2O_EXEC_HRT_GET_MESSAGE *)ASR_fillMessage(&Message,
	  sizeof(I2O_EXEC_HRT_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
	  + sizeof(I2O_SGE_SIMPLE_ELEMENT));
	I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
	  (I2O_VERSION_11
	  + (((sizeof(I2O_EXEC_HRT_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
		   / sizeof(U32)) << 4)));
	I2O_MESSAGE_FRAME_setFunction (&(Message_Ptr->StdMessageFrame),
	  I2O_EXEC_HRT_GET);

	/*
	 *  Set up the buffers as scatter gather elements.
	 */
	SG(&(Message_Ptr->SGL), 0,
	  I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
	  &Hrt, sizeof(Hrt));
	if (ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) != CAM_REQ_CMP) {
		return (ENODEV);
	}
	if ((NumberOfEntries = I2O_HRT_getNumberEntries(&Hrt.Header))
	  > (MAX_CHANNEL + 1)) {
		NumberOfEntries = MAX_CHANNEL + 1;
	}
	for (Entry = Hrt.Header.HRTEntry;
	  NumberOfEntries != 0;
	  ++Entry, --NumberOfEntries) {
		PI2O_LCT_ENTRY Device;

		for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
		  (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
		  ++Device) {
			if (I2O_LCT_ENTRY_getLocalTID(Device)
			  == (I2O_HRT_ENTRY_getAdapterID(Entry) & 0xFFF)) {
				Device->le_bus = I2O_HRT_ENTRY_getAdapterID(
				  Entry) >> 16;
				if ((Device->le_bus > sc->ha_MaxBus)
				 && (Device->le_bus <= MAX_CHANNEL)) {
					sc->ha_MaxBus = Device->le_bus;
				}
			}
		}
	}
	return (0);
} /* ASR_acquireHrt */

/*
 *	Enable the adapter.
 */
static int
ASR_enableSys(Asr_softc_t *sc)
{
	I2O_EXEC_SYS_ENABLE_MESSAGE	Message;
	PI2O_EXEC_SYS_ENABLE_MESSAGE	Message_Ptr;

	Message_Ptr = (PI2O_EXEC_SYS_ENABLE_MESSAGE)ASR_fillMessage(&Message,
	  sizeof(I2O_EXEC_SYS_ENABLE_MESSAGE));
	I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
	  I2O_EXEC_SYS_ENABLE);
	return (ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) != 0);
} /* ASR_enableSys */

/*
 *	Perform the stages necessary to initialize the adapter
 */
static int
ASR_init(Asr_softc_t *sc)
{
	return ((ASR_initOutBound(sc) == 0)
	 || (ASR_setSysTab(sc) != CAM_REQ_CMP)
	 || (ASR_enableSys(sc) != CAM_REQ_CMP));
} /* ASR_init */

/*
 *	Send a Synchronize Cache command to the target device.
 */
static void
ASR_sync(Asr_softc_t *sc, int bus, int target, int lun)
{
	tid_t TID;

	/*
	 * We will not synchronize the device when there are outstanding
	 * commands issued by the OS (this is due to a locked up device,
	 * as the OS normally would flush all outstanding commands before
	 * issuing a shutdown or an adapter reset).
	 */
	if ((sc != NULL)
	 && (LIST_FIRST(&(sc->ha_ccb)) != NULL)
	 && ((TID = ASR_getTid (sc, bus, target, lun)) != (tid_t)-1)
	 && (TID != (tid_t)0)) {
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE	Message;
		PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE	Message_Ptr;

		Message_Ptr = (PRIVATE_SCSI_SCB_EXECUTE_MESSAGE *)&Message;
		bzero(Message_Ptr, sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
		    - sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT));

		I2O_MESSAGE_FRAME_setVersionOffset(
		  (PI2O_MESSAGE_FRAME)Message_Ptr,
		  I2O_VERSION_11
		    | (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
		    - sizeof(I2O_SG_ELEMENT))
			/ sizeof(U32)) << 4));
		I2O_MESSAGE_FRAME_setMessageSize(
		  (PI2O_MESSAGE_FRAME)Message_Ptr,
		  (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
		  - sizeof(I2O_SG_ELEMENT))
			/ sizeof(U32));
		I2O_MESSAGE_FRAME_setInitiatorAddress (
		  (PI2O_MESSAGE_FRAME)Message_Ptr, 1);
		I2O_MESSAGE_FRAME_setFunction(
		  (PI2O_MESSAGE_FRAME)Message_Ptr, I2O_PRIVATE_MESSAGE);
		I2O_MESSAGE_FRAME_setTargetAddress(
		  (PI2O_MESSAGE_FRAME)Message_Ptr, TID);
		I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode (
		  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
		  I2O_SCSI_SCB_EXEC);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setTID(Message_Ptr, TID);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
		    I2O_SCB_FLAG_ENABLE_DISCONNECT
		  | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		  | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
		I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
		  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
		  DPT_ORGANIZATION_ID);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(Message_Ptr, 6);
		Message_Ptr->CDB[0] = SYNCHRONIZE_CACHE;
		Message_Ptr->CDB[1] = (lun << 5);

		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
		  (I2O_SCB_FLAG_XFER_FROM_DEVICE
		    | I2O_SCB_FLAG_ENABLE_DISCONNECT
		    | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		    | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

		(void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

	}
}

static void
ASR_synchronize(Asr_softc_t *sc)
{
	int bus, target, lun;

	for (bus = 0; bus <= sc->ha_MaxBus; ++bus) {
		for (target = 0; target <= sc->ha_MaxId; ++target) {
			for (lun = 0; lun <= sc->ha_MaxLun; ++lun) {
				ASR_sync(sc,bus,target,lun);
			}
		}
	}
}

/*
 *	Reset the HBA, targets and BUS.
 *		Currently this resets *all* the SCSI busses.
 */
static __inline void
asr_hbareset(Asr_softc_t *sc)
{
	ASR_synchronize(sc);
	(void)ASR_reset(sc);
} /* asr_hbareset */

/*
 *	A reduced copy of the real pci_map_mem, incorporating the MAX_MAP
 * limit and a reduction in error checking (in the pre 4.0 case).
 */
static int
asr_pci_map_mem(device_t tag, Asr_softc_t *sc)
{
	int		rid;
	u_int32_t	p, l, s;

	/*
	 * I2O specification says we must find first *memory* mapped BAR
	 */
	for (rid = 0; rid < 4; rid++) {
		p = pci_read_config(tag, PCIR_BAR(rid), sizeof(p));
		if ((p & 1) == 0) {
			break;
		}
	}
	/*
	 *	Give up?
	 */
	if (rid >= 4) {
		rid = 0;
	}
	rid = PCIR_BAR(rid);
	p = pci_read_config(tag, rid, sizeof(p));
	pci_write_config(tag, rid, -1, sizeof(p));
	l = 0 - (pci_read_config(tag, rid, sizeof(l)) & ~15);
	pci_write_config(tag, rid, p, sizeof(p));
	if (l > MAX_MAP) {
		l = MAX_MAP;
	}
	/*
	 * The 2005S Zero Channel RAID solution is not a perfect PCI
	 * citizen. It asks for 4MB on BAR0, and 0MB on BAR1, once
	 * enabled it rewrites the size of BAR0 to 2MB, sets BAR1 to
	 * BAR0+2MB and sets it's size to 2MB. The IOP registers are
	 * accessible via BAR0, the messaging registers are accessible
	 * via BAR1. If the subdevice code is 50 to 59 decimal.
	 */
	s = pci_read_config(tag, PCIR_DEVVENDOR, sizeof(s));
	if (s != 0xA5111044) {
		s = pci_read_config(tag, PCIR_SUBVEND_0, sizeof(s));
		if ((((ADPTDOMINATOR_SUB_ID_START ^ s) & 0xF000FFFF) == 0)
		 && (ADPTDOMINATOR_SUB_ID_START <= s)
		 && (s <= ADPTDOMINATOR_SUB_ID_END)) {
			l = MAX_MAP; /* Conjoined BAR Raptor Daptor */
		}
	}
	p &= ~15;
	sc->ha_mem_res = bus_alloc_resource(tag, SYS_RES_MEMORY, &rid,
	  p, p + l, l, RF_ACTIVE);
	if (sc->ha_mem_res == NULL) {
		return (0);
	}
	sc->ha_Base = rman_get_start(sc->ha_mem_res);
	sc->ha_i2o_bhandle = rman_get_bushandle(sc->ha_mem_res);
	sc->ha_i2o_btag = rman_get_bustag(sc->ha_mem_res);

	if (s == 0xA5111044) { /* Split BAR Raptor Daptor */
		if ((rid += sizeof(u_int32_t)) >= PCIR_BAR(4)) {
			return (0);
		}
		p = pci_read_config(tag, rid, sizeof(p));
		pci_write_config(tag, rid, -1, sizeof(p));
		l = 0 - (pci_read_config(tag, rid, sizeof(l)) & ~15);
		pci_write_config(tag, rid, p, sizeof(p));
		if (l > MAX_MAP) {
			l = MAX_MAP;
		}
		p &= ~15;
		sc->ha_mes_res = bus_alloc_resource(tag, SYS_RES_MEMORY, &rid,
		  p, p + l, l, RF_ACTIVE);
		if (sc->ha_mes_res == NULL) {
			return (0);
		}
		sc->ha_frame_bhandle = rman_get_bushandle(sc->ha_mes_res);
		sc->ha_frame_btag = rman_get_bustag(sc->ha_mes_res);
	} else {
		sc->ha_frame_bhandle = sc->ha_i2o_bhandle;
		sc->ha_frame_btag = sc->ha_i2o_btag;
	}
	return (1);
} /* asr_pci_map_mem */

/*
 *	A simplified copy of the real pci_map_int with additional
 * registration requirements.
 */
static int
asr_pci_map_int(device_t tag, Asr_softc_t *sc)
{
	int rid = 0;

	sc->ha_irq_res = bus_alloc_resource_any(tag, SYS_RES_IRQ, &rid,
	  RF_ACTIVE | RF_SHAREABLE);
	if (sc->ha_irq_res == NULL) {
		return (0);
	}
	if (bus_setup_intr(tag, sc->ha_irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
	  (driver_intr_t *)asr_intr, (void *)sc, &(sc->ha_intr))) {
		return (0);
	}
	sc->ha_irq = pci_read_config(tag, PCIR_INTLINE, sizeof(char));
	return (1);
} /* asr_pci_map_int */

/*
 *	Attach the devices, and virtual devices to the driver list.
 */
static int
asr_attach(device_t tag)
{
	PI2O_EXEC_STATUS_GET_REPLY status;
	PI2O_LCT_ENTRY		 Device;
	Asr_softc_t		 *sc, **ha;
	struct scsi_inquiry_data *iq;
	union asr_ccb		 *ccb;
	int			 bus, size, unit = device_get_unit(tag);

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		return(ENOMEM);
	}
	if (Asr_softc == NULL) {
		/*
		 *	Fixup the OS revision as saved in the dptsig for the
		 *	engine (dptioctl.h) to pick up.
		 */
		bcopy(osrelease, &ASR_sig.dsDescription[16], 5);
	}
	/*
	 *	Initialize the software structure
	 */
	LIST_INIT(&(sc->ha_ccb));
	/* Link us into the HA list */
	for (ha = &Asr_softc; *ha; ha = &((*ha)->ha_next));
		*(ha) = sc;

	/*
	 *	This is the real McCoy!
	 */
	if (!asr_pci_map_mem(tag, sc)) {
		printf ("asr%d: could not map memory\n", unit);
		return(ENXIO);
	}
	/* Enable if not formerly enabled */
	pci_write_config(tag, PCIR_COMMAND,
	    pci_read_config(tag, PCIR_COMMAND, sizeof(char)) |
	    PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN, sizeof(char));
	/* Knowledge is power, responsibility is direct */
	{
		struct pci_devinfo {
			STAILQ_ENTRY(pci_devinfo) pci_links;
			struct resource_list	  resources;
			pcicfgregs		  cfg;
		} * dinfo = device_get_ivars(tag);
		sc->ha_pciBusNum = dinfo->cfg.bus;
		sc->ha_pciDeviceNum = (dinfo->cfg.slot << 3) | dinfo->cfg.func;
	}
	/* Check if the device is there? */
	if ((ASR_resetIOP(sc) == 0) ||
	    ((status = (PI2O_EXEC_STATUS_GET_REPLY)malloc(
	    sizeof(I2O_EXEC_STATUS_GET_REPLY), M_TEMP, M_WAITOK)) == NULL) ||
	    (ASR_getStatus(sc, status) == NULL)) {
		printf ("asr%d: could not initialize hardware\n", unit);
		return(ENODEV);	/* Get next, maybe better luck */
	}
	sc->ha_SystemTable.OrganizationID = status->OrganizationID;
	sc->ha_SystemTable.IOP_ID = status->IOP_ID;
	sc->ha_SystemTable.I2oVersion = status->I2oVersion;
	sc->ha_SystemTable.IopState = status->IopState;
	sc->ha_SystemTable.MessengerType = status->MessengerType;
	sc->ha_SystemTable.InboundMessageFrameSize = status->InboundMFrameSize;
	sc->ha_SystemTable.MessengerInfo.InboundMessagePortAddressLow =
	    (U32)(sc->ha_Base + I2O_REG_TOFIFO);	/* XXX 64-bit */

	if (!asr_pci_map_int(tag, (void *)sc)) {
		printf ("asr%d: could not map interrupt\n", unit);
		return(ENXIO);
	}

	/* Adjust the maximim inbound count */
	if (((sc->ha_QueueSize =
	    I2O_EXEC_STATUS_GET_REPLY_getMaxInboundMFrames(status)) >
	    MAX_INBOUND) || (sc->ha_QueueSize == 0)) {
		sc->ha_QueueSize = MAX_INBOUND;
	}

	/* Adjust the maximum outbound count */
	if (((sc->ha_Msgs_Count =
	    I2O_EXEC_STATUS_GET_REPLY_getMaxOutboundMFrames(status)) >
	    MAX_OUTBOUND) || (sc->ha_Msgs_Count == 0)) {
		sc->ha_Msgs_Count = MAX_OUTBOUND;
	}
	if (sc->ha_Msgs_Count > sc->ha_QueueSize) {
		sc->ha_Msgs_Count = sc->ha_QueueSize;
	}

	/* Adjust the maximum SG size to adapter */
	if ((size = (I2O_EXEC_STATUS_GET_REPLY_getInboundMFrameSize(status) <<
	    2)) > MAX_INBOUND_SIZE) {
		size = MAX_INBOUND_SIZE;
	}
	free(status, M_TEMP);
	sc->ha_SgSize = (size - sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
	  + sizeof(I2O_SG_ELEMENT)) / sizeof(I2O_SGE_SIMPLE_ELEMENT);

	/*
	 *	Only do a bus/HBA reset on the first time through. On this
	 * first time through, we do not send a flush to the devices.
	 */
	if (ASR_init(sc) == 0) {
		struct BufferInfo {
			I2O_PARAM_RESULTS_LIST_HEADER	    Header;
			I2O_PARAM_READ_OPERATION_RESULT	    Read;
			I2O_DPT_EXEC_IOP_BUFFERS_SCALAR	    Info;
		} Buffer;
		PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR Info;
#define FW_DEBUG_BLED_OFFSET 8

		if ((Info = (PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR)
		    ASR_getParams(sc, 0, I2O_DPT_EXEC_IOP_BUFFERS_GROUP_NO,
		    &Buffer, sizeof(struct BufferInfo))) != NULL) {
			sc->ha_blinkLED = FW_DEBUG_BLED_OFFSET +
			    I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialOutputOffset(Info);
		}
		if (ASR_acquireLct(sc) == 0) {
			(void)ASR_acquireHrt(sc);
		}
	} else {
		printf ("asr%d: failed to initialize\n", unit);
		return(ENXIO);
	}
	/*
	 *	Add in additional probe responses for more channels. We
	 * are reusing the variable `target' for a channel loop counter.
	 * Done here because of we need both the acquireLct and
	 * acquireHrt data.
	 */
	for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
	    (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT)); ++Device) {
		if (Device->le_type == I2O_UNKNOWN) {
			continue;
		}
		if (I2O_LCT_ENTRY_getUserTID(Device) == 0xFFF) {
			if (Device->le_target > sc->ha_MaxId) {
				sc->ha_MaxId = Device->le_target;
			}
			if (Device->le_lun > sc->ha_MaxLun) {
				sc->ha_MaxLun = Device->le_lun;
			}
		}
		if (((Device->le_type & I2O_PORT) != 0)
		 && (Device->le_bus <= MAX_CHANNEL)) {
			/* Do not increase MaxId for efficiency */
			sc->ha_adapter_target[Device->le_bus] =
			    Device->le_target;
		}
	}

	/*
	 *	Print the HBA model number as inquired from the card.
	 */

	printf("asr%d:", unit);

	if ((iq = (struct scsi_inquiry_data *)malloc(
	    sizeof(struct scsi_inquiry_data), M_TEMP, M_WAITOK | M_ZERO)) !=
	    NULL) {
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE	Message;
		PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE	Message_Ptr;
		int					posted = 0;

		Message_Ptr = (PRIVATE_SCSI_SCB_EXECUTE_MESSAGE *)&Message;
		bzero(Message_Ptr, sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) -
		    sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT));

		I2O_MESSAGE_FRAME_setVersionOffset(
		    (PI2O_MESSAGE_FRAME)Message_Ptr, I2O_VERSION_11 |
		    (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
		    - sizeof(I2O_SG_ELEMENT)) / sizeof(U32)) << 4));
		I2O_MESSAGE_FRAME_setMessageSize(
		    (PI2O_MESSAGE_FRAME)Message_Ptr,
		    (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) -
		    sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT)) /
		    sizeof(U32));
		I2O_MESSAGE_FRAME_setInitiatorAddress(
		    (PI2O_MESSAGE_FRAME)Message_Ptr, 1);
		I2O_MESSAGE_FRAME_setFunction(
		    (PI2O_MESSAGE_FRAME)Message_Ptr, I2O_PRIVATE_MESSAGE);
		I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode(
		    (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr, I2O_SCSI_SCB_EXEC);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
		    I2O_SCB_FLAG_ENABLE_DISCONNECT
		  | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		  | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setInterpret(Message_Ptr, 1);
		I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
		    (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
		    DPT_ORGANIZATION_ID);
		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(Message_Ptr, 6);
		Message_Ptr->CDB[0] = INQUIRY;
		Message_Ptr->CDB[4] =
		    (unsigned char)sizeof(struct scsi_inquiry_data);
		if (Message_Ptr->CDB[4] == 0) {
			Message_Ptr->CDB[4] = 255;
		}

		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
		  (I2O_SCB_FLAG_XFER_FROM_DEVICE
		    | I2O_SCB_FLAG_ENABLE_DISCONNECT
		    | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
		    | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

		PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(
		  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
		  sizeof(struct scsi_inquiry_data));
		SG(&(Message_Ptr->SGL), 0,
		  I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
		  iq, sizeof(struct scsi_inquiry_data));
		(void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

		if (iq->vendor[0] && (iq->vendor[0] != ' ')) {
			printf (" ");
			ASR_prstring (iq->vendor, 8);
			++posted;
		}
		if (iq->product[0] && (iq->product[0] != ' ')) {
			printf (" ");
			ASR_prstring (iq->product, 16);
			++posted;
		}
		if (iq->revision[0] && (iq->revision[0] != ' ')) {
			printf (" FW Rev. ");
			ASR_prstring (iq->revision, 4);
			++posted;
		}
		free(iq, M_TEMP);
		if (posted) {
			printf (",");
		}
	}
	printf (" %d channel, %d CCBs, Protocol I2O\n", sc->ha_MaxBus + 1,
	  (sc->ha_QueueSize > MAX_INBOUND) ? MAX_INBOUND : sc->ha_QueueSize);

	/*
	 * fill in the prototype cam_path.
	 */
	if ((ccb = asr_alloc_ccb(sc)) == NULL) {
		printf ("asr%d: CAM could not be notified of asynchronous callback parameters\n", unit);
		return(ENOMEM);
	}
	for (bus = 0; bus <= sc->ha_MaxBus; ++bus) {
		struct cam_devq	  * devq;
		int		    QueueSize = sc->ha_QueueSize;

		if (QueueSize > MAX_INBOUND) {
			QueueSize = MAX_INBOUND;
		}

		/*
		 *	Create the device queue for our SIM(s).
		 */
		if ((devq = cam_simq_alloc(QueueSize)) == NULL) {
			continue;
		}

		/*
		 *	Construct our first channel SIM entry
		 */
		sc->ha_sim[bus] = cam_sim_alloc(asr_action, asr_poll, "asr", sc,
						unit, 1, QueueSize, devq);
		if (sc->ha_sim[bus] == NULL) {
			continue;
		}

		if (xpt_bus_register(sc->ha_sim[bus], bus) != CAM_SUCCESS) {
			cam_sim_free(sc->ha_sim[bus],
			  /*free_devq*/TRUE);
			sc->ha_sim[bus] = NULL;
			continue;
		}

		if (xpt_create_path(&(sc->ha_path[bus]), /*periph*/NULL,
		    cam_sim_path(sc->ha_sim[bus]), CAM_TARGET_WILDCARD,
		    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister( cam_sim_path(sc->ha_sim[bus]));
			cam_sim_free(sc->ha_sim[bus], /*free_devq*/TRUE);
			sc->ha_sim[bus] = NULL;
			continue;
		}
	}
	asr_free_ccb(ccb);
	/*
	 *	Generate the device node information
	 */
	sc->ha_devt = make_dev(&asr_cdevsw, unit, UID_ROOT, GID_OPERATOR, 0640,
			       "asr%d", unit);
	if (sc->ha_devt != NULL)
		(void)make_dev_alias(sc->ha_devt, "rdpti%d", unit);
	sc->ha_devt->si_drv1 = sc;
	return(0);
} /* asr_attach */

static void
asr_poll(struct cam_sim *sim)
{
	asr_intr(cam_sim_softc(sim));
} /* asr_poll */

static void
asr_action(struct cam_sim *sim, union ccb  *ccb)
{
	struct Asr_softc *sc;

	debug_asr_printf("asr_action(%lx,%lx{%x})\n", (u_long)sim, (u_long)ccb,
			 ccb->ccb_h.func_code);

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("asr_action\n"));

	ccb->ccb_h.spriv_ptr0 = sc = (struct Asr_softc *)cam_sim_softc(sim);

	switch (ccb->ccb_h.func_code) {

	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		struct Message {
			char M[MAX_INBOUND_SIZE];
		} Message;
		PI2O_MESSAGE_FRAME   Message_Ptr;

		/* Reject incoming commands while we are resetting the card */
		if (sc->ha_in_reset != HA_OPERATIONAL) {
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
			if (sc->ha_in_reset >= HA_OFF_LINE) {
				/* HBA is now off-line */
				ccb->ccb_h.status |= CAM_UNREC_HBA_ERROR;
			} else {
				/* HBA currently resetting, try again later. */
				ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			}
			debug_asr_cmd_printf (" e\n");
			xpt_done(ccb);
			debug_asr_cmd_printf (" q\n");
			break;
		}
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			printf(
			  "asr%d WARNING: scsi_cmd(%x) already done on b%dt%du%d\n",
			  cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)),
			  ccb->csio.cdb_io.cdb_bytes[0],
			  cam_sim_bus(sim),
			  ccb->ccb_h.target_id,
			  ccb->ccb_h.target_lun);
		}
		debug_asr_cmd_printf("(%d,%d,%d,%d)", cam_sim_unit(sim),
				     cam_sim_bus(sim), ccb->ccb_h.target_id,
				     ccb->ccb_h.target_lun);
		debug_asr_dump_ccb(ccb);

		if ((Message_Ptr = ASR_init_message((union asr_ccb *)ccb,
		  (PI2O_MESSAGE_FRAME)&Message)) != NULL) {
			debug_asr_cmd2_printf ("TID=%x:\n",
			  PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getTID(
			    (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr));
			debug_asr_cmd2_dump_message(Message_Ptr);
			debug_asr_cmd1_printf (" q");

			if (ASR_queue (sc, Message_Ptr) == EMPTY_QUEUE) {
				ccb->ccb_h.status &= ~CAM_STATUS_MASK;
				ccb->ccb_h.status |= CAM_REQUEUE_REQ;
				debug_asr_cmd_printf (" E\n");
				xpt_done(ccb);
			}
			debug_asr_cmd_printf(" Q\n");
			break;
		}
		/*
		 *	We will get here if there is no valid TID for the device
		 * referenced in the scsi command packet.
		 */
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_SEL_TIMEOUT;
		debug_asr_cmd_printf (" B\n");
		xpt_done(ccb);
		break;
	}

	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
		/* Rese HBA device ... */
		asr_hbareset (sc);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

#if (defined(REPORT_LUNS))
	case REPORT_LUNS:
#endif
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_SET_TRAN_SETTINGS:
		/* XXX Implement */
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	ccb_trans_settings *cts;
		u_int	target_mask;

		cts = &(ccb->cts);
		target_mask = 0x01 << ccb->ccb_h.target_id;
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
			cts->flags = CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB;
			cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			cts->sync_period = 6; /* 40MHz */
			cts->sync_offset = 15;

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else {
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		}
		xpt_done(ccb);
		break;
	}

	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &(ccb->ccg);
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

		if (size_mb > 4096) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else if (size_mb > 2048) {
			ccg->heads = 128;
			ccg->secs_per_track = 63;
		} else if (size_mb > 1024) {
			ccg->heads = 65;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
		ASR_resetBus (sc, cam_sim_bus(sim));
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &(ccb->cpi);

		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
		cpi->target_sprt = 0;
		/* Not necessary to reset bus, done by HDM initialization */
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = sc->ha_MaxId;
		cpi->max_lun = sc->ha_MaxLun;
		cpi->initiator_id = sc->ha_adapter_target[cam_sim_bus(sim)];
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Adaptec", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
} /* asr_action */

/*
 * Handle processing of current CCB as pointed to by the Status.
 */
static int
asr_intr(Asr_softc_t *sc)
{
	int processed;

	for(processed = 0; asr_get_status(sc) & Mask_InterruptsDisabled;
	    processed = 1) {
		union asr_ccb			   *ccb;
		u_int				    dsc;
		U32				    ReplyOffset;
		PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME Reply;

		if (((ReplyOffset = asr_get_FromFIFO(sc)) == EMPTY_QUEUE)
		 && ((ReplyOffset = asr_get_FromFIFO(sc)) == EMPTY_QUEUE)) {
			break;
		}
		Reply = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)(ReplyOffset
		  - sc->ha_Msgs_Phys + (char *)(sc->ha_Msgs));
		/*
		 * We do not need any (optional byteswapping) method access to
		 * the Initiator context field.
		 */
		ccb = (union asr_ccb *)(long)
		  I2O_MESSAGE_FRAME_getInitiatorContext64(
		    &(Reply->StdReplyFrame.StdMessageFrame));
		if (I2O_MESSAGE_FRAME_getMsgFlags(
		  &(Reply->StdReplyFrame.StdMessageFrame))
		  & I2O_MESSAGE_FLAGS_FAIL) {
			I2O_UTIL_NOP_MESSAGE	Message;
			PI2O_UTIL_NOP_MESSAGE	Message_Ptr;
			U32			MessageOffset;

			MessageOffset = (u_long)
			  I2O_FAILURE_REPLY_MESSAGE_FRAME_getPreservedMFA(
			    (PI2O_FAILURE_REPLY_MESSAGE_FRAME)Reply);
			/*
			 *  Get the Original Message Frame's address, and get
			 * it's Transaction Context into our space. (Currently
			 * unused at original authorship, but better to be
			 * safe than sorry). Straight copy means that we
			 * need not concern ourselves with the (optional
			 * byteswapping) method access.
			 */
			Reply->StdReplyFrame.TransactionContext =
			    bus_space_read_4(sc->ha_frame_btag,
			    sc->ha_frame_bhandle, MessageOffset +
			    offsetof(I2O_SINGLE_REPLY_MESSAGE_FRAME,
			    TransactionContext));
			/*
			 *	For 64 bit machines, we need to reconstruct the
			 * 64 bit context.
			 */
			ccb = (union asr_ccb *)(long)
			  I2O_MESSAGE_FRAME_getInitiatorContext64(
			    &(Reply->StdReplyFrame.StdMessageFrame));
			/*
			 * Unique error code for command failure.
			 */
			I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
			  &(Reply->StdReplyFrame), (u_int16_t)-2);
			/*
			 *  Modify the message frame to contain a NOP and
			 * re-issue it to the controller.
			 */
			Message_Ptr = (PI2O_UTIL_NOP_MESSAGE)ASR_fillMessage(
			    &Message, sizeof(I2O_UTIL_NOP_MESSAGE));
#if (I2O_UTIL_NOP != 0)
				I2O_MESSAGE_FRAME_setFunction (
				  &(Message_Ptr->StdMessageFrame),
				  I2O_UTIL_NOP);
#endif
			/*
			 *  Copy the packet out to the Original Message
			 */
			asr_set_frame(sc, Message_Ptr, MessageOffset,
				      sizeof(I2O_UTIL_NOP_MESSAGE));
			/*
			 *  Issue the NOP
			 */
			asr_set_ToFIFO(sc, MessageOffset);
		}

		/*
		 *	Asynchronous command with no return requirements,
		 * and a generic handler for immunity against odd error
		 * returns from the adapter.
		 */
		if (ccb == NULL) {
			/*
			 * Return Reply so that it can be used for the
			 * next command
			 */
			asr_set_FromFIFO(sc, ReplyOffset);
			continue;
		}

		/* Welease Wadjah! (and stop timeouts) */
		ASR_ccbRemove (sc, ccb);

		dsc = I2O_SINGLE_REPLY_MESSAGE_FRAME_getDetailedStatusCode(
		    &(Reply->StdReplyFrame));
		ccb->csio.scsi_status = dsc & I2O_SCSI_DEVICE_DSC_MASK;
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		switch (dsc) {

		case I2O_SCSI_DSC_SUCCESS:
			ccb->ccb_h.status |= CAM_REQ_CMP;
			break;

		case I2O_SCSI_DSC_CHECK_CONDITION:
			ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR |
			    CAM_AUTOSNS_VALID;
			break;

		case I2O_SCSI_DSC_BUSY:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_ADAPTER_BUSY:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_SCSI_BUS_RESET:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_BUS_BUSY:
			ccb->ccb_h.status |= CAM_SCSI_BUSY;
			break;

		case I2O_SCSI_HBA_DSC_SELECTION_TIMEOUT:
			ccb->ccb_h.status |= CAM_SEL_TIMEOUT;
			break;

		case I2O_SCSI_HBA_DSC_COMMAND_TIMEOUT:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_DEVICE_NOT_PRESENT:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_LUN_INVALID:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_SCSI_TID_INVALID:
			ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
			break;

		case I2O_SCSI_HBA_DSC_DATA_OVERRUN:
			/* FALLTHRU */
		case I2O_SCSI_HBA_DSC_REQUEST_LENGTH_ERROR:
			ccb->ccb_h.status |= CAM_DATA_RUN_ERR;
			break;

		default:
			ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			break;
		}
		if ((ccb->csio.resid = ccb->csio.dxfer_len) != 0) {
			ccb->csio.resid -=
			  I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getTransferCount(
			    Reply);
		}

		/* Sense data in reply packet */
		if (ccb->ccb_h.status & CAM_AUTOSNS_VALID) {
			u_int16_t size = I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getAutoSenseTransferCount(Reply);

			if (size) {
				if (size > sizeof(ccb->csio.sense_data)) {
					size = sizeof(ccb->csio.sense_data);
				}
				if (size > I2O_SCSI_SENSE_DATA_SZ) {
					size = I2O_SCSI_SENSE_DATA_SZ;
				}
				if ((ccb->csio.sense_len)
				 && (size > ccb->csio.sense_len)) {
					size = ccb->csio.sense_len;
				}
				bcopy(Reply->SenseData,
				      &(ccb->csio.sense_data), size);
			}
		}

		/*
		 * Return Reply so that it can be used for the next command
		 * since we have no more need for it now
		 */
		asr_set_FromFIFO(sc, ReplyOffset);

		if (ccb->ccb_h.path) {
			xpt_done ((union ccb *)ccb);
		} else {
			wakeup (ccb);
		}
	}
	return (processed);
} /* asr_intr */

#undef QueueSize	/* Grrrr */
#undef SG_Size		/* Grrrr */

/*
 *	Meant to be included at the bottom of asr.c !!!
 */

/*
 *	Included here as hard coded. Done because other necessary include
 *	files utilize C++ comment structures which make them a nuisance to
 *	included here just to pick up these three typedefs.
 */
typedef U32   DPT_TAG_T;
typedef U32   DPT_MSG_T;
typedef U32   DPT_RTN_T;

#undef SCSI_RESET	/* Conflicts with "scsi/scsiconf.h" defintion */
#include	"dev/asr/osd_unix.h"

#define	asr_unit(dev)	  minor(dev)

static u_int8_t ASR_ctlr_held;

static int
asr_open(struct cdev *dev, int32_t flags, int32_t ifmt, struct thread *td)
{
	int		 s;
	int		 error;

	if (dev->si_drv1 == NULL) {
		return (ENODEV);
	}
	s = splcam ();
	if (ASR_ctlr_held) {
		error = EBUSY;
	} else if ((error = suser(td)) == 0) {
		++ASR_ctlr_held;
	}
	splx(s);
	return (error);
} /* asr_open */

static int
asr_close(struct cdev *dev, int flags, int ifmt, struct thread *td)
{

	ASR_ctlr_held = 0;
	return (0);
} /* asr_close */


/*-------------------------------------------------------------------------*/
/*		      Function ASR_queue_i				   */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :				   */
/*     Asr_softc_t *	  : HBA miniport driver's adapter data storage.	   */
/*     PI2O_MESSAGE_FRAME : Msg Structure Pointer For This Command	   */
/*	I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME following the Msg Structure	   */
/*									   */
/* This Function Will Take The User Request Packet And Convert It To An	   */
/* I2O MSG And Send It Off To The Adapter.				   */
/*									   */
/* Return : 0 For OK, Error Code Otherwise				   */
/*-------------------------------------------------------------------------*/
static int
ASR_queue_i(Asr_softc_t	*sc, PI2O_MESSAGE_FRAME	Packet)
{
	union asr_ccb				   * ccb;
	PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME	     Reply;
	PI2O_MESSAGE_FRAME			     Message_Ptr;
	PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME	     Reply_Ptr;
	int					     MessageSizeInBytes;
	int					     ReplySizeInBytes;
	int					     error;
	int					     s;
	/* Scatter Gather buffer list */
	struct ioctlSgList_S {
		SLIST_ENTRY(ioctlSgList_S) link;
		caddr_t			   UserSpace;
		I2O_FLAGS_COUNT		   FlagsCount;
		char			   KernelSpace[sizeof(long)];
	}					   * elm;
	/* Generates a `first' entry */
	SLIST_HEAD(ioctlSgListHead_S, ioctlSgList_S) sgList;

	if (ASR_getBlinkLedCode(sc)) {
		debug_usr_cmd_printf ("Adapter currently in BlinkLed %x\n",
		  ASR_getBlinkLedCode(sc));
		return (EIO);
	}
	/* Copy in the message into a local allocation */
	if ((Message_Ptr = (PI2O_MESSAGE_FRAME)malloc (
	  sizeof(I2O_MESSAGE_FRAME), M_TEMP, M_WAITOK)) == NULL) {
		debug_usr_cmd_printf (
		  "Failed to acquire I2O_MESSAGE_FRAME memory\n");
		return (ENOMEM);
	}
	if ((error = copyin ((caddr_t)Packet, (caddr_t)Message_Ptr,
	  sizeof(I2O_MESSAGE_FRAME))) != 0) {
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf ("Can't copy in packet errno=%d\n", error);
		return (error);
	}
	/* Acquire information to determine type of packet */
	MessageSizeInBytes = (I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr)<<2);
	/* The offset of the reply information within the user packet */
	Reply = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)((char *)Packet
	  + MessageSizeInBytes);

	/* Check if the message is a synchronous initialization command */
	s = I2O_MESSAGE_FRAME_getFunction(Message_Ptr);
	free(Message_Ptr, M_TEMP);
	switch (s) {

	case I2O_EXEC_IOP_RESET:
	{	U32 status;

		status = ASR_resetIOP(sc);
		ReplySizeInBytes = sizeof(status);
		debug_usr_cmd_printf ("resetIOP done\n");
		return (copyout ((caddr_t)&status, (caddr_t)Reply,
		  ReplySizeInBytes));
	}

	case I2O_EXEC_STATUS_GET:
	{	I2O_EXEC_STATUS_GET_REPLY status;

		if (ASR_getStatus(sc, &status) == NULL) {
			debug_usr_cmd_printf ("getStatus failed\n");
			return (ENXIO);
		}
		ReplySizeInBytes = sizeof(status);
		debug_usr_cmd_printf ("getStatus done\n");
		return (copyout ((caddr_t)&status, (caddr_t)Reply,
		  ReplySizeInBytes));
	}

	case I2O_EXEC_OUTBOUND_INIT:
	{	U32 status;

		status = ASR_initOutBound(sc);
		ReplySizeInBytes = sizeof(status);
		debug_usr_cmd_printf ("intOutBound done\n");
		return (copyout ((caddr_t)&status, (caddr_t)Reply,
		  ReplySizeInBytes));
	}
	}

	/* Determine if the message size is valid */
	if ((MessageSizeInBytes < sizeof(I2O_MESSAGE_FRAME))
	 || (MAX_INBOUND_SIZE < MessageSizeInBytes)) {
		debug_usr_cmd_printf ("Packet size %d incorrect\n",
		  MessageSizeInBytes);
		return (EINVAL);
	}

	if ((Message_Ptr = (PI2O_MESSAGE_FRAME)malloc (MessageSizeInBytes,
	  M_TEMP, M_WAITOK)) == NULL) {
		debug_usr_cmd_printf ("Failed to acquire frame[%d] memory\n",
		  MessageSizeInBytes);
		return (ENOMEM);
	}
	if ((error = copyin ((caddr_t)Packet, (caddr_t)Message_Ptr,
	  MessageSizeInBytes)) != 0) {
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf ("Can't copy in packet[%d] errno=%d\n",
		  MessageSizeInBytes, error);
		return (error);
	}

	/* Check the size of the reply frame, and start constructing */

	if ((Reply_Ptr = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)malloc (
	  sizeof(I2O_MESSAGE_FRAME), M_TEMP, M_WAITOK)) == NULL) {
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf (
		  "Failed to acquire I2O_MESSAGE_FRAME memory\n");
		return (ENOMEM);
	}
	if ((error = copyin ((caddr_t)Reply, (caddr_t)Reply_Ptr,
	  sizeof(I2O_MESSAGE_FRAME))) != 0) {
		free(Reply_Ptr, M_TEMP);
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf (
		  "Failed to copy in reply frame, errno=%d\n",
		  error);
		return (error);
	}
	ReplySizeInBytes = (I2O_MESSAGE_FRAME_getMessageSize(
	  &(Reply_Ptr->StdReplyFrame.StdMessageFrame)) << 2);
	free(Reply_Ptr, M_TEMP);
	if (ReplySizeInBytes < sizeof(I2O_SINGLE_REPLY_MESSAGE_FRAME)) {
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf (
		  "Failed to copy in reply frame[%d], errno=%d\n",
		  ReplySizeInBytes, error);
		return (EINVAL);
	}

	if ((Reply_Ptr = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)malloc (
	  ((ReplySizeInBytes > sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME))
	    ? ReplySizeInBytes : sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)),
	  M_TEMP, M_WAITOK)) == NULL) {
		free(Message_Ptr, M_TEMP);
		debug_usr_cmd_printf ("Failed to acquire frame[%d] memory\n",
		  ReplySizeInBytes);
		return (ENOMEM);
	}
	(void)ASR_fillMessage((void *)Reply_Ptr, ReplySizeInBytes);
	Reply_Ptr->StdReplyFrame.StdMessageFrame.InitiatorContext
	  = Message_Ptr->InitiatorContext;
	Reply_Ptr->StdReplyFrame.TransactionContext
	  = ((PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr)->TransactionContext;
	I2O_MESSAGE_FRAME_setMsgFlags(
	  &(Reply_Ptr->StdReplyFrame.StdMessageFrame),
	  I2O_MESSAGE_FRAME_getMsgFlags(
	    &(Reply_Ptr->StdReplyFrame.StdMessageFrame))
	      | I2O_MESSAGE_FLAGS_REPLY);

	/* Check if the message is a special case command */
	switch (I2O_MESSAGE_FRAME_getFunction(Message_Ptr)) {
	case I2O_EXEC_SYS_TAB_SET: /* Special Case of empty Scatter Gather */
		if (MessageSizeInBytes == ((I2O_MESSAGE_FRAME_getVersionOffset(
		  Message_Ptr) & 0xF0) >> 2)) {
			free(Message_Ptr, M_TEMP);
			I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
			  &(Reply_Ptr->StdReplyFrame),
			  (ASR_setSysTab(sc) != CAM_REQ_CMP));
			I2O_MESSAGE_FRAME_setMessageSize(
			  &(Reply_Ptr->StdReplyFrame.StdMessageFrame),
			  sizeof(I2O_SINGLE_REPLY_MESSAGE_FRAME));
			error = copyout ((caddr_t)Reply_Ptr, (caddr_t)Reply,
			  ReplySizeInBytes);
			free(Reply_Ptr, M_TEMP);
			return (error);
		}
	}

	/* Deal in the general case */
	/* First allocate and optionally copy in each scatter gather element */
	SLIST_INIT(&sgList);
	if ((I2O_MESSAGE_FRAME_getVersionOffset(Message_Ptr) & 0xF0) != 0) {
		PI2O_SGE_SIMPLE_ELEMENT sg;

		/*
		 *	since this code is reused in several systems, code
		 * efficiency is greater by using a shift operation rather
		 * than a divide by sizeof(u_int32_t).
		 */
		sg = (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
		  + ((I2O_MESSAGE_FRAME_getVersionOffset(Message_Ptr) & 0xF0)
		    >> 2));
		while (sg < (PI2O_SGE_SIMPLE_ELEMENT)(((caddr_t)Message_Ptr)
		  + MessageSizeInBytes)) {
			caddr_t v;
			int	len;

			if ((I2O_FLAGS_COUNT_getFlags(&(sg->FlagsCount))
			 & I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT) == 0) {
				error = EINVAL;
				break;
			}
			len = I2O_FLAGS_COUNT_getCount(&(sg->FlagsCount));
			debug_usr_cmd_printf ("SG[%d] = %x[%d]\n",
			  sg - (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
			  + ((I2O_MESSAGE_FRAME_getVersionOffset(
				Message_Ptr) & 0xF0) >> 2)),
			  I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg), len);

			if ((elm = (struct ioctlSgList_S *)malloc (
			  sizeof(*elm) - sizeof(elm->KernelSpace) + len,
			  M_TEMP, M_WAITOK)) == NULL) {
				debug_usr_cmd_printf (
				  "Failed to allocate SG[%d]\n", len);
				error = ENOMEM;
				break;
			}
			SLIST_INSERT_HEAD(&sgList, elm, link);
			elm->FlagsCount = sg->FlagsCount;
			elm->UserSpace = (caddr_t)
			  (I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg));
			v = elm->KernelSpace;
			/* Copy in outgoing data (DIR bit could be invalid) */
			if ((error = copyin (elm->UserSpace, (caddr_t)v, len))
			  != 0) {
				break;
			}
			/*
			 *	If the buffer is not contiguous, lets
			 * break up the scatter/gather entries.
			 */
			while ((len > 0)
			 && (sg < (PI2O_SGE_SIMPLE_ELEMENT)
			  (((caddr_t)Message_Ptr) + MAX_INBOUND_SIZE))) {
				int next, base, span;

				span = 0;
				next = base = KVTOPHYS(v);
				I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg,
				  base);

				/* How far can we go physically contiguously */
				while ((len > 0) && (base == next)) {
					int size;

					next = trunc_page(base) + PAGE_SIZE;
					size = next - base;
					if (size > len) {
						size = len;
					}
					span += size;
					v += size;
					len -= size;
					base = KVTOPHYS(v);
				}

				/* Construct the Flags */
				I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount),
				  span);
				{
					int flags = I2O_FLAGS_COUNT_getFlags(
					  &(elm->FlagsCount));
					/* Any remaining length? */
					if (len > 0) {
					    flags &=
						~(I2O_SGL_FLAGS_END_OF_BUFFER
						 | I2O_SGL_FLAGS_LAST_ELEMENT);
					}
					I2O_FLAGS_COUNT_setFlags(
					  &(sg->FlagsCount), flags);
				}

				debug_usr_cmd_printf ("sg[%d] = %x[%d]\n",
				  sg - (PI2O_SGE_SIMPLE_ELEMENT)
				    ((char *)Message_Ptr
				  + ((I2O_MESSAGE_FRAME_getVersionOffset(
					Message_Ptr) & 0xF0) >> 2)),
				  I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg),
				  span);
				if (len <= 0) {
					break;
				}

				/*
				 * Incrementing requires resizing of the
				 * packet, and moving up the existing SG
				 * elements.
				 */
				++sg;
				MessageSizeInBytes += sizeof(*sg);
				I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
				  I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr)
				  + (sizeof(*sg) / sizeof(U32)));
				{
					PI2O_MESSAGE_FRAME NewMessage_Ptr;

					if ((NewMessage_Ptr
					  = (PI2O_MESSAGE_FRAME)
					    malloc (MessageSizeInBytes,
					     M_TEMP, M_WAITOK)) == NULL) {
						debug_usr_cmd_printf (
						  "Failed to acquire frame[%d] memory\n",
						  MessageSizeInBytes);
						error = ENOMEM;
						break;
					}
					span = ((caddr_t)sg)
					     - (caddr_t)Message_Ptr;
					bcopy(Message_Ptr,NewMessage_Ptr, span);
					bcopy((caddr_t)(sg-1),
					  ((caddr_t)NewMessage_Ptr) + span,
					  MessageSizeInBytes - span);
					free(Message_Ptr, M_TEMP);
					sg = (PI2O_SGE_SIMPLE_ELEMENT)
					  (((caddr_t)NewMessage_Ptr) + span);
					Message_Ptr = NewMessage_Ptr;
				}
			}
			if ((error)
			 || ((I2O_FLAGS_COUNT_getFlags(&(sg->FlagsCount))
			  & I2O_SGL_FLAGS_LAST_ELEMENT) != 0)) {
				break;
			}
			++sg;
		}
		if (error) {
			while ((elm = SLIST_FIRST(&sgList)) != NULL) {
				SLIST_REMOVE_HEAD(&sgList, link);
				free(elm, M_TEMP);
			}
			free(Reply_Ptr, M_TEMP);
			free(Message_Ptr, M_TEMP);
			return (error);
		}
	}

	debug_usr_cmd_printf ("Inbound: ");
	debug_usr_cmd_dump_message(Message_Ptr);

	/* Send the command */
	if ((ccb = asr_alloc_ccb (sc)) == NULL) {
		/* Free up in-kernel buffers */
		while ((elm = SLIST_FIRST(&sgList)) != NULL) {
			SLIST_REMOVE_HEAD(&sgList, link);
			free(elm, M_TEMP);
		}
		free(Reply_Ptr, M_TEMP);
		free(Message_Ptr, M_TEMP);
		return (ENOMEM);
	}

	/*
	 * We do not need any (optional byteswapping) method access to
	 * the Initiator context field.
	 */
	I2O_MESSAGE_FRAME_setInitiatorContext64(
	  (PI2O_MESSAGE_FRAME)Message_Ptr, (long)ccb);

	(void)ASR_queue (sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

	free(Message_Ptr, M_TEMP);

	/*
	 * Wait for the board to report a finished instruction.
	 */
	s = splcam();
	while ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
		if (ASR_getBlinkLedCode(sc)) {
			/* Reset Adapter */
			printf ("asr%d: Blink LED 0x%x resetting adapter\n",
			  cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)),
			  ASR_getBlinkLedCode(sc));
			if (ASR_reset (sc) == ENXIO) {
				/* Command Cleanup */
				ASR_ccbRemove(sc, ccb);
			}
			splx(s);
			/* Free up in-kernel buffers */
			while ((elm = SLIST_FIRST(&sgList)) != NULL) {
				SLIST_REMOVE_HEAD(&sgList, link);
				free(elm, M_TEMP);
			}
			free(Reply_Ptr, M_TEMP);
			asr_free_ccb(ccb);
			return (EIO);
		}
		/* Check every second for BlinkLed */
		/* There is no PRICAM, but outwardly PRIBIO is functional */
		tsleep(ccb, PRIBIO, "asr", hz);
	}
	splx(s);

	debug_usr_cmd_printf ("Outbound: ");
	debug_usr_cmd_dump_message(Reply_Ptr);

	I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
	  &(Reply_Ptr->StdReplyFrame),
	  (ccb->ccb_h.status != CAM_REQ_CMP));

	if (ReplySizeInBytes >= (sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
	  - I2O_SCSI_SENSE_DATA_SZ - sizeof(U32))) {
		I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setTransferCount(Reply_Ptr,
		  ccb->csio.dxfer_len - ccb->csio.resid);
	}
	if ((ccb->ccb_h.status & CAM_AUTOSNS_VALID) && (ReplySizeInBytes
	 > (sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
	 - I2O_SCSI_SENSE_DATA_SZ))) {
		int size = ReplySizeInBytes
		  - sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
		  - I2O_SCSI_SENSE_DATA_SZ;

		if (size > sizeof(ccb->csio.sense_data)) {
			size = sizeof(ccb->csio.sense_data);
		}
		bcopy(&(ccb->csio.sense_data), Reply_Ptr->SenseData, size);
		I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setAutoSenseTransferCount(
		    Reply_Ptr, size);
	}

	/* Free up in-kernel buffers */
	while ((elm = SLIST_FIRST(&sgList)) != NULL) {
		/* Copy out as necessary */
		if ((error == 0)
		/* DIR bit considered `valid', error due to ignorance works */
		 && ((I2O_FLAGS_COUNT_getFlags(&(elm->FlagsCount))
		  & I2O_SGL_FLAGS_DIR) == 0)) {
			error = copyout((caddr_t)(elm->KernelSpace),
			  elm->UserSpace,
			  I2O_FLAGS_COUNT_getCount(&(elm->FlagsCount)));
		}
		SLIST_REMOVE_HEAD(&sgList, link);
		free(elm, M_TEMP);
	}
	if (error == 0) {
	/* Copy reply frame to user space */
		error = copyout((caddr_t)Reply_Ptr, (caddr_t)Reply,
				ReplySizeInBytes);
	}
	free(Reply_Ptr, M_TEMP);
	asr_free_ccb(ccb);

	return (error);
} /* ASR_queue_i */

/*----------------------------------------------------------------------*/
/*			    Function asr_ioctl			       */
/*----------------------------------------------------------------------*/
/* The parameters passed to this function are :				*/
/*     dev  : Device number.						*/
/*     cmd  : Ioctl Command						*/
/*     data : User Argument Passed In.					*/
/*     flag : Mode Parameter						*/
/*     proc : Process Parameter						*/
/*									*/
/* This function is the user interface into this adapter driver		*/
/*									*/
/* Return : zero if OK, error code if not				*/
/*----------------------------------------------------------------------*/

static int
asr_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	Asr_softc_t	*sc = dev->si_drv1;
	int		i, error = 0;
#ifdef ASR_IOCTL_COMPAT
	int		j;
#endif /* ASR_IOCTL_COMPAT */

	if (sc != NULL)
	switch(cmd) {

	case DPT_SIGNATURE:
#ifdef ASR_IOCTL_COMPAT
#if (dsDescription_size != 50)
	case DPT_SIGNATURE + ((50 - dsDescription_size) << 16):
#endif
		if (cmd & 0xFFFF0000) {
			bcopy(&ASR_sig, data, sizeof(dpt_sig_S));
			return (0);
		}
	/* Traditional version of the ioctl interface */
	case DPT_SIGNATURE & 0x0000FFFF:
#endif
		return (copyout((caddr_t)(&ASR_sig), *((caddr_t *)data),
				sizeof(dpt_sig_S)));

	/* Traditional version of the ioctl interface */
	case DPT_CTRLINFO & 0x0000FFFF:
	case DPT_CTRLINFO: {
		struct {
			u_int16_t length;
			u_int16_t drvrHBAnum;
			u_int32_t baseAddr;
			u_int16_t blinkState;
			u_int8_t  pciBusNum;
			u_int8_t  pciDeviceNum;
			u_int16_t hbaFlags;
			u_int16_t Interrupt;
			u_int32_t reserved1;
			u_int32_t reserved2;
			u_int32_t reserved3;
		} CtlrInfo;

		bzero(&CtlrInfo, sizeof(CtlrInfo));
		CtlrInfo.length = sizeof(CtlrInfo) - sizeof(u_int16_t);
		CtlrInfo.drvrHBAnum = asr_unit(dev);
		CtlrInfo.baseAddr = sc->ha_Base;
		i = ASR_getBlinkLedCode (sc);
		if (i == -1)
			i = 0;

		CtlrInfo.blinkState = i;
		CtlrInfo.pciBusNum = sc->ha_pciBusNum;
		CtlrInfo.pciDeviceNum = sc->ha_pciDeviceNum;
#define	FLG_OSD_PCI_VALID 0x0001
#define	FLG_OSD_DMA	  0x0002
#define	FLG_OSD_I2O	  0x0004
		CtlrInfo.hbaFlags = FLG_OSD_PCI_VALID|FLG_OSD_DMA|FLG_OSD_I2O;
		CtlrInfo.Interrupt = sc->ha_irq;
#ifdef ASR_IOCTL_COMPAT
		if (cmd & 0xffff0000)
			bcopy(&CtlrInfo, data, sizeof(CtlrInfo));
		else
#endif /* ASR_IOCTL_COMPAT */
		error = copyout(&CtlrInfo, *(caddr_t *)data, sizeof(CtlrInfo));
	}	return (error);

	/* Traditional version of the ioctl interface */
	case DPT_SYSINFO & 0x0000FFFF:
	case DPT_SYSINFO: {
		sysInfo_S	Info;
#ifdef ASR_IOCTL_COMPAT
		char	      * cp;
		/* Kernel Specific ptok `hack' */
#define		ptok(a) ((char *)(uintptr_t)(a) + KERNBASE)

		bzero(&Info, sizeof(Info));

		/* Appears I am the only person in the Kernel doing this */
		outb (0x70, 0x12);
		i = inb(0x71);
		j = i >> 4;
		if (i == 0x0f) {
			outb (0x70, 0x19);
			j = inb (0x71);
		}
		Info.drive0CMOS = j;

		j = i & 0x0f;
		if (i == 0x0f) {
			outb (0x70, 0x1a);
			j = inb (0x71);
		}
		Info.drive1CMOS = j;

		Info.numDrives = *((char *)ptok(0x475));
#endif /* ASR_IOCTL_COMPAT */

		bzero(&Info, sizeof(Info));

		Info.processorFamily = ASR_sig.dsProcessorFamily;
#if defined(__i386__)
		switch (cpu) {
		case CPU_386SX: case CPU_386:
			Info.processorType = PROC_386; break;
		case CPU_486SX: case CPU_486:
			Info.processorType = PROC_486; break;
		case CPU_586:
			Info.processorType = PROC_PENTIUM; break;
		case CPU_686:
			Info.processorType = PROC_SEXIUM; break;
		}
#elif defined(__alpha__)
		Info.processorType = PROC_ALPHA;
#endif

		Info.osType = OS_BSDI_UNIX;
		Info.osMajorVersion = osrelease[0] - '0';
		Info.osMinorVersion = osrelease[2] - '0';
		/* Info.osRevision = 0; */
		/* Info.osSubRevision = 0; */
		Info.busType = SI_PCI_BUS;
		Info.flags = SI_OSversionValid|SI_BusTypeValid|SI_NO_SmartROM;

#ifdef ASR_IOCTL_COMPAT
		Info.flags |= SI_CMOS_Valid | SI_NumDrivesValid;
		/* Go Out And Look For I2O SmartROM */
		for(j = 0xC8000; j < 0xE0000; j += 2048) {
			int k;

			cp = ptok(j);
			if (*((unsigned short *)cp) != 0xAA55) {
				continue;
			}
			j += (cp[2] * 512) - 2048;
			if ((*((u_long *)(cp + 6))
			  != ('S' + (' ' * 256) + (' ' * 65536L)))
			 || (*((u_long *)(cp + 10))
			  != ('I' + ('2' * 256) + ('0' * 65536L)))) {
				continue;
			}
			cp += 0x24;
			for (k = 0; k < 64; ++k) {
				if (*((unsigned short *)cp)
				 == (' ' + ('v' * 256))) {
					break;
				}
			}
			if (k < 64) {
				Info.smartROMMajorVersion
				    = *((unsigned char *)(cp += 4)) - '0';
				Info.smartROMMinorVersion
				    = *((unsigned char *)(cp += 2));
				Info.smartROMRevision
				    = *((unsigned char *)(++cp));
				Info.flags |= SI_SmartROMverValid;
				Info.flags &= ~SI_NO_SmartROM;
				break;
			}
		}
		/* Get The Conventional Memory Size From CMOS */
		outb (0x70, 0x16);
		j = inb (0x71);
		j <<= 8;
		outb (0x70, 0x15);
		j |= inb(0x71);
		Info.conventionalMemSize = j;

		/* Get The Extended Memory Found At Power On From CMOS */
		outb (0x70, 0x31);
		j = inb (0x71);
		j <<= 8;
		outb (0x70, 0x30);
		j |= inb(0x71);
		Info.extendedMemSize = j;
		Info.flags |= SI_MemorySizeValid;

		/* Copy Out The Info Structure To The User */
		if (cmd & 0xFFFF0000)
			bcopy(&Info, data, sizeof(Info));
		else
#endif /* ASR_IOCTL_COMPAT */
		error = copyout(&Info, *(caddr_t *)data, sizeof(Info));
		return (error); }

		/* Get The BlinkLED State */
	case DPT_BLINKLED:
		i = ASR_getBlinkLedCode (sc);
		if (i == -1)
			i = 0;
#ifdef ASR_IOCTL_COMPAT
		if (cmd & 0xffff0000)
			bcopy(&i, data, sizeof(i));
		else
#endif /* ASR_IOCTL_COMPAT */
		error = copyout(&i, *(caddr_t *)data, sizeof(i));
		break;

		/* Send an I2O command */
	case I2OUSRCMD:
		return (ASR_queue_i(sc, *((PI2O_MESSAGE_FRAME *)data)));

		/* Reset and re-initialize the adapter */
	case I2ORESETCMD:
		return (ASR_reset(sc));

		/* Rescan the LCT table and resynchronize the information */
	case I2ORESCANCMD:
		return (ASR_rescan(sc));
	}
	return (EINVAL);
} /* asr_ioctl */

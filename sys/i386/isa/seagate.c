/*
 * (Free/Net/386)BSD ST01/02, Future Domain TMC-885, TMC-950 SCSI driver for
 * Julians SCSI-code
 *
 * Copyright 1994, Kent Palmkvist (kentp@isy.liu.se)
 * Copyright 1994, Robert Knier (rknier@qgraph.com)
 * Copyright 1992, 1994 Drew Eckhardt (drew@colorado.edu)
 * Copyright 1994, Julian Elischer (julian@tfs.com)
 * Copyright 1994, Serge Vakulenko (vak@cronyx.ru)
 *
 * Others that has contributed by example code is
 * 		Glen Overby (overby@cray.com)
 *		Tatu Yllnen
 *		Brian E Litzinger
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * kentp  940307 alpha version based on newscsi-03 version of Julians SCSI-code
 * kentp  940314 Added possibility to not use messages
 * rknier 940331 Added fast transfer code
 * rknier 940407 Added assembler coded data transfers
 * vak    941226 New probe algorithm, based on expected behaviour
 *               instead of BIOS signatures analysis, better timeout handling,
 *               new asm fragments for data input/output, target-dependent
 *               delays, device flags, polling mode, generic cleanup
 *
 * $Id: seagate.c,v 1.3 1994/06/16 13:26:14 sean Exp $
 */

/*
 * What should really be done:
 *
 * Restructure interrupt enable/disable code (runs too long with int disabled)
 * Find bug? giving problem with tape status
 * Add code to handle Future Domain 840, 841, 880 and 881
 * add code to use tagged commands in SCSI2
 * Add code to handle slow devices better (sleep if device not disconnecting)
 * Fix unnecessary interrupts
 */

/* Note to users trying to share a disk between DOS and unix:
 * The ST01/02 is a translating host-adapter. It is not giving DOS
 * the same number of heads/tracks/sectors as specified by the disk.
 * It is therefore important to look at what numbers DOS thinks the
 * disk has. Use these to disklabel your disk in an appropriate manner
 *
 * About ST02+IDE coexistence: the original Seagate ST02
 * BIOS cannot coexist with IDE or any other disk controller
 * because it does not share BIOS disk drive numbers (80h, 81h)
 * with others.  New probing code allows using ST02 controller
 * without BIOS: just unplug the ST02 BIOS chip from the board.
 *
 * Another problem is the floppy adapter on ST02 which could not be
 * disabled by jumpers.  I commonly use ST02 adapter as a cheap solution
 * for atttaching the tape and CD-ROM drives, and an extra floppy controller
 * is just a headache.  I found a simple workaround: cutting off
 * the AEN signal (A11 contact on ISA connector).  AEN then goes high and
 * disables the floppy adapter port address decoder.
 *
 * I also had a problem with ST02 conflicting with IDE during
 * IDE data write phase.  It seems than ST02 makes some noise
 * on /IOW line.  The /IOW line is used only for floppy controller
 * part of ST02, and because I don't need it, I cut off the /IOW (contact B13)
 * and it helped. (vak)
 *
 * Tested on the following hardware:
 *   Adapter: Seagate ST02
 *      Disk: HP D1686
 * Streamers: Archive Viper 150, Wangtek 5525
 *   CD-ROMs: Toshiba XM-3401, NEC CDR-25
 *
 * Maximum data rate is about 270-280 kbytes/sec (on 386DX/40).
 * (vak)
 */
#undef DEBUG

#include "sea.h"
#if NSEA > 0

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/devconf.h>
#include <i386/isa/isa_device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef DEBUG
#   define PRINT(s)     printf s
#else
#   define PRINT(s)     /*void*/
#endif

#define	SEA_SCB_MAX	8	/* allow maximally 8 scsi control blocks */
#define SCB_TABLE_SIZE	8	/* start with 8 scb entries in table */
#define BLOCK_SIZE	512	/* size of READ/WRITE areas on SCSI card */
#define SEA_SCSI_ADDR   7       /* address of the adapter on the SCSI bus */

/*
 * Defice config flags
 */
#define FLAG_NOPARITY   0x01    /* disable SCSI bus parity check */
#define FLAG_SENSEFIRST 0x02    /* place REQUEST_SENSE ops ahead of queue */

/*
 * Board CONTROL register
 */
#define CMD_RST		0x01		/* scsi reset */
#define CMD_SEL		0x02		/* scsi select */
#define CMD_BSY		0x04		/* scsi busy */
#define	CMD_ATTN	0x08		/* scsi attention */
#define CMD_START_ARB	0x10		/* start arbitration bit */
#define	CMD_EN_PARITY	0x20		/* enable scsi parity generation */
#define CMD_INTR	0x40		/* enable scsi interrupts */
#define CMD_DRVR_ENABLE	0x80		/* scsi enable */

/*
 * Board STATUS register
 */
#define STAT_BSY	0x01		/* scsi busy */
#define STAT_MSG	0x02		/* scsi msg */
#define STAT_IO		0x04		/* scsi I/O */
#define STAT_CD		0x08		/* scsi C/D */
#define STAT_REQ	0x10		/* scsi req */
#define STAT_SEL	0x20		/* scsi select */
#define STAT_PARITY	0x40		/* parity error bit */
#define STAT_ARB_CMPL	0x80		/* arbitration complete bit */
#define STAT_BITS       "\20\1bsy\2msg\3i/o\4c/d\5req\6sel\7parity\10arb"

/*
 * SCSI bus requests
 */
#define REQ_MASK        (STAT_MSG | STAT_CD | STAT_IO)
#define REQ_DATAOUT     0
#define REQ_DATAIN      STAT_IO
#define REQ_CMDOUT      STAT_CD
#define REQ_STATIN      (STAT_CD | STAT_IO)
#define REQ_MSGOUT      (STAT_MSG | STAT_CD)
#define REQ_MSGIN       (STAT_MSG | STAT_CD | STAT_IO)

#define REQ_UNKNOWN	0xff

static char *sea_phase_name[] = {
	"DATAOUT", "Phase1?",  "Phase2?", "Phase3?",
	"DATAIN",  "Phase5?",  "Phase6?", "Phase7?",
	"CMDOUT",  "Phase9?",  "MSGOUT",  "Phase11?",
	"STATIN",  "Phase13?", "MSGIN",   "Phase15?",
};

/*
 * SCSI message codes
 */
#define MSG_COMMAND_COMPLETE	0x00
#define MSG_SAVE_POINTERS	0x02
#define MSG_RESTORE_POINTERS	0x03
#define MSG_DISCONNECT          0x04
#define MSG_ABORT               0x06
#define	MSG_MESSAGE_REJECT	0x07
#define MSG_NOP                 0x08
#define MSG_BUS_DEV_RESET	0x0c
#define MSG_IDENTIFY(lun)       (0xc0 | ((lun) & 0x7))
#define MSG_ISIDENT(m)          ((m) & 0x80)

/*
 * SCSI control block used to keep info about a scsi command
 */
struct sea_scb {
	int flags;                      /* status of the instruction */
#define	SCB_FREE	0
#define	SCB_ACTIVE	1
#define SCB_ABORTED	2
#define SCB_TIMEOUT	4
#define SCB_ERROR	8
#define	SCB_TIMECHK	16		/* We have set a timeout on this one */
	struct sea_scb *next;		/* in free list */
	struct scsi_xfer *xfer;		/* the scsi_xfer for this cmd */
	u_char *data;                   /* position in data buffer so far */
	int32 datalen;			/* bytes remaining to transfer */;
};

typedef enum {
	CTLR_NONE,
	CTLR_SEAGATE,
	CTLR_FUTURE_DOMAIN,
} ctlr_t;

char *sea_name[] = {
	"Unknown",
	"Seagate ST01/ST02",
	"Future Domain TMC-885/TMC-950",
};

/*
 * Flags for waiting for REQ deassert during some SCSI bus phases.
 */
struct sea_phase {
	unsigned cmdout1 : 1;   /* after CMDOUT[0] byte */
	unsigned cmdout : 1;    /* after CMDOUT[1..N] bytes */
	unsigned msgout : 1;    /* after MSGOUT byte */
	unsigned statin : 1;    /* after STATIN byte */
};

/*
 * Data structure describing the target state.
 */
struct sea_target {
	struct sea_data *adapter;       /* pointer to the adapter structure */
	volatile u_char busy;           /* mask of busy luns at device target */
	u_long perrcnt;                 /* counter of target parity errors */
	struct sea_phase ndelay;        /* "don't delay" flags */
	struct sea_phase init;          /* "initialized" flags */
};

/*
 * Data structure describing current status of the scsi bus. One for each
 * controller card.
 */
struct sea_data {
	ctlr_t  type;                   /* Seagate or Future Domain */
	volatile u_char *addr;          /* base address for card */
	volatile u_char *CONTROL;       /* address of control register */
	volatile u_char *STATUS;        /* address of status register */
	volatile u_char *DATA;          /* address of data register */

	u_char  scsi_addr;              /* our scsi address, 0..7 */
	u_char  scsi_id;                /* our scsi id mask */
	u_char  parity;                 /* parity flag: CMD_EN_PARITY or 0 */
	u_char  irq;                    /* IRQ number used or 0 if no IRQ */
	unsigned sensefirst : 1;        /* place REQUEST_SENSE ops ahead */
	unsigned timeout_active : 1;    /* sea_timeout active (requested) */

	struct scsi_link sc_link;	/* struct connecting different data */
	struct sea_scb *connected;	/* currently connected command */
	struct sea_scb *issue_queue;	/* waiting to be issued */
	struct sea_scb *disconnected_queue; /* waiting to reconnect */

	int     numscb;                 /* number of scsi control blocks */
	struct sea_scb scbs[SCB_TABLE_SIZE];
	struct sea_scb *free_scb;	/* free scb list */
	struct sea_target target[8];    /* target state data */
} seadata[NSEA];

#define IS_BUSY(a,b)    ((a)->target[(b)->xfer->sc_link->target].busy &\
				(1 << (b)->xfer->sc_link->lun))
#define SET_BUSY(a,b)   ((a)->target[(b)->xfer->sc_link->target].busy |=\
				(1 << (b)->xfer->sc_link->lun))
#define CLEAR_BUSY(a,b) ((a)->target[(b)->xfer->sc_link->target].busy &=\
				~(1 << (b)->xfer->sc_link->lun))

/*
 * Wait for condition, given as an boolean expression.
 * Print the message on timeout (approx 10 msec).
 */
#define WAITFOR(condition,message) {\
	register long cnt = 100000; char *msg = message;\
	while (cnt-- && ! (condition)) continue;\
	if (cnt == -1 && msg)\
		printf ("sea: timeout waiting for %s\n", msg); }

/*
 * Seagate adapter does not support in hardware
 * waiting for REQ deassert after transferring each data byte.
 * We must do it in software.
 * The problem is that some SCSI devices deassert REQ so fast that
 * we can miss it.  We the flag for each target sayind if we should (not)
 * wait for REQ deassert.  This flag is initialized when the first
 * operation on the target is done.
 * 1) Test if we don't need to wait for REQ deassert (`nodelay' flag).
 *    Initially the flag is off, i.e. wait.  If the flag is set,
 *    go to the step 4.
 * 2) Wait for REQ deassert (call sea_wait_for_req_deassert function).
 *    If REQ deassert got, go to the step 4.  If REQ did not cleared
 *    during timeout period, go to the next step.
 * 3) If `nodelay' flag did not initialized yet (`init' flag),
 *    then set `ndelay' flag.
 * 4) Set `init' flag.  Done.
 */
#define WAITREQ(t,op,cnt) {\
	if (! t->ndelay.op &&\
	    ! sea_wait_for_req_deassert (t->adapter, cnt, #op) &&\
	    ! t->init.op)\
		t->ndelay.op = 1;\
	t->init.op = 1; }

int sea_probe (struct isa_device *dev);
int sea_detect (struct sea_data *sea, struct isa_device *dev);
int sea_attach (struct isa_device *dev);
int seaintr (int unit);
int32 sea_scsi_cmd (struct scsi_xfer *xs);
u_int32 sea_adapter_info (int unit);
void sea_timeout (void *scb);
void seaminphys (struct buf *bp);
void sea_done (int unit, struct sea_scb *scb);
struct sea_scb *sea_get_scb (int unit, int flags);
void sea_free_scb (int unit, struct sea_scb *scb, int flags);
static void sea_start (void);
static void sea_information_transfer (struct sea_data *sea);
int sea_poll (struct scsi_xfer *xs);
int sea_init (struct sea_data *sea, struct isa_device *dev);
int sea_send_scb (struct sea_data *sea, struct sea_scb *scb);
int sea_reselect (struct sea_data *sea);
int sea_select (struct sea_data *sea, struct sea_scb *scb);
int sea_abort (int unit, struct sea_scb *scb);
void sea_msg_output (struct sea_data *sea, u_char msg);
u_char sea_msg_input (struct sea_data *sea);

struct scsi_adapter sea_switch = {
	sea_scsi_cmd, seaminphys, 0, 0,
	sea_adapter_info, "sea", {0},
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device sea_dev = {
	NULL,		/* use default error handler */
	NULL,		/* have a queue, served by this */
	NULL,		/* have no async handler */
	NULL,		/* Use default 'done' routine */
	"sea", 0, {0},
};

struct isa_driver seadriver = { sea_probe, sea_attach, "sea" };

/*
 * Check if the device can be found at the port given and if so,
 * detect the type of board. Set it up ready for further work.
 * Takes the isa_dev structure from autoconf as an argument.
 * Returns 1 if card recognized, 0 if errors.
 */
int sea_probe (struct isa_device *dev)
{
	struct sea_data *sea = &seadata[dev->id_unit];
	static const addrtab[] = {
		0xc8000, 0xca000, 0xcc000, 0xce000, 0xdc000, 0xde000, 0,
	};
	int i;

	dev->id_msize = 0x4000;
	if (! dev->id_maddr)
		for (i=0; addrtab[i]; ++i) {
			dev->id_maddr = (u_char*) KERNBASE + addrtab[i];
			if (sea_detect (sea, dev))
				return (1);
		}
	else if (sea_detect (sea, dev))
		return (1);
	sea->type = CTLR_NONE;
	return (0);
}

int sea_detect (struct sea_data *sea, struct isa_device *dev)
{
	sea->addr      = dev->id_maddr;

	/* Try Seagate. */
	sea->type      = CTLR_SEAGATE;
	sea->CONTROL   = sea->addr + 0x1a00; /* ST01/ST02 register offsets */
	sea->STATUS    = sea->addr + 0x1a00;
	sea->DATA      = sea->addr + 0x1c00;
	if (sea_init (sea, dev) == 0)
		return (1);

	/* Try Future Domain. */
	sea->type      = CTLR_FUTURE_DOMAIN;
	sea->CONTROL   = sea->addr + 0x1c00; /* TMC-885/TMC-950 reg. offsets */
	sea->STATUS    = sea->addr + 0x1c00;
	sea->DATA      = sea->addr + 0x1e00;
	if (sea_init (sea, dev) == 0)
		return (1);

	return (0);
}

/*
 * Probe the adapter, and if found,
 * reset the board and the scsi bus.
 */
int sea_init (struct sea_data *sea, struct isa_device *dev)
{
	volatile u_char *p;
	u_char c;
	int i;

	/* Check that STATUS..STATUS+200h are equal. */
	p = sea->STATUS;
	c = *p;
	if (c == 0xff)
		return (2);
	while (++p < sea->STATUS+0x200)
		if (*p != c)
			return (3);

	/* Check that DATA..DATA+200h are equal. */
	for (p=sea->DATA, c= *p++; p<sea->DATA+0x200; ++p)
		if (*p != c)
			return (4);

	/* Check that addr..addr+1800h are not writable. */
	for (p=sea->addr; p<sea->addr+0x1800; ++p) {
		c = *p;
		*p = ~c;
		if (*p == ~c) {
			*p = c;
			return (5);
		}
	}

	/* Check that addr+1800h..addr+1880h are writable. */
	for (p=sea->addr+0x1800; p<sea->addr+0x1880; ++p) {
		c = *p;
		*p = 0x55;
		if (*p != 0x55) {
			*p = c;
			return (6);
		}
		*p = 0xaa;
		if (*p != 0xaa) {
			*p = c;
			return (7);
		}
	}

	/* Parse device flags. */
	sea->parity = (dev->id_flags & FLAG_NOPARITY) ? 0 : CMD_EN_PARITY;

	/* Reset the scsi bus (I don't know if this is needed). */
	*sea->CONTROL = CMD_RST | CMD_DRVR_ENABLE;
	/* Hold reset for at least 25 microseconds. */
	DELAY (25);
	/* Check that status cleared. */
	if (*sea->STATUS != 0) {
		*sea->CONTROL = 0;
		return (8);
	}

	/* Check that DATA register is writable. */
	for (i=0; i<256; ++i) {
		*sea->DATA = i;
		if (*sea->DATA != i) {
			*sea->CONTROL = 0;
			return (9);
		}
	}

	/* Enable the adapter. */
	*sea->CONTROL = CMD_INTR | sea->parity;
	/* Wait a Bus Clear Delay (800 ns + bus free delay 800 ns). */
	DELAY (10);

	/* Check that DATA register is NOT writable. */
	c = *sea->DATA;
	for (i=0; i<256; ++i) {
		*sea->DATA = i;
		if (*sea->DATA != c) {
			*sea->CONTROL = 0;
			return (10);
		}
	}

	/* Init fields used by our routines */
	sea->sensefirst = (dev->id_flags & FLAG_SENSEFIRST) ? 1 : 0;
	sea->scsi_addr = SEA_SCSI_ADDR;
	sea->scsi_id = 1 << sea->scsi_addr;
	sea->irq = dev->id_irq ? ffs (dev->id_irq) - 1 : 0;
	sea->connected = 0;
	sea->issue_queue = 0;
	sea->disconnected_queue = 0;
	for (i=0; i<8; i++) {
		sea->target[i].adapter = sea;
		sea->target[i].busy = 0;
	}

	/* Link up the free list of scbs */
	sea->numscb = SCB_TABLE_SIZE;
	sea->free_scb = sea->scbs;
	for (i=1; i<SCB_TABLE_SIZE; i++)
		sea->scbs[i-1].next = sea->scbs + i;
	sea->scbs[SCB_TABLE_SIZE-1].next = 0;

	return (0);
}

static char sea_description [80];
static struct kern_devconf sea_kdc[NSEA] = {{
	0, 0, 0, "sea", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, &kdc_isa0, 0,
	DC_BUSY, sea_description,
}};

/*
 * Attach all sub-devices we can find.
 */
int sea_attach (struct isa_device *dev)
{
	int unit = dev->id_unit;
	struct sea_data *sea = &seadata[unit];

	sprintf (sea_description, "%s SCSI controller", sea_name[sea->type]);
	printf ("\n");
	printf ("sea%d: type %s%s%s\n", unit, sea_name[sea->type],
		(dev->id_flags & FLAG_NOPARITY) ? ", no parity" : "",
		(dev->id_flags & FLAG_SENSEFIRST) ? ", sense ahead" : "");

	/* fill in the prototype scsi_link */
	sea->sc_link.adapter_unit = unit;
	sea->sc_link.adapter_targ = sea->scsi_addr;
	sea->sc_link.adapter = &sea_switch;
	sea->sc_link.device = &sea_dev;

	/* ask the adapter what subunits are present */
	scsi_attachdevs (&(sea->sc_link));

	if (dev->id_unit)
		sea_kdc[dev->id_unit] = sea_kdc[0];
	sea_kdc[dev->id_unit].kdc_unit = dev->id_unit;
	sea_kdc[dev->id_unit].kdc_isa = dev;
	dev_attach (&sea_kdc[dev->id_unit]);
	return (1);
}

/*
 * Return some information to the caller about
 * the adapter and its capabilities.
 */
u_int32 sea_adapter_info (int unit)
{
	return (1);
}

void seaminphys (struct buf *bp)
{
}

/*
 * Catch an interrupt from the adaptor.
 */
int seaintr (int unit)
{
	struct sea_data *sea = &seadata[unit];

	PRINT (("sea%d: interrupt status=%b\n", unit, *sea->STATUS, STAT_BITS));
	while ((*sea->STATUS & (STAT_SEL | STAT_IO)) == (STAT_SEL | STAT_IO)) {
		/* Reselect interrupt */
		sea_reselect (sea);
		sea_start ();
	}
	return (1);
}

/*
 * Get a free scb. If there are none, see if we can allocate a new one. If so,
 * put it in the hash table too, otherwise return an error or sleep.
 */
struct sea_scb *sea_get_scb (int unit, int flags)
{
	struct sea_data *sea = &seadata[unit];
	struct sea_scb *scbp;
	int x = 0;

	if (! (flags & SCSI_NOMASK))
		x = splbio ();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	while (! (scbp = sea->free_scb))
		if (sea->numscb < SEA_SCB_MAX) {
			PRINT (("malloced new scb\n"));
			scbp = (struct sea_scb*) malloc (sizeof (struct sea_scb),
				M_TEMP, M_NOWAIT);
			if (scbp) {
				bzero (scbp, sizeof (struct sea_scb));
				sea->numscb++;
				scbp->flags = SCB_ACTIVE;
				scbp->next = 0;
			} else
				printf ("sea%d: can't malloc scb\n",unit);
			goto ret;
		} else if (! (flags & SCSI_NOSLEEP))
			tsleep ((caddr_t)&sea->free_scb, PRIBIO, "seascb", 0);
	if (scbp) {
		/* Get SCB from free list */
		sea->free_scb = scbp->next;
		scbp->next = 0;
		scbp->flags = SCB_ACTIVE;
	}
ret:    if (! (flags & SCSI_NOMASK))
		splx (x);
	return (scbp);
}

void sea_free_scb (int unit, struct sea_scb *scb, int flags)
{
	struct sea_data *sea = &seadata[unit];
	int x = 0;

	if (! (flags & SCSI_NOMASK))
		x = splbio ();

	scb->next = sea->free_scb;
	sea->free_scb = scb;
	scb->flags = SCB_FREE;

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (! scb->next)
		wakeup ((caddr_t) &sea->free_scb);

	if (! (flags & SCSI_NOMASK))
		splx (x);
}

/*
 * Start a scsi operation given the command and the data address.
 * Also needs the unit, target and lu.  Get a free scb and set it up.
 * Call send_scb.  Either start timer or wait until done.
 */
int32 sea_scsi_cmd (struct scsi_xfer *xs)
{
	int unit = xs->sc_link->adapter_unit, flags = xs->flags, x = 0;
	struct sea_data *sea = &seadata[unit];
	struct sea_scb *scb;

	PRINT (("sea%d:%d:%d command 0x%x\n", unit, xs->sc_link->target,
		xs->sc_link->lun, xs->cmd->opcode));
	if (xs->bp)
		flags |= SCSI_NOSLEEP;
	if (flags & ITSDONE) {
		printf ("sea%d: already done?", unit);
		xs->flags &= ~ITSDONE;
	}
	if (! (flags & INUSE)) {
		printf ("sea%d: not in use?", unit);
		xs->flags |= INUSE;
	}
	scb = sea_get_scb (unit, flags);
	if (! scb) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}

	/* Put all the arguments for the xfer in the scb */
	scb->xfer = xs;
	scb->datalen = xs->datalen;
	scb->data = xs->data;

	if (flags & SCSI_RESET)
		/* Try to send a reset command to the card. This is done
		 * by calling the Reset function. Should then return COMPLETE.
		 * Need to take care of the possible current connected command.
		 * Not implemented right now. */
		printf ("sea%d: got a SCSI_RESET!\n", unit);

	if (! (flags & SCSI_NOMASK))
		x = splbio ();

	/* Setup the scb to contain necessary values.
	 * The interesting values can be read from the xs that is saved.
	 * I therefore think that the structure can be kept very small.
	 * The driver doesn't use DMA so the scatter/gather is not needed? */
	/* A check is done to see if the command contains
	 * a REQUEST_SENSE command, and if so the command is put first
	 * in the queue, otherwise the command is added to the end
	 * of the queue. ?? Not correct ?? */
	if (! sea->issue_queue ||
	    (sea->sensefirst && xs->cmd->opcode == REQUEST_SENSE)) {
		scb->next = sea->issue_queue;
		sea->issue_queue = scb;
	} else {
		struct sea_scb *q;

		for (q=sea->issue_queue; q->next; q=q->next)
			continue;
		q->next = scb;
		scb->next = 0;  /* placed at the end of the queue */
	}

	/* Try to send this command to the board. Because this board
	 * does not use any mailboxes, this routine simply adds the command
	 * to the queue held by the sea_data structure. */
	sea_start ();

	/* Usually return SUCCESSFULLY QUEUED. */
	if (! (flags & SCSI_NOMASK)) {
		splx (x);
		if (xs->flags & ITSDONE)
			/* Timeout timer not started, already finished.
			 * Tried to return COMPLETE but the machine hanged
			 * with this. */
			return (SUCCESSFULLY_QUEUED);
		timeout (sea_timeout, (caddr_t) scb, (xs->timeout * hz) / 1000);
		scb->flags |= SCB_TIMECHK;
		PRINT (("sea%d:%d:%d command queued\n", unit,
			xs->sc_link->target, xs->sc_link->lun));
		return (SUCCESSFULLY_QUEUED);
	}

	/* If we can't use interrupts, poll on completion. */
	if (! sea_poll (xs)) {
		/* We timed out, so call the timeout handler manually,
		 * accounting for the fact that the clock is not running yet
		 * by taking out the clock queue entry it makes. */
		sea_timeout ((void*) scb);

		/* Because we are polling, take out the timeout entry
		 * sea_timeout made. */
		untimeout (sea_timeout, (void*) scb);

		if (! sea_poll (xs))
			/* We timed out again... This is bad. Notice that
			 * this time there is no clock queue entry to remove. */
			sea_timeout ((void*) scb);
	}
	PRINT (("sea%d:%d:%d command %s\n", unit,
		xs->sc_link->target, xs->sc_link->lun,
		xs->error ? "failed" : "done"));
	return (xs->error ? HAD_ERROR : COMPLETE);
}

/*
 * Coroutine that runs as long as more work can be done.
 * Both sea_scsi_cmd and sea_intr will try to start it in
 * case it is not running.
 * Always called with interrupts disabled.
 */
static void sea_start (void)
{
	int unit, done;

again:  done = 1;
	for (unit=0; unit<NSEA && seadata[unit].type; ++unit) {
		struct sea_data *sea = &seadata[unit];

		if (! sea->connected) {
			/* Search through the issue_queue for a command
			 * destined for a target that's not busy. */
			struct sea_scb *q, *prev = 0;

			for (q=sea->issue_queue; q; prev=q, q=q->next) {
				if (IS_BUSY (sea, q))
					continue;

				/* First check that if any device has tried
				 * a reconnect while we have done other things
				 * with interrupts disabled. */
				if ((*sea->STATUS & (STAT_SEL | STAT_IO)) ==
				    (STAT_SEL | STAT_IO)) {
					sea_reselect (sea);
					break;
				}

				/* When we find the command, remove it
				 * from the issue queue. */
				if (prev)
					prev->next = q->next;
				else
					sea->issue_queue = q->next;
				q->next = 0;

				/* Attempt to establish an I_T_L nexus here.
				 * On success, sea->connected is set.
				 * On failure, we must add the command back to
				 * the issue queue so we can keep trying. */
				if (sea_select (sea, q))
					break;
				q->next = sea->issue_queue;
				sea->issue_queue = q;
				printf ("sea_start: select failed\n");
			}
		}

		if (sea->connected) {
			/* We are connected. Do the task. */
			sea_information_transfer (sea);
			done = 0;
		}
	}
	if (! done)
		goto again;
}

void sea_timeout (void *arg)
{
	struct sea_scb *scb = (struct sea_scb*) arg;
	int unit = scb->xfer->sc_link->adapter_unit;
	int x = splbio ();

	if (! (scb->xfer->flags & SCSI_NOMASK))
		printf ("sea%d:%d:%d (%s%d) timed out\n", unit,
			scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun,
			scb->xfer->sc_link->device->name,
			scb->xfer->sc_link->dev_unit);

	/*
	 * If it has been through before, then a previous abort has failed,
	 * don't try abort again.
	 */
	if (! (scb->flags & SCB_ABORTED) /*&& sea_abort (unit, scb)*/) {
		sea_abort (unit, scb);
		/* 2 seconds for the abort */
		timeout (sea_timeout, (caddr_t)scb, 2*hz);
		scb->flags |= (SCB_ABORTED | SCB_TIMECHK);
	} else {
		/* abort timed out */
		scb->flags |= SCB_ABORTED;
		scb->xfer->retries = 0;
		sea_done (unit, scb);
	}
	splx (x);
}

/*
 * Establish I_T_L or I_T_L_Q nexus for new or existing command
 * including ARBITRATION, SELECTION, and initial message out
 * for IDENTIFY and queue messages.
 * Return 0 if selection could not execute for some reason, 1 if selection
 * succeded or failed because the target did not respond.
 */
int sea_select (struct sea_data *sea, struct sea_scb *scb)
{
	PRINT (("sea%d:%d:%d select\n", sea->sc_link.adapter_unit,
		scb->xfer->sc_link->target, scb->xfer->sc_link->lun));
	*sea->CONTROL = sea->parity;
	*sea->DATA = sea->scsi_id;
	*sea->CONTROL = CMD_START_ARB | sea->parity;

	/* Wait for arbitration to complete. */
	WAITFOR (*sea->STATUS & STAT_ARB_CMPL, "arbitration");

	if (! (*sea->STATUS & STAT_ARB_CMPL)) {
		if (*sea->STATUS & STAT_SEL) {
			printf ("sea: arbitration lost\n");
			scb->flags |= SCB_ERROR;
		} else {
			printf ("sea: arbitration timeout\n");
			scb->flags |= SCB_TIMEOUT;
		}
		*sea->CONTROL = CMD_INTR | sea->parity;
		return (0);
	}
	DELAY (2);

	*sea->DATA = (1 << scb->xfer->sc_link->target) | sea->scsi_id;
	*sea->CONTROL = CMD_DRVR_ENABLE | CMD_SEL | CMD_ATTN | sea->parity;
	DELAY (1);

	/* Wait for a bsy from target.
	 * If the target is not present on the bus, we get
	 * the timeout.  Don't PRINT any message -- it's not an error. */
	WAITFOR (*sea->STATUS & STAT_BSY, 0);

	if (! (*sea->STATUS & STAT_BSY)) {
		/* The target does not respond.  Not an error, though.
		 * Should return some error to the higher level driver? */
		*sea->CONTROL = CMD_INTR | sea->parity;
		scb->flags |= SCB_TIMEOUT;
		return (1);
	}

	/* Try to make the target to take a message from us. */
	*sea->CONTROL = CMD_DRVR_ENABLE | CMD_ATTN | sea->parity;
	DELAY (1);

	/* Should start a msg_out phase. */
	WAITFOR (*sea->STATUS & STAT_REQ, 0);

	if (! (*sea->STATUS & STAT_REQ)) {
		/* This should not be taken as an error, but more like
		 * an unsupported feature!
		 * Should set a flag indicating that the target don't support
		 * messages, and continue without failure.
		 * (THIS IS NOT AN ERROR!)
		 */
		printf ("sea: target does not support messages, canceled\n");
		scb->flags |= SCB_ERROR;
		*sea->CONTROL = CMD_INTR | sea->parity;
		return (0);
	}

	sea->connected = scb;
	SET_BUSY (sea, scb);
	*sea->CONTROL = CMD_DRVR_ENABLE | sea->parity;

	/* Allow disconnects. */
	sea_msg_output (sea, MSG_IDENTIFY (scb->xfer->sc_link->lun));

	if (! (*sea->STATUS & STAT_BSY))
		printf ("sea: target disconnected after successful arbitrate\n");

	*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | sea->parity;
	return (1);
}

int sea_reselect (struct sea_data *sea)
{
	struct sea_scb *q = 0, *prev = 0;
	u_char msg, target_mask;

	PRINT (("sea%d reselect: ", sea->sc_link.adapter_unit));

	if (! (*sea->STATUS & STAT_SEL)) {
		printf ("sea: wrong state 0x%x\n", *sea->STATUS);
		return (0);
	}

	/* Wait for a device to win the reselection phase. */
	/* Signals this by asserting the I/O signal. */
	WAITFOR ((*sea->STATUS & (STAT_SEL | STAT_IO | STAT_BSY)) ==
		(STAT_SEL | STAT_IO), "reselection phase");

	/* The data bus contains original initiator id ORed with target id. */
	/* See that we really are the initiator. */
	target_mask = *sea->DATA;
	if (! (target_mask & sea->scsi_id)) {
		printf ("sea: polled reselection was not for me: %x\n",
			target_mask);
		return (0);
	}

	/* Find target who won. */
	/* Host responds by asserting the BSY signal. */
	/* Target should respond by deasserting the SEL signal. */
	target_mask &= ~sea->scsi_id;
	*sea->CONTROL = CMD_DRVR_ENABLE | CMD_BSY | sea->parity;
	WAITFOR (! (*sea->STATUS & STAT_SEL), "reselection acknowledge");

	/* Remove the busy status. */
	*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | sea->parity;

	/* We are connected. Now we wait for the MSGIN condition. */
	WAITFOR (*sea->STATUS & STAT_REQ, "identify message");

	/* Hope we get an IDENTIFY message. */
	msg = sea_msg_input (sea);
	if (! MSG_ISIDENT (msg))
		printf ("sea: expecting IDENTIFY message, got 0x%x\n", msg);
	else {
		/* Find the command corresponding to the I_T_L or I_T_L_Q
		 * nexus we just restablished, and remove it from
		 * the disconnected queue. */
		unsigned char lun = (msg & 7);

		for (q=sea->disconnected_queue; q; prev=q, q=q->next) {
			if (target_mask != (1 << q->xfer->sc_link->target))
				continue;
			if (lun != q->xfer->sc_link->lun)
				continue;
			if (prev)
				prev->next = q->next;
			else
				sea->disconnected_queue = q->next;
			q->next = 0;
			sea->connected = q;
			PRINT (("lun %d done\n", lun));
			return (1);
		}
		/* Since we have an established nexus that we can't
		 * do anything with, we must abort it. */
		PRINT (("lun %d aborted\n", lun));
	}

	/* Abort the connection. */
	*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | CMD_ATTN | sea->parity;
	sea_msg_output (sea, MSG_ABORT);
	*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | sea->parity;
	return (0);
}

/*
 * Send an abort to the target.
 * Return 1 success, 0 on failure.
 * Called on splbio level.
 */
int sea_abort (int unit, struct sea_scb *scb)
{
	struct sea_data *sea = &seadata[unit];
	struct sea_scb *q, **prev;

	/* If the command hasn't been issued yet, we simply remove it
	 * from the issue queue. */
	prev = &sea->issue_queue;
	for (q=sea->issue_queue; q; q=q->next) {
		if (scb == q) {
			(*prev) = q->next;
			q->next = 0;
			return (1);
		}
		prev = &q->next;
	}

	/* If any commands are connected, we're going to fail the abort
	 * and let the high level SCSI driver retry at a later time
	 * or issue a reset. */
	if (sea->connected)
		return (0);

	/* If the command is currently disconnected from the bus,
	 * and there are no connected commands, we reconnect
	 * the I_T_L or I_T_L_Q nexus associated with it,
	 * go into message out, and send an abort message. */
	for (q=sea->disconnected_queue; q; q=q->next) {
		if (scb != q)
			continue;

		if (! sea_select (sea, scb))
			return (0);

		*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | CMD_ATTN | sea->parity;
		sea_msg_output (sea, MSG_ABORT);
		*sea->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | sea->parity;

		prev = &sea->disconnected_queue;
		for (q=sea->disconnected_queue; q; q=q->next) {
			if (scb == q) {
				*prev = q->next;
				q->next = 0;
				/* Set some type of error result
				 * for the operation. */
				return (1);
			}
			prev = &q->next;
		}
	}

	/* Command not found in any queue, race condition in the code? */
	return (1);
}

void sea_done (int unit, struct sea_scb *scb)
{
	struct scsi_xfer *xs = scb->xfer;

	if (scb->flags & SCB_TIMECHK)
		untimeout (sea_timeout, (caddr_t) scb);

	/* How much of the buffer was not touched. */
	xs->resid = scb->datalen;

	if (scb->flags != SCB_ACTIVE && ! (xs->flags & SCSI_ERR_OK))
		if (scb->flags & (SCB_TIMEOUT | SCB_ABORTED))
			xs->error = XS_TIMEOUT;
		else if (scb->flags & SCB_ERROR)
			xs->error = XS_DRIVER_STUFFUP;

	xs->flags |= ITSDONE;
	sea_free_scb (unit, scb, xs->flags);
	scsi_done (xs);
}

/*
 * Wait for completion of command in polled mode.
 * Always called with interrupts masked out.
 */
int sea_poll (struct scsi_xfer *xs)
{
	int count;

	for (count=0; count<1000; ++count) {
		if (xs->flags & ITSDONE)
			return (1);
		/* Try to do something. */
		DELAY (30);
		sea_start ();
	}
	return (0);
}

/*
 * Wait until REQ goes down.  This is needed for some devices (CDROMs)
 * after every MSGOUT, MSGIN, CMDOUT, STATIN request.
 * Return true if REQ deassert found.
 */
static inline int sea_wait_for_req_deassert (struct sea_data *sea,
	int cnt, char *msg)
{
	asm ("
	1:      testb $0x10, %2
		jz 2f
		loop 1b
	2:"
	: "=c" (cnt)                            /* output */
	: "0" (cnt), "m" (*sea->STATUS));       /* input */
	if (! cnt) {
		PRINT (("sea%d:%d:%d (%s) timeout waiting for !REQ\n",
			sea->sc_link.adapter_unit,
			sea->connected->xfer->sc_link->target,
			sea->connected->xfer->sc_link->lun, msg));
		return (0);
	}
	/* PRINT (("sea_wait_for_req_deassert %s count=%d\n", msg, cnt)); */
	return (1);
}

/*
 * Send the command to the target.
 */
void sea_cmd_output (struct sea_data *sea, u_char *cmd, int cmdlen)
{
	struct sea_target *t = &sea->target[sea->connected->xfer->sc_link->target];

	if (! cmdlen || ! cmd) {
		printf ("sea%d:%d:%d no command\n",
			sea->sc_link.adapter_unit,
			sea->connected->xfer->sc_link->target,
			sea->connected->xfer->sc_link->lun);
		*sea->DATA = 0;
		sea_wait_for_req_deassert (sea, 1000, "ZCMDOUT");
		return;
	}
	PRINT (("sea%d:%d:%d (CMDOUT) send %d bytes ",
		sea->sc_link.adapter_unit,
		sea->connected->xfer->sc_link->target,
		sea->connected->xfer->sc_link->lun, cmdlen));

	PRINT (("%x", *cmd));
	*sea->DATA = *cmd++;
	WAITREQ (t, cmdout1, 10000);
	--cmdlen;

	while (cmdlen) {
		/* Check for target disconnect. */
		u_char sts = *sea->STATUS;
		if (! (sts & STAT_BSY))
			break;

		/* Check for phase mismatch. */
		if ((sts & REQ_MASK) != REQ_CMDOUT) {
			printf ("sea: sending command: invalid phase %s\n",
				sea_phase_name[sts & REQ_MASK]);
			return;
		}

		/* Wait for REQ. */
		if (! (sts & STAT_REQ))
			continue;

		PRINT (("-%x", *cmd));
		*sea->DATA = *cmd++;
		WAITREQ (t, cmdout, 1000);
		--cmdlen;
	}
	PRINT (("\n"));
}

/*
 * Send the message to the target.
 */
void sea_msg_output (struct sea_data *sea, u_char msg)
{
	struct sea_target *t = sea->connected ?
		&sea->target[sea->connected->xfer->sc_link->target] : 0;
	register u_long cnt = 0;
	register u_char sts;

	/* Wait for REQ, after which the phase bits will be valid. */
again:  sts = *sea->STATUS;
	if (! (sts & STAT_REQ)) {
		if (++cnt > 1000000L) {
			printf ("sea: sending message: no REQ\n");
			return;
		}
		goto again;
	}

	/* Check for phase mismatch. */
	if ((sts & REQ_MASK) != REQ_MSGOUT) {
		printf ("sea: sending message: invalid phase %s\n",
			sea_phase_name[sts & REQ_MASK]);
		return;
	}

	*sea->DATA = msg;
	if (! t)
		sea_wait_for_req_deassert (sea, 1000, "MSG_OUTPUT");
	else
		WAITREQ (t, msgout, 1000);
	PRINT (("sea%d:%d:%d (MSGOUT) send 0x%x\n",
		sea->sc_link.adapter_unit,
		sea->connected ? sea->connected->xfer->sc_link->target : 9,
		sea->connected ? sea->connected->xfer->sc_link->lun : 9,
		msg));
}

/*
 * Get the message from the target.
 * Return the length of the received message.
 */
u_char sea_msg_input (struct sea_data *sea)
{
	register u_long cnt = 0;
	register u_char sts, msg;

	/* Wait for REQ, after which the phase bits will be valid. */
again:  sts = *sea->STATUS;
	if (! (sts & STAT_REQ)) {
		if (++cnt > 1000000L) {
			printf ("sea: receiving message: no REQ\n");
			return (MSG_ABORT);
		}
		goto again;
	}

	/* Check for phase mismatch.
	 * Reached if the target decides that it has finished
	 * the transfer. */
	if ((sts & REQ_MASK) != REQ_MSGIN) {
		printf ("sea: sending message: invalid phase %s\n",
			sea_phase_name[sts & REQ_MASK]);
		return (MSG_ABORT);
	}

	/* Do actual transfer from SCSI bus to/from memory. */
	msg = *sea->DATA;
	sea_wait_for_req_deassert (sea, 1000, "MSG_INPUT");
	PRINT (("sea%d:%d:%d (MSG_INPUT) got 0x%x\n",
		sea->sc_link.adapter_unit,
		sea->connected ? sea->connected->xfer->sc_link->target : 9,
		sea->connected ? sea->connected->xfer->sc_link->lun : 9, msg));
	return (msg);
}

/*
 * This routine is used in the case when we have no IRQ line (sea->irq == 0).
 * It is called every timer tick and polls for reconnect from target.
 */
void sea_tick (void *arg)
{
	struct sea_data *sea = arg;
	int x = splbio ();

	sea->timeout_active = 0;
	while ((*sea->STATUS & (STAT_SEL | STAT_IO)) == (STAT_SEL | STAT_IO)) {
		/* Reselect interrupt */
		sea_reselect (sea);
		sea_start ();
	}
	if (sea->disconnected_queue && ! sea->timeout_active) {
		timeout (sea_tick, sea, 1);
		sea->timeout_active = 1;
	}
	splx (x);
}

/*
 * Do the transfer. We know we are connected. Update the flags,
 * call sea_done when task accomplished. Dialog controlled by the target.
 * Always called with interrupts disabled.
 */
static void sea_information_transfer (register struct sea_data *sea)
{
	struct sea_scb *scb = sea->connected;   /* current control block */
	u_char *data = scb->data;               /* current data buffer */
	u_long datalen = scb->datalen;          /* current data transfer size */
	struct sea_target *t = &sea->target[scb->xfer->sc_link->target];
	register u_char sts;
	u_char msg;

	while ((sts = *sea->STATUS) & STAT_BSY) {
		/* We only have a valid SCSI phase when REQ is asserted. */
		if (! (sts & STAT_REQ))
			continue;
		if (sts & STAT_PARITY) {
			int target = scb->xfer->sc_link->target;
			if (++sea->target[target].perrcnt < 8)
				printf ("sea%d:%d:%d parity error\n",
					sea->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
			else if (sea->target[target].perrcnt == 8)
				printf ("sea%d:%d:%d too many parity errors, not logging any more\n",
					sea->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
		}
		switch (sts & REQ_MASK) {
		case REQ_DATAOUT:
			if (datalen <= 0) {
				printf ("sea%d:%d:%d data length underflow\n",
					sea->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun);
				*sea->DATA = 0;
				break;
			}
			asm ("cld
			1:      movb (%%ebx), %%al
				xorb $1, %%al
				testb $0xf, %%al
				jnz 2f
				testb $0x10, %%al
				jz 1b
				lodsb
				movb %%al, (%%edi)
				loop 1b
			2:"
			: "=S" (data), "=c" (datalen)           /* output */
			: "D" (sea->DATA), "b" (sea->STATUS),   /* input */
				"0" (data), "1" (datalen)
			: "eax", "ebx", "edi");                 /* clobbered */
			PRINT (("sea%d:%d:%d (DATAOUT) send %ld bytes\n",
				sea->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun,
				scb->datalen - datalen));
			break;
		case REQ_DATAIN:
			if (datalen <= 0) {
				PRINT (("@"));
				sts = *sea->DATA;
				break;
			}
			if (datalen >= 512) {
				asm ("  cld
				1:      movb (%%esi), %%al
					xorb $5, %%al
					testb $0xf, %%al
					jnz 2f
					testb $0x10, %%al
					jz 1b
					movb (%%ebx), %%al
					stosb
					loop 1b
				2:"
				: "=D" (data), "=c" (datalen)   /* output */
				: "b" (sea->DATA), "S" (sea->STATUS),
					"0" (data), "1" (datalen) /* input */
				: "eax", "ebx", "esi");         /* clobbered */
			} else {
				asm ("  cld
				1:      movb (%%esi), %%al
					xorb $5, %%al
					testb $0xf, %%al
					jnz 2f
					testb $0x10, %%al
					jz 1b
					movb (%%ebx), %%al
					stosb
					movb $1000, %%al
				3:      testb $0x10, (%%esi)
					jz 4f
					dec %%al
					jnz 3b
				4:      loop 1b
				2:"
				: "=D" (data), "=c" (datalen)   /* output */
				: "b" (sea->DATA), "S" (sea->STATUS),
					"0" (data), "1" (datalen) /* input */
				: "eax", "ebx", "esi");         /* clobbered */
			}
			PRINT (("sea%d:%d:%d (DATAIN) got %ld bytes\n",
				sea->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun,
				scb->datalen - datalen));
			break;
		case REQ_CMDOUT:
			sea_cmd_output (sea, (u_char*) scb->xfer->cmd,
				scb->xfer->cmdlen);
			break;
		case REQ_STATIN:
			scb->xfer->status = *sea->DATA;
			WAITREQ (t, statin, 2000);
			PRINT (("sea%d:%d:%d (STATIN) got 0x%x\n",
				sea->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun,
				(u_char) scb->xfer->status));
			break;
		case REQ_MSGOUT:
			*sea->DATA = MSG_NOP;
			sea_wait_for_req_deassert (sea, 1000, "MSGOUT");
			PRINT (("sea%d:%d:%d (MSGOUT) send NOP\n",
				sea->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun));
			break;
		case REQ_MSGIN:
			/* Don't handle multi-byte messages here, because they
			 * should not be present here. */
			msg = *sea->DATA;
			sea_wait_for_req_deassert (sea, 2000, "MSGIN");
			PRINT (("sea%d:%d:%d (MSGIN) got 0x%x\n",
				sea->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun, msg));
			switch (msg) {
			case MSG_COMMAND_COMPLETE:
				scb->data = data;
				scb->datalen = datalen;
				goto done;
			case MSG_ABORT:
				printf ("sea: command aborted by target\n");
				scb->flags = SCB_ABORTED;
				goto done;
			case MSG_DISCONNECT:
				scb->next = sea->disconnected_queue;
				sea->disconnected_queue = scb;
				sea->connected = 0;
				*sea->CONTROL = CMD_INTR | sea->parity;
				if (! sea->irq && ! sea->timeout_active) {
					timeout (sea_tick, sea, 1);
					sea->timeout_active = 1;
				}
				return;
			case MSG_SAVE_POINTERS:
				scb->data = data;
				scb->datalen = datalen;
				break;
			case MSG_RESTORE_POINTERS:
				data = scb->data;
				datalen = scb->datalen;
				break;
			case MSG_MESSAGE_REJECT:
				PRINT (("sea: message_reject received\n"));
				break;
			default:
				printf ("sea%d:%d:%d unknown message: 0x%x\n",
					sea->sc_link.adapter_unit, scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun, msg);
				break;
			}
			break;
		default:
			printf ("sea: unknown phase: %b\n",
				sts & REQ_MASK, STAT_BITS);
		}
	}
	printf ("sea%d:%d:%d unexpected target disconnect\n",
		sea->sc_link.adapter_unit, scb->xfer->sc_link->target,
		scb->xfer->sc_link->lun);
	scb->flags = SCB_ERROR;
done:   sea->connected = 0;
	CLEAR_BUSY (sea, scb);
	*sea->CONTROL = CMD_INTR | sea->parity;
	sea_done (sea->sc_link.adapter_unit, scb);
}
#endif /* NSEA */

/*
 * (Free/Net/386)BSD ST01/02, Future Domain TMC-885, TMC-950 SCSI driver for
 * Julians SCSI-code
 *
 * Copyright 1994, Kent Palmkvist (kentp@isy.liu.se)
 * Copyright 1994, Robert Knier (rknier@qgraph.com)
 * Copyright 1992, 1994 Drew Eckhardt (drew@colorado.edu)
 * Copyright 1994, Julian Elischer (julian@tfs.com)
 * Copyright 1994-1995, Serge Vakulenko (vak@cronyx.ru)
 * Copyright 1995 Stephen Hocking (sysseh@devetir.qld.gov.au)
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
 * vak    950115 Added request-sense ops
 * seh    950701 Fixed up Future Domain TMC-885 problems with disconnects,
 *               weird phases and the like. (we could probably investigate
 *               what the board's idea of the phases are, but that requires
 *               doco that I don't have). Note that it is slower than the
 *               2.0R driver with both SEA_BLINDTRANSFER & SEA_ASSEMBLER
 *               defined by a factor of more than 2. I'll look at that later!
 * seh    950712 The performance release 8^). Put in the blind transfer code
 *               from the 2.0R source. Don't use it by commenting out the 
 *               SEA_BLINDTRANSFER below. Note that it only kicks in during
 *               DATAOUT or DATAIN and then only when the transfer is a
 *               multiple of BLOCK_SIZE bytes (512). Most devices fit into
 *               that category, with the possible exception of scanners and
 *               some of the older MO drives.
 *
 * $Id: seagate.c,v 1.26 1997/09/21 21:41:35 gibbs Exp $
 */

/*
 * What should really be done:
 *
 * Restructure interrupt enable/disable code (runs too long with int disabled)
 * Add code to handle Future Domain 840, 841, 880 and 881
 * Add code to use tagged commands in SCSI2
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>

#include <scsi/scsiconf.h>

#include "ioconf.h"

#ifdef DEBUG
#   define PRINT(s)     printf s
#else
#   define PRINT(s)     /*void*/
#endif

#define SCB_TABLE_SIZE	8	/* start with 8 scb entries in table */
#define BLOCK_SIZE	512	/* size of READ/WRITE areas on SCSI card */
#define HOST_SCSI_ADDR  7       /* address of the adapter on the SCSI bus */
#define SEA_BLINDTRANSFER	1	/* for quicker than quick xfers */
/*
 * Define config flags
 */
#define FLAG_NOPARITY   0x01    /* disable SCSI bus parity check */

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
 * SCSI bus phases
 */
#define PHASE_MASK              (STAT_MSG | STAT_CD | STAT_IO)
#define PHASE_DATAOUT           0
#define PHASE_DATAIN            STAT_IO
#define PHASE_CMDOUT            STAT_CD
#define PHASE_STATIN            (STAT_CD | STAT_IO)
#define PHASE_MSGOUT            (STAT_MSG | STAT_CD)
#define PHASE_MSGIN             (STAT_MSG | STAT_CD | STAT_IO)
#define PHASE_NAME(ph)          phase_name[(ph)>>2]

static char *phase_name[] = {
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
typedef struct scb {
	int flags;                      /* status of the instruction */
#define SCB_FREE        0x00
#define SCB_ACTIVE      0x01
#define SCB_ABORTED     0x02
#define SCB_TIMEOUT     0x04
#define SCB_ERROR       0x08
#define SCB_TIMECHK     0x10            /* we have set a timeout on this one */
#define SCB_SENSE       0x20            /* sensed data available */
#define SCB_TBUSY       0x40            /* target busy */
	struct scb *next;               /* in free list */
	struct scsi_xfer *xfer;		/* the scsi_xfer for this cmd */
	u_char *data;                   /* position in data buffer so far */
	int32_t datalen;		/* bytes remaining to transfer */
} scb_t;

typedef enum {
	CTLR_NONE,
	CTLR_SEAGATE,
	CTLR_FUTURE_DOMAIN
} ctlr_t;

/*
 * Flags for waiting for REQ deassert during some SCSI bus phases.
 */
typedef struct {
	unsigned cmdout1 : 1;   /* after CMDOUT[0] byte */
	unsigned cmdout : 1;    /* after CMDOUT[1..N] bytes */
	unsigned msgout : 1;    /* after MSGOUT byte */
	unsigned statin : 1;    /* after STATIN byte */
} phase_t;

/*
 * Data structure describing the target state.
 */
typedef struct {
	struct adapter *adapter;        /* pointer to the adapter structure */
	u_char  busy;                   /* mask of busy luns at device target */
	u_long  perrcnt;                /* counter of target parity errors */
	phase_t ndelay;                 /* "don't delay" flags */
	phase_t init;                   /* "initialized" flags */
} target_t;

/*
 * Data structure describing current status of the scsi bus. One for each
 * controller card.
 */
typedef struct adapter {
	ctlr_t  type;                   /* Seagate or Future Domain */
	char    *name;                  /* adapter name */
	volatile u_char *addr;          /* base address for card */

	volatile u_char *CONTROL;       /* address of control register */
	volatile u_char *STATUS;        /* address of status register */
	volatile u_char *DATA;          /* address of data register */

	u_char  scsi_addr;              /* our scsi address, 0..7 */
	u_char  scsi_id;                /* our scsi id mask */
	u_char  parity;                 /* parity flag: CMD_EN_PARITY or 0 */
	u_char  irq;                    /* IRQ number used or 0 if no IRQ */
	u_int   timeout_active : 1;     /* timeout() active (requested) */

	struct scsi_link sc_link;	/* struct connecting different data */
	scb_t   *queue;                 /* waiting to be issued */
	scb_t   *disconnected_queue;    /* waiting to reconnect */

	int     numscb;                 /* number of scsi control blocks */
	scb_t   *free_scb;              /* free scb list */
	scb_t   scbs[SCB_TABLE_SIZE];

	target_t target[8];             /* target state data */
} adapter_t;

static adapter_t seadata[NSEA];

#define IS_BUSY(a,b)    ((a)->target[(b)->xfer->sc_link->target].busy &\
				(1 << (b)->xfer->sc_link->lun))
#define SET_BUSY(a,b)   ((a)->target[(b)->xfer->sc_link->target].busy |=\
				(1 << (b)->xfer->sc_link->lun))
#define CLEAR_BUSY(a,b) ((a)->target[(b)->xfer->sc_link->target].busy &=\
				~(1 << (b)->xfer->sc_link->lun))

/*
 * Wait for condition, given as an boolean expression.
 * Print the message on timeout.
 */
#define WAITFOR(condition,message) {\
	register u_long cnt = 100000; char *_msg = message;\
	while (cnt-- && ! (condition)) continue;\
	if (cnt == -1 && _msg)\
		printf ("sea: %s timeout\n", _msg); }

#define WAITFOR10(condition,message) {\
	register u_long cnt = 1000000; char *_msg = message;\
	while (cnt-- && ! (condition)) continue;\
	if (cnt == -1 && _msg)\
		printf ("sea: %s timeout\n", _msg); }

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
	if (! (t)->ndelay.op &&\
	    ! sea_wait_for_req_deassert ((t)->adapter, cnt, #op) &&\
	    ! (t)->init.op)\
		(t)->ndelay.op = 1;\
	(t)->init.op = 1; }

static int sea_probe (struct isa_device *dev);
static int sea_detect (adapter_t *z, struct isa_device *dev);
static int sea_attach (struct isa_device *dev);
static int32_t sea_scsi_cmd (struct scsi_xfer *xs);
static u_int32_t sea_adapter_info (int unit);
static void sea_timeout (void *scb);
static void seaminphys (struct buf *bp);
static void sea_done (adapter_t *z, scb_t *scb);
static void sea_start (adapter_t *z);
static void sea_information_transfer (adapter_t *z, scb_t *scb);
static int sea_poll (adapter_t *z, scb_t *scb);
static int sea_init (adapter_t *z);
static int sea_reselect (adapter_t *z);
static int sea_select (volatile adapter_t *z, scb_t *scb);
static int sea_abort (adapter_t *z, scb_t *scb);
static void sea_send_abort (adapter_t *z);
static u_char sea_msg_input (adapter_t *z);
static void sea_tick (void *arg);
static int sea_sense (adapter_t *z, scb_t *scb);
static void sea_data_output (adapter_t *z, u_char **pdata, u_long *plen);
static void sea_data_input (adapter_t *z, u_char **pdata, u_long *plen);
static void sea_cmd_output (target_t *z, u_char *cmd, int cmdlen);

static struct scsi_adapter sea_switch = {
	sea_scsi_cmd, seaminphys, 0, 0, sea_adapter_info, "sea", {0},
};
static struct scsi_device sea_dev = { NULL, NULL, NULL, NULL, "sea", 0, {0} };
struct isa_driver seadriver = { sea_probe, sea_attach, "sea" };

/* FD TMC885's can't handle detach & re-attach */
static int sea_select_cmd = CMD_DRVR_ENABLE | CMD_ATTN;

/*
 * Check if the device can be found at the port given and if so,
 * detect the type of board. Set it up ready for further work.
 * Takes the isa_dev structure from autoconf as an argument.
 * Returns 1 if card recognized, 0 if errors.
 */
int sea_probe (struct isa_device *dev)
{
	adapter_t *z = &seadata[dev->id_unit];
	static const addrtab[] = {
		0xc8000, 0xca000, 0xcc000, 0xce000, 0xdc000, 0xde000, 0,
	};
	int i;

	/* Init fields used by our routines */
	z->parity = (dev->id_flags & FLAG_NOPARITY) ? 0 : CMD_EN_PARITY;
	z->scsi_addr = HOST_SCSI_ADDR;
	z->scsi_id = 1 << z->scsi_addr;
	z->irq = dev->id_irq ? ffs (dev->id_irq) - 1 : 0;
	z->queue = 0;
	z->disconnected_queue = 0;
	for (i=0; i<8; i++) {
		z->target[i].adapter = z;
		z->target[i].busy = 0;
	}

	/* Link up the free list of scbs */
	z->numscb = SCB_TABLE_SIZE;
	z->free_scb = z->scbs;
	for (i=1; i<SCB_TABLE_SIZE; i++)
		z->scbs[i-1].next = z->scbs + i;
	z->scbs[SCB_TABLE_SIZE-1].next = 0;

	/* Detect the adapter. */
	dev->id_msize = 0x4000;
	if (! dev->id_maddr)
		for (i=0; addrtab[i]; ++i) {
			dev->id_maddr = (u_char*) KERNBASE + addrtab[i];
			if (sea_detect (z, dev))
				return (1);
		}
	else if (sea_detect (z, dev))
		return (1);

	bzero (z, sizeof (*z));
	return (0);
}

int sea_detect (adapter_t *z, struct isa_device *dev)
{
	z->addr = dev->id_maddr;

	/* Try Seagate. */
	z->type    = CTLR_SEAGATE;
	z->name    = "Seagate ST01/ST02";
	z->CONTROL = z->addr + 0x1a00; /* ST01/ST02 register offsets */
	z->STATUS  = z->addr + 0x1a00;
	z->DATA    = z->addr + 0x1c00;
	if (sea_init (z) == 0)
		return (1);

	/* Try Future Domain. */
	z->type    = CTLR_FUTURE_DOMAIN;
	z->name    = "Future Domain TMC-885/TMC-950";
	z->CONTROL = z->addr + 0x1c00; /* TMC-885/TMC-950 reg. offsets */
	z->STATUS  = z->addr + 0x1c00;
	z->DATA    = z->addr + 0x1e00;
	/* FD TMC885's can't handle detach & re-attach */
	sea_select_cmd = CMD_DRVR_ENABLE;
	/* FD TMC-885 is supposed to be at id 6. How strange. */
	z->scsi_addr = HOST_SCSI_ADDR - 1;
	z->scsi_id = 1 << z->scsi_addr;
	if (sea_init (z) == 0)
		return (1);

	return (0);
}

/*
 * Probe the adapter, and if found, reset the board and the scsi bus.
 * Return 0 if the adapter found.
 */
int sea_init (adapter_t *z)
{
	volatile u_char *p;
	int i, c;

	/* Check that STATUS..STATUS+200h are equal. */
	p = z->STATUS;
	c = *p;
	if (c == 0xff)
		return (2);
	while (++p < z->STATUS+0x200)
		if (*p != c)
			return (3);

	/* Check that DATA..DATA+200h are equal. */
	for (p=z->DATA, c= *p++; p<z->DATA+0x200; ++p)
		if (*p != c)
			return (4);

	/* Check that addr..addr+1800h are not writable. */
	for (p=z->addr; p<z->addr+0x1800; ++p) {
		c = *p;
		*p = ~c;
		if (*p == ~c) {
			*p = c;
			return (5);
		}
	}

	/* Check that addr+1800h..addr+1880h are writable. */
	for (p=z->addr+0x1800; p<z->addr+0x1880; ++p) {
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

	/* Reset the scsi bus (I don't know if this is needed). */
	*z->CONTROL = CMD_RST | CMD_DRVR_ENABLE | z->parity | CMD_INTR;
	/* Hold reset for at least 25 microseconds. */
	DELAY (25);
	/* Check that status cleared. */
	if (*z->STATUS != 0) {
		*z->CONTROL = 0;
		return (8);
	}

	/* Check that DATA register is writable. */
	for (i=0; i<256; ++i) {
		*z->DATA = i;
		if (*z->DATA != i) {
			*z->CONTROL = 0;
			return (9);
		}
	}

	/* Enable the adapter. */
	*z->CONTROL = CMD_INTR | z->parity;
	/* Wait a Bus Clear Delay (800 ns + bus free delay 800 ns). */
	DELAY (10);

	/* Check that DATA register is NOT writable. */
	c = *z->DATA;
	for (i=0; i<256; ++i) {
		*z->DATA = i;
		if (*z->DATA != c) {
			*z->CONTROL = 0;
			return (10);
		}
	}

	return (0);
}

/*
 * Attach all sub-devices we can find.
 */
int sea_attach (struct isa_device *dev)
{
	int unit = dev->id_unit;
	adapter_t *z = &seadata[unit];
	struct scsibus_data *scbus;

	printf ("\nsea%d: type %s%s\n", unit, z->name,
		(dev->id_flags & FLAG_NOPARITY) ? ", no parity" : "");

	/* fill in the prototype scsi_link */
	z->sc_link.adapter_unit = unit;
	z->sc_link.adapter_targ = z->scsi_addr;
	z->sc_link.adapter_softc = z;
	z->sc_link.adapter = &sea_switch;
	z->sc_link.device = &sea_dev;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return 0;
	scbus->adapter_link = &z->sc_link;

	/* ask the adapter what subunits are present */
	scsi_attachdevs (scbus);

	return (1);
}

/*
 * Return some information to the caller about
 * the adapter and its capabilities.
 */
u_int32_t sea_adapter_info (int unit)
{
	return (1);
}

void seaminphys (struct buf *bp)
{
}

/*
 * Catch an interrupt from the adaptor.
 */
void seaintr (int unit)
{
	adapter_t *z = &seadata[unit];

	PRINT (("sea%d: interrupt status=%b\n", unit, *z->STATUS, STAT_BITS));
	sea_start (z);
}

/*
 * This routine is used in the case when we have no IRQ line (z->irq == 0).
 * It is called every timer tick and polls for reconnect from target.
 */
void sea_tick (void *arg)
{
	adapter_t *z = arg;
	int x = splbio ();

	z->timeout_active = 0;
	sea_start (z);
	if (z->disconnected_queue && ! z->timeout_active) {
		timeout (sea_tick, z, 1);
		z->timeout_active = 1;
	}
	splx (x);
}

/*
 * Start a scsi operation given the command and the data address.
 * Also needs the unit, target and lu.  Get a free scb and set it up.
 * Call send_scb.  Either start timer or wait until done.
 */
int32_t sea_scsi_cmd (struct scsi_xfer *xs)
{
	int flags = xs->flags, x = 0;
	adapter_t *z = (adapter_t *)xs->sc_link->adapter_softc;
	scb_t *scb;

	PRINT (("sea%d/%d/%d command 0x%x\n", unit, xs->sc_link->target,
		xs->sc_link->lun, xs->cmd->opcode)); 
	if (xs->bp)
		flags |= SCSI_NOSLEEP;
	if (flags & ITSDONE) {
		printf ("sea%d: already done?", xs->sc_link->adapter_unit);
		xs->flags &= ~ITSDONE;
	}
	if (! (flags & INUSE)) {
		printf ("sea%d: not in use?", xs->sc_link->adapter_unit);
		xs->flags |= INUSE;
	}
	if (flags & SCSI_RESET)
		printf ("sea%d: SCSI_RESET not implemented\n",
			xs->sc_link->adapter_unit);

	if (! (flags & SCSI_NOMASK))
		x = splbio ();

	/* Get a free scb.
	 * If we can and have to, sleep waiting for one to come free. */
	while (! (scb = z->free_scb)) {
		if (flags & SCSI_NOSLEEP) {
			xs->error = XS_DRIVER_STUFFUP;
			if (! (flags & SCSI_NOMASK))
				splx (x);
			return (TRY_AGAIN_LATER);
		}
		tsleep ((caddr_t)&z->free_scb, PRIBIO, "seascb", 0);
	}
	/* Get scb from free list. */
	z->free_scb = scb->next;
	scb->next = 0;
	scb->flags = SCB_ACTIVE;

	/* Put all the arguments for the xfer in the scb */
	scb->xfer = xs;
	scb->datalen = xs->datalen;
	scb->data = xs->data;

	/* Setup the scb to contain necessary values.
	 * The interesting values can be read from the xs that is saved.
	 * I therefore think that the structure can be kept very small.
	 * The driver doesn't use DMA so the scatter/gather is not needed? */
	if (! z->queue) {
		scb->next = z->queue;
		z->queue = scb;
	} else {
		scb_t *q;

		for (q=z->queue; q->next; q=q->next)
			continue;
		q->next = scb;
		scb->next = 0;  /* placed at the end of the queue */
	}

	/* Try to send this command to the board. */
	sea_start (z);

	/* Usually return SUCCESSFULLY QUEUED. */
	if (! (flags & SCSI_NOMASK)) {
		splx (x);
		if (xs->flags & ITSDONE)
			/* Timeout timer not started, already finished.
			 * Tried to return COMPLETE but the machine hanged
			 * with this. */
			return (SUCCESSFULLY_QUEUED);
		xs->timeout_ch = timeout (sea_timeout, (caddr_t) scb,
					  (xs->timeout * hz) / 1000);
		scb->flags |= SCB_TIMECHK;
		PRINT (("sea%d/%d/%d command queued\n",
			xs->sc_link->adapter_unit,
			xs->sc_link->target, xs->sc_link->lun));
		return (SUCCESSFULLY_QUEUED);
	}

	/* If we can't use interrupts, poll on completion. */
	if (! sea_poll (z, scb)) {
		/* We timed out, so call the timeout handler manually,
		 * accounting for the fact that the clock is not running yet
		 * by taking out the clock queue entry it makes. */
		sea_timeout ((void*) scb);

		/* Because we are polling, take out the timeout entry
		 * sea_timeout made. */
		untimeout (sea_timeout, (void*) scb, xs->timeout_ch);

		if (! sea_poll (z, scb))
			/* We timed out again... This is bad. Notice that
			 * this time there is no clock queue entry to remove. */
			sea_timeout ((void*) scb);
	}
	PRINT (("sea%d/%d/%d command %s\n", xs->sc_link->adapter_unit,
		xs->sc_link->target, xs->sc_link->lun,
		xs->error ? "failed" : "done")); 
	return (xs->error ? HAD_ERROR : COMPLETE);
}

/*
 * Coroutine that runs as long as more work can be done.
 * Both scsi_cmd() and intr() will try to start it in
 * case it is not running.
 * Always called with interrupts disabled.
 */
void sea_start (adapter_t *z)
{
	scb_t *q, *prev;
again:
	/* First check that if any device has tried
	 * a reconnect while we have done other things
	 * with interrupts disabled. */
	if (sea_reselect (z))
		goto again;

	/* Search through the queue for a command
	 * destined for a target that's not busy. */
	for (q=z->queue, prev=0; q; prev=q, q=q->next) {
		/* Attempt to establish an I_T_L nexus here. */
		if (IS_BUSY (z, q) || ! sea_select (z, q))
			continue;

		/* Remove the command from the issue queue. */
		if (prev)
			prev->next = q->next;
		else
			z->queue = q->next;
		q->next = 0;

		/* We are connected. Do the task. */
		sea_information_transfer (z, q);
		goto again;
	}
}

void sea_timeout (void *arg)
{
	scb_t *scb = (scb_t*) arg;
	adapter_t *z = (adapter_t *)scb->xfer->sc_link->adapter_softc;
	int x = splbio ();

	if (! (scb->xfer->flags & SCSI_NOMASK))
		printf ("sea%d/%d/%d (%s%d) timed out\n",
			scb->xfer->sc_link->adapter_unit,
			scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun,
			scb->xfer->sc_link->device->name,
			scb->xfer->sc_link->dev_unit);

	/* If it has been through before, then a previous abort has failed,
	 * don't try abort again. */
	if (! (scb->flags & SCB_ABORTED)) {
		sea_abort (z, scb);
		/* 2 seconds for the abort */
		scb->xfer->timeout_ch = timeout (sea_timeout,
						 (caddr_t)scb, 2*hz);
		scb->flags |= (SCB_ABORTED | SCB_TIMECHK);
	} else {
		/* abort timed out */
		scb->flags |= SCB_ABORTED;
		scb->xfer->retries = 0;
		sea_done (z, scb);
	}
	splx (x);
}

/*
 * Wait until REQ goes down.  This is needed for some devices (CDROMs)
 * after every MSGOUT, MSGIN, CMDOUT, STATIN request.
 * Return true if REQ deassert found.
 */
static inline int sea_wait_for_req_deassert (adapter_t *z, int cnt, char *msg)
{
	asm ("
	1:      testb $0x10, %2
		jz 2f
		loop 1b
	2:"
	: "=c" (cnt)                            /* output */
	: "0" (cnt), "m" (*z->STATUS));         /* input */
	if (! cnt) {
		PRINT (("sea%d (%s) timeout waiting for !REQ\n",
			z->sc_link.adapter_unit, msg));
		return (0);
	}
	/* PRINT (("sea_wait_for_req_deassert %s count=%d\n", msg, cnt)); */
	return (1);
}

/*
 * Establish I_T_L or I_T_L_Q nexus for new or existing command
 * including ARBITRATION, SELECTION, and initial message out
 * for IDENTIFY and queue messages.
 * Return 1 if selection succeded.
 */
int sea_select (volatile adapter_t *z, scb_t *scb)
{
	/* Start arbitration. */
	*z->CONTROL = z->parity | CMD_INTR;
	*z->DATA = z->scsi_id;
	*z->CONTROL = CMD_START_ARB | z->parity;

	/* Wait for arbitration to complete. */
	WAITFOR (*z->STATUS & STAT_ARB_CMPL, "arbitration");
	if (! (*z->STATUS & STAT_ARB_CMPL)) {
		if (*z->STATUS & STAT_SEL) {
			printf ("sea: arbitration lost\n");
			scb->flags |= SCB_ERROR;
		} else {
			printf ("sea: arbitration timeout\n");
			scb->flags |= SCB_TIMEOUT;
		}
		*z->CONTROL = CMD_INTR | z->parity;
		return (0);
	}
	DELAY (1);

	*z->DATA = (1 << scb->xfer->sc_link->target) | z->scsi_id;
	*z->CONTROL = sea_select_cmd | CMD_SEL | z->parity;
	DELAY (2);

	/* Wait for a bsy from target.
	 * If the target is not present on the bus, we get
	 * the timeout.  Don't PRINT any message -- it's not an error. */
	WAITFOR (*z->STATUS & STAT_BSY, 0);
	if (! (*z->STATUS & STAT_BSY)) {
		/* The target does not respond.  Not an error, though. */
		PRINT (("sea%d/%d/%d target does not respond\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		*z->CONTROL = CMD_INTR | z->parity;
		scb->flags |= SCB_TIMEOUT;
		return (0);
	}

	/* Try to make the target to take a message from us.
	 * Should start a MSGOUT phase. */
	*z->CONTROL = sea_select_cmd | z->parity;
	DELAY (15);
	WAITFOR (*z->STATUS & STAT_REQ, 0);

	if (z->type == CTLR_FUTURE_DOMAIN)
		*z->CONTROL = CMD_INTR | z->parity | CMD_DRVR_ENABLE;

	WAITFOR (*z->STATUS & STAT_REQ, 0);
	if (! (*z->STATUS & STAT_REQ)) {
		PRINT (("sea%d/%d/%d timeout waiting for REQ\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		scb->flags |= SCB_ERROR;
		*z->CONTROL = CMD_INTR | z->parity;
		return (0);
	}

	/* Check for phase mismatch. FD 885 always seems to get this wrong! */
	if ((*z->STATUS & PHASE_MASK) != PHASE_MSGOUT && z->type != CTLR_FUTURE_DOMAIN) {
		PRINT (("sea%d/%d/%d waiting for MSGOUT: invalid phase %s\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun,
			PHASE_NAME (*z->STATUS & PHASE_MASK)));
		scb->flags |= SCB_ERROR;
		*z->CONTROL = CMD_INTR | z->parity;
		return (0);
	}

	/* Allow disconnects. (except for FD controllers) */
	if (z->type == CTLR_SEAGATE) {
		*z->CONTROL = CMD_DRVR_ENABLE | z->parity;
		*z->DATA = MSG_IDENTIFY (scb->xfer->sc_link->lun);
		WAITREQ (&z->target[scb->xfer->sc_link->target], msgout, 1000);
	}
	*z->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | z->parity;

	SET_BUSY (z, scb);
	return (1);
}

int sea_reselect (adapter_t *z)
{
	scb_t *q = 0, *prev = 0;
	u_char msg, target_mask, lun;
again:
	/* Wait for a device to win the reselection phase. */
	/* Signals this by asserting the I/O signal. */
	if ((*z->STATUS & (STAT_SEL | STAT_IO | STAT_BSY)) !=
	    (STAT_SEL | STAT_IO))
		return (0);

	/* The data bus contains original initiator id ORed with target id. */
	/* See that we really are the initiator. */
	target_mask = *z->DATA;
	if (! (target_mask & z->scsi_id)) {
		PRINT (("sea%d reselect not for me: mask=0x%x, status=%b\n",
			z->sc_link.adapter_unit, target_mask,
			*z->STATUS, STAT_BITS));
		goto again;
	}

	/* Find target who won. */
	/* Host responds by asserting the BSY signal. */
	/* Target should respond by deasserting the SEL signal. */
	target_mask &= ~z->scsi_id;
	*z->CONTROL = CMD_DRVR_ENABLE | CMD_BSY | z->parity | CMD_INTR;
	WAITFOR (! (*z->STATUS & STAT_SEL), "reselection acknowledge");

	/* Remove the busy status. */
	/* Target should set the MSGIN phase. */
	*z->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | z->parity;
	WAITFOR (*z->STATUS & STAT_REQ, "identify message");

	/* Hope we get an IDENTIFY message. */
	msg = sea_msg_input (z);
	if (MSG_ISIDENT (msg)) {
		/* Find the command corresponding to the I_T_L or I_T_L_Q
		 * nexus we just restablished, and remove it from
		 * the disconnected queue. */
		lun = (msg & 7);
		for (q=z->disconnected_queue; q; prev=q, q=q->next) {
			if (target_mask != (1 << q->xfer->sc_link->target))
				continue;
			if (lun != q->xfer->sc_link->lun)
				continue;
			if (prev)
				prev->next = q->next;
			else
				z->disconnected_queue = q->next;
			q->next = 0;
			PRINT (("sea%d/%d/%d reselect done\n",
				z->sc_link.adapter_unit,
				ffs (target_mask) - 1, lun));
			sea_information_transfer (z, q);
			WAITFOR (! (*z->STATUS & STAT_BSY), "reselect !busy");
			return (1);
		}
	} else
		printf ("sea%d reselect: expecting IDENTIFY, got 0x%x\n",
			z->sc_link.adapter_unit, msg);

	/* Since we have an established nexus that we can't
	 * do anything with, we must abort it. */
	sea_send_abort (z);
	PRINT (("sea%d reselect aborted\n", z->sc_link.adapter_unit));
	WAITFOR (! (*z->STATUS & STAT_BSY), "bus free after reselect abort");
	goto again;
}

/*
 * Send an abort to the target.
 * Return 1 success, 0 on failure.
 * Called on splbio level.
 */
int sea_abort (adapter_t *z, scb_t *scb)
{
	scb_t *q, **prev;

	/* If the command hasn't been issued yet, we simply remove it
	 * from the issue queue. */
	prev = &z->queue;
	for (q=z->queue; q; q=q->next) {
		if (scb == q) {
			(*prev) = q->next;
			q->next = 0;
			return (1);
		}
		prev = &q->next;
	}

	/* If the command is currently disconnected from the bus,
	 * we reconnect the I_T_L or I_T_L_Q nexus associated with it,
	 * go into message out, and send an abort message. */
	for (q=z->disconnected_queue; q; q=q->next) {
		if (scb != q)
			continue;

		if (! sea_select (z, scb))
			return (0);
		sea_send_abort (z);

		prev = &z->disconnected_queue;
		for (q=z->disconnected_queue; q; q=q->next) {
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

	/* Command not found in any queue. */
	return (0);
}

/*
 * The task accomplished, mark the i/o control block as done.
 * Always called with interrupts disabled.
 */
void sea_done (adapter_t *z, scb_t *scb)
{
	struct scsi_xfer *xs = scb->xfer;

	if (scb->flags & SCB_TIMECHK)
		untimeout (sea_timeout, (caddr_t) scb, xs->timeout_ch);

	/* How much of the buffer was not touched. */
	xs->resid = scb->datalen;

	if (scb->flags != SCB_ACTIVE && ! (xs->flags & SCSI_ERR_OK))
		if (scb->flags & (SCB_TIMEOUT | SCB_ABORTED))
			xs->error = XS_TIMEOUT;
		else if (scb->flags & SCB_ERROR)
			xs->error = XS_DRIVER_STUFFUP;
		else if (scb->flags & SCB_TBUSY)
			xs->error = XS_BUSY;
		else if (scb->flags & SCB_SENSE)
			xs->error = XS_SENSE;

	xs->flags |= ITSDONE;

	/* Free the control block. */
	scb->next = z->free_scb;
	z->free_scb = scb;
	scb->flags = SCB_FREE;

	/* If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries. */
	if (! scb->next)
		wakeup ((caddr_t) &z->free_scb);

	scsi_done (xs);
}

/*
 * Wait for completion of command in polled mode.
 * Always called with interrupts masked out.
 */
int sea_poll (adapter_t *z, scb_t *scb)
{
	int count;

	for (count=0; count<30; ++count) {
		DELAY (1000);                   /* delay for a while */
		sea_start (z);                  /* retry operation */
		if (scb->xfer->flags & ITSDONE)
			return (1);             /* all is done */
		if (scb->flags & SCB_TIMEOUT)
			return (0);             /* no target present */
	}
	return (0);
}

/*
 * Send data to the target.
 */
void sea_data_output (adapter_t *z, u_char **pdata, u_long *plen)
{
	volatile u_char *data = *pdata;
	volatile u_long len = *plen;

#ifdef SEA_BLINDTRANSFER
	if (len && !(len % BLOCK_SIZE)) {
		while (len) {
			WAITFOR10 (*z->STATUS & STAT_REQ, "blind block read");
	              asm("
			shr $2, %%ecx;
			cld;
			rep;
			movsl; " : :
			"D" (z->DATA), "S" (data), "c" (BLOCK_SIZE) :
			"cx", "si", "di" );
       			data += BLOCK_SIZE;
			len -= BLOCK_SIZE;
		}
	} else {
#endif
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
		: "=S" (data), "=c" (len)               /* output */
		: "D" (z->DATA), "b" (z->STATUS),       /* input */
			"0" (data), "1" (len)
		: "eax", "ebx", "edi");                 /* clobbered */
#ifdef SEA_BLINDTRANSFER
	}
#endif
	PRINT (("sea (DATAOUT) send %ld bytes\n", *plen - len));
	*plen = len;
	*pdata = (u_char *)data;
}

/*
 * Receive data from the target.
 */
void sea_data_input (adapter_t *z, u_char **pdata, u_long *plen)
{
	volatile u_char *data = *pdata;
	volatile u_long len = *plen;

#ifdef SEA_BLINDTRANSFER
	if (len && !(len % BLOCK_SIZE)) {
		while (len) {
			WAITFOR10 (*z->STATUS & STAT_REQ, "blind block read");
	              asm("
			shr $2, %%ecx;
			cld;
			rep;
			movsl; " : :
			"S" (z->DATA), "D" (data), "c" (BLOCK_SIZE) :
			"cx", "si", "di" );
       			data += BLOCK_SIZE;
			len -= BLOCK_SIZE;
		}
	} else {
#endif
		if (len >= 512) {
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
			: "=D" (data), "=c" (len)               /* output */
			: "b" (z->DATA), "S" (z->STATUS),
				"0" (data), "1" (len)           /* input */
			: "eax", "ebx", "esi");                 /* clobbered */
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
			: "=D" (data), "=c" (len)               /* output */
			: "b" (z->DATA), "S" (z->STATUS),
				"0" (data), "1" (len)           /* input */
			: "eax", "ebx", "esi");                 /* clobbered */
		}
#ifdef SEA_BLINDTRANSFER
	}
#endif
	PRINT (("sea (DATAIN) got %ld bytes\n", *plen - len));
	*plen = len;
	*pdata = (u_char *)data;
}

/*
 * Send the command to the target.
 */
void sea_cmd_output (target_t *t, u_char *cmd, int cmdlen)
{
	adapter_t *z = t->adapter;

	PRINT (("sea%d send command (%d bytes) ", z->sc_link.adapter_unit,
		cmdlen));

	PRINT (("%x", *cmd));
	*z->DATA = *cmd++;
	if (z->type == CTLR_SEAGATE)
		WAITREQ (t, cmdout1, 10000);
	--cmdlen;

	while (cmdlen) {
		/* Check for target disconnect. */
		u_char sts = *z->STATUS;
		if (! (sts & STAT_BSY))
			break;

		/* Check for phase mismatch. FD 885 seems to get this wrong! */
		if ((sts & PHASE_MASK) != PHASE_CMDOUT && z->type != CTLR_FUTURE_DOMAIN) {
			printf ("sea: sea_cmd_output: invalid phase %s\n",
				PHASE_NAME (sts & PHASE_MASK));
			return;
		}

		/* Wait for REQ. */
		if (! (sts & STAT_REQ))
			continue;

		PRINT (("-%x", *cmd));
		*z->DATA = *cmd++;
		if (z->type == CTLR_SEAGATE)
			WAITREQ (t, cmdout, 1000);
		--cmdlen;
	}
	PRINT (("\n"));
}

/*
 * Send the message to the target.
 */
void sea_send_abort (adapter_t *z)
{
	u_char sts;

	*z->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | CMD_ATTN | z->parity;

	/* Wait for REQ, after which the phase bits will be valid. */
	WAITFOR (*z->STATUS & STAT_REQ, "abort message");
	sts = *z->STATUS;
	if (! (sts & STAT_REQ))
		goto ret;

	/* Check for phase mismatch. */
	if ((sts & PHASE_MASK) != PHASE_MSGOUT) {
		printf ("sea: sending MSG_ABORT: invalid phase %s\n",
			PHASE_NAME (sts & PHASE_MASK));
		goto ret;
	}

	*z->DATA = MSG_ABORT;
	sea_wait_for_req_deassert (z, 1000, "MSG_OUTPUT");
	PRINT (("sea%d send abort message\n", z->sc_link.adapter_unit));
ret:
	*z->CONTROL = CMD_INTR | CMD_DRVR_ENABLE | z->parity;
}

/*
 * Get the message from the target.
 * Return the length of the received message.
 */
u_char sea_msg_input (adapter_t *z)
{
	u_char sts, msg;

	/* Wait for REQ, after which the phase bits will be valid. */
	WAITFOR (*z->STATUS & STAT_REQ, "message input");
	sts = *z->STATUS;
	if (! (sts & STAT_REQ))
		return (MSG_ABORT);

	/* Check for phase mismatch.
	 * Reached if the target decides that it has finished the transfer. */
	if ((sts & PHASE_MASK) != PHASE_MSGIN) {
		printf ("sea: sea_msg_input: invalid phase %s\n",
			PHASE_NAME (sts & PHASE_MASK));
		return (MSG_ABORT);
	}

	/* Do actual transfer from SCSI bus to/from memory. */
	msg = *z->DATA;
	sea_wait_for_req_deassert (z, 1000, "MSG_INPUT");
	PRINT (("sea%d (MSG_INPUT) got 0x%x\n", z->sc_link.adapter_unit, msg));
	return (msg);
}

/*
 * Send request-sense op to the target.
 * Return 1 success, 0 on failure.
 * Called on splbio level.
 */
int sea_sense (adapter_t *z, scb_t *scb)
{
	u_char cmd[6], status, msg, *data;
	u_long len;

	/* Wait for target to disconnect. */
	WAITFOR (! (*z->STATUS & STAT_BSY), "sense bus free");
	if (*z->STATUS & STAT_BSY)
		return (0);

	/* Select the target again. */
	if (! sea_select (z, scb))
		return (0);

	/* Wait for CMDOUT phase. */
	WAITFOR (*z->STATUS & STAT_REQ, "sense CMDOUT");
	if (! (*z->STATUS & STAT_REQ) ||
	    (*z->STATUS & PHASE_MASK) != PHASE_CMDOUT)
		return (0);

	/* Send command. */
	len = sizeof (scb->xfer->sense);
	cmd[0] = REQUEST_SENSE;
	cmd[1] = scb->xfer->sc_link->lun << 5;
	cmd[2] = 0;
	cmd[3] = 0;
	cmd[4] = len;
	cmd[5] = 0;
	sea_cmd_output (&z->target[scb->xfer->sc_link->target],
		cmd, sizeof (cmd));

	/* Wait for DATAIN phase. */
	WAITFOR (*z->STATUS & STAT_REQ, "sense DATAIN");
	if (! (*z->STATUS & STAT_REQ) ||
	    (*z->STATUS & PHASE_MASK) != PHASE_DATAIN)
		return (0);

	data = (u_char*) &scb->xfer->sense;
	sea_data_input (z, &data, &len);
	PRINT (("sea%d sense %x-%x-%x-%x-%x-%x-%x-%x\n",
		z->sc_link.adapter_unit, scb->xfer->sense.error_code,
		scb->xfer->sense.ext.extended.segment,
		scb->xfer->sense.ext.extended.flags,
		scb->xfer->sense.ext.extended.info[0],
		scb->xfer->sense.ext.extended.info[1],
		scb->xfer->sense.ext.extended.info[2],
		scb->xfer->sense.ext.extended.info[3],
		scb->xfer->sense.ext.extended.extra_len));

	/* Wait for STATIN phase. */
	WAITFOR (*z->STATUS & STAT_REQ, "sense STATIN");
	if (! (*z->STATUS & STAT_REQ) ||
	    (*z->STATUS & PHASE_MASK) != PHASE_STATIN)
		return (0);

	status = *z->DATA;

	/* Wait for MSGIN phase. */
	WAITFOR (*z->STATUS & STAT_REQ, "sense MSGIN");
	if (! (*z->STATUS & STAT_REQ) ||
	    (*z->STATUS & PHASE_MASK) != PHASE_MSGIN)
		return (0);

	msg = *z->DATA;

	if (status != 0 || msg != 0)
		printf ("sea%d: bad sense status=0x%x, msg=0x%x\n",
			z->sc_link.adapter_unit, status, msg);
	return (1);
}

/*
 * Do the transfer. We know we are connected. Update the flags,
 * call sea_done when task accomplished. Dialog controlled by the target.
 * Always called with interrupts disabled.
 */
void sea_information_transfer (adapter_t *z, scb_t *scb)
{
	u_char *data = scb->data;               /* current data buffer */
	u_long datalen = scb->datalen;          /* current data transfer size */
	target_t *t = &z->target[scb->xfer->sc_link->target];
	register u_char sts;
	u_char msg;

	while ((sts = *z->STATUS) & STAT_BSY) {
		/* We only have a valid SCSI phase when REQ is asserted. */
		if (! (sts & STAT_REQ))
			continue;
		if (sts & STAT_PARITY) {
			int target = scb->xfer->sc_link->target;
			if (++z->target[target].perrcnt <= 8)
				printf ("sea%d/%d/%d parity error\n",
					z->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
			if (z->target[target].perrcnt == 8)
				printf ("sea%d/%d/%d too many parity errors, not logging any more\n",
					z->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
		}
		switch (sts & PHASE_MASK) {
		case PHASE_DATAOUT:
			if (datalen <= 0) {
				printf ("sea%d/%d/%d data length underflow\n",
					z->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun);
				/* send zero byte */
				*z->DATA = 0;
				break;
			}
			sea_data_output (z, &data, &datalen);
			break;
		case PHASE_DATAIN:
			if (datalen <= 0) {
				/* Get extra data.  Some devices (e.g. CDROMs)
				 * use fixed-length blocks (e.g. 2k),
				 * even if we need less. */
				PRINT (("@"));
				sts = *z->DATA;
				break;
			}
			sea_data_input (z, &data, &datalen);
			break;
		case PHASE_CMDOUT:
			sea_cmd_output (t, (u_char*) scb->xfer->cmd,
				scb->xfer->cmdlen);
			break;
		case PHASE_STATIN:
			scb->xfer->status = *z->DATA;
			if (z->type == CTLR_SEAGATE)
				WAITREQ (t, statin, 2000);
			PRINT (("sea%d/%d/%d (STATIN) got 0x%x\n",
				z->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun,
				(u_char) scb->xfer->status));
			break;
		case PHASE_MSGOUT:
			/* Send no-op message. */
			*z->DATA = MSG_NOP;
			sea_wait_for_req_deassert (z, 1000, "MSGOUT");
			PRINT (("sea%d/%d/%d (MSGOUT) send NOP\n",
				z->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun));
			break;
		case PHASE_MSGIN:
			/* Don't handle multi-byte messages here, because they
			 * should not be present here. */
			msg = *z->DATA;
			sea_wait_for_req_deassert (z, 2000, "MSGIN");
			PRINT (("sea%d/%d/%d (MSGIN) got 0x%x\n",
				z->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun, msg));
			switch (msg) {
			case MSG_COMMAND_COMPLETE:
				scb->data = data;
				scb->datalen = datalen;
				/* In the case of check-condition status,
				 * perform the request-sense op. */
				switch (scb->xfer->status & 0x1e) {
				case SCSI_CHECK:
					if (sea_sense (z, scb))
						scb->flags = SCB_SENSE;
					break;
				case SCSI_BUSY:
					scb->flags = SCB_TBUSY;
					break;
				}
				goto done;
			case MSG_ABORT:
				printf ("sea: command aborted by target\n");
				scb->flags = SCB_ABORTED;
				goto done;
			case MSG_MESSAGE_REJECT:
				printf ("sea: message rejected\n");
				scb->flags = SCB_ABORTED;
				goto done;
			case MSG_DISCONNECT:
				scb->next = z->disconnected_queue;
				z->disconnected_queue = scb;
				if (! z->irq && ! z->timeout_active) {
					timeout (sea_tick, z, 1);
					z->timeout_active = 1;
				}
				PRINT (("sea%d/%d/%d disconnected\n",
					z->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun));
				goto ret;
			case MSG_SAVE_POINTERS:
				scb->data = data;
				scb->datalen = datalen;
				break;
			case MSG_RESTORE_POINTERS:
				data = scb->data;
				datalen = scb->datalen;
				break;
			default:
				printf ("sea%d/%d/%d unknown message: 0x%x\n",
					z->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun, msg);
				break;
			}
			break;
		default:
			printf ("sea: unknown phase: %b\n", sts, STAT_BITS);
			break;
		}
	}
	printf ("sea%d/%d/%d unexpected target disconnect\n",
		z->sc_link.adapter_unit, scb->xfer->sc_link->target,
		scb->xfer->sc_link->lun);
	scb->flags = SCB_ERROR;
done:
	CLEAR_BUSY (z, scb);
	sea_done (z, scb);
ret:
	*z->CONTROL = CMD_INTR | z->parity;
}
#endif /* NSEA */

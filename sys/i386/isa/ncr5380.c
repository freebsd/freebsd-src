/*
 * FreeBSD generic NCR-5380/NCR-53C400 SCSI driver
 *
 * Copyright (C) 1994 Serge Vakulenko (vak@cronyx.ru)
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
 * Tested on the following hardware:
 *  Adapter: Trantor T130
 * Streamer: Archive Viper 150,
 *   CD-ROM: NEC CDR-25
 */
#undef DEBUG

#include "nca.h"
#if NNCA > 0

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/devconf.h>

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/ic/ncr5380.h>
#include <i386/isa/ic/ncr53400.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef DEBUG
#   define PRINT(s)     printf s
#else
#   define PRINT(s)     /*void*/
#endif

#define SCB_TABLE_SIZE	8	/* start with 8 scb entries in table */
#define BLOCK_SIZE	512	/* size of READ/WRITE areas on SCSI card */
#define HOST_SCSI_ADDR  7       /* address of the adapter on the SCSI bus */

/*
 * Defice config flags
 */
#define FLAG_NOPARITY   0x01    /* disable SCSI bus parity check */

/*
 * ProAudioSpectrum registers
 */
#define PAS16_DATA              8       /* Data Register */
#define PAS16_STAT              9       /* Status Register */
#define PAS16_STAT_DREQ         0x80    /* Pseudo-DMA ready bit */
#define PAS16_REG(r)            (((r) & 0xc) << 11 | ((r) & 3))

static u_char pas16_irq_magic[] =
    { 0,  0,  1,  2,  3,  4,  5,  6, 0,  0,  7,  8,  9,  0, 10, 11 };

/*
 * SCSI bus phases
 */
#define PHASE_MASK              (CSBR_MSG | CSBR_CD | CSBR_IO)
#define PHASE_DATAOUT           0
#define PHASE_DATAIN            CSBR_IO
#define PHASE_CMDOUT            CSBR_CD
#define PHASE_STATIN            (CSBR_CD | CSBR_IO)
#define PHASE_MSGOUT            (CSBR_MSG | CSBR_CD)
#define PHASE_MSGIN             (CSBR_MSG | CSBR_CD | CSBR_IO)
#define PHASE_NAME(ph)          phase_name[(ph)>>2]
#define PHASE_TO_TCR(ph)        ((ph) >> 2)

static char *phase_name[] = {
	"DATAOUT", "DATAIN",  "CMDOUT", "STATIN",
	"Phase4?", "Phase5?", "MSGOUT", "MSGIN",
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
	int32 datalen;			/* bytes remaining to transfer */;
} scb_t;

typedef enum {
	CTLR_NONE,
	CTLR_NCR_5380,
	CTLR_NCR_53C400,
	CTLR_PAS_16,
} ctlr_t;

/*
 * Data structure describing the target state.
 */
typedef struct {
	u_char  busy;                   /* mask of busy luns at device target */
	u_long  perrcnt;                /* counter of target parity errors */
} target_t;

/*
 * Data structure describing current status of the scsi bus. One for each
 * controller card.
 */
typedef struct {
	ctlr_t  type;                   /* Seagate or Future Domain */
	char    *name;                  /* adapter name */

	/* NCR-5380 controller registers */
	u_short ODR;            /* (wo-0) Output Data Register */
	u_short CSDR;           /* (ro-0) Current SCSI Data Register */
	u_short ICR;            /* (rw-1) Initiator Command Register */
	u_short MR;             /* (rw-2) Mode Register */
	u_short TCR;            /* (rw-3) Target Command Register */
	u_short SER;            /* (wo-4) Select Enable Register */
	u_short CSBR;           /* (ro-4) Current SCSI Bus Status Register */
	u_short BSR;            /* (ro-5) Bus and Status Register */
	u_short SDSR;           /* (wo-5) Start DMA Send Register */
	u_short SDIR;           /* (wo-7) Start DMA Initiator Receive Register */
	u_short RPIR;           /* (ro-7) Reset Parity/Interrupt Register */

	/* NCR-53C400 controller registers */
	u_short CSR;            /* (rw-0) Control and Status Register */
	u_short CCR;            /* (rw-1) Clock Counter Register */
	u_short HBR;            /* (rw-4) Host Buffer Register */

	/* ProAudioSpectrum controller registers */
	u_short PDATA;          /* (rw) Pseudo-DMA Data Register */
	u_short PSTAT;          /* (rw) Pseudo-DMA Status Register */

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

adapter_t ncadata[NNCA];

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
#define WAITFOR(condition,count,message) {\
	register u_long cnt = count; char *msg = message;\
	while (cnt-- && ! (condition)) continue;\
	if (cnt == -1 && msg)\
		printf ("nca: %s timeout\n", msg); }

static int nca_probe (struct isa_device *dev);
static int nca_attach (struct isa_device *dev);
static int32 nca_scsi_cmd (struct scsi_xfer *xs);
static u_int32 nca_adapter_info (int unit);
static void nca_timeout (void *scb);
static void ncaminphys (struct buf *bp);
static void nca_done (adapter_t *z, scb_t *scb);
static void nca_start (adapter_t *z);
static void nca_information_transfer (adapter_t *z, scb_t *scb);
static int nca_poll (adapter_t *z, scb_t *scb);
static int nca_init (adapter_t *z);
static int nca_reselect (adapter_t *z);
static int nca_select (adapter_t *z, scb_t *scb);
static int nca_abort (adapter_t *z, scb_t *scb);
static void nca_send_abort (adapter_t *z);
static u_char nca_msg_input (adapter_t *z);
static void nca_tick (void *arg);
static int nca_sense (adapter_t *z, scb_t *scb);
static void nca_data_output (adapter_t *z, u_char **pdata, u_long *plen);
static void nca_data_input (adapter_t *z, u_char **pdata, u_long *plen);
static void nca_cmd_output (adapter_t *z, u_char *cmd, int cmdlen);
static void nca_53400_dma_xfer (adapter_t *z, int r, u_char **dat, u_long *len);
static void nca_pas_dma_xfer (adapter_t *z, int r, u_char **dat, u_long *len);

static struct scsi_adapter nca_switch = {
	nca_scsi_cmd, ncaminphys, 0, 0, nca_adapter_info, "nca", {0},
};
static struct scsi_device nca_dev = { NULL, NULL, NULL, NULL, "nca", 0, {0} };
struct isa_driver ncadriver = { nca_probe, nca_attach, "nca" };

static char nca_description [80];
static struct kern_devconf nca_kdc[NNCA] = {{
	0, 0, 0, "nca", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN, &kdc_isa0, 0,
	DC_UNCONFIGURED, nca_description,
	DC_CLS_MISC		/* host adapters aren't special */
}};

/*
 * Check if the device can be found at the port given and if so,
 * detect the type of board. Set it up ready for further work.
 * Takes the isa_dev structure from autoconf as an argument.
 * Returns 1 if card recognized, 0 if errors.
 */
int nca_probe (struct isa_device *dev)
{
	adapter_t *z = &ncadata[dev->id_unit];
	int i;

	if (dev->id_unit)
		nca_kdc[dev->id_unit] = nca_kdc[0];
	nca_kdc[dev->id_unit].kdc_unit = dev->id_unit;
	nca_kdc[dev->id_unit].kdc_isa = dev;
	dev_attach (&nca_kdc[dev->id_unit]);

	/* Init fields used by our routines */
	z->parity = (dev->id_flags & FLAG_NOPARITY) ? 0 :
		MR_ENABLE_PARITY_CHECKING;
	z->scsi_addr = HOST_SCSI_ADDR;
	z->scsi_id = 1 << z->scsi_addr;
	z->irq = dev->id_irq ? ffs (dev->id_irq) - 1 : 0;
	z->queue = 0;
	z->disconnected_queue = 0;
	for (i=0; i<8; i++)
		z->target[i].busy = 0;

	/* Link up the free list of scbs */
	z->numscb = SCB_TABLE_SIZE;
	z->free_scb = z->scbs;
	for (i=1; i<SCB_TABLE_SIZE; i++)
		z->scbs[i-1].next = z->scbs + i;
	z->scbs[SCB_TABLE_SIZE-1].next = 0;

	/* Try NCR 5380. */
	z->type  = CTLR_NCR_5380;
	z->name  = "NCR-5380";
	z->ODR   = dev->id_iobase + C80_ODR;
	z->CSDR  = dev->id_iobase + C80_CSDR;
	z->ICR   = dev->id_iobase + C80_ICR;
	z->MR    = dev->id_iobase + C80_MR;
	z->TCR   = dev->id_iobase + C80_TCR;
	z->SER   = dev->id_iobase + C80_SER;
	z->CSBR  = dev->id_iobase + C80_CSBR;
	z->BSR   = dev->id_iobase + C80_BSR;
	z->SDSR  = dev->id_iobase + C80_SDSR;
	z->SDIR  = dev->id_iobase + C80_SDIR;
	z->RPIR  = dev->id_iobase + C80_RPIR;
	z->CSR   = 0;
	z->CCR   = 0;
	z->HBR   = 0;
	z->PDATA = 0;
	z->PSTAT = 0;
	if (nca_init (z) == 0)
		return (8);

	/* Try NCR 53C400. */
	z->type  = CTLR_NCR_53C400;
	z->name  = "NCR-53C400";
	z->ODR   = dev->id_iobase + C400_5380_REG_OFFSET + C80_ODR;
	z->CSDR  = dev->id_iobase + C400_5380_REG_OFFSET + C80_CSDR;
	z->ICR   = dev->id_iobase + C400_5380_REG_OFFSET + C80_ICR;
	z->MR    = dev->id_iobase + C400_5380_REG_OFFSET + C80_MR;
	z->TCR   = dev->id_iobase + C400_5380_REG_OFFSET + C80_TCR;
	z->SER   = dev->id_iobase + C400_5380_REG_OFFSET + C80_SER;
	z->CSBR  = dev->id_iobase + C400_5380_REG_OFFSET + C80_CSBR;
	z->BSR   = dev->id_iobase + C400_5380_REG_OFFSET + C80_BSR;
	z->SDSR  = dev->id_iobase + C400_5380_REG_OFFSET + C80_SDSR;
	z->SDIR  = dev->id_iobase + C400_5380_REG_OFFSET + C80_SDIR;
	z->RPIR  = dev->id_iobase + C400_5380_REG_OFFSET + C80_RPIR;
	z->CSR   = dev->id_iobase + C400_CSR;
	z->CCR   = dev->id_iobase + C400_CCR;
	z->HBR   = dev->id_iobase + C400_HBR;
	z->PDATA = 0;
	z->PSTAT = 0;
	if (nca_init (z) == 0)
		return (16);

	/* Try ProAudioSpectrum-16. */
	z->type  = CTLR_PAS_16;
	z->name  = "ProAudioSpectrum"; /* changed later */
	z->ODR   = dev->id_iobase ^ PAS16_REG (C80_ODR);
	z->CSDR  = dev->id_iobase ^ PAS16_REG (C80_CSDR);
	z->ICR   = dev->id_iobase ^ PAS16_REG (C80_ICR);
	z->MR    = dev->id_iobase ^ PAS16_REG (C80_MR);
	z->TCR   = dev->id_iobase ^ PAS16_REG (C80_TCR);
	z->SER   = dev->id_iobase ^ PAS16_REG (C80_SER);
	z->CSBR  = dev->id_iobase ^ PAS16_REG (C80_CSBR);
	z->BSR   = dev->id_iobase ^ PAS16_REG (C80_BSR);
	z->SDSR  = dev->id_iobase ^ PAS16_REG (C80_SDSR);
	z->SDIR  = dev->id_iobase ^ PAS16_REG (C80_SDIR);
	z->RPIR  = dev->id_iobase ^ PAS16_REG (C80_RPIR);
	z->CSR   = 0;
	z->CCR   = 0;
	z->HBR   = 0;
	z->PDATA = dev->id_iobase ^ PAS16_REG (PAS16_DATA);
	z->PSTAT = dev->id_iobase ^ PAS16_REG (PAS16_STAT);
	if (nca_init (z) == 0)
		return (4);

	bzero (z, sizeof (*z));
	return (0);
}

/*
 * Probe the adapter, and if found, reset the board and the scsi bus.
 * Return 0 if the adapter found.
 */
int nca_init (adapter_t *z)
{
	int i, c;

	if (z->type == CTLR_NCR_53C400) {
		if (inb (z->CSR) == 0xFF)
			return (100);

		/* Reset 53C400. */
		outb (z->CSR, CSR_5380_ENABLE);

		/* Enable interrupts. */
		outb (z->CSR, z->irq ? CSR_5380_INTR : 0);
	}

	if (z->type == CTLR_PAS_16) {
		u_short base = z->PDATA & 0x3FF;

		outb (0x9a01, 0xbc + (z-ncadata));      /* unit number */
		outb (0x9a01, base >> 2);

		if (inb (base^0x803) == 0xFF)
			return (200);

		if (inb (z->CSDR) == 0xFF && inb (z->CSDR^0x2000) == 0xFF &&
		    inb (z->CSDR) == 0xFF && inb (z->CSDR^0x2000) == 0xFF &&
		    inb (z->CSDR) == 0xFF && inb (z->CSDR^0x2000) == 0xFF &&
		    inb (z->CSDR) == 0xFF && inb (z->CSDR^0x2000) == 0xFF)
			return (201);

		i = inb (base^0x803);
		outb (base^0x803, i ^ 0xE0);
		c = inb (base^0x803);
		outb (base^0x803, 1);
		if (i != c)
			return (202);

		/* Various magic. */
		outb (base^0x4000, 0x30);  /* Timeout counter */
		outb (base^0x4001, 0x01);  /* Reset TC */
		outb (base^0xbc00, 0x01);  /* 1 Wait state */
		outb (base^0x8003, 0x4d);  /* sysconfig_4 */
		i = pas16_irq_magic[z->irq];
		if (!i) {
		    z->irq = 0;
		} else {
		    outb (base^0xf002, i << 4);
		    outb (base^0x8003, 0x6d);  /* sysconfig_4 */
		}

		switch (inb (base^0xEC03) & 0xF) {
		case 6:  z->name = "ProAudioSpectrum-Plus"; break;
		case 12: z->name = "ProAudioSpectrum-16D";  break;
		case 14: z->name = "ProAudioSpectrum-CDPC"; break;
		case 15: z->name = "ProAudioSpectrum-16";   break;
		default: return (203);
		}
	}

	/* Read RPI port, resetting parity/interrupt state. */
	inb (z->RPIR);

	/* Test BSR: parity error, interrupt request and busy loss state
	 * should be cleared. */
	if (inb (z->BSR) & (BSR_PARITY_ERROR |
	    BSR_INTERRUPT_REQUEST_ACTIVE | BSR_BUSY_ERROR)) {
		PRINT (("nca: invalid bsr[0x%x]=%b\n", z->BSR,
			inb (z->BSR), BSR_BITS));
		return (1);
	}

	/* Reset the SCSI bus. */
	outb (z->ICR, ICR_ASSERT_RST);
	outb (z->ODR, 0);
	/* Hold reset for at least 25 microseconds. */
	DELAY (25);
	/* Check that status cleared. */
	if (inb (z->CSBR) != CSBR_RST) {
		PRINT (("nca: invalid csbr[0x%x]=%b\n", z->CSBR,
			inb (z->CSBR), CSBR_BITS));
		outb (z->ICR, 0);
		return (2);
	}
	/* Clear reset. */
	outb (z->ICR, 0);
	/* Wait a Bus Clear Delay (800 ns + bus free delay 800 ns). */
	DELAY (2);

	/* Enable data drivers. */
	outb (z->ICR, ICR_ASSERT_DATA_BUS);

	/* Check that data register is writable. */
	for (i=0; i<256; ++i) {
		outb (z->ODR, i);
		DELAY (1);
		if (inb (z->CSDR) != i) {
			PRINT (("nca: ODR[0x%x] not writable: 0x%x should be 0x%x\n",
				z->ODR, inb (z->CSDR), i));
			outb (z->ICR, 0);
			return (3);
		}
	}

	/* Disable data drivers. */
	outb (z->ICR, 0);

	/* Check that data register is NOT writable. */
	c = inb (z->CSDR);
	for (i=0; i<256; ++i) {
		outb (z->ODR, i);
		DELAY (1);
		if (inb (z->CSDR) != c) {
			PRINT (("nca: ODR[0x%x] writable: 0x%x should be 0x%x\n",
				z->ODR, inb (z->CSDR), c));
			return (4);
		}
	}

	/* Initialize the controller. */
	outb (z->MR, z->parity);
	outb (z->TCR, 0);
	outb (z->SER, z->scsi_id);
	return (0);
}

/*
 * Attach all sub-devices we can find.
 */
int nca_attach (struct isa_device *dev)
{
	int unit = dev->id_unit;
	adapter_t *z = &ncadata[unit];
	struct scsibus_data *scbus;

	sprintf (nca_description, "%s SCSI controller", z->name);
	printf ("nca%d: type %s%s\n", unit, z->name,
		(dev->id_flags & FLAG_NOPARITY) ? ", no parity" : "");

	/* fill in the prototype scsi_link */
	z->sc_link.adapter_unit = unit;
	z->sc_link.adapter_targ = z->scsi_addr;
	z->sc_link.adapter = &nca_switch;
	z->sc_link.device = &nca_dev;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
	        return 0;
	scbus->adapter_link = &z->sc_link;

	/* ask the adapter what subunits are present */
	nca_kdc[unit].kdc_state = DC_BUSY;
	scsi_attachdevs (scbus);

	return (1);
}

/*
 * Return some information to the caller about
 * the adapter and its capabilities.
 */
u_int32 nca_adapter_info (int unit)
{
	return (1);
}

void ncaminphys (struct buf *bp)
{
}

/*
 * Catch an interrupt from the adaptor.
 */
void ncaintr (int unit)
{
	adapter_t *z = &ncadata[unit];

	PRINT (("nca%d: interrupt bsr=%b csbr=%b\n", unit,
		inb (z->BSR), BSR_BITS, inb (z->CSBR), CSBR_BITS));
	nca_start (z);
	/* Reset interrupt state. */
	inb (z->RPIR);
}

/*
 * This routine is used in the case when we have no IRQ line (z->irq == 0).
 * It is called every timer tick and polls for reconnect from target.
 */
void nca_tick (void *arg)
{
	adapter_t *z = arg;
	int x = splbio ();

	z->timeout_active = 0;
	nca_start (z);
	/* Reset interrupt state. */
	inb (z->RPIR);
	if (z->disconnected_queue && ! z->timeout_active) {
		timeout (nca_tick, z, 1);
		z->timeout_active = 1;
	}
	splx (x);
}

/*
 * Start a scsi operation given the command and the data address.
 * Also needs the unit, target and lu.  Get a free scb and set it up.
 * Call send_scb.  Either start timer or wait until done.
 */
int32 nca_scsi_cmd (struct scsi_xfer *xs)
{
	int unit = xs->sc_link->adapter_unit, flags = xs->flags, x = 0;
	adapter_t *z = &ncadata[unit];
	scb_t *scb;

	/* PRINT (("nca%d/%d/%d command 0x%x\n", unit, xs->sc_link->target,
		xs->sc_link->lun, xs->cmd->opcode)); */
	if (xs->bp)
		flags |= SCSI_NOSLEEP;
	if (flags & ITSDONE) {
		printf ("nca%d: already done?", unit);
		xs->flags &= ~ITSDONE;
	}
	if (! (flags & INUSE)) {
		printf ("nca%d: not in use?", unit);
		xs->flags |= INUSE;
	}
	if (flags & SCSI_RESET)
		printf ("nca%d: SCSI_RESET not implemented\n", unit);

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
		tsleep ((caddr_t)&z->free_scb, PRIBIO, "ncascb", 0);
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
	nca_start (z);

	/* Usually return SUCCESSFULLY QUEUED. */
	if (! (flags & SCSI_NOMASK)) {
		splx (x);
		if (xs->flags & ITSDONE)
			/* Timeout timer not started, already finished.
			 * Tried to return COMPLETE but the machine hanged
			 * with this. */
			return (SUCCESSFULLY_QUEUED);
		timeout (nca_timeout, (caddr_t) scb, (xs->timeout * hz) / 1000);
		scb->flags |= SCB_TIMECHK;
		PRINT (("nca%d/%d/%d command queued\n", unit,
			xs->sc_link->target, xs->sc_link->lun));
		return (SUCCESSFULLY_QUEUED);
	}

	/* If we can't use interrupts, poll on completion. */
	if (! nca_poll (z, scb)) {
		/* We timed out, so call the timeout handler manually,
		 * accounting for the fact that the clock is not running yet
		 * by taking out the clock queue entry it makes. */
		nca_timeout ((void*) scb);

		/* Because we are polling, take out the timeout entry
		 * nca_timeout made. */
		untimeout (nca_timeout, (void*) scb);

		if (! nca_poll (z, scb))
			/* We timed out again... This is bad. Notice that
			 * this time there is no clock queue entry to remove. */
			nca_timeout ((void*) scb);
	}
	/* PRINT (("nca%d/%d/%d command %s\n", unit,
		xs->sc_link->target, xs->sc_link->lun,
		xs->error ? "failed" : "done")); */
	return (xs->error ? HAD_ERROR : COMPLETE);
}

/*
 * Coroutine that runs as long as more work can be done.
 * Both scsi_cmd() and intr() will try to start it in
 * case it is not running.
 * Always called with interrupts disabled.
 */
void nca_start (adapter_t *z)
{
	scb_t *q, *prev;
again:
	/* First check that if any device has tried
	 * a reconnect while we have done other things
	 * with interrupts disabled. */
	if (nca_reselect (z))
		goto again;

	/* Search through the queue for a command
	 * destined for a target that's not busy. */
	for (q=z->queue, prev=0; q; prev=q, q=q->next) {
		/* Attempt to establish an I_T_L nexus here. */
		if (IS_BUSY (z, q) || ! nca_select (z, q))
			continue;

		/* Remove the command from the issue queue. */
		if (prev)
			prev->next = q->next;
		else
			z->queue = q->next;
		q->next = 0;

		/* We are connected. Do the task. */
		nca_information_transfer (z, q);
		goto again;
	}
}

void nca_timeout (void *arg)
{
	scb_t *scb = (scb_t*) arg;
	int unit = scb->xfer->sc_link->adapter_unit;
	adapter_t *z = &ncadata[unit];
	int x = splbio ();

	if (! (scb->xfer->flags & SCSI_NOMASK))
		printf ("nca%d/%d/%d (%s%d) timed out\n", unit,
			scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun,
			scb->xfer->sc_link->device->name,
			scb->xfer->sc_link->dev_unit);

	/* If it has been through before, then a previous abort has failed,
	 * don't try abort again. */
	if (! (scb->flags & SCB_ABORTED)) {
		nca_abort (z, scb);
		/* 2 seconds for the abort */
		timeout (nca_timeout, (caddr_t)scb, 2*hz);
		scb->flags |= (SCB_ABORTED | SCB_TIMECHK);
	} else {
		/* abort timed out */
		scb->flags |= SCB_ABORTED;
		scb->xfer->retries = 0;
		nca_done (z, scb);
	}
	splx (x);
}

static inline void nca_sendbyte (adapter_t *z, u_char data)
{
	outb (z->ODR, data);
	outb (z->ICR, ICR_ASSERT_DATA_BUS | ICR_ASSERT_ACK);
	WAITFOR (! (inb (z->CSBR) & CSBR_REQ), 10000, "sendbyte");
	outb (z->ICR, ICR_ASSERT_DATA_BUS);
}

static inline u_char nca_recvbyte (adapter_t *z)
{
	u_char data;

	data = inb (z->CSDR);
	outb (z->ICR, ICR_ASSERT_ACK);
	WAITFOR (! (inb (z->CSBR) & CSBR_REQ), 10000, "recvbyte");
	outb (z->ICR, 0);
	return (data);
}

/*
 * Establish I_T_L or I_T_L_Q nexus for new or existing command
 * including ARBITRATION, SELECTION, and initial message out
 * for IDENTIFY and queue messages.
 * Return 1 if selection succeded.
 */
int nca_select (adapter_t *z, scb_t *scb)
{
	/* Set the phase bits to 0, otherwise the NCR5380 won't drive the
	 * data bus during SELECTION. */
	outb (z->TCR, 0);

	/* Start arbitration. */
	outb (z->ODR, z->scsi_id);
	outb (z->MR, MR_ARBITRATE);

	/* Wait for arbitration logic to complete (20 usec) */
	WAITFOR (inb (z->ICR) & ICR_ARBITRATION_IN_PROGRESS, 200, 0);
	if (! (inb (z->ICR) & ICR_ARBITRATION_IN_PROGRESS)) {
		PRINT (("nca%d/%d/%d no arbitration progress, bsr=%b csbr=%b\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun, inb (z->BSR), BSR_BITS,
			inb (z->CSBR), CSBR_BITS));
		outb (z->MR, z->parity);
		return (0);
	}
	DELAY (3);

	/* Check for lost arbitration. */
	if ((inb (z->ICR) & ICR_LOST_ARBITRATION) ||
	    (inb (z->CSDR) >> 1 >> z->scsi_addr) ||
	    (inb (z->ICR) & ICR_LOST_ARBITRATION)) {
		PRINT (("nca%d/%d/%d arbitration lost\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		outb (z->MR, z->parity);
		return (0);
	}

	outb (z->ICR, ICR_ASSERT_SEL);
	if (inb (z->ICR) & ICR_LOST_ARBITRATION) {
		PRINT (("nca%d/%d/%d arbitration lost after SEL\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		outb (z->ICR, 0);
		outb (z->MR, z->parity);
		return (0);
	}
	DELAY (2);

	/* Start selection, asserting the host and target ID's on the bus. */
	outb (z->SER, 0);
	outb (z->ODR, z->scsi_id | (1 << scb->xfer->sc_link->target));
	outb (z->ICR, ICR_ASSERT_DATA_BUS | ICR_ASSERT_BSY |
		ICR_ASSERT_SEL);

	/* Finish arbitration, drop BSY. */
	outb (z->MR, 0);
	outb (z->ICR, ICR_ASSERT_DATA_BUS | ICR_ASSERT_SEL |
		ICR_ASSERT_ATN);
	DELAY (1);

	/* The SCSI specification calls for a 250 ms timeout for the actual
	 * selection. */
	WAITFOR (inb (z->CSBR) & CSBR_BSY, 100000, 0);
	if (! (inb (z->CSBR) & CSBR_BSY)) {
		/* The target does not respond.  Not an error, though. */
		PRINT (("nca%d/%d/%d target does not respond\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		outb (z->ICR, 0);
		outb (z->SER, z->scsi_id);
		outb (z->MR, z->parity);
		scb->flags |= SCB_TIMEOUT;
		return (0);
	}

	/* Clear SEL and SCSI id.
	 * Wait for start of REQ/ACK handshake. */
	outb (z->ICR, ICR_ASSERT_DATA_BUS | ICR_ASSERT_ATN);
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 100000, 0);
	if (! (inb (z->CSBR) & CSBR_REQ)) {
		PRINT (("nca%d/%d/%d timeout waiting for REQ\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun));
		outb (z->ICR, 0);
		outb (z->SER, z->scsi_id);
		outb (z->MR, z->parity);
		scb->flags |= SCB_ERROR;
		return (0);
	}

	/* Check for phase mismatch. */
	if ((inb (z->CSBR) & PHASE_MASK) != PHASE_MSGOUT) {
		/* This should not be taken as an error, but more like
		 * an unsupported feature!
		 * Should set a flag indicating that the target don't support
		 * messages, and continue without failure.
		 * (THIS IS NOT AN ERROR!) */
		PRINT (("nca%d/%d/%d waiting for MSGOUT: invalid phase %s\n",
			z->sc_link.adapter_unit, scb->xfer->sc_link->target,
			scb->xfer->sc_link->lun,
			PHASE_NAME (inb (z->CSBR) & PHASE_MASK)));
		outb (z->ICR, 0);
		outb (z->SER, z->scsi_id);
		outb (z->MR, z->parity);
		scb->flags |= SCB_ERROR;
		return (0);
	}

	/* Allow disconnects. */
	outb (z->TCR, PHASE_TO_TCR (PHASE_MSGOUT));
	outb (z->ICR, ICR_ASSERT_DATA_BUS);
	nca_sendbyte (z, MSG_IDENTIFY (scb->xfer->sc_link->lun));
	outb (z->ICR, 0);
	outb (z->SER, z->scsi_id);
	outb (z->MR, z->parity);

	SET_BUSY (z, scb);
	return (1);
}

int nca_reselect (adapter_t *z)
{
	scb_t *q = 0, *prev = 0;
	u_char msg, target_mask, lun;
again:
	/* Wait for a device to win the reselection phase. */
	/* Signals this by asserting the I/O signal. */
	if ((inb (z->CSBR) & (CSBR_SEL | CSBR_IO | CSBR_BSY)) !=
	    (CSBR_SEL | CSBR_IO))
		return (0);

	/* The data bus contains original initiator id ORed with target id. */
	/* See that we really are the initiator. */
	target_mask = inb (z->CSDR);
	if (! (target_mask & z->scsi_id)) {
		PRINT (("nca%d reselect not for me: mask=0x%x, csbr=%b\n",
			z->sc_link.adapter_unit, target_mask,
			inb (z->CSBR), CSBR_BITS));
		goto again;
	}

	/* Find target who won. */
	/* Host responds by asserting the BSY signal. */
	/* Target should respond by deasserting the SEL signal. */
	target_mask &= ~z->scsi_id;
	outb (z->ICR, ICR_ASSERT_BSY);
	WAITFOR (! (inb (z->CSBR) & CSBR_SEL), 10000, "SEL deassert");

	/* Remove the busy status. */
	/* Target should set the MSGIN phase. */
	outb (z->ICR, 0);
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 10000, "MSGIN");

	/* Hope we get an IDENTIFY message. */
	msg = nca_msg_input (z);
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
			PRINT (("nca%d/%d/%d reselect done\n",
				z->sc_link.adapter_unit,
				ffs (target_mask) - 1, lun));
			nca_information_transfer (z, q);
			WAITFOR (! (inb (z->CSBR) & CSBR_BSY), 100000, "reselect !busy");
			return (1);
		}
	} else
		printf ("nca%d reselect: expecting IDENTIFY, got 0x%x\n",
			z->sc_link.adapter_unit, msg);

	/* Since we have an established nexus that we can't
	 * do anything with, we must abort it. */
	nca_send_abort (z);
	PRINT (("nca%d reselect aborted\n", z->sc_link.adapter_unit));
	WAITFOR (! (inb (z->CSBR) & CSBR_BSY), 100000, "reselect abort !busy");
	goto again;
}

/*
 * Send an abort to the target.
 * Return 1 success, 0 on failure.
 * Called on splbio level.
 */
int nca_abort (adapter_t *z, scb_t *scb)
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

		if (! nca_select (z, scb))
			return (0);
		nca_send_abort (z);

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
void nca_done (adapter_t *z, scb_t *scb)
{
	struct scsi_xfer *xs = scb->xfer;

	if (scb->flags & SCB_TIMECHK)
		untimeout (nca_timeout, (caddr_t) scb);

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
int nca_poll (adapter_t *z, scb_t *scb)
{
	int count;

	for (count=0; count<30; ++count) {
		DELAY (1000);                   /* delay for a while */
		nca_start (z);                  /* retry operation */
		if (scb->xfer->flags & ITSDONE)
			return (1);             /* all is done */
		if (scb->flags & SCB_TIMEOUT)
			return (0);             /* no target present */
	}
	return (0);
}

/*
 * Perform NCR-53C400 pseudo-dma data transfer.
 */
void nca_53400_dma_xfer (adapter_t *z, int read, u_char **pdata, u_long *plen)
{
	/* Set dma direction. */
	outb (z->CSR, read ? CSR_TRANSFER_DIRECTION : 0);

	/* Enable dma mode. */
	outb (z->MR, MR_DMA_MODE | (read ? z->parity : 0));

	/* Start dma transfer. */
	outb (read ? z->SDIR : z->SDSR, 0);

	/* Set up clock counter. */
	outb (z->CCR, *plen/128);

	for (; *plen>=128; *plen-=128, *pdata+=128) {
		/* Wait for 53C400 host buffer ready. */
		WAITFOR (! (inb (z->CSR) & CSR_HOST_BUF_NOT_READY), 100000, 0);
		if (inb (z->CSR) & CSR_HOST_BUF_NOT_READY)
			break;

		/* Transfer 128 bytes of data. */
		if (read)
			insw (z->HBR, *pdata, 64);
		else
			outsw (z->HBR, *pdata, 64);
	}

	/* Wait for 5380 registers ready. */
	WAITFOR (inb (z->CSR) & CSR_5380_ENABLE, 10000, 0);
	if (! (inb (z->CSR) & CSR_5380_ENABLE)) {
		/* Reset 53C400. */
		PRINT (("nca%d: reset: pseudo-dma incomplete, csr=%b\n",
			z->sc_link.adapter_unit, inb (z->CSR), CSR_BITS));
		outb (z->CSR, CSR_5380_ENABLE);
		outb (z->CSR, 0);
	}

	/* Wait for FIFO flush on write. */
	if (! read)
		WAITFOR (inb (z->TCR) & TCR_LAST_BYTE_SENT, 10000, "last byte");

	/* Clear dma mode. */
	outb (z->MR, z->parity);

	/* Re-enable interrupts. */
	outb (z->CSR, z->irq ? CSR_5380_INTR : 0);
}

/*
 * Perform PAS-16 pseudo-dma data transfer.
 */
void nca_pas_dma_xfer (adapter_t *z, int read, u_char **pdata, u_long *plen)
{
	/* Enable dma mode. */
	outb (z->MR, MR_DMA_MODE | (read ? z->parity : 0));

	/* Start dma transfer. */
	outb (read ? z->SDIR : z->SDSR, 0);

	for (; *plen>=512; *plen-=512, *pdata+=512) {
		/* Wait for pseudo-DMA request. */
		WAITFOR (inb (z->PSTAT) & PAS16_STAT_DREQ, 10000, "pseudo-dma");
		if (! (inb (z->PSTAT) & PAS16_STAT_DREQ))
			break;

		/* Transfer 512 bytes of data. */
		if (read)
			insb (z->PDATA, *pdata, 512);
		else
			outsb (z->PDATA, *pdata, 512);
	}

	/* Clear dma mode. */
	outb (z->MR, z->parity);
}

/*
 * Send data to the target.
 */
void nca_data_output (adapter_t *z, u_char **pdata, u_long *plen)
{
	u_char *data = *pdata;
	u_long len = *plen;

	outb (z->ICR, ICR_ASSERT_DATA_BUS);
	if (z->type == CTLR_NCR_53C400 && len%128 == 0)
		/* Use NCR-53C400 pseudo-dma for data transfer. */
		nca_53400_dma_xfer (z, 0, &data, &len);
	else if (z->type == CTLR_PAS_16 && len%512 == 0)
		/* Use PAS-16 pseudo-dma for data transfer. */
		nca_pas_dma_xfer (z, 0, &data, &len);
	else
		for (;;) {
			/* Check SCSI bus phase. */
			u_char s = inb (z->CSBR) ^ (CSBR_BSY | PHASE_DATAOUT);
			if (s & (CSBR_BSY | PHASE_MASK))
				break;

			/* Wait for REQ. */
			if (! (s & CSBR_REQ))
				continue;

			/* Output data. */
			outb (z->ODR, *data++);

			/* Assert ACK and wait for REQ deassert,
			 * with irqs disabled. */
			disable_intr ();
			outb (z->ICR, ICR_ASSERT_ACK | ICR_ASSERT_DATA_BUS);
			WAITFOR (! (inb (z->CSBR) & CSBR_REQ), 1000, 0);
			enable_intr ();

			/* Deassert ACK. */
			outb (z->ICR, ICR_ASSERT_DATA_BUS);
			--len;
		}
	outb (z->ICR, 0);
	PRINT (("nca (DATAOUT) send %ld bytes\n", *plen - len));
	*plen = len;
	*pdata = data;
}

/*
 * Receive data from the target.
 */
void nca_data_input (adapter_t *z, u_char **pdata, u_long *plen)
{
	u_char *data = *pdata;
	u_long len = *plen;

	if (z->type == CTLR_NCR_53C400 && len%128 == 0)
		/* Use NCR-53C400 pseudo-dma for data transfer. */
		nca_53400_dma_xfer (z, 1, &data, &len);
	else if (z->type == CTLR_PAS_16 && len%512 == 0)
		/* Use PAS-16 pseudo-dma for data transfer. */
		nca_pas_dma_xfer (z, 1, &data, &len);
	else
		for (;;) {
			/* Check SCSI bus phase. */
			u_char s = inb (z->CSBR) ^ (CSBR_BSY | PHASE_DATAIN);
			if (s & (CSBR_BSY | PHASE_MASK))
				break;

			/* Wait for REQ. */
			if (! (s & CSBR_REQ))
				continue;

			/* Input data. */
			*data++ = inb (z->CSDR);

			/* Assert ACK and wait for REQ deassert,
			 * with irqs disabled. */
			disable_intr ();
			outb (z->ICR, ICR_ASSERT_ACK);
			WAITFOR (! (inb (z->CSBR) & CSBR_REQ), 1000, 0);
			enable_intr ();

			/* Deassert ACK. */
			outb (z->ICR, 0);
			--len;
		}
	PRINT (("nca (DATAIN) got %ld bytes\n", *plen - len));
	*plen = len;
	*pdata = data;
}

/*
 * Send the command to the target.
 */
void nca_cmd_output (adapter_t *z, u_char *cmd, int cmdlen)
{
	PRINT (("nca%d send command (%d bytes) ", z->sc_link.adapter_unit,
		cmdlen));

	outb (z->ICR, ICR_ASSERT_DATA_BUS);
	while (cmdlen) {
		/* Check for target disconnect. */
		u_char sts = inb (z->CSBR);
		if (! (sts & CSBR_BSY))
			break;

		/* Check for phase mismatch. */
		if ((sts & PHASE_MASK) != PHASE_CMDOUT) {
			printf ("nca: sending command: invalid phase %s\n",
				PHASE_NAME (sts & PHASE_MASK));
			break;
		}

		/* Wait for REQ. */
		if (! (sts & CSBR_REQ))
			continue;

		PRINT (("-%x", *cmd));
		nca_sendbyte (z, *cmd++);
		--cmdlen;
	}
	outb (z->ICR, 0);
	PRINT (("\n"));
}

/*
 * Send the message to the target.
 */
void nca_send_abort (adapter_t *z)
{
	u_char sts;

	outb (z->ICR, ICR_ASSERT_ATN);

	/* Wait for REQ, after which the phase bits will be valid. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 1000000, "abort message");
	sts = inb (z->CSBR);
	if (! (sts & CSBR_REQ))
		goto ret;

	/* Check for phase mismatch. */
	if ((sts & PHASE_MASK) != PHASE_MSGOUT) {
		printf ("nca: sending MSG_ABORT: invalid phase %s\n",
			PHASE_NAME (sts & PHASE_MASK));
		goto ret;
	}

	outb (z->ICR, ICR_ASSERT_DATA_BUS);
	outb (z->TCR, PHASE_TO_TCR (PHASE_MSGOUT));
	nca_sendbyte (z, MSG_ABORT);

	PRINT (("nca%d send MSG_ABORT\n", z->sc_link.adapter_unit));
ret:    outb (z->ICR, 0);
}

/*
 * Get the message from the target.
 * Return the length of the received message.
 */
u_char nca_msg_input (adapter_t *z)
{
	u_char sts, msg;

	/* Wait for REQ, after which the phase bits will be valid. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 1000000, "message input");
	sts = inb (z->CSBR);
	if (! (sts & CSBR_REQ))
		return (MSG_ABORT);

	/* Check for phase mismatch.
	 * Reached if the target decides that it has finished the transfer. */
	if ((sts & PHASE_MASK) != PHASE_MSGIN) {
		printf ("nca: sending message: invalid phase %s\n",
			PHASE_NAME (sts & PHASE_MASK));
		return (MSG_ABORT);
	}

	/* Do actual transfer from SCSI bus to memory. */
	outb (z->TCR, PHASE_TO_TCR (PHASE_MSGIN));
	msg = nca_recvbyte (z);
	PRINT (("nca%d (MSG_INPUT) got 0x%x\n", z->sc_link.adapter_unit, msg));
	return (msg);
}

/*
 * Send request-sense op to the target.
 * Return 1 success, 0 on failure.
 * Called on splbio level.
 */
int nca_sense (adapter_t *z, scb_t *scb)
{
	u_char cmd[6], status, msg, *data;
	u_long len;

	/* Wait for target to disconnect. */
	WAITFOR (! (inb (z->CSBR) & CSBR_BSY), 100000, "sense bus free");
	if (inb (z->CSBR) & CSBR_BSY)
		return (0);

	/* Select the target again. */
	if (! nca_select (z, scb))
		return (0);

	/* Wait for CMDOUT phase. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 100000, "sense CMDOUT");
	if (! (inb (z->CSBR) & CSBR_REQ) ||
	    (inb (z->CSBR) & PHASE_MASK) != PHASE_CMDOUT)
		return (0);
	outb (z->TCR, PHASE_TO_TCR (PHASE_CMDOUT));

	/* Send command. */
	len = sizeof (scb->xfer->sense);
	cmd[0] = REQUEST_SENSE;
	cmd[1] = scb->xfer->sc_link->lun << 5;
	cmd[2] = 0;
	cmd[3] = 0;
	cmd[4] = len;
	cmd[5] = 0;
	nca_cmd_output (z, cmd, sizeof (cmd));

	/* Wait for DATAIN phase. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 100000, "sense DATAIN");
	if (! (inb (z->CSBR) & CSBR_REQ) ||
	    (inb (z->CSBR) & PHASE_MASK) != PHASE_DATAIN)
		return (0);
	outb (z->TCR, PHASE_TO_TCR (PHASE_DATAIN));

	data = (u_char*) &scb->xfer->sense;
	nca_data_input (z, &data, &len);
	PRINT (("nca%d sense %x-%x-%x-%x-%x-%x-%x-%x\n",
		z->sc_link.adapter_unit, scb->xfer->sense.error_code,
		scb->xfer->sense.ext.extended.segment,
		scb->xfer->sense.ext.extended.flags,
		scb->xfer->sense.ext.extended.info[0],
		scb->xfer->sense.ext.extended.info[1],
		scb->xfer->sense.ext.extended.info[2],
		scb->xfer->sense.ext.extended.info[3],
		scb->xfer->sense.ext.extended.extra_len));

	/* Wait for STATIN phase. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 100000, "sense STATIN");
	if (! (inb (z->CSBR) & CSBR_REQ) ||
	    (inb (z->CSBR) & PHASE_MASK) != PHASE_STATIN)
		return (0);
	outb (z->TCR, PHASE_TO_TCR (PHASE_STATIN));

	status = nca_recvbyte (z);

	/* Wait for MSGIN phase. */
	WAITFOR (inb (z->CSBR) & CSBR_REQ, 100000, "sense MSGIN");
	if (! (inb (z->CSBR) & CSBR_REQ) ||
	    (inb (z->CSBR) & PHASE_MASK) != PHASE_MSGIN)
		return (0);
	outb (z->TCR, PHASE_TO_TCR (PHASE_MSGIN));

	msg = nca_recvbyte (z);

	if (status != 0 || msg != 0)
		printf ("nca%d: bad sense status=0x%x, msg=0x%x\n",
			z->sc_link.adapter_unit, status, msg);
	return (1);
}

/*
 * Do the transfer. We know we are connected. Update the flags,
 * call nca_done when task accomplished. Dialog controlled by the target.
 * Always called with interrupts disabled.
 */
void nca_information_transfer (adapter_t *z, scb_t *scb)
{
	u_char *data = scb->data;               /* current data buffer */
	u_long datalen = scb->datalen;          /* current data transfer size */
	register u_char sts;
	u_char msg;

	while ((sts = inb (z->CSBR)) & CSBR_BSY) {
		/* We only have a valid SCSI phase when REQ is asserted. */
		if (! (sts & CSBR_REQ))
			continue;
		if (inb (z->BSR) & BSR_PARITY_ERROR) {
			int target = scb->xfer->sc_link->target;
			if (++z->target[target].perrcnt <= 8)
				printf ("nca%d/%d/%d parity error\n",
					z->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
			if (z->target[target].perrcnt == 8)
				printf ("nca%d/%d/%d too many parity errors, not logging any more\n",
					z->sc_link.adapter_unit, target,
					scb->xfer->sc_link->lun);
			/* Clear parity error. */
			inb (z->RPIR);
		}
		outb (z->TCR, PHASE_TO_TCR (sts & PHASE_MASK));
		switch (sts & PHASE_MASK) {
		case PHASE_DATAOUT:
			if (datalen <= 0) {
				printf ("nca%d/%d/%d data length underflow\n",
					z->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun);
				/* send zero byte */
				outb (z->ICR, ICR_ASSERT_DATA_BUS);
				nca_sendbyte (z, 0);
				outb (z->ICR, 0);
				break;
			}
			nca_data_output (z, &data, &datalen);
			break;
		case PHASE_DATAIN:
			if (datalen <= 0) {
				/* Get extra data.  Some devices (e.g. CDROMs)
				 * use fixed-length blocks (e.g. 2k),
				 * even if we need less. */
				PRINT (("@"));
				nca_recvbyte (z);
				break;
			}
			nca_data_input (z, &data, &datalen);
			break;
		case PHASE_CMDOUT:
			nca_cmd_output (z, (u_char*) scb->xfer->cmd,
				scb->xfer->cmdlen);
			break;
		case PHASE_STATIN:
			scb->xfer->status = nca_recvbyte (z);
			PRINT (("nca%d/%d/%d (STATIN) got 0x%x\n",
				z->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun,
				(u_char) scb->xfer->status));
			break;
		case PHASE_MSGOUT:
			/* Send no-op message. */
			outb (z->ICR, ICR_ASSERT_DATA_BUS);
			nca_sendbyte (z, MSG_NOP);
			outb (z->ICR, 0);
			PRINT (("nca%d/%d/%d (MSGOUT) send NOP\n",
				z->sc_link.adapter_unit,
				scb->xfer->sc_link->target,
				scb->xfer->sc_link->lun));
			break;
		case PHASE_MSGIN:
			/* Don't handle multi-byte messages here, because they
			 * should not be present here. */
			msg = nca_recvbyte (z);
			PRINT (("nca%d/%d/%d (MSGIN) got 0x%x\n",
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
					if (nca_sense (z, scb))
						scb->flags = SCB_SENSE;
					break;
				case SCSI_BUSY:
					scb->flags = SCB_TBUSY;
					break;
				}
				goto done;
			case MSG_ABORT:
				printf ("nca: command aborted by target\n");
				scb->flags = SCB_ABORTED;
				goto done;
			case MSG_MESSAGE_REJECT:
				printf ("nca: message rejected\n");
				scb->flags = SCB_ABORTED;
				goto done;
			case MSG_DISCONNECT:
				scb->next = z->disconnected_queue;
				z->disconnected_queue = scb;
				if (! z->irq && ! z->timeout_active) {
					timeout (nca_tick, z, 1);
					z->timeout_active = 1;
				}
				PRINT (("nca%d/%d/%d disconnected\n",
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
				printf ("nca%d/%d/%d unknown message: 0x%x\n",
					z->sc_link.adapter_unit,
					scb->xfer->sc_link->target,
					scb->xfer->sc_link->lun, msg);
				break;
			}
			break;
		default:
			printf ("nca: unknown phase: %b\n", sts, CSBR_BITS);
			break;
		}
	}
	printf ("nca%d/%d/%d unexpected target disconnect\n",
		z->sc_link.adapter_unit, scb->xfer->sc_link->target,
		scb->xfer->sc_link->lun);
	scb->flags = SCB_ERROR;
done:
	CLEAR_BUSY (z, scb);
	nca_done (z, scb);
ret:
	outb (z->ICR, 0);
	outb (z->TCR, 0);
	outb (z->SER, z->scsi_id);
	WAITFOR (! (inb (z->CSBR) & CSBR_BSY), 100000, "xfer bus free");
}
#endif /* NNCA */

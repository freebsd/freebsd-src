/*
 * Copyright (c) 1996, 1997, 1998, 1999
 *    Mark Dawson and David James. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
 *
 * $FreeBSD$
 *
 */

/*
 * Compaq SMART disk array controller driver for FreeBSD.
 * Supports the Compaq SMART-2 and SMART-3 families of disk
 * array controllers.
 *
 */

#include "id.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
/*#include <sys/dkbad.h>*/
#include <sys/devicestat.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/pmap.h>

#include <pci.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <sys/reboot.h>
extern u_long bootdev;

#define IDA_VERSION 1

#define PAGESIZ 4096

/* IDA wdc vector stealing (cuckoo) control */
#define IDA_CUCKOO_NEVER 0		/* never steal wdc vectors */
#define IDA_CUCKOO_ROOTWD 1		/* steal iff rootdev is wd device */
#define IDA_CUCKOO_ROOTNOTIDA 2		/* steal if rootdev not ida device */
#define IDA_CUCKOO_ALWAYS 3		/* always steal wdc vectors */

#ifndef IDA_CUCKOO_MODE
#define IDA_CUCKOO_MODE IDA_CUCKOO_ALWAYS
#endif

/* IDA PCI controller */

#define PCI_DEVICE_ID_COMPAQ_SMART2P 0xae100e11ul
#define PCI_CONTROLLER(ctlp) (ctlp->ident == PCI_DEVICE_ID_COMPAQ_SMART2P)

typedef struct ida_pci_reg {
  u_long unknown;
  u_long initiate_fifo;
#define IDA_PCI_BUSY 1
  u_long complete_fifo;
  u_long interrupt;
#define IDA_PCI_ENABLE_INTS 1
#define IDA_PCI_DISABLE_INTS 0
  u_long status;
#define IDA_PCI_PENDING 1
#define IDA_PCI_READY 2
} ida_pci_reg_t;

/* IDA controller register definitions */

#define	R_ID0		0xc80	/* id byte 0 */
#define	R_ID1		0xc81	/* id byte 1 */
#define	R_ID2		0xc82	/* id byte 2 */
#define	R_ID3		0xc83	/* id byte 3 */
#define	R_CONF		0xc88	/* global configuration */
#define	R_SYSINT	0xc89	/* system interrupt enable/ctrl */
#define	R_SEM0		0xc8a	/* semaphore port 0 */
#define	R_SEM1		0xc8b	/* semaphore port 1 */
#define	R_LBELL_E	0xc8c	/* local doorbell enable */
#define	R_LBELL_I	0xc8d	/* local doorbell int/status */
#define	R_EBELL_E	0xc8e	/* EISA doorbell enable */
#define	R_EBELL_I	0xc8f	/* EISA doorbell int/status */
#define R_SUB_ADDR	0xc90	/* submit address */
#define R_SUB_LEN	0xc94	/* submit cmdlist size */
#define R_COM_ADDR	0xc98	/* completion address */
#define R_COM_OFF	0xc9c	/* completion request offset */
#define R_COM_STAT	0xc9e	/* completion cmdlist status */
#define R_INTDEF	0xcc0	/* interrupt definition */

/*
 * BMIC doorbell status codes
 */

#define	BMIC_DATA_READY		0x01	/* data ready bit */
#define	BMIC_CHAN_CLEAR		0x02	/* channel clear bit */


/* IDA controller command list return status values */

#define	IDA_COMPL_OK		0x01	/* command list completed ok */
#define	IDA_NON_FATAL		0x02	/* non-fatal error */
#define	IDA_FATAL		0x04	/* fatal error */
#define	IDA_ABORTED		0x08	/* command list aborted */
#define	IDA_INVAL_REQ		0x10	/* bad request block */
#define	IDA_INVAL_LIST		0x20	/* bad command list */
#define	IDA_AARGH_LIST		0x40	/* totally disastrous command list */


/* IDA controller command codes */

#define IDA_GET_DRV_INFO	0x10
#define IDA_GET_CTL_INFO	0x11
#define IDA_READ_DATA		0x20
#define IDA_WRITE_DATA		0x30
#define IDA_FLUSH_CACHE		0xc2


/* Interrupt definition codes */

#define IDA_IRQ_MASK		0xfc
#define IDA_IRQ_10		0x20
#define IDA_IRQ_11		0x10
#define IDA_IRQ_14		0x40
#define IDA_IRQ_15		0x80


/* IDA controller hardware command structure definitions */

typedef u_long physaddr_t;

struct ida_hdr {
  u_long drive:8;		/* logical drive */
  u_long priority:8;		/* block priority */
  u_long flags:16;		/* control flags */
};

struct ida_req {
  u_long next:16;		/* offset of next request */
  u_long command:8;		/* command */
  u_long error:8;		/* return error code */
  u_long blkno;			/* block number */
  u_short bcount;		/* block count */
  u_short sgcount;		/* number of scatter gather entries */

  /* a struct ida_req is followed physically by an array of struct ida_sgb */
};

struct ida_sgb {
  u_long len;			/* length of scatter gather segmmentk */
  physaddr_t addr;		/* physical address of block */
};


/* first some handy definitions as FreeBSD gcc doesn't do #pragma pack() */

#define pack_char(name)  u_char name[1]
#define pack_short(name) u_char name[2]
#define pack_int(name)   u_char name[4]
#define pack_long(name)  u_char name[4]

/* ugly, but not inefficient, as it gets evaluated at compile time by gcc */

#define u_unpack(member) ( \
			  (sizeof(member) == 1) ? *(u_char *)(member) \
			  : (sizeof(member) == 2) ? *(u_short *)(member) \
			  : *(u_int *)(member) \
			   )

#define s_unpack(member) ( \
			  (sizeof(member) == 1) ? *(char *)(member) \
			  : (sizeof(member) == 2) ? *(short *)(member) \
			  : *(int *)(member) \
			   )

/* IDA controller hardware returned data structure definitions */

struct ida_ctl_info {
  pack_char(num_drvs);
  pack_long(signature);
  pack_long(firm_rev);
};

struct ida_drv_info {
  pack_short(secsize);
  pack_long(secperunit);
  pack_short(ncylinders);
  pack_char(ntracks);
  pack_char(signature);
  pack_char(psectors);
  pack_short(wprecomp);
  pack_char(max_acc);
  pack_char(control);
  pack_short(pcylinders);
  pack_char(ptracks);
  pack_short(landing_zone);
  pack_char(nsectors);
  pack_char(checksum);
  pack_char(mirror);
};


/* IDA driver queue command block */

#define IDA_MAX_SGLEN 32	/* maximum entries in scatter gather list */
#define IDA_MAX_DRVS_CTLR 8	/* maximum logical drives per controller */
#define IDA_DEF_PRIORITY 16	/* default priority for command list */

#define IDA_SCSI_TARGET_ID 7
#define IDA_QCB_MAX 256

struct ida_qcb {
  /* first some hardware specific fields ... */

  struct ida_hdr hdr;
  struct ida_req req;
  struct ida_sgb sglist[IDA_MAX_SGLEN];

  /* and then some driver queue managment stuff */

  u_int flags;			/* qcb type */
#define QCB_FREE       0	/* ready for a new command */
#define QCB_ACTIVE     1	/* waiting to be sent to the controller */
#define QCB_SENT       2	/* waiting for interrupt from the controller */
#define QCB_IMMED      4	/* immediate (non-queued) command */
#define QCB_IMMED_FAIL 8
  struct ida_qcb *next;	        /* next ida command block of this type */
  struct ida_qcb *last;	        /* last ida command block of this type */
  struct buf *buf;		/* buf associated with this qcb */
  physaddr_t paddr;		/* physical address of this struct */
  struct ida_qcb *nexthash;     /* next ida command block with same hash value */
};
typedef struct ida_qcb qcb_t;


/* IDA driver controller and drive information blocks */

#define QCB_HASH_SIZE 257 /* some suitable prime number */
#define QCB_HASH(h) ((h) % QCB_HASH_SIZE)

struct ida_drv {
  u_int flags;
#define ID_INIT      0x0001
#define ID_WRITEPROT 0x0002
#define ID_DEV_OPEN  0x0004
  u_int ctl_unit;		/* which controller is this drive on */
  u_int drv_unit;               /* number of this virtual disk */
  struct ida_drv_info drv_info; /* data from the controller */
  struct diskslices *slices;    /* new slice code */
  struct devstat dk_stats;	/* devstat entry */
};

struct ida_ctl {
  u_int ident;                  /* controller identifier */
  u_int flags;
  u_int iobase;
  u_short inside;		/* number of qcbs in the controller */
  u_short max_inside;		/* maximum number simulaneously active */
  u_short num_qcbs;		/* number of qcbs allocated */
  u_char num_drvs;
  u_char irq;
  u_char com_status;		/* status of last completed command list */
  physaddr_t com_addr;		/* address of last completed command list */
  u_short com_offset;		/* offset of last completed command list */
  qcb_t *freelist;		/* linked list of free qcbs */
  qcb_t *send_next;		/* doubly-linked list of unsent qcbs */
  qcb_t *send_last;		/* because we must treat all jobs equally */
  qcb_t *hashlist[QCB_HASH_SIZE];
};

extern struct ida_ctl *idadata[NIDA];


/* Useful IDA controller IO macro definitions */

#define IDA_DISABLE_INTERRUPT(iobase) outb(iobase + R_SYSINT, 0)
#define IDA_ENABLE_INTERRUPT(ctlp) outb(ctlp->iobase + R_SYSINT, 1)

#define IDA_SET_READY(ctlp) outb(ctlp->iobase + R_EBELL_E, 1)

#define IDA_DATA_READY(ctlp) ((inb(ctlp->iobase + R_EBELL_I) & 1))
#define IDA_CHAN_CLEAR(ctlp) ((inb(ctlp->iobase + R_EBELL_I) & 2))

/* enable/disable interrupts on a change of channel clear status (?) */

#define IDA_ENABLE_CHAN(ctlp) \
outb(ctlp->iobase + R_EBELL_E, inb(ctlp->iobase + R_EBELL_E) | 2)
#define IDA_DISABLE_CHAN(ctlp) \
outb(ctlp->iobase + R_EBELL_E, inb(ctlp->iobase + R_EBELL_E) & ~0x2)

/* acknowledge the completion of a command */

#define IDA_ACK_CMD_COM(ctlp) \
(outb(ctlp->iobase + R_EBELL_I, 1), outb(ctlp->iobase + R_LBELL_I, 2))

/* set submission details for a command list */

#define IDA_SET_SUB_ADDR(ctlp, addr) outl(ctlp->iobase + R_SUB_ADDR, addr)
#define IDA_SET_SUB_LEN(ctlp, size) outw(ctlp->iobase + R_SUB_LEN, size)

/* get completion details for a command list */

#define IDA_GET_COM_ADDR(ctlp) inl(ctlp->iobase + R_COM_ADDR)
#define IDA_GET_COM_OFFSET(ctlp) inw(ctlp->iobase + R_COM_OFF)
#define IDA_GET_COM_STATUS(ctlp) inb(ctlp->iobase + R_COM_STAT)


#define IDA_READ_EBELL_I(ctlp) inb(ctlp->iobase + R_EBELL_I)
#define IDA_READ_EBELL_E(ctlp) inb(ctlp->iobase + R_EBELL_E)
#define IDA_READ_LBELL_I(ctlp) inb(ctlp->iobase + R_LBELL_I)
#define IDA_SET_EBELL_I(ctlp, n) outb(ctlp->iobase + R_EBELL_I, n)
#define IDA_SET_LBELL_I(ctlp, n) outb(ctlp->iobase + R_LBELL_I, n)

#define JOB_SUCCESS 0
#define JOB_FAILURE 1
#define JOB_ABORTED 2

/* debugging aids */

#ifdef IDADEBUG
# define IDA_MAXQCBS  (1<<0)
# define IDA_SHOWQCBS (1<<1)
# define IDA_SHOWSUBS (1<<2)
# define IDA_SHOWINTS (1<<3)
# define IDA_SHOWCMDS (1<<4)
# define IDA_SHOWMISC (1<<5)
void ida_print_qcb(qcb_t *qcbp);
void ida_print_active_qcb(int unit);
static int ida_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, ida_debug, CTLFLAG_RW, &ida_debug, 0, "");
#endif

static int ida_soft_errors = 0;
SYSCTL_INT(_debug, OID_AUTO, ida_soft_errors,
	   CTLFLAG_RW, &ida_soft_errors, 0, "");

/* EISA probe and board identification definitions */

#define MAX_EISA_SLOT 16
#define IDA_EISA_PROD_ID 0x40

union eisa_id {
  u_int value;
  struct {
    u_int rev:8;
    u_int prod:8;
    u_int mfr2:5;
    u_int mfr1:5;
    u_int mfr0:5;
  } split;
};


/* test the manufacturer ID within an EISA board ID */

#define EISA_MFR_EQ(ident, mfr) ( \
				 (ident).split.mfr0 + '@' == (mfr)[0] && \
				 (ident).split.mfr1 + '@' == (mfr)[1] && \
				 (ident).split.mfr2 + '@' == (mfr)[2] \
				  )

/* generates a list of EISA board ID values, suitable for a printf */

#define EISA_ID_LIST(ident) \
(ident).split.mfr0 + '@', \
  (ident).split.mfr1 + '@', \
  (ident).split.mfr2 + '@', \
  (ident).split.prod, \
  (ident).split.rev

extern void DELAY(int millisecs);

/* FreeBSD IDA driver forward function definitions */

static	d_open_t	idopen;
static	d_read_t	idread;
static	d_write_t	idwrite;
static	d_close_t	idclose;
static	d_strategy_t	idstrategy;
static	d_ioctl_t	idioctl;
static	d_dump_t	iddump;
static	d_psize_t	idsize;

static pci_inthand_t idaintr;

static const char *ida_pci_probe(pcici_t tag, pcidi_t type);
static void ida_pci_attach(pcici_t config_id, int unit);

static int ida_eisa_probe __P((struct isa_device *dev));
static int ida_eisa_attach __P((struct isa_device *dev));
static int ida_poll __P((int unit, int wait));

static void idaminphys __P((struct buf *bp));
static void ida_start __P((int unit));

static int ida_get_ctl_info(int unit);
static int ida_attach_drives(int unit);
void ida_done(int cntlr, qcb_t *qcbp, int state);
static void ida_queue_buf(int cntlr, struct buf *bp);
static void ida_newqueue(int cntlr);
static qcb_t *ida_dequeue(int cntlr);
static void ida_enqueue(int cntlr, qcb_t *qcbp);
static int ida_submit(int unit, qcb_t *qcbp, int size);
static int ida_submit_wait(int unit, qcb_t *qcbp, int size);
static qcb_t *ida_get_qcb(int unit);
static void ida_free_qcb(int unit, qcb_t *qcbp);
static qcb_t *ida_qcb_phys_kv(struct ida_ctl *ida, physaddr_t ida_qcb_phys);

static struct cdevsw id_cdevsw;

struct isa_driver idadriver = {
  ida_eisa_probe,
  ida_eisa_attach,
  "ida"
};

static u_long ida_pci_count;

static struct  pci_device ida_pci_driver = {
  "ida",
  ida_pci_probe,
  ida_pci_attach,
  &ida_pci_count
};

DATA_SET (pcidevice_set, ida_pci_driver);

/* definitions for stealing wd driver's vectors */

#define ID_BDMAJ 29
#define ID_CDMAJ 109
#define WD_BDMAJ 0
#define WD_CDMAJ 3

struct isa_driver wdcdriver;
static int stub_probe __P((struct isa_device *dev));
static int stub_attach __P((struct isa_device *dev));

static struct isa_driver nodriver = {
  stub_probe,
  stub_attach,
  "stub"
};

/* steal the wdc driver's vectors if we have booted off a wd device */

static
void
ida_cuckoo_wdc(void)
{
  int cuckoo = IDA_CUCKOO_MODE;
  int steal = 0;
  char *mode;
  int major = B_TYPE(bootdev);

  if (cuckoo == IDA_CUCKOO_NEVER) {
    mode = "never";
  } else if (cuckoo == IDA_CUCKOO_ROOTWD) {
    mode = "rootwd";
    steal = (major == WD_BDMAJ);
  } else if (cuckoo == IDA_CUCKOO_ROOTNOTIDA) {
    mode = "notida";
    /* check for magic value of 3 rather than ID_BDMAJ as boot code
     * pretends we are a wt device (not normally bootable)
     */
    steal = (major != 3);
  } else {
    mode = "always";
    steal = 1;
  }

  printf("ida: wdc vector stealing %s (mode = %s, boot major = %d)\n",
	 (steal ? "on" : "off"), mode, major);
  if (!steal) return;

  /* OK - we have a controller, so steal wd driver's vectors */
  wdcdriver = nodriver;
  bdevsw[WD_BDMAJ]->d_open     = cdevsw[WD_CDMAJ]->d_open     = idopen;
  bdevsw[WD_BDMAJ]->d_close    = cdevsw[WD_CDMAJ]->d_close    = idclose;
  bdevsw[WD_BDMAJ]->d_read     = cdevsw[WD_CDMAJ]->d_read     = idread;
  bdevsw[WD_BDMAJ]->d_write    = cdevsw[WD_CDMAJ]->d_write    = idwrite;
  bdevsw[WD_BDMAJ]->d_strategy = cdevsw[WD_CDMAJ]->d_strategy = idstrategy;
  bdevsw[WD_BDMAJ]->d_ioctl    = cdevsw[WD_CDMAJ]->d_ioctl    = idioctl;
  bdevsw[WD_BDMAJ]->d_dump     = iddump;
  bdevsw[WD_BDMAJ]->d_psize    = idsize;
  return;
}

static struct ida_ctl *idadata[NIDA];   /* controller structures */
static struct ida_drv *id_drive[NID];	/* table of drives */
static int id_unit = 0;                 /* number of drives found */

/* general purpose data buffer for 'special' IDA driver commands */

union ida_buf {
  char pad[512];
  struct ida_ctl_info ctl;
  struct ida_drv_info drv;
} ida_buf;

static int
stub_probe(struct isa_device *dev)
{
  return 0;
}

static int
stub_attach(struct isa_device *dev)
{
  return 0;
}

const char *
ida_pci_probe(pcici_t tag, pcidi_t type)
{
  switch (type) {
  case PCI_DEVICE_ID_COMPAQ_SMART2P:
    return "Compaq SMART-2/P array controller";
    break;
  default:
    break;
  }
  return NULL;
}

static void
ida_pci_attach(pcici_t config_id, int unit)
{
  ida_pci_reg_t *reg;
  struct ida_ctl *ctlp;
  u_long id;
  vm_offset_t paddr, vaddr;

  id = pci_conf_read(config_id, PCI_ID_REG);
  switch (id) {
  case PCI_DEVICE_ID_COMPAQ_SMART2P:
    break;
  default:
    break;
  }

  if (!pci_map_mem(config_id, 0x14, &vaddr, &paddr)) {
    printf("ida: map failed.\n");
    return;
  }

  /* allocate and initialise a storage area for this controller */
  if (idadata[unit]) {
    printf("ida%d: controller structure already allocated\n", unit);
    return;
  }
  if ((ctlp = malloc(sizeof(struct ida_ctl), M_TEMP, M_NOWAIT)) == NULL) {
    printf("ida%d: unable to allocate controller structure\n", unit);
    return;
  }

  idadata[unit] = ctlp;
  bzero(ctlp, sizeof(struct ida_ctl));
  ctlp->ident = id;
  ctlp->iobase = vaddr;

  /* Install the interrupt handler. */
  if (!pci_map_int (config_id, idaintr, (void *)unit, &bio_imask)) {
    printf ("ida%d: failed to assign an interrupt handler\n", unit);
    free((caddr_t)ctlp, M_DEVBUF);
    idadata[unit] = 0;
    return;
  }

  if (!(ida_get_ctl_info(unit) && ida_attach_drives(unit))) {
    return;
  }

  reg = (ida_pci_reg_t *) vaddr;
  reg->interrupt = IDA_PCI_ENABLE_INTS;
  ida_cuckoo_wdc();
  return;
}

int
ida_eisa_probe(struct isa_device *dev)
{
  static u_int ida_used = 0;
  u_int slot;
  u_int port;
  u_char intdef;
  u_char irq;
  int unit = dev->id_unit;
  union eisa_id ident;
  struct ida_ctl *ctlp;

  if (dev->id_iobase) {
    /* check out the configured iobase if given one */
    slot = dev->id_iobase / 0x1000;
    if (slot == 0 || slot > MAX_EISA_SLOT) {
      printf("ida: port address (0x%x) out of range\n", dev->id_iobase);
      return 0;
    }
  } else {
    /* otherwise, search from the beginning for an unused slot to check out */
    slot = 1;
  }

  while (1) {
    while (ida_used & (1 << slot)) {
      if (slot++ == MAX_EISA_SLOT) return 0;
    }

    ida_used |= (1 << slot);
    port = slot * 0x1000;

    /* read the EISA identification bytes */
    ident.value = inb(port + R_ID0);
    ident.value <<= 8;
    ident.value |= inb(port + R_ID1);
    ident.value <<= 8;
    ident.value |= inb(port + R_ID2);
    ident.value <<= 8;
    ident.value |= inb(port + R_ID3);
  
    /* check that the card is the right type ? */
    if (EISA_MFR_EQ(ident, "CPQ") && ident.split.prod == IDA_EISA_PROD_ID) {
      break;
    }

    /* if we were config'ed with an iobase, then don't probe any more slots */
    if (dev->id_iobase) return 0;
  }

  /* disable interrupts and find out what interrupt this controller uses */
  IDA_DISABLE_INTERRUPT(port);

  intdef = inb(port + R_INTDEF);
  switch (intdef & IDA_IRQ_MASK) {
  case IDA_IRQ_10:
    irq = 10;
    break;

  case IDA_IRQ_11:
    irq = 11;
    break;

  case IDA_IRQ_14:
    irq = 14;
    break;

  case IDA_IRQ_15:
    irq = 15;
    break;

  default:
    printf("ida: slot %d bogus interrupt setting (0x%02x)\n", slot, intdef);
    return 0;
  }
  dev->id_irq = (1 << irq);
  dev->id_drq = -1;

  /* allocate and initialise a storage area for this controller */
  if (idadata[unit]) {
    printf("ida%d: controller structure already allocated\n", unit);
    return 0;
  }
  if ((ctlp = malloc(sizeof(struct ida_ctl), M_TEMP, M_NOWAIT)) == NULL) {
    printf("ida%d: unable to allocate controller structure\n", unit);
    return 0;
  }
  idadata[unit] = ctlp;

  bzero(ctlp, sizeof(struct ida_ctl));
  ctlp->iobase = dev->id_iobase = port;
  ctlp->ident = ident.value;
  ctlp->irq = irq;

  if (ida_get_ctl_info(unit) == 0) {
    return 0;
  }

  /* return range of io ports used */
  return 0x1000;
}

int
ida_get_ctl_info(int unit)
{
  struct ida_ctl *ctlp = idadata[unit];
  qcb_t qcb;
  qcb_t *qcbp = &qcb;

  ida_newqueue(unit);
  /* controller capacity statistics */
  ctlp->inside = 0;
  ctlp->max_inside = 0;
    
  /* ask the controller to tell us about itself with an IDA_GET_CTL_INFO */
  bzero(qcbp, sizeof(qcb_t));
  qcbp->paddr = vtophys(qcbp);
  if (PCI_CONTROLLER(ctlp)) {
    qcbp->hdr.priority = 0x00;
    qcbp->hdr.flags = 0x24;
  } else {
    qcbp->hdr.priority = IDA_DEF_PRIORITY;
    qcbp->hdr.flags = 0x12;
  }
  qcbp->req.command = IDA_GET_CTL_INFO;
  qcbp->req.bcount = 1;
  qcbp->req.sgcount = 1;
  qcbp->sglist[0].len = sizeof(ida_buf);
  qcbp->sglist[0].addr = vtophys(&ida_buf);

  if (ida_submit_wait(unit, qcbp, sizeof(struct ida_qcb))) {
    printf("ida%d: idasubmit failed on IDA_GET_CTL_INFO\n", unit);
    return 0;
  }

  if (!PCI_CONTROLLER(ctlp)) {
    if (ctlp->com_status != IDA_COMPL_OK) {
      printf("ida%d: bad status 0x%02x from IDA_GET_CTL_INFO\n",
	     unit, ctlp->com_status);
      return 0;
    }
  }

  /* got the information at last, print it and note the number of drives */
  printf("ida%d: drvs=%d firm_rev=%c%c%c%c\n", unit,
	 u_unpack(ida_buf.ctl.num_drvs), ida_buf.ctl.firm_rev[0],
	 ida_buf.ctl.firm_rev[1], ida_buf.ctl.firm_rev[2],
	 ida_buf.ctl.firm_rev[3]);
  ctlp->num_drvs = u_unpack(ida_buf.ctl.num_drvs);

  return 1;
}

int
ida_attach_drives(int cntlr)
{
  struct ida_ctl *ctlp = idadata[cntlr];
  qcb_t qcb;
  qcb_t *qcbp = &qcb;
  struct ida_drv *drv;
  int drive;
  int unit;

  /* prepare to interrogate the drives */
  bzero(qcbp, sizeof(qcb_t));
  qcbp->req.command = IDA_GET_DRV_INFO;
  qcbp->paddr = vtophys(qcbp);
  if (PCI_CONTROLLER(ctlp)) {
    qcbp->hdr.priority = 0x00;
    qcbp->hdr.flags = 0x24;
  } else {
    qcbp->hdr.priority = IDA_DEF_PRIORITY;
    qcbp->hdr.flags = 0x12;
  }
  qcbp->req.bcount = 1;
  qcbp->req.sgcount = 1;
  qcbp->sglist[0].len = sizeof(ida_buf);
  qcbp->sglist[0].addr = vtophys(&ida_buf);

  for (drive = 0 ; drive < ctlp->num_drvs ; drive++) {
    qcbp->hdr.drive = drive;

    if (ida_submit_wait(cntlr, qcbp, sizeof(struct ida_qcb))) {
      printf("ida%d: ida_submit_wait failed on IDA_GET_DRV_INFO\n", cntlr);
      return 0;
    }

    if (!PCI_CONTROLLER(ctlp)) {
      if (ctlp->com_status != IDA_COMPL_OK) {
        printf("ida%d: bad status 0x%02x from IDA_GET_DRV_INFO\n",
	       cntlr, ctlp->com_status);
        return 0;
      }
    }

    if ((drv = malloc(sizeof(struct ida_drv), M_TEMP, M_NOWAIT)) == NULL) {
      printf("ida%d: unable to allocate drive structure\n", cntlr);
      return 0;
    }

    bzero(drv, sizeof(struct ida_drv));
    drv->ctl_unit = cntlr;
    drv->drv_unit = drive;
    drv->drv_info = ida_buf.drv;
    drv->flags |= ID_INIT;

    unit = id_unit;
    id_unit++; /* XXX unsure if this is the right way to do things */
    id_drive[unit] = drv;

    printf("ida%d: unit %d (id%d): <%s>\n",
	   cntlr, drive, unit, "Compaq Logical Drive");
    printf("id%d: %luMB (%lu total sec), ",
	   unit,
	   (u_long)(u_unpack(drv->drv_info.secperunit) / 2048)
	   * (u_unpack(drv->drv_info.secsize) / 512),
	   (u_long)u_unpack(drv->drv_info.secperunit));
    printf("%lu cyl, %lu head, %lu sec, bytes/sec %lu\n",
	   (u_long)u_unpack(drv->drv_info.ncylinders),
	   (u_long)u_unpack(drv->drv_info.ntracks),
	   (u_long)u_unpack(drv->drv_info.nsectors),
	   (u_long)u_unpack(drv->drv_info.secsize));	       
    
    /*
     * Export the drive to the devstat interface.
     */
    devstat_add_entry(&drv->dk_stats, "id", 
		      unit, (u_int32_t)drv->drv_info.secsize,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		      DEVSTAT_PRIORITY_DA);

#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWMISC) {
      printf("ida%d: drive %d secsize=%d secperunit=%d ncylinders=%d ntracks=%d\n",
	     unit,
	     drive,
	     u_unpack(ida_buf.drv.secsize),
	     u_unpack(ida_buf.drv.secperunit),
	     u_unpack(ida_buf.drv.ncylinders),
	     u_unpack(ida_buf.drv.ntracks));
      printf("         signature=0x%02x psectors=%d wprecomp=%d max_acc=%d control=0x%02x\n",
	     u_unpack(ida_buf.drv.signature),
	     u_unpack(ida_buf.drv.psectors),
	     u_unpack(ida_buf.drv.wprecomp),
	     u_unpack(ida_buf.drv.max_acc),
	     u_unpack(ida_buf.drv.control));
      printf("         pcylinders=%d ptracks=%d landing_zone=%d nsectors=%d checksum=0x%02x\n",
	     u_unpack(ida_buf.drv.pcylinders),
	     u_unpack(ida_buf.drv.ptracks),
	     u_unpack(ida_buf.drv.landing_zone),
	     u_unpack(ida_buf.drv.nsectors),
	     u_unpack(ida_buf.drv.checksum));
    }
#endif
  }

  return 1;
}

/* Attach all the sub-devices we can find. */
int
ida_eisa_attach(struct isa_device *dev)
{
  int cntlr = dev->id_unit;
  struct ida_ctl *ctlp = idadata[cntlr];

  if (ida_attach_drives(cntlr)) {
    IDA_ENABLE_INTERRUPT(ctlp);
    IDA_SET_READY(ctlp);
    ida_cuckoo_wdc();
    return 1;
  } else {
    return 0;
  }
}

/*
 * Initialize a drive.
 */
int
idopen(dev_t dev, int flags, int fmt, struct proc *p)
{
  struct ida_drv *drv;
  int part = dkpart(dev);
  int unit = dkunit(dev);
  struct disklabel label;
  int err;

  if (unit >= NID || part >= MAXPARTITIONS) /* bounds check */
    return(ENXIO);

  drv = id_drive[unit];
  if (!drv || !(drv->flags & ID_INIT)) /* drive not initialised */
    return(ENXIO);

  drv->flags |= ID_DEV_OPEN;

  /* knock up a label for the whole disk. */
  bzero(&label, sizeof label);
  label.d_secsize = u_unpack(drv->drv_info.secsize);
  label.d_nsectors = u_unpack(drv->drv_info.nsectors);
  label.d_ntracks = u_unpack(drv->drv_info.ntracks);
  label.d_ncylinders = u_unpack(drv->drv_info.ncylinders);
  label.d_secpercyl =
    u_unpack(drv->drv_info.ntracks) * u_unpack(drv->drv_info.nsectors);
  if (label.d_secpercyl == 0)
    label.d_secpercyl = 100; /* prevent accidental division by zero */
  label.d_secperunit = u_unpack(drv->drv_info.secperunit);

  /* Initialize slice tables. */
  if ((err = dsopen("id", dev, fmt, 0, &drv->slices, &label, idstrategy,
		    (ds_setgeom_t *)NULL, &id_cdevsw)) == NULL) {
    return 0;
  }

  if (!dsisopen(drv->slices)) {
    drv->flags &= ~ID_DEV_OPEN;
  }
  return err;
}

/* ARGSUSED */
int
idclose(dev_t dev, int flags, int fmt, struct proc *p)
{
  struct ida_drv *drv;
  int	part = dkpart(dev);
  int unit = dkunit(dev);

  if (unit >= NID || part >= MAXPARTITIONS) /* bounds check */
    return(ENXIO);

  drv = id_drive[unit];
  dsclose(dev, fmt, drv->slices);
  if (!dsisopen(drv->slices)) {
    drv->flags &= ~ID_DEV_OPEN;
  }
  return 0;
}



int
idioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
  struct ida_drv *drv;
  int part = dkpart(dev);
  int unit = dkunit(dev);
  int err;

  if (unit >= NID || part >= MAXPARTITIONS ||
      !(drv = id_drive[unit]) || !(drv->flags & ID_INIT)) /* sanity check */
    return(ENXIO);

  err = dsioctl("id", dev, cmd, addr, flag, &drv->slices,
		idstrategy, (ds_setgeom_t *)NULL);

  if (err != -1)
    return (err);

  if (dkpart(dev) != RAW_PART)
    return (ENOTTY);

  return (0);
}
static int
idread(dev_t dev, struct uio *uio, int ioflag)
{
  return (physio(idstrategy, NULL, dev, 1, minphys, uio));
}

static int
idwrite(dev_t dev, struct uio *uio, int ioflag)
{
  return (physio(idstrategy, NULL, dev, 0, minphys, uio));
}


/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
void
idstrategy(struct buf *bp)
{
  int unit = dkunit(bp->b_dev);
  struct ida_drv *drv;
  int opri;

  if (unit >= NID) {
    printf("ida: unit out of range\n");
    bp->b_error = EINVAL;
    goto bad;
  }

  if (!(drv = id_drive[unit]) || !(drv->flags & ID_INIT)) {
    printf("id%d: drive not initialised\n", unit);
    bp->b_error = EINVAL;
    goto bad;
  }

  if (bp->b_blkno < 0) {
    printf("id%d: negative block requested\n", unit);
    bp->b_error = EINVAL;
    goto bad;
  }

  if (bp->b_bcount % DEV_BSIZE != 0) { /* bounds check */
    printf("id%d: count (%lu) not a multiple of a block\n",
	   unit, bp->b_bcount);
    bp->b_error = EINVAL;
    goto bad;
  }

  idaminphys(bp); /* adjust the transfer size */

  /* "soft" write protect check */
  if ((drv->flags & ID_WRITEPROT) && (bp->b_flags & B_READ) == 0) {
    bp->b_error = EROFS;
    goto bad;
  }

  /* If it's a null transfer, return immediately */
  if (bp->b_bcount == 0) {
    goto done;
  }

  if (dscheck(bp, drv->slices) <= 0) {
    goto done;
  }
   
  opri = splbio();
  ida_queue_buf(unit, bp);
  devstat_start_transaction(&drv->dk_stats);
  ida_start(drv->ctl_unit); /* hit the appropriate controller */
  splx(opri);

  return /*0*/;

bad:
  bp->b_flags |= B_ERROR;
 
done:
  /* correctly set the buf to indicate a completed xfer */
  bp->b_resid = bp->b_bcount;
  biodone(bp);
  return /*0*/;
}


void
idaminphys(bp)
struct buf *bp;
{
  /* assumes each page requires an sgb entry */
  int max = (IDA_MAX_SGLEN - 1) * PAGESIZ;
  if (bp->b_bcount > max) {
    bp->b_bcount = max;
  }
}


/* Get a free qcb.
 * If there are none, see if we can allocate a new one.
 * If so, put it in the hash table too,
 * otherwise either return an error or sleep.
 */
static qcb_t *
ida_get_qcb(int unit)
{
  struct ida_ctl *ida = idadata[unit];
  unsigned opri = 0;
  qcb_t *qcbp;
  int hashnum;
    
  opri = splbio();

  /* if the freelist is empty - create a qcb until limit is reached */
  while (!(qcbp = ida->freelist)) {
    if (ida->num_qcbs < IDA_QCB_MAX) {
      qcbp = (qcb_t *)malloc(sizeof(qcb_t), M_TEMP, M_NOWAIT);
      if (qcbp) {
	bzero(qcbp, sizeof(qcb_t));
	ida->num_qcbs++;
	qcbp->flags = QCB_ACTIVE;
#ifdef IDADEBUG
	if (ida_debug & IDA_SHOWQCBS)
	  printf("ida_get_qcb%d: qcb %d created\n",
		 unit, ida->num_qcbs);
#endif
	/* Put in the phystokv hash table. */
	/* Never gets taken out. */
	qcbp->paddr = vtophys(qcbp);
	hashnum = QCB_HASH(qcbp->paddr);
	qcbp->nexthash = ida->hashlist[hashnum];
	ida->hashlist[hashnum] = qcbp;
      } else {
	printf("ida%d: Can't malloc QCB\n", unit);
      } goto gottit;
    } else {
      /* reached maximum allocation of qcbs - sleep until one is freed */
      tsleep((caddr_t)&ida->freelist, PRIBIO, "idaqcb", 0);
    }
  } if (qcbp) {
    /* qet the qcb from from the (non-empty) free list */
    ida->freelist = qcbp->next;
    qcbp->flags = QCB_ACTIVE;
  }
gottit:
  splx(opri);

#ifdef IDADEBUG
  if (ida_debug & IDA_SHOWQCBS)
    printf("ida_get_qcb%d: returns 0x%x\n", unit, qcbp);
#endif

  return (qcbp);
}


/* Return a qcb to the free list */
static void
ida_free_qcb(int unit, qcb_t *qcbp)
{
  unsigned int opri = 0;
  struct ida_ctl *ida = idadata[unit];

#ifdef IDADEBUG
  if (ida_debug & IDA_SHOWQCBS)
    printf("ida_free_qcb%d: freeing 0x%x\n", unit, qcbp);
#endif

  opri = splbio();

  qcbp->next = ida->freelist;
  ida->freelist = qcbp;
  qcbp->flags = QCB_FREE;

  /* if the free list was empty, wakeup anyone sleeping */

  if (!qcbp->next) {
#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWQCBS)
      printf("ida_free_qcb%d: about to wakeup 0x%x queue\n",
	     unit, &ida->freelist);
#endif
    wakeup((caddr_t)&ida->freelist);
  }

  splx(opri);
}

/* Find the ida_qcb having a given physical address */
qcb_t *
ida_qcb_phys_kv(ida, ida_qcb_phys)
struct ida_ctl *ida;
physaddr_t ida_qcb_phys;
{
  int hash = QCB_HASH(ida_qcb_phys);
  qcb_t *qcbp = ida->hashlist[hash];
    
  while (qcbp) {
    if (qcbp->paddr == ida_qcb_phys)
      break;
    qcbp = qcbp->nexthash;
  }

#ifdef IDADEBUG
  if (ida_debug & IDA_SHOWQCBS)
    printf("ida?: ida_qcb_phys_kv(0x%x) = 0x%x\n", ida_qcb_phys, qcbp);
#endif

  return qcbp;
}

void
ida_queue_buf(int unit, struct buf *bp)
{
  struct ida_drv *drv = id_drive[unit];
  int cntlr = drv->ctl_unit;
  qcb_t *qcbp = ida_get_qcb(cntlr); /* may cause us to wait */
  struct ida_ctl *ida = idadata[cntlr];
  unsigned int datalen = bp->b_bcount;
  int thiskv = (int)bp->b_data;
  physaddr_t thisphys = vtophys(thiskv);
  int nsgb = 0; /* number of scatter/gather blocks used */
  struct ida_sgb *sg = &(qcbp->sglist[0]);

  /* fill in the qcb command header */

  if (PCI_CONTROLLER(ida)) {
    qcbp->hdr.priority = 0x00;
    qcbp->hdr.flags =
      (sizeof(struct ida_req) + sizeof(struct ida_sgb) * IDA_MAX_SGLEN) >> 2;
  } else {
    qcbp->hdr.priority = IDA_DEF_PRIORITY;
    qcbp->hdr.flags = 0x10;
  }
  qcbp->hdr.drive = drv->drv_unit; /* logical drive number */
  qcbp->buf = bp; /* the buf this command came from */

  /* set up the scatter-gather list in the qcb */

  while ((datalen) && (nsgb < IDA_MAX_SGLEN)) {
    int bytes_this_seg = 0;
    physaddr_t nextphys;
	
    /* put in the base address */
    sg->addr = thisphys;

    /* do it at least once */
    nextphys = thisphys;
    while ((datalen) && (thisphys == nextphys)) {
      int bytes_this_page;

      /* This page is contiguous (physically) with the the last, */
      /* just extend the length */
	    
      /* how far to the end of the page ... */
      nextphys = (thisphys & (~(PAGESIZ - 1))) + PAGESIZ;
      bytes_this_page = nextphys - thisphys;
      /* ... or to the end of the data */
      bytes_this_page = min(bytes_this_page, datalen);
      bytes_this_seg += bytes_this_page;
      datalen -= bytes_this_page;

      /* get ready for the next page */
      thiskv = (thiskv & (~(PAGESIZ - 1))) + PAGESIZ;
      if (datalen)
	thisphys = vtophys(thiskv);
    }

    /* next page isn't contiguous, finish the seg */
    sg->len = bytes_this_seg;
    sg++;
    nsgb++;
  }

  if (datalen) { /* still data => command block too small */
    printf("ida_queue_buf%d: more than %d scatter/gather blocks needed\n",
	   cntlr, IDA_MAX_SGLEN);
    bp->b_error = EIO;
    bp->b_flags |= B_ERROR;
    biodone(bp);
    return;
  }

  /* fill-in the I/O request block */

  qcbp->req.error = 0;
  qcbp->req.next = 0;
  qcbp->req.blkno = bp->b_pblkno;
  qcbp->req.bcount = bp->b_bcount >> 9;
  qcbp->req.sgcount = nsgb;
  qcbp->req.command = (bp->b_flags & B_READ ? IDA_READ_DATA : IDA_WRITE_DATA);

#ifdef IDADEBUG
  if (ida_debug & IDA_SHOWQCBS) {
    printf("ida_rw%d: queuing:\n", cntlr);
    ida_print_qcb(qcbp);
  }
#endif

  /* queue for submission to the controller */
  ida_enqueue(cntlr, qcbp);
}


void
ida_start(int cntlr)
{
  struct ida_ctl *ida = idadata[cntlr];
  qcb_t *qcbp;
  int count = 0;
  int opri = splbio();

  if (!ida->send_next) { /* check there is a job in the queue */
    splx(opri);
    return;
  }

  if (PCI_CONTROLLER(ida)) {
    ida_pci_reg_t *reg = (ida_pci_reg_t *)ida->iobase;
    u_int fifo = reg->initiate_fifo;
    if (fifo == 1) {
      splx(opri);
      return;                        /* not sent - must try again later */
    }

    /* submit upto 16 jobs at once into the initiate fifo */
    while (count < 16 && (fifo = reg->initiate_fifo) != 1
	   && (qcbp = ida_dequeue(cntlr))) {
      reg->initiate_fifo = qcbp->paddr;
      qcbp->flags = QCB_SENT;
      ida->inside++;
      count++;
    }
  } else {

    if (!IDA_CHAN_CLEAR(ida)) {
      IDA_ENABLE_CHAN(ida);
      splx(opri);
      return;
    }

    qcbp = ida_dequeue(cntlr);

#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWQCBS)
      printf("ida%d: ida_start: sending 0x%x\n", cntlr, qcbp);
#endif

    IDA_SET_EBELL_I(ida, 2);
    IDA_SET_SUB_ADDR(ida, qcbp->paddr); /* physical address of this qcb */
    IDA_SET_SUB_LEN(ida, sizeof(qcb_t));
    IDA_SET_LBELL_I(ida, 1);    

    qcbp->flags = QCB_SENT;
    ida->inside++;
    count++;
  }

  if (ida->inside > ida->max_inside) {
    ida->max_inside = ida->inside; /* new maximum */
    splx(opri);

#ifdef IDADEBUG
    if (ida_debug & IDA_MAXQCBS)
      printf("ida%d: qcbs %d/%d\n", cntlr, ida->inside, ida->num_qcbs);
#endif
  } else {
    splx(opri);
  }

#ifdef IDADEBUG
  if ((ida_debug & IDA_SHOWSUBS) && count > 1)
    printf("ida%d: %d jobs submitted (queue %s).\n",
	   cntlr, count, ida->send_next ? "not emptied" : "emptied");
#endif
}

void
ida_newqueue(int cntlr)
{
  struct ida_ctl *ida = idadata[cntlr];
  ida->send_next = 0;
  ida->send_last = 0;
}

qcb_t *
ida_dequeue(int cntlr)
{
  struct ida_ctl *ida = idadata[cntlr];
  qcb_t *qcbp = ida->send_next; /* who is next? */

  if (qcbp) {	/* queue is not empty */
    qcb_t* nextp = qcbp->next;

    if (nextp) { /* more than one element in the queue */
      nextp->last = 0; /* we are the first */
      ida->send_next = nextp; /* hence first to go */
    } else { /* exactly one element in the queue */
      ida->send_last = ida->send_next = 0;
    }
  }

  return qcbp;
}

static void
ida_enqueue(int cntlr, qcb_t *qcbp)
{
  struct ida_ctl *ida = idadata[cntlr];
  qcb_t *lastp = ida->send_last; /* who is last? */
  int opri = splbio();

  if (lastp) { /* if the queue is not empty */
    lastp->next = qcbp; /* then we go after the last */
  } else { /* if the queue was empty */
    ida->send_next = qcbp; /* then we go next */
  }

  qcbp->last = lastp; /* we follow the last */
  qcbp->next = 0;	/* and nothing follows us */

  ida->send_last = qcbp; /* we go last */
  splx(opri);
}

void
idaintr(void *arg)
{
  int cntlr = (int)arg;
  qcb_t *qcbp;
  struct ida_ctl *ida = idadata[cntlr];
  u_char status;
  physaddr_t paddr, paddr1;	/* physical address of the qcb */
  int offset;			/* qcb offset */
  u_char cstat;			/* job status */

  if (PCI_CONTROLLER(ida)) { /*pci:*/
    ida_pci_reg_t *reg = (ida_pci_reg_t *)ida->iobase;
    int status = reg->status;
#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWINTS)
      printf("ida%d: idaintr: status=%x (before complete)\n"
	     , cntlr, status);
#endif
    while (status & IDA_PCI_PENDING) {
      paddr1 = reg->complete_fifo;
      paddr = paddr1 & ~3;
      qcbp = ida_qcb_phys_kv(ida, paddr);
#ifdef IDADEBUG
      if (ida_debug & IDA_SHOWQCBS) {
	printf("ida%d: idaintr: qcb(%x) completed\n", cntlr, qcbp);
	ida_print_qcb(qcbp);
      }
#endif
      if (qcbp) {
	if (qcbp->req.error & 3) ida_soft_errors++;
	ida_done(cntlr, qcbp, (qcbp->req.error>>2) ? JOB_FAILURE : JOB_SUCCESS);
      } else {
	printf("ida%d: idaintr: completion (%x) ignored\n", cntlr, paddr1);
      }
      status = reg->status;
    }
#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWINTS)
      printf("ida%d: idaintr: status=%x (before initiate)\n"
	     , cntlr, status);
#endif
    if (status & IDA_PCI_READY) {
      ida_start(cntlr);
    }
  } else { 
    while(1) {
      status = IDA_READ_EBELL_I(ida) & IDA_READ_EBELL_E(ida);

#ifdef IDADEBUG
      if (ida_debug & IDA_SHOWINTS)
	printf("ida%d: idaintr: status = 0x%x\n", cntlr, status);
#endif

      if ((status & (BMIC_DATA_READY | BMIC_CHAN_CLEAR)) == 0) break;

      if (status & BMIC_DATA_READY) { /* data ready */
	int job_status;

	if (IDA_READ_LBELL_I(ida) & JOB_ABORTED) {
	  printf("ida%d: idaintr: status:%x local channel should be busy! ",
		 cntlr, status);
	}

	paddr = IDA_GET_COM_ADDR(ida);
	offset = IDA_GET_COM_OFFSET(ida);
	cstat = IDA_GET_COM_STATUS(ida);

	/* acknowledge interrupt */
	IDA_ACK_CMD_COM(ida);

	/* determine which job completed */
	qcbp = ida_qcb_phys_kv(ida, paddr);

	/* analyse the job status code */
	if (cstat & IDA_COMPL_OK) {
	  job_status = JOB_SUCCESS;
	} else {
	  printf("ida%d: idaintr: return code %x=", cntlr, cstat);
	  if (cstat & IDA_NON_FATAL) printf("recoverable error! ");
	  if (cstat & IDA_FATAL) printf("fatal error! ");
	  if (cstat & IDA_ABORTED) printf("aborted! ");
	  if (cstat & IDA_INVAL_REQ) printf("invalid request block! ");
	  if (cstat & IDA_INVAL_LIST) printf("cmd list error! ");
	  if (cstat & IDA_AARGH_LIST) printf("really bad cmd list! ");
	  job_status = JOB_FAILURE;
	}

#ifdef IDADEBUG
	if (ida_debug & IDA_SHOWQCBS) {
	  printf("ida%d: idaintr: qcb(%x) returned.\n", cntlr, qcbp);
	  ida_print_qcb(qcbp);
	}
#endif

	ida_done(cntlr, qcbp, job_status); /* retire the job */
	ida_start(cntlr);		/* send the controller another job */
      }

      if (status & BMIC_CHAN_CLEAR) {
	/* channel not clear */
	IDA_DISABLE_CHAN(ida);
	ida_start(cntlr);		/* send the controller another job */
      }
    } /*eisa*/
  }
}


int
ida_poll(cntlr, wait)
int cntlr;
int wait;			/* delay in milliseconds */
{
  struct ida_ctl *ctlp = idadata[cntlr];

  if (PCI_CONTROLLER(ctlp)) {
    printf("ida%d: error: ida_poll called on a PCI controller\n", cntlr);
    return EIO;
  }

  while (wait-- > 0) {
    if (IDA_DATA_READY(ctlp)) {
      ctlp->com_addr = IDA_GET_COM_ADDR(ctlp);
      ctlp->com_offset = IDA_GET_COM_OFFSET(ctlp);
      ctlp->com_status = IDA_GET_COM_STATUS(ctlp);
      IDA_ACK_CMD_COM(ctlp);

      if (0) printf("ida_poll: addr=0x%08x off=0x%04x cmdstatus=0x%02x\n",
		    (u_int)ctlp->com_addr,
		    ctlp->com_offset,
		    ctlp->com_status);

      return 0;
    }
    DELAY(1000);
  }

  printf("ida%d: board not responding\n", cntlr);
  return EIO;
}


int
ida_submit(int cntlr, qcb_t *qcbp, int size)
{
  struct ida_ctl *ida = idadata[cntlr];
  int s = splbio();

  if (PCI_CONTROLLER(ida)) {
    ida_pci_reg_t *reg = (ida_pci_reg_t *)ida->iobase;
    u_int fifo = reg->initiate_fifo;
    if (fifo == 1) {
      splx(s);
#ifdef IDADEBUG
      if (ida_debug & IDA_SHOWSUBS)
	printf("ida%d: ida_submit(%x): fifo=1 not submitting\n",
	       cntlr, qcbp, fifo);
#endif
      return(1);                        /* not sent - must try again later */
    }
#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWSUBS)
      printf("ida%d: ida_submit(%x): fifo=%d submitting\n", cntlr, qcbp, fifo);
#endif
    reg->initiate_fifo = qcbp->paddr;

  } else {
    if (!IDA_CHAN_CLEAR(ida)) {
      IDA_ENABLE_CHAN(ida);
      splx(s);
      return(1);			/* not sent - must try again later */
    }

    IDA_SET_EBELL_I(ida, 2);
    IDA_SET_SUB_ADDR(ida, qcbp->paddr); /* physical address of this qcb */
    IDA_SET_SUB_LEN(ida, size);
    IDA_SET_LBELL_I(ida, 1);
  }

  splx(s);
  return(0);			/* sent */
}

static
void
ida_empty_pci_complete_fifo(int cntlr, ida_pci_reg_t *reg) {
  u_long paddr;
  if (paddr = reg->complete_fifo) {
    int count = 200;
    while (paddr && count > 0) {
      printf("ida%d: command completion discarded (0x%x).\n",
	     cntlr, (u_int)paddr);
      if (paddr = reg->complete_fifo) {
	DELAY(100);
	count--;
      }
    }
  }
  return;
}

static
u_long
ida_complete_pci_command(int cntlr, ida_pci_reg_t *reg) {
  int count = 1;
  u_long paddr;
  while (count < 1000000) {
    if (reg->status & IDA_PCI_PENDING) {
      if ((paddr = reg->complete_fifo) == 0)
        printf("ida%d: ida_complete_pci_command: zero address returned.\n",
	       cntlr);
      else
	return paddr;
    }
    DELAY(10);
  }
  return 1;
}

static
int
ida_submit_wait(int cntlr, qcb_t *qcbp, int size)
{
  struct ida_ctl *ida = idadata[cntlr];

  if (PCI_CONTROLLER(ida)) {
    ida_pci_reg_t *reg = (ida_pci_reg_t *)ida->iobase;
    int i, count = 1000000;
    u_long paddr;
    ida_empty_pci_complete_fifo(cntlr, reg);
    reg->interrupt = IDA_PCI_DISABLE_INTS;
    while (count > 0 && (i = reg->initiate_fifo) > 16) {
      DELAY(10);
      count--;
    }
    if (count == 0) {
      printf("ida%d: ida_pci_submit_wait: fifo failed to clear - controller has failed.\n", cntlr);
      return 1;
    }
    reg->initiate_fifo = qcbp->paddr;
    paddr = ida_complete_pci_command(cntlr, reg);
    if (paddr == 1) {
      printf("ida%d: ida_pci_submit_wait timeout.  No command list returned.\n",
	     cntlr);
      return 1;
    }
    if (paddr != qcbp->paddr) {
      printf("ida%d: ida_pci_submit_wait error. Invalid command list returned.\n",
	     cntlr);
      return 1;
    }
    if (qcbp->req.error != 0xfe && qcbp->req.error == 0x40) {
      printf("ida%d: ida_pci_submit_wait: Job error.\n", cntlr);
      return 1;
    }
  } else {
    if (ida_submit(cntlr, qcbp, size)) {
      return 1;
    }
    if (ida_poll(cntlr, 10)) {
      return 1;
    }
  }

  return 0;			/* sent */
}


void
ida_done(int cntlr, qcb_t *qcbp, int state)
{
  struct buf *bp = qcbp->buf;

  if (idadata[cntlr] > 0)
    idadata[cntlr]->inside--; /* one less job inside the controller */

  if (state != JOB_SUCCESS) {
#ifdef IDADEBUG
    if (ida_debug & IDA_SHOWMISC)
      printf("ida%d: ida_done: job failed 0x%x\n", cntlr, state);
#endif
    /* we had a problem */
    bp->b_error = EIO;
    bp->b_flags |= B_ERROR;
  } else {
    struct ida_drv *drv = id_drive[dkunit(bp->b_dev)];
    bp->b_resid = 0;
    /* Update device stats */
    devstat_end_transaction(&drv->dk_stats,
			    bp->b_bcount - bp->b_resid,
			    DEVSTAT_TAG_NONE,
			    (bp->b_flags & B_READ) ? DEVSTAT_READ : DEVSTAT_WRITE);
  }

  ida_free_qcb(cntlr, qcbp);
  biodone(bp);
}


int
idsize(dev_t dev)
{
  int unit = dkunit(dev);
  struct ida_drv *drv;

  if (unit >= NID)
    return (-1);

  drv = id_drive[unit];
  if (!drv || !(drv->flags & ID_INIT))
    return (-1);

  return (dssize(dev, &drv->slices, idopen, idclose));
}

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
int
iddump(dev_t dev)
{				/* dump core after a system crash */
  return 0; /* XXX */
}

static id_devsw_installed = 0;

static void
id_drvinit(void *unused)
{
  if( ! id_devsw_installed ) {
    cdevsw_add_generic(ID_BDMAJ,ID_CDMAJ, &id_cdevsw);
    id_devsw_installed = 1;
  }
}

SYSINIT(iddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+ID_CDMAJ,id_drvinit,NULL)

#ifdef IDADEBUG
void
ida_print_qcb(qcb_t *qcbp)
{
  int i;
  printf("qcb(%x): drive=%x priority=%x flags=%x sgcount=%d\n"
	 ,qcbp
	 ,qcbp->hdr.drive
	 ,qcbp->hdr.priority
	 ,qcbp->hdr.flags
	 ,qcbp->req.sgcount);
  printf("qcb(%x): next=%x command=%x error=%x blkno=%x bcount=%x\n"
	 ,qcbp
	 ,qcbp->req.next
	 ,qcbp->req.command
	 ,qcbp->req.error
	 ,qcbp->req.blkno
	 ,qcbp->req.bcount);
  for (i=0; i < qcbp->req.sgcount; i++)
    printf("qcb(%x): %x len=%x addr=%x\n"
	   ,qcbp
	   ,i
	   ,qcbp->sglist[i].len
	   ,qcbp->sglist[i].addr);
}

void
ida_print_active_qcb(int cntlr)
{
  struct ida_ctl *ida = idadata[cntlr];
  int i;

  for (i=0; i < QCB_HASH_SIZE; i++) {
    qcb_t *qcbp = ida->hashlist[i];
    while (qcbp) {
      if (qcbp->flags != QCB_FREE) {
	ida_print_qcb(qcbp);
      }
      qcbp = qcbp->nexthash;
    }
  }
}
#endif /*IDADEBUG */


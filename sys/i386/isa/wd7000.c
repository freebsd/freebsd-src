/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * Written by Olof Johansson (offe@ludd.luth.se) 1995.
 * Based on code written by Theo de Raadt (deraadt@fsa.ca).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */

/*
 * offe 01/07/95
 *
 * This version of the driver _still_ doesn't implement scatter/gather for the
 * WD7000-FASST2. This is due to the fact that my controller doesn't seem to
 * support it. That, and the lack of documentation makes it impossible for
 * me to implement it.
 * What I've done instead is allocated a local buffer, contiguous buffer big
 * enough to handle the requests. I haven't seen any read/write bigger than 64k,
 * so I allocate a buffer of 64+16k. The data that needs to be DMA'd to/from
 * the controller is copied to/from that buffer before/after the command is
 * sent to the card.
 */

#include "wds.h"
#if NWDS > 0

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/devconf.h>

#include <machine/clock.h>
#include <machine/cpu.h>

#include <vm/vm.h>

#include <i386/isa/isa_device.h>

static struct kern_devconf kdc_wds[NWDS] = { {
  0, 0, 0,
  "wds", 0, {MDDT_ISA, 0, "bio"},
  isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
  &kdc_isa0,
  0,
  DC_UNCONFIGURED,		/* state */
  "Western Digital WD-7000 SCSI host adapter",
  DC_CLS_MISC			/* class */
} };

static struct scsi_device wds_dev =
{
 	NULL,
	NULL,
	NULL,
	NULL,
	"wds",
	0,
	{ 0, 0 }
};

/*
  XXX   THIS SHOULD BE FIXED!
  I haven't got the KERNBASE-version to work, but on my system the kernel
  is at virtual address 0xFxxxxxxx, responding to physical address
  0x0xxxxxxx.
#define PHYSTOKV(x)	((x) + KERNBASE)
*/
#define PHYSTOKV(x)	((x) | 0xf0000000)
#define KVTOPHYS(x)	vtophys(x)
/* 0x10000 (64k) should be enough. But just to be sure... */
#define BUFSIZ 		0x12000


/* WD7000 registers */
#define WDS_STAT		0	/* read */
#define WDS_IRQSTAT		1	/* read */

#define WDS_CMD			0	/* write */
#define WDS_IRQACK		1	/* write */
#define WDS_HCR			2	/* write */

/* WDS_STAT (read) defs */
#define WDS_IRQ			0x80
#define WDS_RDY			0x40
#define WDS_REJ			0x20
#define WDS_INIT		0x10

/* WDS_IRQSTAT (read) defs */
#define WDSI_MASK		0xc0
#define WDSI_ERR		0x00
#define WDSI_MFREE		0x80
#define WDSI_MSVC		0xc0

/* WDS_CMD (write) defs */
#define WDSC_NOOP		0x00
#define WDSC_INIT		0x01
#define WDSC_DISUNSOL		0x02
#define WDSC_ENAUNSOL		0x03
#define WDSC_IRQMFREE		0x04
#define WDSC_SCSIRESETSOFT	0x05
#define WDSC_SCSIRESETHARD	0x06
#define WDSC_MSTART(m)		(0x80 + (m))
#define WDSC_MMSTART(m)		(0xc0 + (m))

/* WDS_HCR (write) defs */
#define WDSH_IRQEN		0x08
#define WDSH_DRQEN		0x04
#define WDSH_SCSIRESET		0x02
#define WDSH_ASCRESET		0x01

struct wds_cmd {
  u_char cmd;
  u_char targ;
  u_char scb[12];	/*u_char scb[12];*/
  u_char stat;
  u_char venderr;
  u_char len[3];
  u_char data[3];
  u_char next[3];
  u_char write;
  u_char xx[6];
};

struct wds_req {
  struct wds_cmd cmd;
  struct wds_cmd sense;
  struct scsi_xfer *sxp;
  int busy, polled;
  int done, ret, ombn;
};

#define WDSX_SCSICMD		0x00
#define WDSX_OPEN_RCVBUF	0x80
#define WDSX_RCV_CMD		0x81
#define WDSX_RCV_DATA		0x82
#define WDSX_RCV_DATASTAT	0x83
#define WDSX_SND_DATA		0x84
#define WDSX_SND_DATASTAT	0x85
#define WDSX_SND_CMDSTAT	0x86
#define WDSX_READINIT		0x88
#define WDSX_READSCSIID		0x89
#define WDSX_SETUNSOLIRQMASK	0x8a
#define WDSX_GETUNSOLIRQMASK	0x8b
#define WDSX_GETFIRMREV		0x8c
#define WDSX_EXECDIAG		0x8d
#define WDSX_SETEXECPARM	0x8e
#define WDSX_GETEXECPARM	0x8f

struct wds_mb {
  u_char stat;
  u_char addr[3];
};
/* ICMB status value */
#define ICMB_OK			0x01
#define ICMB_OKERR		0x02
#define ICMB_ETIME		0x04
#define ICMB_ERESET		0x05
#define ICMB_ETARCMD		0x06
#define ICMB_ERESEL		0x80
#define ICMB_ESEL		0x81
#define ICMB_EABORT		0x82
#define ICMB_ESRESET		0x83
#define ICMB_EHRESET		0x84

struct wds_setup {
  u_char cmd;
  u_char scsi_id;
  u_char buson_t;
  u_char busoff_t;
  u_char xx;
  u_char mbaddr[3];
  u_char nomb;
  u_char nimb;
};

#define WDS_NOMB	8
#define WDS_NIMB	8
#define MAXSIMUL	8

static int wdsunit=0;

u_char wds_data[NWDS][BUFSIZ];
u_char wds_data_in_use[NWDS];

struct wds {
  int addr;
  struct wds_req wdsr[MAXSIMUL];
  struct wds_mb ombs[WDS_NOMB], imbs[WDS_NIMB];
  struct scsi_link sc_link;
} wds[NWDS];

static int wdsprobe(struct isa_device *);
static void wds_minphys(struct buf *);
static struct wds_req *wdsr_alloc(int);
static int32 wds_scsi_cmd(struct scsi_xfer *);
static u_int32 wds_adapter_info(int);
static int wds_done(int, struct wds_cmd *, u_char);
static int wdsattach(struct isa_device *);
static int wds_init(struct isa_device *);
static int wds_cmd(int, u_char *, int);
static void wds_wait(int, int, int);

struct isa_driver wdsdriver =
{
  wdsprobe,
  wdsattach,
  "wds"
};

static struct scsi_adapter wds_switch =
{
	wds_scsi_cmd,
	wds_minphys,
	0,
	0,
	wds_adapter_info,
	"wds",
	{0,0}
};

int
wdsprobe(struct isa_device *dev)
{
  if(wdsunit > NWDS)
    return 0;

  dev->id_unit = wdsunit;	/* XXX WRONG! */
  wds[wdsunit].addr = dev->id_iobase;

  if(dev->id_unit)
    kdc_wds[dev->id_unit] = kdc_wds[0];
  kdc_wds[dev->id_unit].kdc_unit = dev->id_unit;
  kdc_wds[dev->id_unit].kdc_parentdata = dev;
  dev_attach(&kdc_wds[dev->id_unit]);

  if(wds_init(dev) != 0)
    return 0;
  wdsunit++;
  return 8;
}

void
wds_minphys(struct buf *bp)
{
  if(bp->b_bcount > BUFSIZ)
    bp->b_bcount = BUFSIZ;
}

struct wds_req *
wdsr_alloc(int unit)
{
  struct wds_req *r;
  int x;
  int i;

  r = NULL;
  x = splbio();
  for(i=0; i<MAXSIMUL; i++)
    if(!wds[unit].wdsr[i].busy)
    {
      r = &wds[unit].wdsr[i];
      r->busy = 1;
      break;
    }
  if(!r)
  {
    splx(x);
    return NULL;
  }

  r->ombn = -1;
  for(i=0; i<WDS_NOMB; i++)
    if(!wds[unit].ombs[i].stat)
    {
      wds[unit].ombs[i].stat = 1;
      r->ombn = i;
      break;
    }
  if(r->ombn == -1 )
  {
    r->busy = 0;
    splx(x);
    return NULL;
  }
  splx(x);
  return r;
}

int32
wds_scsi_cmd(struct scsi_xfer *sxp)
{
  struct wds_req *r;
  int unit = sxp->sc_link->adapter_unit;
  int base;
  u_char c, *p;
  int i;

  base = wds[unit].addr;

  if( sxp->flags & SCSI_RESET)
  {
    printf("reset!\n");
    return COMPLETE;
  }

  r = wdsr_alloc(unit);
  if(r==NULL)
  {
    printf("no request slot available!\n");
    sxp->error = XS_DRIVER_STUFFUP;
    return TRY_AGAIN_LATER;
  }
  r->done = 0;
  r->sxp = sxp;

  if(sxp->flags & SCSI_DATA_UIO)
  {
    printf("UIO!\n");
    sxp->error = XS_DRIVER_STUFFUP;
    return TRY_AGAIN_LATER;
  }

  scsi_uto3b(KVTOPHYS(&r->cmd),wds[unit].ombs[r->ombn].addr);

  bzero(&r->cmd, sizeof r->cmd);
  r->cmd.cmd = WDSX_SCSICMD;
  r->cmd.targ = (sxp->sc_link->target << 5) | sxp->sc_link->lun;
  bcopy(sxp->cmd, &r->cmd.scb, sxp->cmdlen<12 ? sxp->cmdlen : 12);
  scsi_uto3b(sxp->datalen, r->cmd.len);

  if(wds_data_in_use[unit])
  {
    sxp->error = XS_DRIVER_STUFFUP;
    return TRY_AGAIN_LATER;
  }
  else
    wds_data_in_use[unit] = 1;

  if(sxp->datalen && !(sxp->flags&SCSI_DATA_IN))
    bcopy(sxp->data, wds_data[unit], sxp->datalen);

  scsi_uto3b(sxp->datalen ? KVTOPHYS(wds_data[unit]) : 0, r->cmd.data);

  r->cmd.write = (sxp->flags&SCSI_DATA_IN)? 0x80 : 0x00;

  scsi_uto3b(KVTOPHYS(&r->sense),r->cmd.next);

  bzero(&r->sense, sizeof r->sense);
  r->sense.cmd = r->cmd.cmd;
  r->sense.targ = r->cmd.targ;
  r->sense.scb[0] = REQUEST_SENSE;
  scsi_uto3b(KVTOPHYS(&sxp->sense),r->sense.data);
  scsi_uto3b(sizeof(sxp->sense), r->sense.len);
  r->sense.write = 0x80;

  if(sxp->flags & SCSI_NOMASK)
  {
    outb(base+WDS_HCR, WDSH_DRQEN);
    r->polled = 1;
  } else
  {
    outb(base+WDS_HCR, WDSH_IRQEN|WDSH_DRQEN);
    r->polled = 0;
  }

  c = WDSC_MSTART(r->ombn);

  if( wds_cmd(base, &c, sizeof c) != 0)
  {
    printf("wds%d: unable to start outgoing mbox\n", unit);
    r->busy = 0;
    wds[unit].ombs[r->ombn].stat = 0;

    return TRY_AGAIN_LATER;
  }

  if(sxp->flags & SCSI_NOMASK)
  {
  repoll:

    i = 0;
    while(!(inb(base+WDS_STAT) & WDS_IRQ))
    {

      DELAY(20000);
      if(++i == 20)
      {
        outb(base + WDS_IRQACK, 0);
	/*r->busy = 0;*/
	sxp->error = XS_TIMEOUT;
	return HAD_ERROR;
      }
    }
    wdsintr(unit);
    if(r->done)
    {
      r->sxp->flags |= ITSDONE;
      r->busy = 0;
      return r->ret;
    }
    goto repoll;
  }

  return SUCCESSFULLY_QUEUED;
}

u_int32
wds_adapter_info(int unit)
{
  return 1;
}

void
wdsintr(int unit)
{
  struct wds_cmd *pc, *vc;
  struct wds_mb *in;
  u_char stat;
  u_char c;

  if(!inb(wds[unit].addr+WDS_STAT) & WDS_IRQ)
  {
    outb(wds[unit].addr + WDS_IRQACK, 0);
    return;
  }

  c = inb(wds[unit].addr + WDS_IRQSTAT);
  if( (c&WDSI_MASK) == WDSI_MSVC)
  {
    c = c & ~WDSI_MASK;
    in = &wds[unit].imbs[c];

    pc = (struct wds_cmd *)scsi_3btou(in->addr);
    vc = (struct wds_cmd *)PHYSTOKV((long)pc);
    stat = in->stat;

    wds_done(unit, vc, stat);
    in->stat = 0;

    outb(wds[unit].addr + WDS_IRQACK, 0);
  }
}

int
wds_done(int unit, struct wds_cmd *c, u_char stat)
{
  struct wds_req *r;
  int i;
  char slask[80];

  r = (struct wds_req *)NULL;

  for(i=0; i<MAXSIMUL; i++)
    if( c == &wds[unit].wdsr[i].cmd && !wds[unit].wdsr[i].done)
    {
      r = &wds[unit].wdsr[i];
      break;
    }
  if(r == (struct wds_req *)NULL)
  {
    /* failed to find request! */
    return 1;
  }

  r->done = 1;
  wds[unit].ombs[r->ombn].stat = 0;
  r->ret = HAD_ERROR;
  switch(stat)
  {
  case ICMB_OK:
    r->ret = COMPLETE;
    if(r->sxp)
      r->sxp->resid = 0;
    break;
  case ICMB_OKERR:
    if(!(r->sxp->flags & SCSI_ERR_OK) && c->stat)
    {
      r->sxp->sense.error_code = c->venderr;
      r->sxp->error=XS_SENSE;
    }
    else
      r->sxp->error=XS_NOERROR;
    r->ret = COMPLETE;
    break;
  case ICMB_ETIME:
    r->sxp->error = XS_TIMEOUT;
    r->ret = HAD_ERROR;
    break;
  case ICMB_ERESET:
  case ICMB_ETARCMD:
  case ICMB_ERESEL:
  case ICMB_ESEL:
  case ICMB_EABORT:
  case ICMB_ESRESET:
  case ICMB_EHRESET:
    r->sxp->error = XS_DRIVER_STUFFUP;
    r->ret = HAD_ERROR;
    break;
  }

  if(r->sxp)
    if(r->sxp->datalen && (r->sxp->flags&SCSI_DATA_IN))
      bcopy(wds_data[unit],r->sxp->data,r->sxp->datalen);

  wds_data_in_use[unit] = 0;

  if(!r->polled)
  {
    r->sxp->flags |= ITSDONE;
    scsi_done(r->sxp);
  }

  r->busy = 0;

  return 0;
}

static int
wds_getvers(int unit)
{
  struct wds_req *r;
  int base;
  u_char c, *p;
  int i;

  base = wds[unit].addr;

  r = wdsr_alloc(unit);
  if(!r)
  {
    printf("wds%d: no request slot available!\n", unit);
    return -1;
  }

  r->done = 0;
  r->sxp = NULL;

  scsi_uto3b(KVTOPHYS(&r->cmd), wds[unit].ombs[r->ombn].addr);

  bzero(&r->cmd, sizeof r->cmd);
  r->cmd.cmd = WDSX_GETFIRMREV;

  outb(base+WDS_HCR, WDSH_DRQEN);
  r->polled = 1;

  c = WDSC_MSTART(r->ombn);
  if(wds_cmd(base, (u_char *)&c, sizeof c))
  {
    printf("wds%d: version request failed\n", unit);
    r->busy = 0;
    wds[unit].ombs[r->ombn].stat = 0;
    return -1;
  }

  while(1)
  {
    i = 0;
    while( (inb(base+WDS_STAT) & WDS_IRQ) == 0)
    {
      DELAY(9000);
      if(++i == 20)
	return -1;
    }
    wdsintr(unit);
    if(r->done)
    {
      printf("wds%d: firmware version %d.%02d\n", unit,
	     r->cmd.targ, r->cmd.scb[0]);
      r->busy = 0;
      return 0;
    }
  }
}

int
wdsattach(struct isa_device *dev)
{
  int masunit;
  static int firstswitch[NWDS];
  static u_long versprobe=0;	/* max 32 controllers */
  int r;
  int unit = dev->id_unit;
  struct scsibus_data *scbus;

  masunit = dev->id_unit;

  if( !(versprobe & (1<<masunit)))
  {
    versprobe |= (1<<masunit);
    if(wds_getvers(masunit)==-1)
      printf("wds%d: getvers failed\n", masunit);
  }

  printf("wds%d: using %d bytes for dma buffer\n",unit,BUFSIZ);

  wds[unit].sc_link.adapter_unit = unit;
  wds[unit].sc_link.adapter_targ = 7;
  wds[unit].sc_link.adapter = &wds_switch;
  wds[unit].sc_link.device = &wds_dev;
  wds[unit].sc_link.flags = SDEV_BOUNCE;

  /*
   * Prepare the scsibus_data area for the upperlevel
   * scsi code.
   */
  scbus = scsi_alloc_bus();
  if(!scbus)
    return 0;
  scbus->adapter_link = &wds[unit].sc_link;

  kdc_wds[unit].kdc_state = DC_BUSY;

  scsi_attachdevs(scbus);

  return 1;
}

int
wds_init(struct isa_device *dev)
{
  struct wds_setup init;
  int base;
  u_char *p, c;
  int unit, i;
  struct wds_cmd wc;

  unit = dev->id_unit;
  base = wds[unit].addr;

  /*
   * Sending a command causes the CMDRDY bit to clear.
   */

  outb(base+WDS_CMD, WDSC_NOOP);
  if( inb(base+WDS_STAT) & WDS_RDY)
    return 1;

  /*
   * the controller exists. reset and init.
   */
  outb(base+WDS_HCR, WDSH_ASCRESET|WDSH_SCSIRESET);
  DELAY(30);
  outb(base+WDS_HCR, 0);

  outb(base+WDS_HCR, WDSH_DRQEN);

  isa_dmacascade(dev->id_drq);

  if( (inb(base+WDS_STAT) & (WDS_RDY)) != WDS_RDY)
  {
    for(i=0; i<10; i++)
    {
      if( (inb(base+WDS_STAT) & (WDS_RDY)) == WDS_RDY)
	break;
      DELAY(40000);
    }
    if( (inb(base+WDS_STAT) & (WDS_RDY)) != WDS_RDY) /* probe timeout */
      return 1;
  }

  bzero(&init, sizeof init);
  init.cmd = WDSC_INIT;
  init.scsi_id = 7;
  init.buson_t = 24;
  init.busoff_t = 48;
  scsi_uto3b(KVTOPHYS(wds[unit].ombs), init.mbaddr);
  init.xx = 0;
  init.nomb = WDS_NOMB;
  init.nimb = WDS_NIMB;

  wds_wait(base+WDS_STAT, WDS_RDY, WDS_RDY);
  if( wds_cmd(base, (u_char *)&init, sizeof init) != 0)
  {
    printf("wds%d: wds_cmd failed\n", unit);
    return 1;
  }

  wds_wait(base+WDS_STAT, WDS_INIT, WDS_INIT);

  wds_wait(base+WDS_STAT, WDS_RDY, WDS_RDY);

  bzero(&wc,sizeof wc);
  wc.cmd = WDSC_DISUNSOL;
  if( wds_cmd(base, (char *)&wc, sizeof wc) != 0)
  {
    printf("wds%d: wds_cmd failed\n", unit);
    return 1;
  }

  return 0;
}

int
wds_cmd(int base, u_char *p, int l)
{
  int s=splbio();
  u_char c;

  while(l--)
  {
    do
    {
      outb(base+WDS_CMD,*p);
      wds_wait(base+WDS_STAT,WDS_RDY,WDS_RDY);
    } while (inb(base+WDS_STAT) & WDS_REJ);
    p++;
  }

  wds_wait(base+WDS_STAT,WDS_RDY,WDS_RDY);

  splx(s);

  return 0;
}

void
wds_wait(int reg, int mask, int val)
{
  while((inb(reg) & mask) != val);
}

#endif

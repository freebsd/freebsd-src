/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
 * Copyright (C) 1994	Poul-Henning Kamp
 * All rights reserved.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: pas.c,v 1.8.2.1 1994/07/11 00:02:32 cgd Exp $
 *
 */

/* Modified for use with the pc532 by Phil Nelson, Feb 94.
 * Modified for use with MediaVision ProAudioSpectrum type adapters
 * under FreeBSD by Poul-Henning Kamp,
 */


#include "pas.h"
#if NPAS > 0
#include "types.h"
#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "buf.h"
#include "stat.h"
#include "uio.h"
#include "ioctl.h"
#include "cdio.h"
#include "errno.h"
#include "dkbad.h"
#include "disklabel.h"
#include "icu.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include "ic/ncr_5380.h"

/* What we need to debug the driver */

#ifdef PAS_DEBUG
static int		pas_show_scsi_cmd(struct scsi_xfer *xs);
#define HERE() printf("<%d>",__LINE__)
#define ARGH() printf("[%d]",__LINE__)
#else /* PAS_DEBUG */
#define HERE() /**/
#define ARGH() /**/
#endif /* PAS_DEBUG */

#define SCSI_PHASE_DATA_OUT	0x0
#define SCSI_PHASE_DATA_IN	0x1
#define SCSI_PHASE_CMD		0x2
#define SCSI_PHASE_STATUS	0x3
#define SCSI_PHASE_UNSPEC1	0x4
#define SCSI_PHASE_UNSPEC2	0x5
#define SCSI_PHASE_MESSAGE_OUT	0x6
#define SCSI_PHASE_MESSAGE_IN	0x7

#define SCSI_PHASE(x)	((x)&0x7)

#define SCSI_RET_SUCCESS	0
#define SCSI_RET_RETRY		1
#define SCSI_RET_DEVICE_DOWN	2
#define SCSI_RET_COMMAND_FAIL	3

/* Our per device (card) structure */
static
struct pas_softc {
	struct scsi_link sc_link;
	int	mv_unit;
	u_short iobase;
#define sci_data(ptr)		(ptr->iobase+0x0)
#define sci_icmd(ptr)		(ptr->iobase+0x1)
#define sci_mode(ptr)		(ptr->iobase+0x2)
#define sci_tcmd(ptr)		(ptr->iobase+0x3)
#define sci_bus_csr(ptr)	(ptr->iobase+0x2000)
#define sci_csr(ptr)		(ptr->iobase+0x2001)
#define sci_idata(ptr)		(ptr->iobase+0x2002)
#define sci_iack(ptr)		(ptr->iobase+0x2003)
#define sci_pdmadata(ptr)	(ptr->iobase+0x4000)
#define sci_pdmastat(ptr)	(ptr->iobase+0x4001)
} s_pas[NPAS];

/* Our access to the 5380 chip */
#define P_PAS struct pas_softc *
#define R_PAS(ptr,foo)		inb(foo(ptr))
#define W_PAS(ptr,foo,val) 	outb(foo(ptr),val)
#define M_PAS(ptr,foo,opr,arg) 	outb(foo(ptr),R_PAS(ptr,foo) opr arg)
#define PSEUDO_DMA 0
#if PSEUDO_DMA 
/*
static int		sci_pdma_out(P_PAS, int, int, u_char *);
*/
#define sci_pdma_out	sci_data_out
static int		sci_pdma_in(P_PAS, int, int, u_char *);

#else /* PSEUDO_DMA */
#define sci_pdma_out	sci_data_out
#define sci_pdma_in	sci_data_in
#endif /* PSEUDO_DMA */



static int pas_foo;
#define SCI_CLR_INTR(ptr)	{pas_foo += R_PAS(ptr,sci_iack);}
#define SCI_PHASE_DISC		0	/* sort of ... */
#define SCI_ACK(ptr,phase)	{W_PAS(ptr,sci_tcmd,phase);}
#define SCSI_TIMEOUT_VAL	10000000
#define WAIT_FOR_NOT_REQ(ptr) {	\
    int scsi_timeout = SCSI_TIMEOUT_VAL; \
    while ( (R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) && \
	    (R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) && \
	    (R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) && \
	     (--scsi_timeout) ); \
    if (!scsi_timeout) { \
	printf("scsi timeout--WAIT_FOR_NOT_REQ-- pas.c:%d.\n", __LINE__); \
	goto scsi_timeout_error; \
    } \
}
#define WAIT_FOR_REQ(ptr) {	\
    int scsi_timeout = SCSI_TIMEOUT_VAL; \
    while ( ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) == 0) && \
	    ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) == 0) && \
	    ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_REQ) == 0) && \
	     (--scsi_timeout) ); \
    if (!scsi_timeout) { \
	printf("scsi timeout--WAIT_FOR_REQ-- pas.c:%d.\n", __LINE__); \
	goto scsi_timeout_error; \
    } \
}
#define WAIT_FOR_BSY(ptr) {	\
    int scsi_timeout = SCSI_TIMEOUT_VAL; \
    while ( ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_BSY) == 0) && \
	    ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_BSY) == 0) && \
	    ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_BSY) == 0) && \
	     (--scsi_timeout) ); \
    if (!scsi_timeout) { \
	printf("scsi timeout--WAIT_FOR_BSY-- pas.c:%d.\n", __LINE__); \
	goto scsi_timeout_error; \
    } \
}

static	u_int32		pas_adapter_info(int adapter_number);
static	void		pas_minphys(struct buf *bp);
static	int32		pas_scsi_cmd(struct scsi_xfer *xs);

static	int		pas_reset_target(int adapter, int target);
static	int		pas_poll(int adapter, int timeout);
static	int		pas_send_cmd(struct scsi_xfer *xs);
static	int 		scsi_req(P_PAS,int,int,u_char *,int,
			    u_char *, int , int *, int *);

static	int		scsi_group0(int adapter, int id, int lun,
			    int opcode, int addr, int len,
			    int flags, caddr_t databuf, int datalen);

static int		sci_data_out(P_PAS, int, int, u_char *);
static int		sci_data_in(P_PAS, int, int, u_char *);

extern	int		pasprobe(struct isa_device *dev);
extern	int		pasattach(struct isa_device *dev);
struct	isa_driver	pasdriver = { pasprobe, pasattach, "pas" };


static
struct scsi_adapter pas_adapter = {
    pas_scsi_cmd,	/* scsi_cmd()		*/
    pas_minphys,	/* scsi_minphys()	*/
    0,			/* open_target_lu()	*/
    0,			/* close_target_lu()	*/
    pas_adapter_info,	/* adapter_info()	*/
"pas",			/* name			*/
    { 0, 0 }		/* spare[2]		*/
};

static
struct scsi_device pas_dev = {
    NULL,		/* Use default error handler.	    */
    NULL,		/* have a queue, served by this (?) */
    NULL,		/* have no async handler.	    */
    NULL,		/* Use default "done" routine.	    */
    "pas",
    0,
    { 0, 0 }
};

static char *mv_type[] = { "?" "PAS","PAS+","CDPC","PAS16C","PAS16D" };


int
pasprobe(struct isa_device *dev)
{
    int port = dev->id_iobase;	
    int base = port - 0x1c00;
    int unit = dev->id_unit;
    int i, j;

    /* Tell the PAS16 we want to talk to, where to listen */
    outb(0x9a01,0xbc + unit);
    outb(0x9a01,base >> 2);

    /* Various magic */
    outb(base+0x4000,0x30);
    outb(base+0x4001,0x01);
    outb(base+0xbc00,0x01);
 
    /* Killer one */
    i = inb(base + 0x803);
    if(i == 0xff) return 0;

    /* killer two */
    outb(base+0x803,i ^ 0xe0);
    j = inb(base + 0x803);
    outb(base+0x803,1);
    if(i != j) return 0;

    /* killer three */
    if((0x03 & inb(base+0xec03)) != 0x03) return 0;
	
    /* killer four */
    for(i=0;i<4;i++) {
	if(inb(port) != 0xff || inb(port+0x2000) != 0xff)
	    return 1;
    }
    return 0;
}

static u_int32
pas_adapter_info(int adapter_number)
{
    return 1;
}

int
pasattach(struct isa_device *dev)
{
    int i,j;
    struct pas_softc *ppas;
    i = inb(dev->id_iobase - 0x1c00 + 0xFC00) >> 5;
    if(i >= (sizeof mv_type / sizeof *mv_type))
	j=0;
    else
	j = i+1;
    printf("pas%d: Type = %d <%s>\n",dev->id_unit,i,mv_type[j]);
    ppas = s_pas + dev->id_unit;
    ppas->sc_link.adapter_unit = dev->id_unit;
    ppas->sc_link.adapter_targ = 7;
    ppas->sc_link.adapter = &pas_adapter;
    ppas->sc_link.device = &pas_dev;
    ppas->iobase=dev->id_iobase;
#ifdef STILL_NO_INTR
/* As of yet we havn't bothered with interrupts, so don't bother */
    j = inb(dev->id_iobase - 0x1c00 + 0xf002);
    switch (dev->id_irq) {
	case IRQ2:	i=1;	break;
	case IRQ3:	i=2;	break;
	case IRQ4:	i=3;	break;
	case IRQ5:	i=4;	break;
	case IRQ6:	i=5;	break;
	case IRQ7:	i=6;	break;
	case IRQ10:	i=7;	break;
	case IRQ11:	i=8;	break;
	case IRQ12:	i=9;	break;
	case IRQ14:	i=10;	break;
	case IRQ15:	i=11;	break;
	default:
	    printf("Intr %d unknown\n",dev->id_irq);
	    panic("brag!");
    }
    printf("0xf002 irq%d code=%x, before %x,",dev->id_irq,i,j);
    j &= 0x0f;
    j |= (i << 4);
    printf(" after= %x \n",j);
    outb(dev->id_iobase - 0x1c00 + 0xf002,j);
    outb(dev->id_iobase - 0x1c00 + 0x8003,0x4d);
#endif /* STILL_NO_INTR */
    scsi_attachdevs(&(ppas->sc_link));
    return 1;
}

static void
pas_minphys(struct buf *bp)
{
#define MIN_PHYS	65536	/*BARF!!!!*/
    if (bp->b_bcount > MIN_PHYS) {
	printf("Uh-oh...  pas_minphys setting bp->b_bcount = %x.\n", MIN_PHYS);
	bp->b_bcount = MIN_PHYS;
    }
#undef MIN_PHYS
}

static int32
pas_scsi_cmd(struct scsi_xfer *xs)
{
    int flags, s, r;

    flags = xs->flags;
    if (xs->bp) flags |= (SCSI_NOSLEEP);
    if ( flags & ITSDONE ) {
	printf("Already done?");
	xs->flags &= ~ITSDONE;
    }
    if ( ! ( flags & INUSE ) ) {
	printf("Not in use?");
	xs->flags |= INUSE;
    }

    if ( flags & SCSI_RESET ) {
	printf("flags & SCSIRESET.\n");
	if ( ! ( flags & SCSI_NOSLEEP ) ) {
	    s = splbio();
	    pas_reset_target(xs->sc_link->adapter_unit,
		xs->sc_link->target);
	    splx(s);
	    return(SUCCESSFULLY_QUEUED);
	} else {
	    pas_reset_target(xs->sc_link->adapter_unit, xs->sc_link->target);
	    if (pas_poll(xs->sc_link->adapter_unit, xs->timeout)) {
		return (HAD_ERROR);
	    }
	    return (COMPLETE);
	}
    }
    /*
     * OK.  Now that that's over with, let's pack up that
     * SCSI puppy and send it off.  If we can, we'll just
     * queue and go; otherwise, we'll wait for the command
     * to finish.
    if ( ! ( flags & SCSI_NOSLEEP ) ) {
	s = splbio();
	pas_send_cmd(xs);
	splx(s);
	return(SUCCESSFULLY_QUEUED);
    }
     */

    r = pas_send_cmd(xs);
    xs->flags |= ITSDONE;
    scsi_done(xs);
    if (xs->flags&SCSI_NOMASK) {
	return (r);
    }
    return SUCCESSFULLY_QUEUED;
}

#ifdef PAS_DEBUG
static int
pas_show_scsi_cmd(struct scsi_xfer *xs)
{
    u_char	*b = (u_char *) xs->cmd;
    int	i  = 0;

    if ( ! ( xs->flags & SCSI_RESET ) ) {
	printf("pas(%d:%d:%d) ",
	    xs->sc_link->adapter_unit, xs->sc_link->target, xs->sc_link->lun);
	printf("%d@%x, %d@%x <",xs->cmdlen,xs->cmd,xs->datalen,xs->data);
	    while (i < xs->cmdlen) {
		if (i) printf(",");
		printf("%x",b[i++]);
	    }
	printf(">\n");
    } else {
	printf("pas(%d:%d:%d)-RESET-\n",
	    xs->sc_link->adapter_unit, xs->sc_link->target,
		xs->sc_link->lun);
    }
    return 0;
}
#endif /* PAS_DEBUG */

/*
 * Actual chip control.
 */

extern void
pasintr(int adapter)
{
    struct pas_softc *ppas = s_pas + adapter;
    printf ("pasintr\n");
    SCI_CLR_INTR(ppas);
    W_PAS(ppas,sci_mode,0x00);
}

#if PHK
extern int
scsi_irq_intr(void)
{

/*  if (R_PAS(ptr,sci_csr) != SCI_CSR_PHASE_MATCH)
	printf("scsi_irq_intr called (not just phase match -- "
	    "csr = 0x%x, bus_csr = 0x%x).\n",
	    R_PAS(ptr,sci_csr), R_PAS(ptr,sci_bus_csr));
    pas_intr(0); */
    return 1;
}
#endif

static int
pas_reset_target(int adapter, int target)
{
    struct pas_softc *ppas = s_pas + adapter;

    W_PAS(ppas,sci_icmd,SCI_ICMD_TEST);
    W_PAS(ppas,sci_icmd,SCI_ICMD_TEST | SCI_ICMD_RST);
    DELAY(2500);
    W_PAS(ppas,sci_icmd,0);

    W_PAS(ppas,sci_mode,0);
    W_PAS(ppas,sci_tcmd,SCI_PHASE_DISC);
    W_PAS(ppas,sci_sel_enb,0);

    SCI_CLR_INTR(ppas);
    SCI_CLR_INTR(ppas);
    return 0;
}

static int
pas_poll(int adapter, int timeout)
{
    return 0;
}

static int
pas_send_cmd(struct scsi_xfer *xs)
{
    P_PAS ptr = s_pas + xs->sc_link->adapter_unit;
    int	s,sent,ret;
    int	sense;

#ifdef PAS_DEBUG
    pas_show_scsi_cmd(xs); 
#endif /* PAS_DEBUG */
    s = splbio();
    sense = scsi_req(ptr,
	      xs->sc_link->target, xs->sc_link->lun,
	      (u_char*)xs->cmd, xs->cmdlen, xs->data, xs->datalen,
	      &sent, &ret);
    splx(s);
#ifdef PAS_DEBUG
HERE();
    printf("sent=%d,ret=%d,sense=%x ",sent,ret,sense);
#endif /* PAS_DEBUG */
    switch (sense) {
	case 0x00:
	    xs->error = XS_NOERROR;
	    xs->resid = sent;
#ifdef PAS_DEBUG
	    printf("\n");
#endif /* PAS_DEBUG */
	    return (COMPLETE);
	case 0x02:	/* Check condition */
#ifdef PAS_DEBUG
	    printf("check cond. targ= %d.\n", xs->sc_link->target); 
#endif
	    spinwait(10);
	    s = splbio();
	    scsi_group0(xs->sc_link->adapter_unit,
		xs->sc_link->target, xs->sc_link->lun,
		0x3, 0x0,
		sizeof(struct scsi_sense_data),
		0, (caddr_t) &(xs->sense),
		sizeof(struct scsi_sense_data));
	    splx(s);
	    xs->error = XS_SENSE;
	    return HAD_ERROR;
	case 0x08:	/* Busy */
ARGH();
	    xs->error = XS_BUSY;
	    return HAD_ERROR;
	default:
ARGH();
	    xs->error = XS_DRIVER_STUFFUP;
	    return HAD_ERROR;
    }
}

static int
select_target(P_PAS ptr, u_char myid, u_char tid, int with_atn)
{
    register u_char bid, icmd;
    int	ret = SCSI_RET_RETRY;
    if ((R_PAS(ptr,sci_bus_csr) & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	(R_PAS(ptr,sci_bus_csr) & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	(R_PAS(ptr,sci_bus_csr) & (SCI_BUS_BSY|SCI_BUS_SEL)))
	    return ret;

    /* for our purposes.. */
    myid = 1 << myid;
    tid = 1 << tid;

    W_PAS(ptr,sci_sel_enb,0); 	/* we don't want any interrupts. */
    W_PAS(ptr,sci_tcmd,0);		/* get into a harmless state */
    W_PAS(ptr,sci_mode,0);		/* get into a harmless state */

    W_PAS(ptr,sci_odata,myid);
    W_PAS(ptr,sci_mode,SCI_MODE_ARB);

    /* AIP might not set if BSY went true after we checked */
    for (bid = 0; bid < 20; bid++)	/* 20usec circa */
	if (R_PAS(ptr,sci_icmd) & SCI_ICMD_AIP)
	    break;
    if ((R_PAS(ptr,sci_icmd) & SCI_ICMD_AIP) == 0) {
ARGH();
	goto lost;
    }

    spinwait(2 /* was 2 */);	/* 2.2us arb delay */

    if (R_PAS(ptr,sci_icmd) & SCI_ICMD_LST) {
printf ("lost 1\n");
	goto lost;
    }

    M_PAS(ptr,sci_mode,&, ~SCI_MODE_PAR_CHK);
    bid = R_PAS(ptr,sci_data);

    if ((bid & ~myid) > myid) {
printf ("lost 2\n");
	goto lost;
    }
    if (R_PAS(ptr,sci_icmd) & SCI_ICMD_LST) {
printf ("lost 3\n");
	goto lost;
    }

	/* Won arbitration, enter selection phase now */	
    icmd = R_PAS(ptr,sci_icmd) & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
    icmd |= (with_atn ? (SCI_ICMD_SEL|SCI_ICMD_ATN) : SCI_ICMD_SEL);
    icmd |= SCI_ICMD_BSY;
    W_PAS(ptr,sci_icmd,icmd);

    if (R_PAS(ptr,sci_icmd) & SCI_ICMD_LST) {
printf ("nosel\n");
	goto nosel;
    }

    /* XXX a target that violates specs might still drive the bus XXX */
    /* XXX should put our id out, and after the delay check nothi XXX */
    /* XXX ng else is out there.				      XXX */

    DELAY(0);

    W_PAS(ptr,sci_tcmd,0);
    W_PAS(ptr,sci_odata,myid|tid);
    W_PAS(ptr,sci_sel_enb,0);

    M_PAS(ptr,sci_mode, &, ~SCI_MODE_ARB); /* 2 deskew delays, too */
    W_PAS(ptr,sci_mode,0);
	
    icmd |= SCI_ICMD_DATA;
    icmd &= ~(SCI_ICMD_BSY);

    W_PAS(ptr,sci_icmd,icmd);

    /* bus settle delay, 400ns */
    DELAY(2); /* too much (was 2) ? */

/*  M_PAS(ptr,sci_mode, |, SCI_MODE_PAR_CHK); */

    {
	register int timeo  = 2500;/* 250 msecs in 100 usecs chunks */
	while ((R_PAS(ptr,sci_bus_csr) & SCI_BUS_BSY) == 0) {
	    if (--timeo > 0) {
		DELAY(100);
	    } else {
ARGH();
		goto nodev;
	    }
	}
    }

    icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_SEL);
    W_PAS(ptr,sci_icmd,icmd);
/*	ptr->sci_sel_enb = myid;*/	/* looks like we should NOT have it */
    return SCSI_RET_SUCCESS;
nodev:
    ret = SCSI_RET_DEVICE_DOWN;
    W_PAS(ptr,sci_sel_enb,myid);
nosel:
    icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_SEL|SCI_ICMD_ATN);
    W_PAS(ptr,sci_icmd,icmd);
lost:
    W_PAS(ptr,sci_mode,0);

    return ret;
}

static int
sci_data_out(P_PAS ptr, int phase, int count, u_char *data)
{
    register unsigned char	icmd;
    register int		cnt=0;

#ifdef PAS_DEBUG
    printf("out%d@%x ",count,data);
#endif /* PAS_DEBUG */
    /* ..checks.. */

    icmd = R_PAS(ptr,sci_icmd) & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
loop:
    if (SCI_CUR_PHASE(R_PAS(ptr,sci_bus_csr)) != phase)
	return cnt;

    WAIT_FOR_REQ(ptr);
    icmd |= SCI_ICMD_DATA;
    W_PAS(ptr,sci_icmd,icmd);
    W_PAS(ptr,sci_odata, *data++);
    icmd |= SCI_ICMD_ACK;
    W_PAS(ptr,sci_icmd,icmd);

    icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_ACK);
    WAIT_FOR_NOT_REQ(ptr);
    W_PAS(ptr,sci_icmd,icmd);
    ++cnt;
    if (--count > 0)
	goto loop;
scsi_timeout_error:
    return cnt;
}

static int
sci_data_in(P_PAS ptr, int phase, int count, u_char *data)
{
    register unsigned char	icmd;
    register int		cnt=0;

#ifdef PAS_DEBUG
    printf("in%d@%x ",count,data);
#endif /* PAS_DEBUG */
    /* ..checks.. */

    icmd = R_PAS(ptr,sci_icmd) & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);

loop:
    if (SCI_CUR_PHASE(R_PAS(ptr,sci_bus_csr)) != phase)
	return cnt;

    WAIT_FOR_REQ(ptr);
    *data++ = R_PAS(ptr,sci_data);

    icmd |= SCI_ICMD_ACK;
    W_PAS(ptr,sci_icmd,icmd);

    icmd &= ~SCI_ICMD_ACK;
    WAIT_FOR_NOT_REQ(ptr);
    W_PAS(ptr,sci_icmd,icmd);
    ++cnt;
    if (--count > 0)
	goto loop;

scsi_timeout_error:
    return cnt;
}

#if PSEUDO_DMA

static int
sci_pdma_in(P_PAS ptr, int phase, int count, u_char *data)
{
    register unsigned char	icmd;
    register int		cnt=0;

#ifdef PAS_DEBUG
    printf("in%d@%x ",count,data);
#endif /* PAS_DEBUG */

    WAIT_FOR_BSY(ptr);
    M_PAS(ptr,sci_mode, |, SCI_MODE_DMA);
    W_PAS(ptr,sci_dma_send, 0);
    M_PAS(ptr,sci_icmd, |, SCI_ICMD_DATA);

/*
    while(R_PAS(ptr,sci_pdmastat) & 0x80) ;
*/

    for(; cnt < count; cnt++)
	*data++ = R_PAS(ptr,sci_pdmadata);

scsi_timeout_error:
    M_PAS(ptr,sci_mode, &, ~SCI_MODE_DMA);
    M_PAS(ptr,sci_icmd, &, ~SCI_ICMD_DATA);
    return cnt;
}
#endif /* PSEUDO_DMA */

static int
cmd_xfer(P_PAS ptr, int maxlen, u_char *data, u_char *status, u_char *msg)
{
    int	xfer=0, phase;

#ifdef PAS_DEBUG
    printf("cmd_xfer called for 0x%x.\n", *data); 
#endif /* PAS_DEBUG */

    W_PAS(ptr,sci_icmd,0);

    while (1) {

	WAIT_FOR_REQ(ptr);

	phase = SCI_CUR_PHASE(R_PAS(ptr,sci_bus_csr));

	switch (phase) {
	    case SCSI_PHASE_CMD:
		SCI_ACK(ptr,SCSI_PHASE_CMD);
		xfer += sci_data_out(ptr, SCSI_PHASE_CMD, maxlen, data);
		return xfer;
	    case SCSI_PHASE_DATA_IN:
		printf("Data in phase in cmd_xfer?\n");
		return 0;
	    case SCSI_PHASE_DATA_OUT:
		printf("Data out phase in cmd_xfer?\n");
		return 0;
	    case SCSI_PHASE_STATUS:
		SCI_ACK(ptr,SCSI_PHASE_STATUS);
		printf("status in cmd_xfer.\n");
		sci_data_in(ptr, SCSI_PHASE_STATUS, 1, status);
		break;
	    case SCSI_PHASE_MESSAGE_IN:
		SCI_ACK(ptr,SCSI_PHASE_MESSAGE_IN);
		printf("msgin in cmd_xfer.\n");
		sci_data_in(ptr, SCSI_PHASE_MESSAGE_IN, 1, msg);
		break;
	    case SCSI_PHASE_MESSAGE_OUT:
		SCI_ACK(ptr,SCSI_PHASE_MESSAGE_OUT);
		sci_data_out(ptr, SCSI_PHASE_MESSAGE_OUT, 1, msg);
		break;
	    default:
		printf("Unexpected phase 0x%x in cmd_xfer()\n", phase);
scsi_timeout_error:
		return xfer;
		break;
	}
    }
}

static int
data_xfer(P_PAS ptr, int maxlen, u_char *data, u_char *status, u_char *msg)
{
    int	retlen = 0, xfer, phase;

    W_PAS(ptr,sci_icmd,0);

    *status = 0;

    while (1) {

	WAIT_FOR_REQ(ptr);

	phase = SCI_CUR_PHASE(R_PAS(ptr,sci_bus_csr));
	switch (phase) {
	    case SCSI_PHASE_CMD:
		printf("Command phase in data_xfer().\n");
		return retlen;
	    case SCSI_PHASE_DATA_IN:
		SCI_ACK(ptr,SCSI_PHASE_DATA_IN);
		xfer = sci_pdma_in (ptr, SCSI_PHASE_DATA_IN, maxlen, data);
		retlen += xfer;
		maxlen -= xfer;
		break;
	    case SCSI_PHASE_DATA_OUT:
		SCI_ACK(ptr,SCSI_PHASE_DATA_OUT);
		xfer = sci_pdma_out (ptr, SCSI_PHASE_DATA_OUT, maxlen, data);
		retlen += xfer;
		maxlen -= xfer;
		break;
	    case SCSI_PHASE_STATUS:
		SCI_ACK(ptr,SCSI_PHASE_STATUS);
		sci_data_in(ptr, SCSI_PHASE_STATUS, 1, status);
		break;
	    case SCSI_PHASE_MESSAGE_IN:
		SCI_ACK(ptr,SCSI_PHASE_MESSAGE_IN);
		sci_data_in(ptr, SCSI_PHASE_MESSAGE_IN, 1, msg);
		if (*msg == 0) {
		    return retlen;
		} else {
		    printf( "message 0x%x in data_xfer.\n", *msg);
		}
		break;
	    case SCSI_PHASE_MESSAGE_OUT:
		SCI_ACK(ptr,SCSI_PHASE_MESSAGE_OUT);
		sci_data_out(ptr, SCSI_PHASE_MESSAGE_OUT, 1, msg);
		break;
	    default:
		printf( "Unexpected phase 0x%x in data_xfer().\n",
		    phase);
scsi_timeout_error:
		return retlen;
		break;
	}
    }
}

static int
scsi_req(P_PAS ptr, int target, int lun, u_char *cmd, int cmdlen,
		u_char *databuf, int datalen, int *sent, int *ret)
{
/* Returns 0 on success, -1 on internal error, or the status byte */
    int	cmd_bytes_sent, r;
    u_char	stat, msg, c;

    *sent = 0;
    if ( ( r = select_target(ptr, 7, target, 1) ) != SCSI_RET_SUCCESS) {
		*ret = r;
	SCI_CLR_INTR(ptr);
	switch (r) {
	    case SCSI_RET_RETRY:
ARGH();
		return 0x08;
	    default:
		printf("select_target(target %d, lun %d) failed(%d).\n",
		    target, lun, r);
	    case SCSI_RET_DEVICE_DOWN:
		return -1;
	}
    }

    c = 0x80 | lun;

    if ((cmd_bytes_sent = cmd_xfer(ptr, cmdlen, cmd, &stat, &c))
	     != cmdlen) {
ARGH();
	SCI_CLR_INTR(ptr);
	*ret = SCSI_RET_COMMAND_FAIL;
	printf("Data underrun sending CCB (%d bytes of %d, sent).\n",
	    cmd_bytes_sent, cmdlen);
	return -1;
    }

    *sent=data_xfer(ptr, datalen, databuf, &stat, &msg);

    *ret = 0;
#ifdef PAS_DEBUG
    printf("scsi_req,stat=0x%x ",stat);
#endif /* PAS_DEBUG */
    return stat;
}

static int
scsi_group0(int adapter, int id, int lun,
		int opcode, int addr, int len,
		int flags, caddr_t databuf, int datalen)
{
    P_PAS ptr = s_pas + adapter;
    unsigned char cmd[6];
    int i,sent,ret;

    cmd[0] = opcode;			/* Operation code */
    cmd[1] = (lun<<5) | ((addr>>16) & 0x1F); /* Lun & MSB of addr */
    cmd[2] = (addr >> 8) & 0xFF;	/* addr	*/
    cmd[3] = addr & 0xFF;		/* LSB of addr */
    cmd[4] = len;			/* Allocation length */
    cmd[5] = flags;			/* Link/Flag */

    i = scsi_req(ptr, id, lun, cmd, 6, databuf, datalen, &sent, &ret);

    return i;
}

/* pseudo-dma action */

#if 0 && PSEUDO_DMA

static int pas_debug=1;
#define W1	*byte_data = *data++
#define W4	*long_data = *((long*)data)++

int
sci_pdma_out(ptr, phase, count, data)
	P_PAS ptr;
	int					phase;
	int					count;
	u_char					*data;
{
	register volatile long		*long_data = sci_4byte_addr;
	register volatile u_char	*byte_data = sci_1byte_addr;
	register int			len = count, i;

pas_debug=1;

	if (count < 128)
		return sci_data_out(ptr, phase, count, data);

	WAIT_FOR_BSY(ptr);
	M_PAS(ptr,sci_mode, |, SCI_MODE_DMA);
	M_PAS(ptr,sci_icmd, |, SCI_ICMD_DATA);
	W_PAS(ptr,sci_dma_send,0);

	while ( len >= 64 ) {
		READY(1); W1; READY(1); W1; READY(1); W1; READY(1); W1;
		READY(1);
		W4;W4;W4; W4;W4;W4;W4; W4;W4;W4;W4; W4;W4;W4;W4;
		len -= 64;
	}
	while (len) {
		READY(1);
		W1;
		len--;
	}
	i = TIMEOUT;
	while ( ((R_PAS(ptr,sci_csr) & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH))
		== SCI_CSR_PHASE_MATCH) && --i);
	if (!i)
		printf("pas.c:%d: timeout waiting for SCI_CSR_DREQ.\n", __LINE__);
	*byte_data = 0;
scsi_timeout_error:
	M_PAS(ptr,sci_mode,&, ~SCI_MODE_DMA);
	return count-len;
}

#undef  W1
#undef  W4

#define R4	*((long *)data)++ = *long_data
#define R1	*data++ = *byte_data

int
sci_pdma_in(ptr, phase, count, data)
	P_PAS ptr;
	int					phase;
	int					count;
	u_char					*data;
{
	register volatile long		*long_data = sci_4byte_addr;
	register volatile u_char	*byte_data = sci_1byte_addr;
	register int			len = count, i;

pas_debug=2;
	if (count < 128)
		return sci_data_in(ptr, phase, count, data);

/*	printf("Called sci_pdma_in(0x%x, 0x%x, %d, 0x%x.\n", ptr, phase, count, data); */

	WAIT_FOR_BSY(ptr);
	M_PAS(ptr,sci_mode, |, SCI_MODE_DMA);
	M_PAS(ptr,sci_icmd, |, SCI_ICMD_DATA);
	W_PAS(ptr,sci_irecv, 0);

	while (len >= 1024) {
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 128 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 256 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 384 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 512 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 640 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 768 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 896 */
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /*1024 */
		len -= 1024;
	}
	while (len >= 128) {
		READY(0);
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; 
		R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; R4;R4;R4;R4; /* 128 */
		len -= 128;
	}
	while (len) {
		READY(0);
		R1;
		len--;
	}
scsi_timeout_error:
	M_PAS(ptr,sci_mode, &, ~SCI_MODE_DMA);
	return count - len;
}
#undef R4
#undef R1
#endif
#endif /* NPAS */

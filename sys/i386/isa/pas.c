/*
 * Copyright (C) 1994	Poul-Henning Kamp
 *
 * All rights reserved.
 *
 * This file contains some material which are covered by the message after 
 * this message.
 *
 * The rest of this file is covered by the following clause:
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: pas.c,v 1.3 1994/09/13 06:44:39 phk Exp $
 *
 * This is a driver for the one particular kind of the "ProAudioSpectrum"
 * card from MediaVision.  To find out if your card is supported, you can
 * either try out the driver, or you can look for a chip a little less than
 * 1" square in one end of the card, with writing on it that say ...5380...
 *
 * Up to four of these cards can be in the same computer.  If you have 
 * multiple cards, you need to set the "card-id" jumpers correspondingly.
 *
 * The driver uses no interrupts, so don't expect record-breaking performance.
 *
 * Poul-Henning Kamp <phk@freefall.cdrom.com>
 */

/*
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 * Was: Id: pas.c,v 1.8.2.1 1994/07/11 00:02:32 cgd Exp
 *
 * Modified for use with the pc532 by Phil Nelson, Feb 94.
 */


#include "pas.h"
#if NPAS > 0

#include "param.h"
#include "systm.h"
#include "types.h"
#include "buf.h"
#include "i386/isa/isa_device.h"
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include "ic/ncr_5380.h"

/*
 * Define this macro to disable the PSEUDO-DMA transfers.
 */
#undef NO_PAS16_PSEUDO_DMA

/*
 * This parameter determines how many byte we "insb" between each check for
 * a change of phase.
 */

#ifndef PAS16_PSEUDO_DMA_SIZE
#  define PAS16_PSEUDO_DMA_SIZE 512
#endif

/*
 * How many times we attempt to select a device.
 */
#ifndef ATTEMPT_SELECTION 
#  define ATTEMPT_SELECTION 5
#endif /* ATTEMPT_SELECTION */

/*
 * How many microseconds between each attempt.
 */
#ifndef SELECTION_DELAY 
#  define SELECTION_DELAY 10000
#endif /* SELECTION_DELAY */

#ifndef PAS16_TIMEOUT
#  define PAS16_TIMEOUT	1000000
#endif /* PAS16_TIMEOUT */
/* What we need to debug the driver */

#ifdef PAS_DEBUG
#  if PAS_DEBUG == 0
#    undef PAS_DEBUG
#  else
     static int		pas_show_scsi_cmd(struct scsi_xfer *xs);
#    ifndef PAS_DEBUG_REQUEST_SENSE
#      define PAS_DEBUG_REQUEST_SENSE
#    endif
#  endif
#endif /* PAS_DEBUG */

#define SCSI_PHASE_DATA_OUT	0x0
#define SCSI_PHASE_DATA_IN	0x1
#define SCSI_PHASE_CMD		0x2
#define SCSI_PHASE_STATUS	0x3
#define SCSI_PHASE_UNSPEC1	0x4
#define SCSI_PHASE_UNSPEC2	0x5
#define SCSI_PHASE_MESSAGE_OUT	0x6
#define SCSI_PHASE_MESSAGE_IN	0x7

#define SCSI_CUR_PHASE(x)	((x>>2)&0x7)

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
#define xor_0000 iobase
	u_short xor_0001;
	u_short xor_0002;
	u_short xor_0003;
	u_short xor_2000;
	u_short xor_2001;
	u_short xor_2002;
	u_short xor_2003;
	u_short xor_4000;
	u_short xor_4001;
	u_short xor_4003;
} s_pas[NPAS];

/*
 * Register Access-macros use these
 */
#define r_csdr(ptr)	(ptr->xor_0000)
#define w_odr(ptr)	(ptr->xor_0000)
#define r_icr(ptr)	(ptr->xor_0001)
#define w_icr(ptr)	(ptr->xor_0001)
#define rw_mr(ptr)	(ptr->xor_0002)
#define r_tcr(ptr)	(ptr->xor_0003)
#define w_tcr(ptr)	(ptr->xor_0003)
#define r_cscr(ptr)	(ptr->xor_2000)
#define w_ser(ptr)	(ptr->xor_2000)
#define r_bsr(ptr)	(ptr->xor_2001)
#define w_sdsr(ptr)	(ptr->xor_2001)
#define r_idr(ptr)	(ptr->xor_2002)
#define w_sdtr(ptr)	(ptr->xor_2002)
#define r_rpir(ptr)	(ptr->xor_2003)
#define w_sdir(ptr)	(ptr->xor_2003)
#define pas_data(ptr)	(ptr->xor_4000)
#define pas_stat(ptr)	(ptr->xor_4001)
#define R_PAS_STAT_DREQ		0x80
#define pas_irq(ptr)	(ptr->xor_4003)

/*
 * The actual macros used to access the chip
 */
#define P_PAS struct pas_softc *
#define R_PAS(ptr,foo)		inb(foo(ptr))
#define W_PAS(ptr,foo,val) 	outb(foo(ptr),val)
#define M_PAS(ptr,foo,opr,arg) 	outb(foo(ptr),R_PAS(ptr,foo) opr arg)

#define WAIT_FOR_NOT_REQ(ptr) {	\
    int scsi_timeout = PAS16_TIMEOUT; \
    while ( (R_PAS(ptr,r_cscr) & R_CSCR_REQ) && \
	    (R_PAS(ptr,r_cscr) & R_CSCR_REQ) && \
	    (R_PAS(ptr,r_cscr) & R_CSCR_REQ) && \
	     (--scsi_timeout) ); \
    if (!scsi_timeout) { \
	printf("scsi timeout--WAIT_FOR_NOT_REQ-- pas.c:%d.\n", __LINE__); \
	goto scsi_timeout_error; \
    } \
}
#define WAIT_FOR_REQ(ptr) {	\
    int scsi_timeout = PAS16_TIMEOUT; \
    while ( ((R_PAS(ptr,r_cscr) & R_CSCR_REQ) == 0) && \
	    ((R_PAS(ptr,r_cscr) & R_CSCR_REQ) == 0) && \
	    ((R_PAS(ptr,r_cscr) & R_CSCR_REQ) == 0) && \
	     (--scsi_timeout) ); \
    if (!scsi_timeout) { \
	printf("scsi timeout--WAIT_FOR_REQ-- pas.c:%d.\n", __LINE__); \
	goto scsi_timeout_error; \
    } \
}

static	u_int32		pas_adapter_info(int adapter_number);
static	void		pas_minphys(struct buf *bp);
static	int32		pas_scsi_cmd(struct scsi_xfer *xs);
static	int		pas_reset(int);
static	int		pas_send_cmd(struct scsi_xfer *xs);
static	int 		scsi_req(P_PAS,int,int,u_char *,int,
			    u_char *, int , int *, int *);
static	int		sci_data_out(P_PAS, int, int, u_char *);
static	int		sci_data_in(P_PAS, int, int, u_char *);
static	int 		select_target(P_PAS, int);

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
    "pas",		/* name			*/
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

static char *mv_type[] = {
	0,0,0,0,0,0,"PAS+",0,0,0,0,0,"PAS16D",0,"CDPC","PAS16"};

int
pasprobe(struct isa_device *dev)
{
    int port = dev->id_iobase;	
    int base = port - 0x1c00;
    int unit = dev->id_unit;
    int i, j;

    if(base & 3) {
	printf("pas%d cannot operate on address %x.", unit,port);
	printf("  Address must be a multiple of four.\n");
	return 0;
    }
    /* Tell the PAS16 we want to talk to, where to listen */
    outb(0x9a01,0xbc + unit);
    outb(0x9a01,base >> 2);

#ifdef PAS_DEBUG
    printf("%x: 0x803=%x 0xec03=%x 0xff88=%x\n",
	base,inb(base^0x803),inb(base^0xec03),inb(base^0xfc00));
#endif

    /* Killer one */
    i = inb(base ^ 0x803);
    if(i == 0xff) return 0;

    /* killer two */
    if((0x03 & inb(base^0xec03)) != 0x03) return 0;
	
    /* killer three */
    for(i=0;i<4;i++) {
	if(inb(port) != 0xff || inb(port^0x2000) != 0xff)
	    goto ok;
    }
    return 0;
ok:
    /* killer four */
    i = inb(base ^ 0x803);
    outb(base^0x803,i ^ 0xe0);
    j = inb(base^0x803);
    outb(base^0x803,1);
    if(i != j) return 0;

    return 4;
}

static u_int32
pas_adapter_info(int adapter_number)
{
    return 1;
}

int
pasattach(struct isa_device *dev)
{
    int i,base=dev->id_iobase-0x1c00;
    struct pas_softc *ppas;
    i = inb(base^0xEC03) & 0x0f;
    printf("pas%d: Type = %d <%s>\n",dev->id_unit,i,mv_type[i]);
    ppas = s_pas + dev->id_unit;
    ppas->sc_link.adapter_unit = dev->id_unit;
    ppas->sc_link.adapter_targ = 7;
    ppas->sc_link.adapter = &pas_adapter;
    ppas->sc_link.device = &pas_dev;
    ppas->iobase=dev->id_iobase;
    ppas->xor_0001 = ppas->iobase^0x0001;
    ppas->xor_0002 = ppas->iobase^0x0002;
    ppas->xor_0003 = ppas->iobase^0x0003;
    ppas->xor_2000 = ppas->iobase^0x2000;
    ppas->xor_2001 = ppas->iobase^0x2001;
    ppas->xor_2002 = ppas->iobase^0x2002;
    ppas->xor_2003 = ppas->iobase^0x2003;
    ppas->xor_4000 = ppas->iobase^0x4000;
    ppas->xor_4001 = ppas->iobase^0x4001;
    ppas->xor_4003 = ppas->iobase^0x4003;

    /* Various magic */
    outb(base^0x4000,0x30);
    outb(base^0x4001,0x01);
    outb(base^0xbc00,0x01);
    outb(base^0x8003,0x4d);

    scsi_attachdevs(&(ppas->sc_link));
    pas_reset(dev->id_unit);
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
	    pas_reset(xs->sc_link->adapter_unit);
	    splx(s);
	    return(SUCCESSFULLY_QUEUED);
	} else {
	    pas_reset(xs->sc_link->adapter_unit);
	    return (COMPLETE);
	}
    }

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
	printf("%d@%x, %d@%x <",xs->cmdlen,(u_long)xs->cmd,
	    xs->datalen,(u_long)xs->data);
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

static int
pas_reset(int adapter)
{
    struct pas_softc *ppas = s_pas + adapter;

    /*
     * Reset Hold Time is 25 uSec
     */
    W_PAS(ppas,w_icr,W_ICR_ASSERT_RST);
    DELAY(25);
    W_PAS(ppas,w_icr,0);

    W_PAS(ppas,rw_mr,0);
    W_PAS(ppas,w_ser,0);

    return 0;
}

static int
pas_send_cmd(struct scsi_xfer *xs)
{
    P_PAS ptr = s_pas + xs->sc_link->adapter_unit;
    int	s,sent,ret;
    int	sense;
    u_char cmd[6];

#ifdef PAS_DEBUG
    pas_show_scsi_cmd(xs); 
#endif /* PAS_DEBUG */
    s = splbio();
    sense = scsi_req(ptr,
	      xs->sc_link->target, xs->sc_link->lun,
	      (u_char*)xs->cmd, xs->cmdlen, xs->data, xs->datalen,
	      &sent, &ret);
    splx(s);
    switch (sense) {
	case 0x00:
	    xs->error = XS_NOERROR;
	    xs->resid = sent;
	    return (COMPLETE);
	case 0x02:	/* Check condition */

#ifdef PAS_DEBUG_REQUEST_SENSE
	    printf("pas%d, target%d: sent=%d,ret=%d,sense=%x check. cond.\n", 
		xs->sc_link->adapter_unit, xs->sc_link->target,sent,ret,sense);
	    printf("Cmd:<%x",((u_char*)(xs->cmd))[0]);
	    for(sent=1;sent < sizeof xs->cmdlen;sent++) {
		printf(",%x",((u_char*)(xs->cmd))[sent]);
	    }
	    printf(">\n");
#endif /* PAS_DEBUG_REQUEST_SENSE */
	    cmd[0] = 3;
	    cmd[1] = xs->sc_link->lun<<5;
	    cmd[2] = 0;
	    cmd[3] = 0;
	    cmd[4] = sizeof xs->sense;
	    cmd[5] = 0;

	    DELAY(10);
	    s = splbio();
	    scsi_req(ptr, xs->sc_link->target, xs->sc_link->lun,
		cmd, sizeof cmd,
		(caddr_t) &(xs->sense), sizeof xs->sense,	
		&sent, &ret);
	    splx(s);
#ifdef PAS_DEBUG_REQUEST_SENSE
	    printf("Sense result: (sent=%d, ret=%d)\n",sent,ret);
	    printf("<%x",((u_char*)(&xs->sense))[0]);
	    for(sent=1;sent < sizeof xs->sense;sent++) {
		printf(",%x",((u_char*)(&xs->sense))[sent]);
	    }
	    printf(">\n");
#endif /* PAS_DEBUG_REQUEST_SENSE */
	    xs->error = XS_SENSE;
	    return HAD_ERROR;
	case 0x08:	/* Busy */
	    xs->error = XS_BUSY;
	    return HAD_ERROR;
	default:
	    xs->error = XS_DRIVER_STUFFUP;
	    return HAD_ERROR;
    }
}

/*
 * Perform arbitration and selection.  Figure 5-1, more or less.
 */

static int
select_target(P_PAS ptr, int tid)
{
    int myid = ptr->sc_link.adapter_targ;
    int ret =  SCSI_RET_RETRY;
    int tries = ATTEMPT_SELECTION;
    int delay = SELECTION_DELAY;
    int i;

    /* 
     * Convert the ID's to bit-maps.
     */
    myid = 1 << myid;
    tid = 1 << tid;

    /*
     * Not documented, but clearly needed for now.
     * XXX is this missing somewhere else, and this is just a hack ?
     */
loop:
    W_PAS(ptr,w_tcr,0);


    /*
     * Write ID Bit to Output Register.
     */
    W_PAS(ptr,w_odr,myid);

    /*
     * Set "ARBITRATE" Bit.
     */
    W_PAS(ptr,rw_mr,RW_MR_ARBITRATE);

    /*
     * Check "Arbitration in progress" Bit.
     */
    for (i = 0; i < 20; i++)	/* 20usec circa */
	if (R_PAS(ptr,r_icr) & R_ICR_ARBITRATION_IN_PROGRESS)
	    goto aip;
    goto lost;

aip:
    /*
     * Wait 2.2 usec Arbitration Delay.
     */
    DELAY(2);

    /*
     * Check "Lost Arbitration" Bit.
     */
    if (R_PAS(ptr,r_icr) & R_ICR_LOST_ARBITRATION)
	goto lost;

    /*
     * Higher priority ID present ?
     */
    if ((R_PAS(ptr,r_csdr) & ~myid) > myid)
	goto lost;

    /*
     * Check "Lost Arbitration" Bit.
     */
    if (R_PAS(ptr,r_icr) & R_ICR_LOST_ARBITRATION)
	goto lost;

    /*
     * Set "Assert SEL/".
     */
    W_PAS(ptr,w_icr,W_ICR_ASSERT_SEL);

    /*
     * Check "Lost Arbitration" Bit.
     */
    if (R_PAS(ptr,r_icr) & R_ICR_LOST_ARBITRATION) {
	goto nosel;
    }

    /*
     * Wait 1.2 usec minimum. (Bus Clear + Settle)
     */
    DELAY(2);

    /*
     * Write Target and Initiator's ID bits to output Register.
     */
    W_PAS(ptr,w_odr,myid|tid);

    /*
     * Set "Assert BSY/" + "Assert Data Bus".
     */
    W_PAS(ptr,w_icr,
	W_ICR_ASSERT_SEL | W_ICR_ASSERT_BSY | W_ICR_ASSERT_DATA_BUS);

    /*
     * Reset "ARBITRATE" Bit.
     */
    W_PAS(ptr,rw_mr,0);

    /*
     * Reset "Select Enable" Register.
     */
    W_PAS(ptr,w_ser,0);
	
    /*
     * Reset "Assert BSY/".
     */
    W_PAS(ptr,w_icr,
	W_ICR_ASSERT_SEL | W_ICR_ASSERT_DATA_BUS);

    /*
     * "BSY/" Asserted within 250 msec ?
     */
    for(i=0;i<250000;i+=100) {
	if (R_PAS(ptr,r_cscr) & R_CSCR_BSY)
	    goto resp;
	DELAY(100);
    }
    goto nodev;
resp:

    /*
     * Reset "Assert SEL/" + "Assert Data Bus".
     */
    W_PAS(ptr,w_icr,0);

    return SCSI_RET_SUCCESS;

nodev:
    ret = SCSI_RET_DEVICE_DOWN;

nosel:
    W_PAS(ptr,w_icr,0);

lost:
    W_PAS(ptr,rw_mr,0);
    if(--tries) {
	DELAY(delay);
	goto loop;
    }

    return ret;
}

/*
 * Figure 5-2.
 */
int PHK;
static int
sci_data_out(P_PAS ptr, int phase, int count, u_char *data)
{
    register unsigned char	icmd;
    register int		cnt=0,zonk;

    if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
	return cnt;

    icmd = R_PAS(ptr,r_icr) & RW_ICR_MASK;

#ifndef NO_PAS16_PSEUDO_DMA
    /*
     * The PAS16 has special provisions for doing pseudo-dma.
     */
     
    /* 
     * Don't use pseudo-dma unless we need much data.  This way we avoid all
     * the variable length stuff entirely.
     *
     * XXX something screws up for big transfers :-(
     */

    if(count >= 128 && count <= 8192) {

	M_PAS(ptr,rw_mr,|,RW_MR_DMA_MODE);

	W_PAS(ptr,w_icr,icmd | W_ICR_ASSERT_DATA_BUS);

	W_PAS(ptr,w_sdsr,0);

	while(count >= PAS16_PSEUDO_DMA_SIZE) {
	    zonk=0;
	    while(!(R_PAS(ptr,pas_stat) & R_PAS_STAT_DREQ)) 
		if (++zonk > 1000 || SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
		    return cnt;

	    outsb(pas_data(ptr),data,PAS16_PSEUDO_DMA_SIZE);

	    data += PAS16_PSEUDO_DMA_SIZE;
	    cnt += PAS16_PSEUDO_DMA_SIZE;
	    count -= PAS16_PSEUDO_DMA_SIZE;

	}

	if(count) {
	    while(!(R_PAS(ptr,pas_stat) & R_PAS_STAT_DREQ))
		if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
		    return cnt;
	    outsb(pas_data(ptr),data,count);
	    cnt += count;
	    data += count;
	}

        M_PAS(ptr,rw_mr,&,~RW_MR_DMA_MODE);
	W_PAS(ptr,w_icr,icmd);
	return cnt;
    }
#endif /* NO_PAS16_PSEUDO_DMA */
loop:
    if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
	return cnt;

    W_PAS(ptr,w_odr, *data);
    WAIT_FOR_REQ(ptr);
    icmd |= W_ICR_ASSERT_DATA_BUS;
    W_PAS(ptr,w_icr,icmd);
    icmd |= W_ICR_ASSERT_ACK;
    W_PAS(ptr,w_icr,icmd);

    icmd &= ~(W_ICR_ASSERT_DATA_BUS|W_ICR_ASSERT_ACK);
    WAIT_FOR_NOT_REQ(ptr);
    W_PAS(ptr,w_icr,icmd);
    ++cnt;
    data++;
    if (--count > 0)
	goto loop;
scsi_timeout_error:
    return cnt;
}

static int
sci_data_in(P_PAS ptr, int phase, int count, u_char *data)
{
    unsigned char icmd;
    int	cnt=0;

    if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
	return cnt;

#ifndef NO_PAS16_PSEUDO_DMA
    /*
     * The PAS16 has special provisions for doing pseudo-dma.
     */
     
    /* 
     * Don't use pseudo-dma unless we need much data.  This way we avoid all
     * the variable length stuff entirely.
     */
    if(count >= 128) {

	M_PAS(ptr,rw_mr,|,RW_MR_DMA_MODE);

	W_PAS(ptr,w_sdir,0);

	while(count > PAS16_PSEUDO_DMA_SIZE) {
	    while(!(R_PAS(ptr,pas_stat) & R_PAS_STAT_DREQ)) ;

	    insb(pas_data(ptr),data,PAS16_PSEUDO_DMA_SIZE);

	    data += PAS16_PSEUDO_DMA_SIZE;
	    cnt += PAS16_PSEUDO_DMA_SIZE;
	    count -= PAS16_PSEUDO_DMA_SIZE;

	    if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
		return cnt;
	}

	while(!(R_PAS(ptr,pas_stat) & R_PAS_STAT_DREQ)) ;

	insb(pas_data(ptr),data,count-1);
        cnt += count-1;
	data += count-1;

	while(!(R_PAS(ptr,pas_stat) & R_PAS_STAT_DREQ)) ;
	while((R_PAS(ptr,r_cscr) & R_CSCR_REQ));
	*data = R_PAS(ptr,r_idr);
	cnt++;
        M_PAS(ptr,rw_mr,&,~RW_MR_DMA_MODE);
	return cnt;
    }
#endif /* NO_PAS16_PSEUDO_DMA */

    icmd = R_PAS(ptr,r_icr) & RW_ICR_MASK;

loop:
    if (SCSI_CUR_PHASE(R_PAS(ptr,r_cscr)) != phase)
	return cnt;

    WAIT_FOR_REQ(ptr);
    *data++ = R_PAS(ptr,r_csdr);

    icmd |= W_ICR_ASSERT_ACK;
    W_PAS(ptr,w_icr,icmd);
    icmd &= ~W_ICR_ASSERT_ACK;

    WAIT_FOR_NOT_REQ(ptr);
    W_PAS(ptr,w_icr,icmd);
    ++cnt;
    if (--count > 0)
	goto loop;

scsi_timeout_error:
    return cnt;
}


static int
cmd_xfer(P_PAS ptr, int maxlen, u_char *data, u_char *status, u_char *msg)
{
    int	xfer=0, phase;

    W_PAS(ptr,w_icr,0);

    while (1) {

	WAIT_FOR_REQ(ptr);

	phase = SCSI_CUR_PHASE(R_PAS(ptr,r_cscr));

	switch (phase) {
	    case SCSI_PHASE_CMD:
		W_PAS(ptr,w_tcr,SCSI_PHASE_CMD);
		xfer += sci_data_out(ptr, SCSI_PHASE_CMD, maxlen, data);
		return xfer;
	    case SCSI_PHASE_DATA_IN:
		printf("Data in phase in cmd_xfer?\n");
		return 0;
	    case SCSI_PHASE_DATA_OUT:
		printf("Data out phase in cmd_xfer?\n");
		return 0;
	    case SCSI_PHASE_STATUS:
		W_PAS(ptr,w_tcr,SCSI_PHASE_STATUS);
		printf("status in cmd_xfer.\n");
		sci_data_in(ptr, SCSI_PHASE_STATUS, 1, status);
		break;
	    case SCSI_PHASE_MESSAGE_IN:
		W_PAS(ptr,w_tcr,SCSI_PHASE_MESSAGE_IN);
		printf("msgin in cmd_xfer.\n");
		sci_data_in(ptr, SCSI_PHASE_MESSAGE_IN, 1, msg);
		break;
	    case SCSI_PHASE_MESSAGE_OUT:
		W_PAS(ptr,w_tcr,SCSI_PHASE_MESSAGE_OUT);
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

    W_PAS(ptr,w_icr,0);

    *status = 0;

    while (1) {

	WAIT_FOR_REQ(ptr);

	phase = SCSI_CUR_PHASE(R_PAS(ptr,r_cscr));
	switch (phase) {
	    case SCSI_PHASE_CMD:
		printf("Command phase in data_xfer().\n");
		return retlen;
	    case SCSI_PHASE_DATA_IN:
		W_PAS(ptr,w_tcr,SCSI_PHASE_DATA_IN);
		xfer = sci_data_in (ptr, SCSI_PHASE_DATA_IN, maxlen, data);
		retlen += xfer;
		maxlen -= xfer;
		break;
	    case SCSI_PHASE_DATA_OUT:
		W_PAS(ptr,w_tcr,SCSI_PHASE_DATA_OUT);
		xfer = sci_data_out (ptr, SCSI_PHASE_DATA_OUT, maxlen, data);
		retlen += xfer;
		maxlen -= xfer;
		break;
	    case SCSI_PHASE_STATUS:
		W_PAS(ptr,w_tcr,SCSI_PHASE_STATUS);
		sci_data_in(ptr, SCSI_PHASE_STATUS, 1, status);
		break;
	    case SCSI_PHASE_MESSAGE_IN:
		W_PAS(ptr,w_tcr,SCSI_PHASE_MESSAGE_IN);
		sci_data_in(ptr, SCSI_PHASE_MESSAGE_IN, 1, msg);
		if (*msg == 0) {
		    return retlen;
		} else {
		    printf( "message 0x%x in data_xfer.\n", *msg);
		}
		break;
	    case SCSI_PHASE_MESSAGE_OUT:
		W_PAS(ptr,w_tcr,SCSI_PHASE_MESSAGE_OUT);
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
    u_char stat, msg, c;

    *sent = 0;
    if ( ( r = select_target(ptr, target) ) != SCSI_RET_SUCCESS) {
	*ret = r;
	switch (r) {
	    case SCSI_RET_RETRY:
		return 0x08;
	    default:
		printf("select_target(target %d, lun %d) failed(%d).\n",
		    target, lun, r);
	    case SCSI_RET_DEVICE_DOWN:
		return -1;
	}
    }

    c = 0x80 | lun;

    if ((cmd_bytes_sent = cmd_xfer(ptr, cmdlen, cmd, &stat, &c)) != cmdlen) {
	*ret = SCSI_RET_COMMAND_FAIL;
	printf("Data underrun sending CCB (%d bytes of %d, sent).\n",
	    cmd_bytes_sent, cmdlen);
	return -1;
    }

    *sent=data_xfer(ptr, datalen, databuf, &stat, &msg);

    *ret = 0;
    return stat;
}

#endif /* NPAS */

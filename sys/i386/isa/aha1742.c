/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 *
 *	$Id: aha1742.c,v 1.10 1993/10/12 07:15:32 rgrimes Exp $
 */

#include <sys/types.h>
#include <ahb.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#ifdef  MACH    /* EITHER CMU OR OSF */
#include <i386/ipl.h>
#include <i386at/scsi.h>
#include <i386at/scsiconf.h>

#ifdef  OSF     /* OSF ONLY */
#include <sys/table.h>
#include <i386/handler.h>
#include <i386/dispatcher.h>
#include <i386/AT386/atbus.h>

#else   OSF     /* CMU ONLY */
#include <i386at/atbus.h>
#include <i386/pio.h>
#endif  OSF
#endif  MACH    /* end of MACH specific */

#ifdef  __386BSD__      /* 386BSD specific */
#define isa_dev isa_device
#define dev_unit id_unit
#define dev_addr id_iobase

#include <i386/include/pio.h>
#include <i386/isa/isa_device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif  __386BSD__

/**/

#ifdef  __386BSD__
#include "ddb.h"
#if     NDDB > 0
int     Debugger();
#else   NDDB
#define Debugger() panic("should call debugger here (adaptec.c)")
#endif  NDDB
#endif  __386BSD__

#ifdef  MACH
int     Debugger();
#endif  MACH

typedef unsigned long int physaddr;
extern int 	hz;

#ifdef        MACH
extern physaddr kvtophys();
#define PHYSTOKV(x)   phystokv(x)
#define KVTOPHYS(x)   kvtophys(x)
#endif MACH

#ifdef        __386BSD__
#define KVTOPHYS(x)   vtophys(x)
#endif        __386BSD__

extern int delaycount;  /* from clock setup code */
#define	NUM_CONCURRENT	16	/* number of concurrent ops per board */
#define	AHB_NSEG	33	/* number of dma segments supported	*/
#define FUDGE(X)	(X>>1)	/* our loops are slower than spinwait() */
/**/
/***********************************************************************\
* AHA1740 standard EISA Host ID regs  (Offset from slot base)		*
\***********************************************************************/
#define HID0		0xC80 /* 0,1: msb of ID2, 3-7: ID1	*/
#define HID1		0xC81 /* 0-4: ID3, 4-7: LSB ID2		*/
#define HID2		0xC82 /* product, 0=174[20] 1 = 1744	*/
#define HID3		0xC83 /* firmware revision		*/

#define CHAR1(B1,B2) (((B1>>2) & 0x1F) | '@')
#define CHAR2(B1,B2) (((B1<<3) & 0x18) | ((B2>>5) & 0x7)|'@')
#define CHAR3(B1,B2) ((B2 & 0x1F) | '@')

/* AHA1740 EISA board control registers (Offset from slot base) */
#define	EBCTRL		0xC84
#define  CDEN		0x01
/***********************************************************************\
* AHA1740 EISA board mode registers (Offset from slot base)		*
\***********************************************************************/
#define PORTADDR	0xCC0
#define	 PORTADDR_ENHANCED	0x80
#define BIOSADDR	0xCC1
#define	INTDEF		0xCC2
#define	SCSIDEF		0xCC3
#define	BUSDEF		0xCC4
#define	RESV0		0xCC5
#define	RESV1		0xCC6
#define	RESV2		0xCC7
/**** bit definitions for INTDEF ****/
#define	INT9	0x00
#define	INT10	0x01
#define	INT11	0x02
#define	INT12	0x03
#define	INT14	0x05
#define	INT15	0x06
#define INTHIGH 0x08    /* int high=ACTIVE (else edge) */
#define	INTEN	0x10
/**** bit definitions for SCSIDEF ****/
#define	HSCSIID	0x0F	/* our SCSI ID */
#define	RSTPWR	0x10	/* reset scsi bus on power up or reset */
/**** bit definitions for BUSDEF ****/
#define	B0uS	0x00	/* give up bus immediatly */
#define	B4uS	0x01	/* delay 4uSec. */
#define	B8uS	0x02
/***********************************************************************\
* AHA1740 ENHANCED mode mailbox control regs (Offset from slot base)	*
\***********************************************************************/
#define MBOXOUT0	0xCD0
#define MBOXOUT1	0xCD1
#define MBOXOUT2	0xCD2
#define MBOXOUT3	0xCD3

#define	ATTN		0xCD4
#define	G2CNTRL		0xCD5
#define	G2INTST		0xCD6
#define G2STAT		0xCD7

#define	MBOXIN0		0xCD8
#define	MBOXIN1		0xCD9
#define	MBOXIN2		0xCDA
#define	MBOXIN3		0xCDB

#define G2STAT2		0xCDC

/*******************************************************\
* Bit definitions for the 5 control/status registers	*
\*******************************************************/
#define	ATTN_TARGET		0x0F
#define	ATTN_OPCODE		0xF0
#define  OP_IMMED		0x10
#define	  AHB_TARG_RESET	0x80
#define  OP_START_ECB		0x40
#define  OP_ABORT_ECB		0x50

#define	G2CNTRL_SET_HOST_READY	0x20
#define	G2CNTRL_CLEAR_EISA_INT	0x40
#define	G2CNTRL_HARD_RESET	0x80

#define	G2INTST_TARGET		0x0F
#define	G2INTST_INT_STAT	0xF0
#define	 AHB_ECB_OK		0x10
#define	 AHB_ECB_RECOVERED	0x50
#define	 AHB_HW_ERR		0x70
#define	 AHB_IMMED_OK		0xA0
#define	 AHB_ECB_ERR		0xC0
#define	 AHB_ASN		0xD0	/* for target mode */
#define	 AHB_IMMED_ERR		0xE0

#define	G2STAT_BUSY		0x01
#define	G2STAT_INT_PEND		0x02
#define	G2STAT_MBOX_EMPTY	0x04

#define	G2STAT2_HOST_READY	0x01
/**/

struct	ahb_dma_seg
{
	physaddr	addr;
	long		len;
};

struct	ahb_ecb_status
{
	u_short	status;
#	 define	ST_DON	0x0001
#	 define	ST_DU	0x0002
#	 define	ST_QF	0x0008
#	 define	ST_SC	0x0010
#	 define	ST_DO	0x0020
#	 define	ST_CH	0x0040
#	 define	ST_INT	0x0080
#	 define	ST_ASA	0x0100
#	 define	ST_SNS	0x0200
#	 define	ST_INI	0x0800
#	 define	ST_ME	0x1000
#	 define	ST_ECA	0x4000
	u_char	ha_status;
#	 define	HS_OK			0x00
#	 define	HS_CMD_ABORTED_HOST	0x04
#	 define	HS_CMD_ABORTED_ADAPTER	0x05
#	 define	HS_TIMED_OUT		0x11
#	 define	HS_HARDWARE_ERR		0x20
#	 define	HS_SCSI_RESET_ADAPTER	0x22
#	 define	HS_SCSI_RESET_INCOMING	0x23
	u_char	targ_status;
#	 define	TS_OK			0x00
#	 define	TS_CHECK_CONDITION	0x02
#	 define	TS_BUSY			0x08
	u_long	resid_count;
	u_long	resid_addr;
	u_short	addit_status;
	u_char	sense_len;
	u_char	unused[9];
	u_char	cdb[6];
};

/**/

struct	ecb
{
	u_char	opcode;
#	 define	ECB_SCSI_OP	0x01
	u_char	:4;
	u_char	options:3;
	u_char	:1;
	short opt1;
#	 define	ECB_CNE	0x0001
#	 define	ECB_DI	0x0080
#	 define	ECB_SES	0x0400
#	 define	ECB_S_G	0x1000
#	 define	ECB_DSB	0x4000
#	 define	ECB_ARS	0x8000
	short opt2;
#	 define	ECB_LUN	0x0007
#	 define	ECB_TAG	0x0008
#	 define	ECB_TT	0x0030
#	 define	ECB_ND	0x0040
#	 define	ECB_DAT	0x0100
#	 define	ECB_DIR	0x0200
#	 define	ECB_ST	0x0400
#	 define	ECB_CHK	0x0800
#	 define	ECB_REC	0x4000
#	 define	ECB_NRB	0x8000
	u_short		unused1;
	physaddr	data;
	u_long		datalen;
	physaddr	status;
	physaddr	chain;
	short		unused2;
	short		unused3;
	physaddr	sense;
	u_char		senselen;
	u_char		cdblen;
	short		cksum;
	u_char		cdb[12];
	/*-----------------end of hardware supported fields----------------*/
	struct	ecb	*next;	/* in free list */
	struct	scsi_xfer *xs; /* the scsi_xfer for this cmd */
	int		flags;
#define ECB_FREE	0
#define ECB_ACTIVE	1
#define ECB_ABORTED	2
#define ECB_IMMED	4
#define ECB_IMMED_FAIL	8
	struct	ahb_dma_seg	ahb_dma[AHB_NSEG];
	struct	ahb_ecb_status	ecb_status;
	struct	scsi_sense_data	ecb_sense;
};

/**/

struct	ahb_data
{
	int	flags;
#define	AHB_INIT	0x01;
	int	baseport;
	struct	ecb ecbs[NUM_CONCURRENT];
	struct	ecb *free_ecb;
	int	our_id;			/* our scsi id */
	int	vect;
	struct	ecb *immed_ecb; 	/* an outstanding immediete command */
} ahb_data[NAHB];

int	ahbprobe();
int	ahb_attach();
int	ahbintr();
int	ahb_scsi_cmd();
int	ahb_timeout();
struct	ecb *cheat;
void	ahbminphys();
long int ahb_adapter_info();

#ifdef  MACH
struct  isa_driver      ahbdriver = { ahbprobe, 0, ahb_attach, "ahb", 0, 0, 0};
int (*ahbintrs[])() = {ahbintr, 0};
#endif  MACH

#ifdef  __386BSD__
struct  isa_driver      ahbdriver = { ahbprobe, ahb_attach, "ahb"};
#endif  __386BSD__

#define	MAX_SLOTS	8
static	ahb_slot = 0;	/* slot last board was found in */
static	ahb_unit = 0;
int	ahb_debug = 0;
#define AHB_SHOWECBS 0x01
#define AHB_SHOWINTS 0x02
#define AHB_SHOWCMDS 0x04
#define AHB_SHOWMISC 0x08
#define FAIL	1
#define SUCCESS 0
#define PAGESIZ 4096

struct	scsi_switch	ahb_switch = 
{
	ahb_scsi_cmd,
	ahbminphys,
	0,
	0,
	ahb_adapter_info,
	"ahb",
	0,0
};

/**/
/***********************************************************************\
* Function to send a command out through a mailbox			*
\***********************************************************************/
ahb_send_mbox(	int		unit
		,int		opcode
		,int		target
		,struct ecb	*ecb)
{
	int	port = ahb_data[unit].baseport;
	int	spincount = FUDGE(delaycount) * 1; /* 1ms should be enough */
	int	s = splbio();
	int	stport = port + G2STAT;

	while(      ((inb(stport) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
					!= (G2STAT_MBOX_EMPTY))
		&& (spincount--));
	if(spincount == -1)
	{
		printf("ahb%d: board not responding\n",unit);
		Debugger();
	}

	outl(port + MBOXOUT0,KVTOPHYS(ecb));	/* don't know this will work */
	outb(port + ATTN, opcode|target);

	splx(s);
}

/***********************************************************************\
* Function to poll for command completion when in poll mode		*
\***********************************************************************/
ahb_poll(int unit ,int wait) /* in msec  */
{
	int	port = ahb_data[unit].baseport;
	int	spincount = FUDGE(delaycount) * wait; /* in msec */
	int	stport = port + G2STAT;
int	start = spincount;

retry:
	while( (spincount--) && (!(inb(stport) &  G2STAT_INT_PEND)));
	if(spincount == -1)
	{
		printf("ahb%d: board not responding\n",unit);
		return(EIO);
	}
if ((int)cheat != PHYSTOKV(inl(port + MBOXIN0)))
{
	printf("discarding %x ",inl(port + MBOXIN0));
	outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
	spinwait(50);
	goto retry;
}/* don't know this will work */
	ahbintr(unit);
	return(0);
}
/***********************************************************************\
* Function to  send an immediate type command to the adapter		*
\***********************************************************************/
ahb_send_immed(	int		unit
		,int		target
		,u_long		cmd)
{
	int	port = ahb_data[unit].baseport;
	int	spincount = FUDGE(delaycount) * 1; /* 1ms should be enough */
	int	s = splbio();
	int	stport = port + G2STAT;

	while(      ((inb(stport) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
					!= (G2STAT_MBOX_EMPTY))
		&& (spincount--));
	if(spincount == -1)
	{
		printf("ahb%d: board not responding\n",unit);
		Debugger();
	}

	outl(port + MBOXOUT0,cmd);	/* don't know this will work */
	outb(port + G2CNTRL, G2CNTRL_SET_HOST_READY);
	outb(port + ATTN, OP_IMMED | target);
	splx(s);
}

/**/

/*******************************************************\
* Check  the slots looking for a board we recognise	*
* If we find one, note it's address (slot) and call	*
* the actual probe routine to check it out.		*
\*******************************************************/
ahbprobe(dev)
struct isa_dev *dev;
{
	int	port;
	u_char	byte1,byte2,byte3;
	ahb_slot++;
	while (ahb_slot<8)
	{
		port = 0x1000 * ahb_slot;
		byte1 = inb(port + HID0);
		byte2 = inb(port + HID1);
		byte3 = inb(port + HID2);
		if(byte1 == 0xff)
		{
			ahb_slot++;
			continue;
		}
		if ((CHAR1(byte1,byte2) == 'A')
		 && (CHAR2(byte1,byte2) == 'D')
		 && (CHAR3(byte1,byte2) == 'P')
		 && ((byte3 == 0 ) || (byte3 == 1)))
		{
			dev->dev_addr = port;
			return(ahbprobe1(dev));
		}
		ahb_slot++;
	}
	return(0);
}
/*******************************************************\
* Check if the device can be found at the port given    *
* and if so, set it up ready for further work           *
* as an argument, takes the isa_dev structure from      *
* autoconf.c                                            *
\*******************************************************/
ahbprobe1(dev)
struct isa_dev *dev;
{
	/***********************************************\
	* find unit and check we have that many defined	*
	\***********************************************/
	int     unit = ahb_unit;
#if defined(OSF)
	static ihandler_t ahb_handler[NAHB];
	static ihandler_id_t *ahb_handler_id[NAHB];
	register ihandler_t *chp = &ahb_handler[unit];;
#endif /* defined(OSF) */

	dev->dev_unit = unit;
	ahb_data[unit].baseport = dev->dev_addr;
	if(unit >= NAHB) 
	{
		printf("ahb: unit number (%d) too high\n",unit);
		return(0);
	}
	/***********************************************\
	* Try initialise a unit at this location	*
	* sets up dma and bus speed, loads ahb_data[unit].vect*
	\***********************************************/
	if (ahb_init(unit) != 0)
	{
		return(0);
	}

	/***********************************************\
	* If it's there, put in it's interrupt vectors	*
	\***********************************************/
#ifdef	MACH
	dev->dev_pic = ahb_data[unit].vect;
#if defined(OSF)				/* OSF */
	chp->ih_level = dev->dev_pic;
	chp->ih_handler = dev->dev_intr[0];
	chp->ih_resolver = i386_resolver;
	chp->ih_rdev = dev;
	chp->ih_stats.intr_type = INTR_DEVICE;
	chp->ih_stats.intr_cnt = 0;
	chp->ih_hparam[0].intparam = unit;
	if ((ahb_handler_id[unit] = handler_add(chp)) != NULL)
		handler_enable(ahb_handler_id[unit]);
	else
		panic("Unable to add ahb interrupt handler");
#else 						/* CMU */
	take_dev_irq(dev);
#endif /* !defined(OSF) */
	printf("port=%x spl=%d\n", dev->dev_addr, dev->dev_spl);
#endif	MACH
#ifdef  __386BSD__				/* 386BSD */
        dev->id_irq = (1 << ahb_data[unit].vect);
        dev->id_drq = -1; /* use EISA dma */
#endif  __386BSD__

	ahb_unit++;
	return(1);
}

/***********************************************\
* Attach all the sub-devices we can find	*
\***********************************************/
ahb_attach(dev)
struct	isa_dev	*dev;
{
	int	unit = dev->dev_unit;

	/***********************************************\
	* ask the adapter what subunits are present	*
	\***********************************************/
	scsi_attachdevs( unit, ahb_data[unit].our_id, &ahb_switch);
#if defined(OSF)
	ahb_attached[unit]=1;
#endif /* defined(OSF) */
	return;
}

/***********************************************\
* Return some information to the caller about   *
* the adapter and it's capabilities             *
\***********************************************/
long int ahb_adapter_info(unit)
int	unit;
{
	return(2);      /* 2 outstanding requests at a time per device */
}

/***********************************************\
* Catch an interrupt from the adaptor		*
\***********************************************/
ahbintr(unit)
{
	struct ecb	*ecb;
	unsigned char	stat;
	register	i;
	u_char		ahbstat;
	int		target;
	long int 	mboxval;

	int	port = ahb_data[unit].baseport;

#ifdef	AHBDEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("ahbintr ");
#endif	/*AHBDEBUG*/

#if defined(OSF)
	if (!ahb_attached[unit])
	{
		return(1);
	}
#endif /* defined(OSF) */
	while(inb(port + G2STAT) & G2STAT_INT_PEND)
	{
		/***********************************************\
		* First get all the information and then 	*
		* acknowlege the interrupt			*
		\***********************************************/
		ahbstat = inb(port + G2INTST);
		target = ahbstat & G2INTST_TARGET;
		stat = ahbstat & G2INTST_INT_STAT;
		mboxval = inl(port + MBOXIN0);/* don't know this will work */
		outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
#ifdef	AHBDEBUG
		if(scsi_debug & TRACEINTERRUPTS)
			printf("status = 0x%x ",stat);
#endif	/*AHBDEBUG*/
		/***********************************************\
		* Process the completed operation		*
		\***********************************************/
	
		if(stat == AHB_ECB_OK) /* common case is fast */
		{
			ecb = (struct ecb *)PHYSTOKV(mboxval); 
		}
		else
		{
			switch(stat)
			{
			case	AHB_IMMED_OK:
				ecb = ahb_data[unit].immed_ecb;
				ahb_data[unit].immed_ecb = 0;
				break;
			case	AHB_IMMED_ERR:
				ecb = ahb_data[unit].immed_ecb;
				ecb->flags |= ECB_IMMED_FAIL;
				ahb_data[unit].immed_ecb = 0;
				break;
			case	AHB_ASN:	/* for target mode */
				printf("ahb%d: Unexpected ASN interrupt(%x)\n",
					unit, mboxval);
				ecb = 0;
				break;
			case	AHB_HW_ERR:
				printf("ahb%d: Hardware error interrupt(%x)\n",
					unit, mboxval);
				ecb = 0;
				break;
			case	AHB_ECB_RECOVERED:
				ecb = (struct ecb *)PHYSTOKV(mboxval); 
				break;
			case	AHB_ECB_ERR:
				ecb = (struct ecb *)PHYSTOKV(mboxval); 
				break;
			default:
				printf(" Unknown return from ahb%d(%x)\n",unit,ahbstat);
				ecb=0;
			}
		}
		if(ecb)
		{
#ifdef	AHBDEBUG
			if(ahb_debug & AHB_SHOWCMDS )
			{
				ahb_show_scsi_cmd(ecb->xs);
			}
			if((ahb_debug & AHB_SHOWECBS) && ecb)
				printf("<int ecb(%x)>",ecb);
#endif	/*AHBDEBUG*/
			untimeout(ahb_timeout,ecb);
			ahb_done(unit,ecb,((stat == AHB_ECB_OK)?SUCCESS:FAIL));
		}
	}
	return(1);
}

/***********************************************\
* We have a ecb which has been processed by the	*
* adaptor, now we look to see how the operation	*
* went.						*
\***********************************************/
ahb_done(unit,ecb,state)
int	unit,state;
struct ecb *ecb;
{
	struct	ahb_ecb_status	*stat = &ecb->ecb_status;
	struct	scsi_sense_data *s1,*s2;
	struct	scsi_xfer *xs = ecb->xs;

#ifdef	AHBDEBUG
	if(scsi_debug & (PRINTROUTINES | TRACEINTERRUPTS))
		printf("ahb_done ");
#endif	/*AHBDEBUG*/
	/***********************************************\
	* Otherwise, put the results of the operation	*
	* into the xfer and call whoever started it	*
	\***********************************************/
	if(ecb->flags & ECB_IMMED)
	{
		if(ecb->flags & ECB_IMMED_FAIL)
		{
			xs->error = XS_DRIVER_STUFFUP;
		}
		goto done;
	}
	if ( (state == SUCCESS) || (xs->flags & SCSI_ERR_OK))
	{		/* All went correctly  OR errors expected */
		xs->resid = 0;
		xs->error = 0;
	}
	else
	{

		s1 = &(ecb->ecb_sense);
		s2 = &(xs->sense);

		if(stat->ha_status)
		{
			switch(stat->ha_status)
			{
			case	HS_SCSI_RESET_ADAPTER:
				break;
			case	HS_SCSI_RESET_INCOMING:
				break;
			case	HS_CMD_ABORTED_HOST:	/* No response */
			case	HS_CMD_ABORTED_ADAPTER:	/* No response */
				break;
			case	HS_TIMED_OUT:		/* No response */
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC)
				{
					printf("timeout reported back\n");
				}
#endif	/*AHBDEBUG*/
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC)
				{
					printf("unexpected ha_status: %x\n",
						stat->ha_status);
				}
#endif	/*AHBDEBUG*/
			}

		}
		else
		{
			switch(stat->targ_status)
			{
			case TS_CHECK_CONDITION:
				/* structure copy!!!!!*/
				*s2=*s1;
				xs->error = XS_SENSE;
				break;
			case TS_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC)
				{
					printf("unexpected targ_status: %x\n",
						stat->targ_status);
				}
#endif	/*AHBDEBUG*/
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
done:	xs->flags |= ITSDONE;
	ahb_free_ecb(unit,ecb, xs->flags);
	if(xs->when_done)
		(*(xs->when_done))(xs->done_arg,xs->done_arg2);
}

/***********************************************\
* A ecb (and hence a mbx-out is put onto the 	*
* free list.					*
\***********************************************/
ahb_free_ecb(unit,ecb, flags)
struct ecb *ecb;
{
	unsigned int opri;
	
#ifdef	AHBDEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("ecb%d(0x%x)> ",unit,flags);
#endif	/*AHBDEBUG*/
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();

	ecb->next = ahb_data[unit].free_ecb;
	ahb_data[unit].free_ecb = ecb;
	ecb->flags = ECB_FREE;
	/***********************************************\
	* If there were none, wake abybody waiting for	*
	* one to come free, starting with queued entries*
	\***********************************************/
	if (!ecb->next) {
		wakeup(&ahb_data[unit].free_ecb);
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
}

/***********************************************\
* Get a free ecb (and hence mbox-out entry)	*
\***********************************************/
struct ecb *
ahb_get_ecb(unit,flags)
{
	unsigned opri;
	struct ecb *rc;

#ifdef	AHBDEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("<ecb%d(0x%x) ",unit,flags);
#endif	/*AHBDEBUG*/
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();
	/***********************************************\
	* If we can and have to, sleep waiting for one	*
	* to come free					*
	\***********************************************/
	while ((!(rc = ahb_data[unit].free_ecb)) && (!(flags & SCSI_NOSLEEP)))
	{
		sleep(&ahb_data[unit].free_ecb, PRIBIO);
	}
	if (rc) 
	{
		ahb_data[unit].free_ecb = rc->next;
		rc->flags = ECB_ACTIVE;
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
	return(rc);
}
		


/***********************************************\
* Start the board, ready for normal operation	*
\***********************************************/
ahb_init(unit)
int	unit;
{
	int	port = ahb_data[unit].baseport;
	int	intdef;
	int	spincount = FUDGE(delaycount) * 1000; /* 1 sec enough? */
	int	i;
	int	stport = port + G2STAT;
#define	NO_NO 1
#ifdef NO_NO
	/***********************************************\
	* reset board, If it doesn't respond, assume 	*
	* that it's not there.. good for the probe	*
	\***********************************************/
	outb(port + EBCTRL,CDEN);	/* enable full card */
	outb(port + PORTADDR,PORTADDR_ENHANCED);

	outb(port + G2CNTRL,G2CNTRL_HARD_RESET);
	spinwait(1);
	outb(port + G2CNTRL,0);
	spinwait(10);
	while(      ((inb(stport) & G2STAT_BUSY ))
		&& (spincount--));
	if(spincount == -1)
	{
#ifdef	AHBDEBUG
		if (ahb_debug & AHB_SHOWMISC)
			printf("ahb_init: No answer from bt742a board\n");
#endif	/*AHBDEBUG*/
		return(ENXIO);
	}
	i = inb(port + MBOXIN0) & 0xff;
	if(i)
	{
		printf("self test failed, val = 0x%x\n",i);
		return(EIO);
	}
#endif
	while( inb(stport) &  G2STAT_INT_PEND)
	{
		printf(".");
		outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		spinwait(10);
	}
	outb(port + EBCTRL,CDEN);	/* enable full card */
	outb(port + PORTADDR,PORTADDR_ENHANCED);
	/***********************************************\
	* Assume we have a board at this stage		*
	* setup dma channel from jumpers and save int	*
	* level						*
	\***********************************************/
#ifdef	__386BSD__
	printf("ahb%d: reading board settings, ",unit);
#else	__386BSD__
	printf("ahb%d:",unit);
#endif	__386BSD__

	intdef = inb(port + INTDEF);
	switch(intdef & 0x07)
	{
	case	INT9:
		ahb_data[unit].vect = 9;
		break;
	case	INT10:
		ahb_data[unit].vect = 10;
		break;
	case	INT11:
		ahb_data[unit].vect = 11;
		break;
	case	INT12:
		ahb_data[unit].vect = 12;
		break;
	case	INT14:
		ahb_data[unit].vect = 14;
		break;
	case	INT15:
		ahb_data[unit].vect = 15;
		break;
	default:
		printf("illegal int setting\n");
		return(EIO);
	}
#ifdef	__386BSD__
	printf("int=%d\n",ahb_data[unit].vect);
#else	__386BSD__
	printf("int=%d ",ahb_data[unit].vect);
#endif	__386BSD__

	outb(port + INTDEF ,(intdef | INTEN)); /* make sure we can interrupt */
	/* who are we on the scsi bus */
	ahb_data[unit].our_id = (inb(port + SCSIDEF) & HSCSIID);

	/***********************************************\
	* link up all our ECBs into a free list		*
	\***********************************************/
	for (i=0; i < NUM_CONCURRENT; i++)
	{
		ahb_data[unit].ecbs[i].next = ahb_data[unit].free_ecb;
		ahb_data[unit].free_ecb = &ahb_data[unit].ecbs[i];
		ahb_data[unit].free_ecb->flags = ECB_FREE;
	}

	/***********************************************\
	* Note that we are going and return (to probe)	*
	\***********************************************/
	ahb_data[unit].flags |= AHB_INIT;
	return( 0 );
}


#ifndef	min
#define min(x,y) (x < y ? x : y)
#endif	min


void ahbminphys(bp)
struct	buf *bp;
{
#ifdef	MACH
#if	!defined(OSF)
	bp->b_flags |= B_NPAGES;		/* can support scat/gather */
#endif	/* defined(OSF) */
#endif	MACH
	if(bp->b_bcount > ((AHB_NSEG-1) * PAGESIZ))
	{
		bp->b_bcount = ((AHB_NSEG-1) * PAGESIZ);
	}
}
	
/***********************************************\
* start a scsi operation given the command and	*
* the data address. Also needs the unit, target	*
* and lu					*
\***********************************************/
int	ahb_scsi_cmd(xs)
struct scsi_xfer *xs;
{
	struct	scsi_sense_data *s1,*s2;
	struct ecb *ecb;
	struct ahb_dma_seg *sg;
	int	seg;	/* scatter gather seg being worked on */
	int i	= 0;
	int rc	=  0;
	int	thiskv;
	physaddr	thisphys,nextphys;
	int	unit =xs->adapter;
	int	bytes_this_seg,bytes_this_page,datalen,flags;
	struct	iovec	*iovp;
	int	s;
#ifdef	AHBDEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("ahb_scsi_cmd ");
#endif	/*AHBDEBUG*/
	/***********************************************\
	* get a ecb (mbox-out) to use. If the transfer	*
	* is from a buf (possibly from interrupt time)	*
	* then we can't allow it to sleep		*
	\***********************************************/
	flags = xs->flags;
	if(xs->bp) flags |= (SCSI_NOSLEEP); /* just to be sure */
	if(flags & ITSDONE)
	{
		printf("ahb%d: Already done?",unit);
		xs->flags &= ~ITSDONE;
	}
	if(!(flags & INUSE))
	{
		printf("ahb%d: Not in use?",unit);
		xs->flags |= INUSE;
	}
	if (!(ecb = ahb_get_ecb(unit,flags)))
	{
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

cheat = ecb;
#ifdef	AHBDEBUG
	if(ahb_debug & AHB_SHOWECBS)
				printf("<start ecb(%x)>",ecb);
	if(scsi_debug & SHOWCOMMANDS)
	{
		ahb_show_scsi_cmd(xs);
	}
#endif	/*AHBDEBUG*/
	ecb->xs = xs;
	/***********************************************\
	* If it's a reset, we need to do an 'immediate'	*
	* command, and store it's ccb for later		*
	* if there is already an immediate waiting, 	*
	* then WE must wait				*
	\***********************************************/
	if(flags & SCSI_RESET)
	{
		ecb->flags |= ECB_IMMED;
		if(ahb_data[unit].immed_ecb)
		{
			return(TRY_AGAIN_LATER);
		}
		ahb_data[unit].immed_ecb = ecb;
		if (!(flags & SCSI_NOMASK))
		{
			s = splbio();
			ahb_send_immed(unit,xs->targ,AHB_TARG_RESET);
			timeout(ahb_timeout,ecb,(xs->timeout * hz)/1000);
			splx(s);
			return(SUCCESSFULLY_QUEUED);
		}
		else
		{
			ahb_send_immed(unit,xs->targ,AHB_TARG_RESET);
			/***********************************************\
			* If we can't use interrupts, poll on completion*
			\***********************************************/
#ifdef	AHBDEBUG
			if(scsi_debug & TRACEINTERRUPTS)
				printf("wait ");
#endif	/*AHBDEBUG*/
			if( ahb_poll(unit,xs->timeout))
			{
				ahb_free_ecb(unit,ecb,flags);
				xs->error = XS_TIMEOUT;
				return(HAD_ERROR);
			}
			return(COMPLETE);
		}
	}	
	/***********************************************\
	* Put all the arguments for the xfer in the ecb	*
	\***********************************************/
	ecb->opcode = ECB_SCSI_OP;
	ecb->opt1 = ECB_SES|ECB_DSB|ECB_ARS;
	if(xs->datalen)
	{
		ecb->opt1 |= ECB_S_G;
	}
	ecb->opt2 		=	xs->lu | ECB_NRB;
	ecb->cdblen		=	xs->cmdlen;
	ecb->sense		=	KVTOPHYS(&(ecb->ecb_sense));
	ecb->senselen		=	sizeof(ecb->ecb_sense);
	ecb->status		=	KVTOPHYS(&(ecb->ecb_status));

	if(xs->datalen)
	{ /* should use S/G only if not zero length */
		ecb->data 	=	KVTOPHYS(ecb->ahb_dma);
		sg		=	ecb->ahb_dma ;
		seg 		=	0;
		if(flags & SCSI_DATA_UIO)
		{
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < AHB_NSEG))
			{
				sg->addr = (physaddr)iovp->iov_base;
				xs->datalen += sg->len = iovp->iov_len;	
#ifdef	AHBDEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x@0x%x)"
							,iovp->iov_len
							,iovp->iov_base);
#endif	/*AHBDEBUG*/
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		}
		else
		{
			/***********************************************\
			* Set up the scatter gather block		*
			\***********************************************/
		
#ifdef	AHBDEBUG
			if(scsi_debug & SHOWSCATGATH)
				printf("%d @0x%x:- ",xs->datalen,xs->data);
#endif	/*AHBDEBUG*/
			datalen		=	xs->datalen;
			thiskv		=	(int)xs->data;
			thisphys	=	KVTOPHYS(thiskv);
		
			while ((datalen) && (seg < AHB_NSEG))
			{
				bytes_this_seg	= 0;
	
				/* put in the base address */
				sg->addr = thisphys;
		
#ifdef	AHBDEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("0x%x",thisphys);
#endif	/*AHBDEBUG*/
	
				/* do it at least once */
				nextphys = thisphys;	
				while ((datalen) && (thisphys == nextphys))
				/*********************************************\
				* This page is contiguous (physically) with   *
				* the the last, just extend the length	      *
				\*********************************************/
				{
					/* how far to the end of the page */
					nextphys= (thisphys & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					bytes_this_page	= nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page	= min(bytes_this_page
								,datalen);
					bytes_this_seg	+= bytes_this_page;
					datalen		-= bytes_this_page;
		
					/* get more ready for the next page */
					thiskv	= (thiskv & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					if(datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/********************************************\
				* next page isn't contiguous, finish the seg *
				\********************************************/
#ifdef	AHBDEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x)",bytes_this_seg);
#endif	/*AHBDEBUG*/
				sg->len = bytes_this_seg;	
				sg++;
				seg++;
			}
		} /*end of iov/kv decision */
		ecb->datalen = seg * sizeof(struct ahb_dma_seg);
#ifdef	AHBDEBUG
		if(scsi_debug & SHOWSCATGATH)
			printf("\n");
#endif	/*AHBDEBUG*/
		if (datalen)
		{ /* there's still data, must have run out of segs! */
			printf("ahb_scsi_cmd%d: more than %d DMA segs\n",
				unit,AHB_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahb_free_ecb(unit,ecb,flags);
			return(HAD_ERROR);
		}

	}
	else
	{	/* No data xfer, use non S/G values */
		ecb->data = (physaddr)0;
		ecb->datalen = 0;
	}
	ecb->chain = (physaddr)0;
	/***********************************************\
	* Put the scsi command in the ecb and start it	*
	\***********************************************/
	bcopy(xs->cmd, ecb->cdb, xs->cmdlen);
	/***********************************************\
	* Usually return SUCCESSFULLY QUEUED		*
	\***********************************************/
	if (!(flags & SCSI_NOMASK))
	{
		s = splbio();
		ahb_send_mbox(unit,OP_START_ECB,xs->targ,ecb);
		timeout(ahb_timeout,ecb,(xs->timeout * hz)/1000);
		splx(s);
#ifdef	AHBDEBUG
		if(scsi_debug & TRACEINTERRUPTS)
			printf("cmd_sent ");
#endif	/*AHBDEBUG*/
		return(SUCCESSFULLY_QUEUED);
	}
	/***********************************************\
	* If we can't use interrupts, poll on completion*
	\***********************************************/
	ahb_send_mbox(unit,OP_START_ECB,xs->targ,ecb);
#ifdef	AHBDEBUG
	if(scsi_debug & TRACEINTERRUPTS)
		printf("cmd_wait ");
#endif	/*AHBDEBUG*/
	do
	{
		if(ahb_poll(unit,xs->timeout))
		{
			if (!(xs->flags & SCSI_SILENT)) printf("cmd fail\n");
			ahb_send_mbox(unit,OP_ABORT_ECB,xs->targ,ecb);
			if(ahb_poll(unit,2000))
			{
				printf("abort failed in wait\n");
				ahb_free_ecb(unit,ecb,flags);
			}
			xs->error = XS_DRIVER_STUFFUP;
			return(HAD_ERROR);
		}
	} while (!(xs->flags & ITSDONE));/* something (?) else finished */
	if(xs->error)
	{
		return(HAD_ERROR);
	}
	return(COMPLETE);
}


ahb_timeout(struct ecb *ecb)
{
	int	unit;
	int	s	= splbio();

	unit = ecb->xs->adapter;
	printf("ahb%d:%d device timed out\n",unit
			,ecb->xs->targ);
#ifdef	AHBDEBUG
	if(ahb_debug & AHB_SHOWECBS)
		ahb_print_active_ecb(unit);
#endif	/*AHBDEBUG*/

	/***************************************\
	* If it's immediate, don't try abort it *
	\***************************************/
	if(ecb->flags & ECB_IMMED)
	{
		ecb->xs->retries = 0; /* I MEAN IT ! */
		ecb->flags |= ECB_IMMED_FAIL;
                              ahb_done(unit,ecb,FAIL);
		splx(s);
		return;
	}
	/***************************************\
	* If it has been through before, then	*
	* a previous abort has failed, don't	*
	* try abort again			*
	\***************************************/
	if(ecb->flags == ECB_ABORTED) /* abort timed out */
	{
		printf("AGAIN");
		ecb->xs->retries = 0; /* I MEAN IT ! */
		ecb->ecb_status.ha_status = HS_CMD_ABORTED_HOST;
		ahb_done(unit,ecb,FAIL);
	}
	else	/* abort the operation that has timed out */
	{
		printf("\n");
		ahb_send_mbox(unit,OP_ABORT_ECB,ecb->xs->targ,ecb);
			/* 2 secs for the abort */
		timeout(ahb_timeout,ecb,2 * hz);
		ecb->flags = ECB_ABORTED;
	}
	splx(s);
}

#ifdef	AHBDEBUG
ahb_show_scsi_cmd(struct scsi_xfer *xs)
{
	u_char	*b = (u_char *)xs->cmd;
	int i = 0;
	if(!(xs->flags & SCSI_RESET))
	{
		printf("ahb%d:%d:%d-"
			,xs->adapter
			,xs->targ
			,xs->lu);
		while(i < xs->cmdlen )
		{
			if(i) printf(",");
			printf("%x",b[i++]);
		}
		printf("-\n");
	}
	else
	{
		printf("ahb%d:%d:%d-RESET-\n" 
			,xs->adapter 
			,xs->targ
			,xs->lu
		);
	}
}
ahb_print_ecb(ecb)
struct	ecb *ecb;
{
	printf("ecb:%x op:%x cmdlen:%d senlen:%d\n"
		,ecb
		,ecb->opcode
		,ecb->cdblen
		,ecb->senselen);
	printf("	datlen:%d hstat:%x tstat:%x flags:%x\n"
		,ecb->datalen
		,ecb->ecb_status.ha_status
		,ecb->ecb_status.targ_status
		,ecb->flags);
	ahb_show_scsi_cmd(ecb->xs);
}

ahb_print_active_ecb(int unit)
{
	struct	ecb *ecb = ahb_data[unit].ecbs;
	int	i = NUM_CONCURRENT;

	while(i--)
	{
		if(ecb->flags != ECB_FREE)
		{
			ahb_print_ecb(ecb);
		}
		ecb++;
	}
}
#endif	/*AHBDEBUG */

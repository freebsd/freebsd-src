/*
 * Ported for use with the UltraStor 14f by Gary Close (gclose@wvnvms.wvnet.edu)
 * Thanks to Julian Elischer for advice and help with this port.
 *
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
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 * slight mod to make work with 34F as well: Wed Jun  2 18:05:48 WST 1993
 */
 
#include <sys/types.h>
#include <uha.h>

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
#define Debugger() panic("should call debugger here")
#endif  NDDB
#endif  __386BSD__

#ifdef  MACH
int     Debugger();
#endif  MACH

typedef struct {unsigned char addr[4]; } physaddr;
typedef struct {unsigned char len[4]; } physlen;


#ifdef        MACH
extern physaddr kvtophys();
#define PHYSTOKV(x)   phystokv(x)
#define KVTOPHYS(x)   kvtophys(x)
#endif MACH

#ifdef        __386BSD__
#define PHYSTOKV(x)   (x | 0xFE000000)
#define KVTOPHYS(x)   vtophys(x)
#endif        __386BSD__

extern int delaycount;  /* from clock setup code */
#define NUM_CONCURRENT	16	/* number of concurrent ops per board */
#define UHA_NSEG        33      /* number of dma segments supported     */
#define FUDGE(X)        (X>>1)  /* our loops are slower than spinwait() */
/**/
/************************** board definitions *******************************/
/*
 * I/O Port Interface
*/
 #define UHA_LMASK              (0x000)  /* local doorbell mask reg */
 #define UHA_LINT               (0x001)  /* local doorbell int/stat reg */
 #define UHA_SMASK              (0x002)  /* system doorbell mask reg */
 #define UHA_SINT               (0x003)  /* system doorbell int/stat reg */
 #define UHA_ID0                (0x004)  /* product id reg 0 */
 #define UHA_ID1                (0x005)  /* product id reg 1 */
 #define UHA_CONF1              (0x006)  /* config reg 1 */
 #define UHA_CONF2              (0x007)  /* config reg 2 */
 #define UHA_OGM0               (0x008)  /* outgoing mail ptr 0 least sig */
 #define UHA_OGM1               (0x009)  /* outgoing mail ptr 1 least mid */
 #define UHA_OGM2               (0x00a)  /* outgoing mail ptr 2 most mid  */
 #define UHA_OGM3               (0x00b)  /* outgoing mail ptr 3 most sig  */
 #define UHA_ICM0               (0x00c)  /* incoming mail ptr 0 */
 #define UHA_ICM1               (0x00d)  /* incoming mail ptr 1 */
 #define UHA_ICM2               (0x00e)  /* incoming mail ptr 2 */
 #define UHA_ICM3               (0x00f)  /* incoming mail ptr 3 */

 /*
* UHA_LMASK bits (read only) 
*/

#define UHA_LDIE                0x80    /* local doorbell int enabled */
#define UHA_SRSTE               0x40    /* soft reset enabled */
#define UHA_ABORTEN             0x10    /* abort MSCP enabled */
#define UHA_OGMINTEN            0x01    /* outgoing mail interrupt enabled */

/*
* UHA_LINT bits (read)
*/

#define UHA_LDIP                0x80    /* local doorbell int pending */

/*
* UHA_LINT bits (write)
*/

#define UHA_ADRST               0x40    /* adapter soft reset */
#define UHA_SBRST               0x20    /* scsi bus reset */
#define UHA_ASRST               0x60    /* adapter and scsi reset */
#define UHA_ABORT               0x10    /* abort MSCP */
#define UHA_OGMINT              0x01    /* tell adapter to get mail */

/*
* UHA_SMASK bits (read)
*/

#define UHA_SINTEN              0x80    /* system doorbell interupt Enabled */
#define UHA_ABORT_COMPLETE_EN   0x10    /* abort MSCP command complete int Enabled */
#define UHA_ICM_ENABLED         0x01    /* ICM interrupt enabled

/*
* UHA_SMASK bits (write)
*/

#define UHA_ENSINT              0x80    /* enable system doorbell interrupt */
#define UHA_EN_ABORT_COMPLETE   0x10    /* enable abort MSCP complete int */
#define UHA_ENICM               0x01    /* enable ICM interrupt */

/*
* UHA_SINT bits (read)
*/

#define UHA_SINTP               0x80    /* system doorbell int pending */
#define UHA_ABORT_SUCC          0x10    /* abort MSCP successful */
#define UHA_ABORT_FAIL          0x18    /* abort MSCP failed */

/*
* UHA_SINT bits (write)
*/

#define UHA_ABORT_ACK           0x18    /* acknowledge status and clear */
#define UHA_ICM_ACK             0x01    /* acknowledge ICM and clear */

/* 
* UHA_CONF1 bits (read only)
*/

#define UHA_DMA_CH5             0x00    /* DMA channel 5 */
#define UHA_DMA_CH6             0x40    /* 6 */
#define UHA_DMA_CH7             0x80    /* 7 */
#define UHA_IRQ15               0x00    /* IRQ 15 */
#define UHA_IRQ14               0x10    /* 14 */
#define UHA_IRQ11               0x20    /* 11 */
#define UHA_IRQ10               0x30    /* 10 */

/***********************************
* ha_status error codes
\***********************************/

#define UHA_NO_ERR		0x00		/* No error supposedly */
#define UHA_SBUS_ABORT_ERR	0x84		/* scsi bus abort error */
#define UHA_SBUS_TIMEOUT	0x91		/* scsi bus selection timeout */
#define UHA_SBUS_OVER_UNDER	0x92		/* scsi bus over/underrun */
#define UHA_BAD_SCSI_CMD	0x96		/* illegal scsi command */
#define UHA_AUTO_SENSE_ERR	0x9b		/* auto request sense err */
#define UHA_SBUS_RES_ERR	0xa3		/* scsi bus reset error */
#define UHA_BAD_SG_LIST		0xff		/* invalid scatter gath list */

/**/

struct  uha_dma_seg
{
	physaddr			addr;
	physlen				len;
};
/**/

struct  mscp
{
	unsigned char		opcode:3;
	#define U14_HAC		0x01 		/*host adapter command*/
	#define U14_TSP		0x02		/*target scsi pass through command*/
	#define U14_SDR		0x04		/*scsi device reset*/
	unsigned char		xdir:2;		/*xfer direction*/
	#define U14_SDET	0x00		/*determined by scsi command*/
	#define U14_SDIN	0x01		/*scsi data in*/
	#define U14_SDOUT	0x02		/*scsi data out*/
	#define U14_NODATA	0x03		/*no data xfer*/
	unsigned char		dcn:1;		/*disable disconnect for this command*/
	unsigned char		ca:1;		/*Cache control*/
	unsigned char		sgth:1;		/*scatter gather flag*/
	unsigned char		target:3;
	unsigned char		chan:2;		/*scsi channel (always 0 for 14f)*/
	unsigned char		lun:3;
	physaddr		data;
	physlen			datalen;
	physaddr		link;
	unsigned char		link_id;
	unsigned char		sg_num;		/*number of scat gath segs */
						/*in s-g list if sg flag is*/
						/*set. starts at 1, 8bytes per*/
	unsigned char		senselen;
	unsigned char		cdblen;
	unsigned char		cdb[12];
	unsigned char		ha_status;
	unsigned char		targ_status;
	physaddr		sense;		/* if 0 no auto sense */
	/*-----------------end of hardware supported fields----------------*/
	struct  mscp     *next;  /* in free list */
	struct  scsi_xfer *xs; /* the scsi_xfer for this cmd */
	long    int     delta;  /* difference from previous*/
	struct  mscp     *later,*sooner;
	int             flags;
#define MSCP_FREE        0
#define MSCP_ACTIVE      1
#define MSCP_ABORTED     2
	struct  uha_dma_seg     uha_dma[UHA_NSEG];
	struct  scsi_sense_data mscp_sense;
};

struct  mscp     *uha_soonest    = (struct mscp *)0;
struct  mscp     *uha_latest     = (struct mscp *)0;
long    int     uha_furtherest  = 0; /* longest time in the timeout queue */
/**/

struct  uha_data
{
	int     flags;
#define UHA_INIT        0x01;
	int     baseport;
	struct  mscp mscps[NUM_CONCURRENT];
	struct  mscp *free_mscp;
	int     our_id;                 /* our scsi id */
	int     vect;
	int	dma;
} uha_data[NUHA];

int     uhaprobe();
int     uha_attach();
int     uhaintr();
int     uha_scsi_cmd();
int     uha_timeout();
int	uha_abort();
struct  mscp *cheat;
void    uhaminphys();
long int uha_adapter_info();

unsigned long int scratch;

#ifdef  MACH
struct  isa_driver      uhadriver = { uhaprobe, 0, uha_attach, "uha", 0, 0, 0};
int (*uhaintrs[])() = {uhaintr, 0};
#endif  MACH

#ifdef  __386BSD__
struct  isa_driver      uhadriver = { uhaprobe, uha_attach, "uha"};
#endif  __386BSD__

static	uha_unit = 0;
int     uha_debug = 0;
#define UHA_SHOWMSCPS 0x01
#define UHA_SHOWINTS 0x02
#define UHA_SHOWCMDS 0x04
#define UHA_SHOWMISC 0x08
#define FAIL    1
#define SUCCESS 0
#define PAGESIZ 4096

struct  scsi_switch     uha_switch = 
{
	uha_scsi_cmd,
	uhaminphys,
	0,
	0,
	uha_adapter_info,
	0,0,0
};

/**/
/***********************************************************************\
* Function to send a command out through a mailbox                      *
\***********************************************************************/
uha_send_mbox(  int             unit
		,struct mscp     *mscp)
{
	int     port = uha_data[unit].baseport;
	int     spincount = FUDGE(delaycount) * 1000; /* 1s should be enough */
	int     s = splbio();
		
	while(      ((inb(port + UHA_LINT) & (UHA_LDIP))
					!= (0))
		&& (spincount--));
	if(spincount == -1)
	{
		printf("uha%d: board not responding\n",unit);
		Debugger();
	}

	outl(port + UHA_OGM0,KVTOPHYS(mscp)); 
	outb(port + UHA_LINT, (UHA_OGMINT));
	splx(s);
}

/***********************************************************************\
* Function to send abort to 14f                                         *
\***********************************************************************/

uha_abort(	int		unit
		,struct mscp	*mscp)
{
	int	port = uha_data[unit].baseport;
	int	spincount = FUDGE(delaycount) * 1;
	int	abortcount = FUDGE(delaycount) * 2000;
	int	s = splbio();
	
	while(((inb(port + UHA_LINT) & (UHA_LDIP))
				!= (0))
		&& (spincount--));
	if(spincount == -1);
	{
		printf("uha%d: board not responding\n",unit);
		Debugger();
	}

	outl(port + UHA_OGM0,KVTOPHYS(mscp));
	outb(port + UHA_LINT,UHA_ABORT);

	while((abortcount--) && (!(inb(port + UHA_SINT) & UHA_ABORT_FAIL)));
	if(abortcount == -1)
	{
		printf("uha%d: board not responding\n",unit);
		Debugger();
	}
	if((inb(port + UHA_SINT) & 0x10) != 0)
	{
		outb(port + UHA_SINT,UHA_ABORT_ACK);
		return(1);
	}
	else
	{
		outb(port + UHA_SINT,UHA_ABORT_ACK);
		return(0);
	}
}

/***********************************************************************\
* Function to poll for command completion when in poll mode             *
\***********************************************************************/
uha_poll(int unit ,int wait) /* in msec  */
{
	int     port = uha_data[unit].baseport;
	int     spincount = FUDGE(delaycount) * wait; /* in msec */
	int     stport = port + UHA_SINT;
	int     start = spincount;

retry:
	while( (spincount--) && (!(inb(stport) &  UHA_SINTP)));
	if(spincount == -1)
	{
		printf("uha%d: board not responding\n",unit);
		return(EIO);
	}
if ((int)cheat != PHYSTOKV(inl(port + UHA_ICM0)))
{
	printf("discarding %x ",inl(port + UHA_ICM0));
	outb(port + UHA_SINT, UHA_ICM_ACK);
	spinwait(50);
	goto retry;
}/* don't know this will work */
	uhaintr(unit);
	return(0);
}

/*******************************************************\
* Check if the device can be found at the port given    *
* and if so, set it up ready for further work           *
* as an argument, takes the isa_dev structure from      *
* autoconf.c                                            *
\*******************************************************/
uhaprobe(dev)
struct isa_dev *dev;
{
	int	unit = uha_unit;
	dev->dev_unit = unit;
	uha_data[unit].baseport = dev->dev_addr;
	if(unit >= NUHA)
	{
		printf("uha: unit number (%d) too high\n",unit);
		return(0);
	}
	
	/*try and initialize unit at this location*/
	if (uha_init(unit) != 0)
	{
		return(0);
	}

	/* if its there put in it's interrupt and DRQ vectors */

	dev->id_irq = (1 << uha_data[unit].vect);
	dev->id_drq = uha_data[unit].dma;

	
	uha_unit ++;
return(1);
}

/***********************************************\
* Attach all the sub-devices we can find        *
\***********************************************/
uha_attach(dev)
struct  isa_dev *dev;
{
	int     unit = dev->dev_unit;


#ifdef  __386BSD__
	printf(" probing for scsi devices**\n");
#endif  __386BSD__

	/***********************************************\
	* ask the adapter what subunits are present     *
	\***********************************************/
	scsi_attachdevs( unit, uha_data[unit].our_id, &uha_switch);

#if defined(OSF)
	uha_attached[unit]=1;
#endif /* defined(OSF) */
	if(!unit)  /* only one for all boards */
	{
		uha_timeout(0);
	}


#ifdef  __386BSD__
	printf("uha%d",unit);
#endif  __386BSD__
	return;
}

/***********************************************\
* Return some information to the caller about   *
* the adapter and it's capabilities             *
\***********************************************/
long int uha_adapter_info(unit)
int     unit;
{
	return(2);      /* 2 outstanding requests at a time per device */
}

/***********************************************\
* Catch an interrupt from the adaptor           *
\***********************************************/
uhaintr(unit)
{
	struct mscp      *mscp;
	u_char          uhastat;
	unsigned long int        mboxval;

	int     port = uha_data[unit].baseport;


	if(scsi_debug & PRINTROUTINES)
		printf("uhaintr ");

#if defined(OSF)
	if (!uha_attached[unit])
	{
		return(1);
	}
#endif /* defined(OSF) */
	while(inb(port + UHA_SINT) & UHA_SINTP)
	{
		/***********************************************\
		* First get all the information and then        *
		* acknowlege the interrupt                      *
		\***********************************************/
		uhastat = inb(port + UHA_SINT);
		mboxval = inl(port + UHA_ICM0);
		outb(port + UHA_SINT,UHA_ICM_ACK);

		if(scsi_debug & TRACEINTERRUPTS)
			printf("status = 0x%x ",uhastat);
		/***********************************************\
		* Process the completed operation               *
		\***********************************************/
	
			mscp = (struct mscp *)(PHYSTOKV(mboxval)); 

			if(uha_debug & UHA_SHOWCMDS )
			{
				uha_show_scsi_cmd(mscp->xs);
			}
			if((uha_debug & UHA_SHOWMSCPS) && mscp)
				printf("<int mscp(%x)>",mscp);
			uha_remove_timeout(mscp);

			uha_done(unit,mscp);
	}
	return(1);
}

/***********************************************\
* We have a mscp which has been processed by the *
* adaptor, now we look to see how the operation *
* went.                                         *
\***********************************************/

uha_done(unit,mscp)
int     unit;
struct mscp *mscp;
{
	struct  scsi_sense_data *s1,*s2;
	struct  scsi_xfer *xs = mscp->xs;

	if(scsi_debug & (PRINTROUTINES | TRACEINTERRUPTS))
		printf("uha_done ");
	/***********************************************\
	* Otherwise, put the results of the operation   *
	* into the xfer and call whoever started it     *
	\***********************************************/
	if ( (mscp->ha_status == UHA_NO_ERR) || (xs->flags & SCSI_ERR_OK))
	{               /* All went correctly  OR errors expected */
		xs->resid = 0;
		xs->error = 0;
	}
	else
	{

		s1 = &(mscp->mscp_sense);
		s2 = &(xs->sense);

		if(mscp->ha_status != UHA_NO_ERR)
		{
			switch(mscp->ha_status)
			{
			case    UHA_SBUS_TIMEOUT:           /* No response */
				if (uha_debug & UHA_SHOWMISC)
				{
					printf("timeout reported back\n");
				}
				xs->error = XS_TIMEOUT;
				break;
			case	UHA_SBUS_OVER_UNDER:
				if (uha_debug & UHA_SHOWMISC)
				{
					printf("scsi bus xfer over/underrun\n");
				}
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case	UHA_BAD_SG_LIST:
				if (uha_debug & UHA_SHOWMISC)
				{
					printf("bad sg list reported back\n");
				}
				xs->error = XS_DRIVER_STUFFUP;
				break;
			default:        /* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				if (uha_debug & UHA_SHOWMISC)
				{
					printf("unexpected ha_status: %x\n",
						mscp->ha_status);
				}
			}

		}
		else
		{

			if (mscp->targ_status != 0)
/**************************************************************************\
* I have no information for any possible value of target status field     *
* other than 0 means no error!! So I guess any error is unexpected in that *
* event!! 								   *
\**************************************************************************/

			{	
				if (uha_debug & UHA_SHOWMISC)
				{
					printf("unexpected targ_status: %x\n",
						mscp->targ_status);
				}
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
done:   xs->flags |= ITSDONE;
	uha_free_mscp(unit,mscp, xs->flags);
	if(xs->when_done)
		(*(xs->when_done))(xs->done_arg,xs->done_arg2);
}

/***********************************************\
* A mscp (and hence a mbx-out is put onto the    *
* free list.                                    *
\***********************************************/
uha_free_mscp(unit,mscp, flags)
struct mscp *mscp;
{
	unsigned int opri;
	
	if(scsi_debug & PRINTROUTINES)
		printf("mscp%d(0x%x)> ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
		opri = splbio();

	mscp->next = uha_data[unit].free_mscp;
	uha_data[unit].free_mscp = mscp;
	mscp->flags = MSCP_FREE;
	/***********************************************\
	* If there were none, wake abybody waiting for  *
	* one to come free, starting with queued entries*
	\***********************************************/
	if (!mscp->next) {
		wakeup(&uha_data[unit].free_mscp);
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
}

/***********************************************\
* Get a free mscp (and hence mbox-out entry)     *
\***********************************************/
struct mscp *
uha_get_mscp(unit,flags)
{
	unsigned opri;
	struct mscp *rc;

	if(scsi_debug & PRINTROUTINES)
		printf("<mscp%d(0x%x) ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
		opri = splbio();
	/***********************************************\
	* If we can and have to, sleep waiting for one  *
	* to come free                                  *
	\***********************************************/
	while ((!(rc = uha_data[unit].free_mscp)) && (!(flags & SCSI_NOSLEEP)))
	{
		sleep(&uha_data[unit].free_mscp, PRIBIO);
	}
	if (rc) 
	{
		uha_data[unit].free_mscp = rc->next;
		rc->flags = MSCP_ACTIVE;
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
	return(rc);
}
		


/***********************************************\
* Start the board, ready for normal operation   *
\***********************************************/

uha_init(unit)
int	unit;
{	
	unsigned char ad[4];
	volatile unsigned char model;
	volatile unsigned char submodel;
	unsigned char config_reg1;
	unsigned char config_reg2;
	unsigned char dma_ch;
	unsigned char irq_ch;
	unsigned char uha_id;
	int	port = uha_data[unit].baseport;
	int	i;
	int	resetcount = FUDGE(delaycount) * 4000;

	model = inb(port + UHA_ID0);
	submodel = inb(port + UHA_ID1);
		 if ((model != 0x56) & (submodel != 0x40))
		      { printf("ultrastor 14f not responding\n");
			return(ENXIO); }

	printf("uha%d reading board settings, ",unit);

	config_reg1 = inb(port + UHA_CONF1);
	config_reg2 = inb(port + UHA_CONF2);
	dma_ch = (config_reg1 & 0xc0);
	irq_ch = (config_reg1 & 0x30);
	uha_id = (config_reg2 & 0x07);

	switch(dma_ch)
	{
	case	UHA_DMA_CH5:
		uha_data[unit].dma = 5;
		printf("dma=5 ");
		break;
	case	UHA_DMA_CH6:
		uha_data[unit].dma = 6;
		printf("dma=6 ");
		break;
	case	UHA_DMA_CH7:
		uha_data[unit].dma = 7;
		printf("dma=7 ");
		break;
	default:
		printf("illegal dma jumper setting\n");
		return(EIO);
	}
	switch(irq_ch)
	{
	case	UHA_IRQ10:
		uha_data[unit].vect = 10;
		printf("int=10 ");
		break;
	case	UHA_IRQ11:
		uha_data[unit].vect = 11;
		printf("int=11 ");
		break;
	case	UHA_IRQ14:
		uha_data[unit].vect = 14;
		printf("int=14 ");
		break;
	case	UHA_IRQ15:
		uha_data[unit].vect = 15;
		printf("int=15 ");
		break;
	default:
		printf("illegal int jumper setting\n");
		return(EIO);
	}
	/* who are we on the scsi bus */
	printf("id=%x\n",uha_id);
	uha_data[unit].our_id = uha_id;

	
	/***********************************************\
	* link up all our MSCPs into a free list         *
	\***********************************************/
	for (i=0; i < NUM_CONCURRENT; i++)
	{
		uha_data[unit].mscps[i].next = uha_data[unit].free_mscp;
		uha_data[unit].free_mscp = &uha_data[unit].mscps[i];
		uha_data[unit].free_mscp->flags = MSCP_FREE;
	}

	/***********************************************\
	* Note that we are going and return (to probe)  *
	\***********************************************/
	outb(port + UHA_LINT, UHA_ASRST);	
	while( (resetcount--) && (!(inb(port + UHA_LINT))));
	if(resetcount == -1)
	{
		printf("uha%d: board timed out during reset\n",unit);
		return(ENXIO);
	}

	outb(port + UHA_SMASK, 0x81); /* make sure interrupts are enabled */
	uha_data[unit].flags |= UHA_INIT;
	return(0);
}



#ifndef min
#define min(x,y) (x < y ? x : y)
#endif  min


void uhaminphys(bp)
struct  buf *bp;
{
#ifdef  MACH
#if     !defined(OSF)
	bp->b_flags |= B_NPAGES;                /* can support scat/gather */
#endif  /* defined(OSF) */
#endif  MACH
	if(bp->b_bcount > ((UHA_NSEG-1) * PAGESIZ))
	{
		bp->b_bcount = ((UHA_NSEG-1) * PAGESIZ);
	}
}

/***********************************************\
* start a scsi operation given the command and  *
* the data address. Also needs the unit, target *
* and lu                                        *
\***********************************************/
int     uha_scsi_cmd(xs)
struct scsi_xfer *xs;
{
	struct  scsi_sense_data *s1,*s2;
	struct mscp *mscp;
	struct uha_dma_seg *sg;
	int     seg;    /* scatter gather seg being worked on */
	int i   = 0;
	int rc  =  0;
	int     thiskv;
	unsigned long int        thisphys,nextphys;
	int     unit =xs->adapter;
	int     bytes_this_seg,bytes_this_page,datalen,flags;
	struct  iovec   *iovp;
	int     s;
	unsigned int stat;
	int	port = uha_data[unit].baseport;
	unsigned long int templen;


	if(scsi_debug & PRINTROUTINES)
		printf("uha_scsi_cmd ");
	/***********************************************\
	* get a mscp (mbox-out) to use. If the transfer  *
	* is from a buf (possibly from interrupt time)  *
	* then we can't allow it to sleep               *
	\***********************************************/
	flags = xs->flags;
	if(xs->bp) flags |= (SCSI_NOSLEEP); /* just to be sure */
	if(flags & ITSDONE)
	{
		printf("Already done?");
		xs->flags &= ~ITSDONE;
	}
	if(!(flags & INUSE))
	{
		printf("Not in use?");
		xs->flags |= INUSE;
	}
	if (!(mscp = uha_get_mscp(unit,flags)))
	{
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

cheat = mscp;
	if(uha_debug & UHA_SHOWMSCPS)
				printf("<start mscp(%x)>",mscp);
	if(scsi_debug & SHOWCOMMANDS)
	{
		uha_show_scsi_cmd(xs);
	}
	mscp->xs = xs;
	/***********************************************\
	* Put all the arguments for the xfer in the mscp *
	\***********************************************/

	if (flags & SCSI_RESET)
	{
		mscp->opcode = 0x04;
		mscp->ca = 0x01;
	}
	else
	{
		mscp->opcode = 0x02;
		mscp->ca = 0x01;
	}		

	if (flags & SCSI_DATA_IN)
	{
		mscp->xdir = 0x01;
	}
	if (flags & SCSI_DATA_OUT)
	{
		mscp->xdir = 0x02;
	}

	if (xs->lu != 0)
	{
		xs->error = XS_DRIVER_STUFFUP;
		uha_free_mscp(unit,mscp,flags);
		return(HAD_ERROR);
	}
	
	mscp->dcn	= 0x00;
	mscp->chan	= 0x00;
	mscp->target	= xs->targ;
	mscp->lun	= xs->lu;
	mscp->link.addr[0]	= 0x00;
	mscp->link.addr[1]	= 0x00;
	mscp->link.addr[2]	= 0x00;
	mscp->link.addr[3]	= 0x00;
	mscp->link_id	= 0x00;
	mscp->cdblen             =       xs->cmdlen;
	scratch			=	KVTOPHYS(&(mscp->mscp_sense));
	mscp->sense.addr[0]     =	(scratch & 0xff);
	mscp->sense.addr[1]	=	((scratch >> 8) & 0xff);
	mscp->sense.addr[2]	=	((scratch >> 16) & 0xff);
	mscp->sense.addr[3]	=	((scratch >> 24) & 0xff);
	mscp->senselen          =       sizeof(mscp->mscp_sense);
	mscp->ha_status		=	0x00;
	mscp->targ_status	=	0x00;

	if(xs->datalen)
	{ /* should use S/G only if not zero length */
		scratch			=	KVTOPHYS(mscp->uha_dma);
		mscp->data.addr[0]      =	(scratch & 0xff);
		mscp->data.addr[1]	=	((scratch >> 8) & 0xff);
		mscp->data.addr[2]	=	((scratch >> 16) & 0xff);
		mscp->data.addr[3]	=	((scratch >> 24) & 0xff);
		sg              =       mscp->uha_dma ;
		seg             =       0;
		mscp->sgth	=	0x01;

		if(flags & SCSI_DATA_UIO)
		{
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < UHA_NSEG))
			{
				scratch  = (unsigned long)iovp->iov_base;
				sg->addr.addr[0] = (scratch & 0xff); 
				sg->addr.addr[1] = ((scratch >> 8) & 0xff);
				sg->addr.addr[2] = ((scratch >> 16) & 0xff);
				sg->addr.addr[3] = ((scratch >> 24) & 0xff);
				xs->datalen += *(unsigned long *)sg->len.len = iovp->iov_len; 
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x@0x%x)"
							,iovp->iov_len
							,iovp->iov_base);
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		}
		else
		{
			/***********************************************\
			* Set up the scatter gather block               *
			\***********************************************/
		
			if(scsi_debug & SHOWSCATGATH)
				printf("%d @0x%x:- ",xs->datalen,xs->data);
			datalen         =       xs->datalen;
			thiskv          =       (int)xs->data;
			thisphys        =       KVTOPHYS(thiskv);
			templen		=	0;
		
			while ((datalen) && (seg < UHA_NSEG))
			{
				bytes_this_seg  = 0;
	
				/* put in the base address */
				sg->addr.addr[0] = (thisphys & 0xff);
				sg->addr.addr[1] = ((thisphys >> 8) & 0xff);
				sg->addr.addr[2] = ((thisphys >> 16) & 0xff);
				sg->addr.addr[3] = ((thisphys >> 24) & 0xff);
		
				if(scsi_debug & SHOWSCATGATH)
					printf("0x%x",thisphys);
	
				/* do it at least once */
				nextphys = thisphys;    
				while ((datalen) && (thisphys == nextphys))
				/*********************************************\
				* This page is contiguous (physically) with   *
				* the the last, just extend the length        *
				\*********************************************/
				{
					/* how far to the end of the page */
					nextphys = (thisphys & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page
								,datalen);
					bytes_this_seg  += bytes_this_page;
					datalen         -= bytes_this_page;
		
					/* get more ready for the next page */
					thiskv  = (thiskv & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					if(datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/********************************************\
				* next page isn't contiguous, finish the seg *
				\********************************************/
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x)",bytes_this_seg);
				sg->len.len[0] = (bytes_this_seg & 0xff);
				sg->len.len[1] = ((bytes_this_seg >> 8) & 0xff);
				sg->len.len[2] = ((bytes_this_seg >> 16) & 0xff);
				sg->len.len[3] = ((bytes_this_seg >> 24) & 0xff);
				templen += bytes_this_seg;
				sg++;
				seg++;
			}
		} /*end of iov/kv decision */
		mscp->datalen.len[0] = (templen & 0xff);
		mscp->datalen.len[1] = ((templen >> 8) & 0xff);
		mscp->datalen.len[2] = ((templen >> 16) & 0xff);
		mscp->datalen.len[3] = ((templen >> 24) & 0xff);
		mscp->sg_num = seg;

		if(scsi_debug & SHOWSCATGATH)
			printf("\n");
		if (datalen)
		{ /* there's still data, must have run out of segs! */
			printf("uha_scsi_cmd%d: more than %d DMA segs\n",
				unit,UHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			uha_free_mscp(unit,mscp,flags);
			return(HAD_ERROR);
		}

	}
	else
	{       /* No data xfer, use non S/G values */
		mscp->data.addr[0] = 0x00;
		mscp->data.addr[1] = 0x00;
		mscp->data.addr[2] = 0x00;
		mscp->data.addr[3] = 0x00;
		mscp->datalen.len[0] = 0x00;
		mscp->datalen.len[1] = 0x00;
		mscp->datalen.len[2] = 0x00;
		mscp->datalen.len[3] = 0x00;
		mscp->xdir = 0x03;
		mscp->sgth = 0x00;
		mscp->sg_num = 0x00;	
	}

	/***********************************************\
	* Put the scsi command in the mscp and start it  *
	\***********************************************/
	bcopy(xs->cmd, mscp->cdb, xs->cmdlen); 

	/***********************************************\
	* Usually return SUCCESSFULLY QUEUED            *
	\***********************************************/
	if (!(flags & SCSI_NOMASK))
	{
		s = splbio();
		uha_send_mbox(unit,mscp);
		uha_add_timeout(mscp,xs->timeout);
		splx(s);
		if(scsi_debug & TRACEINTERRUPTS)
			printf("cmd_sent ");
		return(SUCCESSFULLY_QUEUED);
	}
	/***********************************************\
	* If we can't use interrupts, poll on completion*
	\***********************************************/
	uha_send_mbox(unit,mscp);
	if(scsi_debug & TRACEINTERRUPTS)
		printf("cmd_wait ");
	do
	{
		if(uha_poll(unit,xs->timeout))
		{
			if (!(xs->flags & SCSI_SILENT)) printf("cmd fail\n");
			if(!(uha_abort(unit,mscp)))
			{
				printf("abort failed in wait\n");
				uha_free_mscp(unit,mscp,flags);
			}
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return(HAD_ERROR);
		}
	} while (!(xs->flags & ITSDONE));/* something (?) else finished */
	splx(s);
scsi_debug = 0;uha_debug = 0;
	if(xs->error)
	{
		return(HAD_ERROR);
	}
	return(COMPLETE);
}

/*
 *                +----------+    +----------+    +----------+
 * uha_soonest--->|    later |--->|     later|--->|     later|--->0
 *                | [Delta]  |    | [Delta]  |    | [Delta]  |
 *           0<---|sooner    |<---|sooner    |<---|sooner    |<---uha_latest
 *                +----------+    +----------+    +----------+
 *
 *     uha_furtherest = sum(Delta[1..n])
 */
uha_add_timeout(mscp,time)
struct  mscp     *mscp;
int     time;
{
	int     timeprev;
	struct mscp *prev;
	int     s = splbio();

	if(prev = uha_latest) /* yes, an assign */
	{
		timeprev = uha_furtherest;
	}
	else
	{
		timeprev = 0;
	}
	while(prev && (timeprev > time)) 
	{
		timeprev -= prev->delta;
		prev = prev->sooner;
	}
	if(prev)
	{
		mscp->delta = time - timeprev;
		if( mscp->later = prev->later) /* yes an assign */
		{
			mscp->later->sooner = mscp;
			mscp->later->delta -= mscp->delta;
		}
		else
		{
			uha_furtherest = time;
			uha_latest = mscp;
		}
		mscp->sooner = prev;
		prev->later = mscp;
	}
	else
	{
		if( mscp->later = uha_soonest) /* yes, an assign*/
		{
			mscp->later->sooner = mscp;
			mscp->later->delta -= time;
		}
		else
		{
			uha_furtherest = time;
			uha_latest = mscp;
		}
		mscp->delta = time;
		mscp->sooner = (struct mscp *)0;
		uha_soonest = mscp;
	}
	splx(s);
}

uha_remove_timeout(mscp)
struct  mscp     *mscp;
{
	int     s = splbio();

	if(mscp->sooner)
	{
		mscp->sooner->later = mscp->later;
	}
	else
	{
		uha_soonest = mscp->later;
	}
	if(mscp->later)
	{
		mscp->later->sooner = mscp->sooner;
		mscp->later->delta += mscp->delta;
	}
	else
	{
		uha_latest = mscp->sooner;
		uha_furtherest -= mscp->delta;
	}
	mscp->sooner = mscp->later = (struct mscp *)0;
	splx(s);
}


extern int      hz;
#define ONETICK 500 /* milliseconds */
#define SLEEPTIME ((hz * 1000) / ONETICK)
uha_timeout(arg)
int     arg;
{
	struct  mscp  *mscp;
	int     unit;
	int     s       = splbio();
	unsigned int stat;
	int	port = uha_data[unit].baseport;

	while( mscp = uha_soonest )
	{
		if(mscp->delta <= ONETICK)
		/***********************************************\
		* It has timed out, we need to do some work     *
		\***********************************************/
		{
			unit = mscp->xs->adapter;
			printf("uha%d:%d device timed out\n",unit
					,mscp->xs->targ);
			if(uha_debug & UHA_SHOWMSCPS)
				uha_print_active_mscp();

			/***************************************\
			* Unlink it from the queue              *
			\***************************************/
			uha_remove_timeout(mscp);

			if((uha_abort(unit,mscp) !=1) || (mscp->flags = MSCP_ABORTED))
			{
				printf("AGAIN");
				mscp->xs->retries = 0; /* I MEAN IT ! */
				uha_done(unit,mscp,FAIL);
			}
			else    /* abort the operation that has timed out */
			{
				printf("\n");
				uha_add_timeout(mscp,2000 + ONETICK);
				mscp->flags = MSCP_ABORTED;
			}
		}
		else
		/***********************************************\
		* It has not timed out, adjust and leave        *
		\***********************************************/
		{
			mscp->delta -= ONETICK;
			uha_furtherest -= ONETICK;
			break;
		}
	}
	splx(s);
	timeout(uha_timeout,arg,SLEEPTIME);
}

uha_show_scsi_cmd(struct scsi_xfer *xs)
{
	u_char  *b = (u_char *)xs->cmd;
	int i = 0;
	if(!(xs->flags & SCSI_RESET))
	{
		printf("uha%d:%d:%d-"
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
		printf("uha%d:%d:%d-RESET-\n" 
			,xs->adapter 
			,xs->targ
			,xs->lu
		);
	}
}
uha_print_mscp(mscp)
struct  mscp *mscp;
{
	printf("mscp:%x op:%x cmdlen:%d senlen:%d\n"
		,mscp
		,mscp->opcode
		,mscp->cdblen
		,mscp->senselen);
	printf("	sg:%d sgnum:%x datlen:%d hstat:%x tstat:%x delta:%d flags:%x\n"
		,mscp->sgth
		,mscp->sg_num
		,mscp->datalen
		,mscp->ha_status
		,mscp->targ_status
		,mscp->delta
		,mscp->flags);
	uha_show_scsi_cmd(mscp->xs);
}

uha_print_active_mscp()
{
	struct  mscp *mscp;
	mscp = uha_soonest;

	while(mscp)
	{
		uha_print_mscp(mscp);
		mscp = mscp->later;
	}
	printf("Furtherest = %d\n",uha_furtherest);
}

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
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 */

/*
 * HISTORY
 * $Log: bt742a.c,v $
 * Revision 1.7  1992/08/24  22:40:16  jason
 * BIG_DMA ifdef for 512 dma segments instead of 128 segments
 *
 * Revision 1.6  1992/08/24  21:01:58  jason
 * many changes and bugfixes for osf1
 *
 * Revision 1.5  1992/07/31  01:22:03  julian
 * support improved scsi.h layout
 *
 * Revision 1.4  1992/07/25  03:11:26  julian
 * check each request fro sane flags.
 *
 * Revision 1.3  1992/07/24  00:52:45  julian
 * improved timeout handling.
 * added support for two arguments to the sd_done (or equiv) call so that
 * they can pre-queue several arguments.
 * slightly clean up error handling
 *
 * Revision 1.2  1992/07/17  22:03:54  julian
 * upgraded the timeout code.
 * added support for UIO-based i/o (as used for pmem operations)
 *
 * Revision 1.1  1992/05/27  00:51:12  balsup
 * machkern/cor merge
 * 
 */

/*
 * bt742a BT-1542A SCSI driver
 */

#include <sys/types.h>
#include <bt.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#ifdef	MACH	/* EITHER CMU OR OSF */
#include <i386/ipl.h>
#include <i386at/scsi.h>
#include <i386at/scsiconf.h>

#ifdef	OSF	/* OSF ONLY */
#include <sys/table.h>
#include <i386/handler.h>
#include <i386/dispatcher.h>
#include <i386/AT386/atbus.h>

#else	OSF	/* CMU ONLY */
#include <i386at/atbus.h>
#include <i386/pio.h>
#endif	OSF
#endif	MACH	/* end of MACH specific */

#ifdef	__386BSD__	/* 386BSD specific */
#define isa_dev isa_device
#define dev_unit id_unit
#define dev_addr id_iobase

#include <i386/isa/isa_device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif	__386BSD__


#ifdef	__386BSD__
#include "ddb.h"
#if	NDDB > 0
int	Debugger();
#else	NDDB
#define	Debugger() panic("should call debugger here (adaptec.c)")
#endif	NDDB
#endif	__386BSD__

#ifdef	MACH
int	Debugger();
#endif	MACH

extern int delaycount;	/* from clock setup code */
typedef unsigned long int physaddr;

/*
 * I/O Port Interface
 */

#define	BT_BASE		bt_base[unit]
#define	BT_CTRL_STAT_PORT	(BT_BASE + 0x0)	/* control & status */
#define	BT_CMD_DATA_PORT	(BT_BASE + 0x1)	/* cmds and datas */
#define	BT_INTR_PORT		(BT_BASE + 0x2)	/* Intr. stat */

/*
 * BT_CTRL_STAT bits (write)
 */

#define BT_HRST		0x80	/* Hardware reset */
#define BT_SRST		0x40	/* Software reset */
#define BT_IRST		0x20	/* Interrupt reset */
#define BT_SCRST	0x10	/* SCSI bus reset */

/*
 * BT_CTRL_STAT bits (read)
 */

#define BT_STST		0x80	/* Self test in Progress */
#define BT_DIAGF	0x40	/* Diagnostic Failure */
#define BT_INIT		0x20	/* Mbx Init required */
#define BT_IDLE		0x10	/* Host Adapter Idle */
#define BT_CDF		0x08	/* cmd/data out port full */
#define BT_DF		0x04	/* Data in port full */
#define BT_INVDCMD	0x01	/* Invalid command */

/*
 * BT_CMD_DATA bits (write)
 */

#define	BT_NOP			0x00	/* No operation */
#define BT_MBX_INIT		0x01	/* Mbx initialization */
#define BT_START_SCSI		0x02	/* start scsi command */
#define BT_START_BIOS		0x03	/* start bios command */
#define BT_INQUIRE		0x04	/* Adapter Inquiry */
#define BT_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define BT_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define BT_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define BT_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define BT_SPEED_SET		0x09	/* set transfer speed */
#define BT_DEV_GET		0x0a	/* return installed devices */
#define BT_CONF_GET		0x0b	/* return configuration data */
#define BT_TARGET_EN		0x0c	/* enable target mode */
#define BT_SETUP_GET		0x0d	/* return setup data */
#define BT_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define BT_READ_CH2		0x1b	/* read channel 2 buffer */
#define BT_WRITE_FIFO		0x1c	/* write fifo buffer */
#define BT_READ_FIFO		0x1d	/* read fifo buffer */
#define BT_ECHO			0x1e	/* Echo command data */
#define BT_MBX_INIT_EXTENDED	0x81	/* Mbx initialization */
#define BT_INQUIRE_EXTENDED	0x8D	/* Adapter Setup Inquiry */

struct bt_cmd_buf {
	 u_char byte[16];	
};

/*
 * BT_INTR_PORT bits (read)
 */

#define BT_ANY_INTR		0x80	/* Any interrupt */
#define BT_SCRD		0x08	/* SCSI reset detected */
#define BT_HACC		0x04	/* Command complete */
#define BT_MBOA		0x02	/* MBX out empty */
#define BT_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs 
 */

#define BT_MBX_SIZE		16	/* mail box size */

struct bt_mbx
{
	struct bt_mbx_out {
		physaddr	ccb_addr;
		unsigned char	dummy[3];
		unsigned char	cmd;
	} mbo [BT_MBX_SIZE];
	struct bt_mbx_in{
		physaddr	ccb_addr;
		unsigned char	btstat;
		unsigned char 	sdstat;
		unsigned char	dummy;
		unsigned char	stat;
	} mbi[BT_MBX_SIZE];
};

/*
 * mbo.cmd values
 */

#define BT_MBO_FREE	0x0	/* MBO entry is free */
#define BT_MBO_START	0x1	/* MBO activate entry */
#define BT_MBO_ABORT	0x2	/* MBO abort entry */

#define BT_MBI_FREE	0x0	/* MBI entry is free */
#define BT_MBI_OK	0x1	/* completed without error */
#define BT_MBI_ABORT	0x2	/* aborted ccb */
#define BT_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define BT_MBI_ERROR	0x4	/* Completed with error */

extern struct bt_mbx bt_mbx[];

#if	defined(BIG_DMA)
/* #define	BT_NSEG	8192	/* Number of scatter gather segments - to much vm */
#define	BT_NSEG	512
#else
#define	BT_NSEG	33
#endif	/* BIG_DMA */
struct	bt_scat_gath
	{
		unsigned long	seg_len;
		physaddr	seg_addr;
	};

struct bt_ccb {
	unsigned char		opcode;
	unsigned char		:3,data_in:1,data_out:1,:3;
	unsigned char		scsi_cmd_length;
	unsigned char		req_sense_length;
	/*------------------------------------longword boundary */
	unsigned long		data_length;
	/*------------------------------------longword boundary */
	physaddr		data_addr;
	/*------------------------------------longword boundary */
	unsigned char		dummy[2];
	unsigned char		host_stat;
	unsigned char		target_stat;
	/*------------------------------------longword boundary */
	unsigned char		target;
	unsigned char		lun;
	unsigned char 		scsi_cmd[12];	/* 12 bytes (bytes only)*/
	unsigned char		dummy2[1];
	unsigned char		link_id;
	/*------------------------------------4 longword boundary */
	physaddr		link_addr;
	/*------------------------------------longword boundary */
	physaddr		sense_ptr;
	/*------------------------------------longword boundary */
	struct	scsi_sense_data	scsi_sense;
	/*------------------------------------longword boundary */
	struct	bt_scat_gath	scat_gath[BT_NSEG];
	/*------------------------------------longword boundary */
	struct	bt_ccb		*next;
	/*------------------------------------longword boundary */
	struct	scsi_xfer	*xfer;		/* the scsi_xfer for this cmd */
	/*------------------------------------longword boundary */
	struct	bt_mbx_out	*mbx;		/* pointer to mail box */
	/*------------------------------------longword boundary */
	long	int	delta;	/* difference from previous*/
	struct bt_ccb	*later,*sooner;
	int		flags;
#define	CCB_FREE	0
#define CCB_ACTIVE	1
#define	CCB_ABORTED	2
};

struct	bt_ccb *bt_soonest = (struct bt_ccb *)0;
struct	bt_ccb *bt_latest = (struct bt_ccb *)0;
long int	bt_furtherest = 0;	/* longest time in the timeout queue */
/*
 * opcode fields
 */

#define BT_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define BT_TARGET_CCB		0x01	/* SCSI Target CCB */
#define BT_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scattter gather*/
#define BT_RESET_CCB		0x81	/* SCSI Bus reset */


/*
 * bt_ccb.host_stat values
 */

#define BT_OK		0x00	/* cmd ok */
#define BT_LINK_OK	0x0a	/* Link cmd ok */
#define BT_LINK_IT	0x0b	/* Link cmd ok + int */
#define BT_SEL_TIMEOUT	0x11	/* Selection time out */
#define BT_OVER_UNDER	0x12	/* Data over/under run */
#define BT_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define BT_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define BT_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define BT_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define BT_BAD_LINK	0x17	/* Not same values of LUN for links */
#define BT_INV_TARGET	0x18	/* Invalid target direction */
#define BT_CCB_DUP	0x19	/* Duplicate CCB received */
#define BT_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define BT_ABORTED	42	/* pseudo value from driver */



struct bt_setup
{
	u_char	sync_neg:1;
	u_char	parity:1;
	u_char	:6;
	u_char	speed;
	u_char	bus_on;
	u_char	bus_off;
	u_char	num_mbx;
	u_char	mbx[4];
	struct
	{
		u_char	offset:4;
		u_char	period:3;
		u_char	valid:1;
	}sync[8];
	u_char	disc_sts;
};

struct	bt_config
{
	u_char	chan;
	u_char	intr;
	u_char	scsi_dev:3;
	u_char	:5;
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define EISADMA	0x00
#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80




#ifdef        MACH
extern physaddr	kvtophys();
#define PHYSTOKV(x)   phystokv(x)
#define KVTOPHYS(x)   kvtophys(x)
#endif MACH

#ifdef        __386BSD__
#define PHYSTOKV(x)   (x | 0xFE000000)
#define KVTOPHYS(x)   vtophys(x)
#endif        __386BSD__



#define PAGESIZ 	4096
#define INVALIDATE_CACHE {asm volatile( ".byte	0x0F ;.byte 0x08" ); }


u_char			bt_scratch_buf[256];
#ifdef	MACH
caddr_t			bt_base[NBT];		/* base port for each board */
#else	MACH
short			bt_base[NBT];		/* base port for each board */
#endif	MACH
struct	bt_mbx		bt_mbx[NBT];
struct	bt_ccb		*bt_ccb_free[NBT];
struct	bt_ccb		bt_ccb[NBT][BT_MBX_SIZE];
struct	scsi_xfer	bt_scsi_xfer[NBT];
struct	isa_dev		*btinfo[NBT];
struct	bt_ccb		*bt_get_ccb();
int			bt_int[NBT];
int			bt_dma[NBT];
int			bt_scsi_dev[NBT];
int			bt_initialized[NBT];
#if defined(OSF)
int			bt_attached[NBT];
#endif /* defined(OSF) */

/***********debug values *************/
#define	BT_SHOWCCBS 0x01
#define	BT_SHOWINTS 0x02
#define	BT_SHOWCMDS 0x04
#define	BT_SHOWMISC 0x08
int	bt_debug = 0;


int btprobe(), btattach();
int btintr();

#ifdef	MACH
struct	isa_driver	btdriver = { btprobe, 0, btattach, "bt", 0, 0, 0};
int (*btintrs[])() = {btintr, 0};
#endif	MACH

#ifdef	__386BSD__
struct	isa_driver	btdriver = { btprobe, btattach, "bt"};
#endif	__386BSD__

static	int	btunit = 0;

#define bt_abortmbx(mbx) \
	(mbx)->cmd = BT_MBO_ABORT; \
	outb(BT_CMD_DATA_PORT, BT_START_SCSI);
#define bt_startmbx(mbx) \
	(mbx)->cmd = BT_MBO_START; \
	outb(BT_CMD_DATA_PORT, BT_START_SCSI);



int	bt_scsi_cmd();
int	bt_timeout();
void	btminphys();
long int bt_adapter_info();

struct	scsi_switch	bt_switch =
{
	bt_scsi_cmd,
	btminphys,
	0,
	0,
	bt_adapter_info,
	0,0,0
};	
#define BT_CMD_TIMEOUT_FUDGE 200 /* multiplied to get Secs */
#define BT_RESET_TIMEOUT 1000000
#define BT_SCSI_TIMEOUT_FUDGE 20 /* divided by for mSecs */


/***********************************************************************\
* bt_cmd(unit,icnt, ocnt,wait, retval, opcode, args)			*
* Activate Adapter command						*
*	icnt:	number of args (outbound bytes written after opcode)	*
*	ocnt:	number of expected returned bytes			*
*	wait:   number of seconds to wait for response			*
*	retval:	buffer where to place returned bytes			*
*	opcode:	opcode BT_NOP, BT_MBX_INIT, BT_START_SCSI ...		*
*	args:	parameters						*
*									*
* Performs an adapter command through the ports. Not to be confused	*
*	with a scsi command, which is read in via the dma		*
* One of the adapter commands tells it to read in a scsi command	*
\***********************************************************************/
bt_cmd(unit,icnt, ocnt, wait,retval, opcode, args)

u_char *retval;
unsigned opcode;
u_char args;
{
	unsigned *ic = &opcode;
	u_char oc;
	register i;
	int	sts;

	/*******************************************************\
	* multiply the wait argument by a big constant		*
	* zero defaults to 1					*
	\*******************************************************/
	if(!wait) 
		wait = BT_CMD_TIMEOUT_FUDGE * delaycount; 
	else
		wait *= BT_CMD_TIMEOUT_FUDGE * delaycount; 
	/*******************************************************\
	* Wait for the adapter to go idle, unless it's one of	*
	* the commands which don't need this			*
	\*******************************************************/
	if (opcode != BT_MBX_INIT && opcode != BT_START_SCSI)
	{
		i = BT_CMD_TIMEOUT_FUDGE * delaycount; /* 1 sec?*/
		while (--i)
		{
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts & BT_IDLE)
			{
				break;
			}
		}
		if (!i)
		{
			printf("bt_cmd: bt742a host not idle(0x%x)\n",sts);
			return(ENXIO);
		}
	}
	/*******************************************************\
	* Now that it is idle, if we expect output, preflush the*
	* queue feeding to us.					*
	\*******************************************************/
	if (ocnt)
	{
		while((inb(BT_CTRL_STAT_PORT)) & BT_DF)
			inb(BT_CMD_DATA_PORT);
	}
			
	/*******************************************************\
	* Output the command and the number of arguments given	*
	* for each byte, first check the port is empty.		*
	\*******************************************************/
	icnt++;		/* include the command */
	while (icnt--)
	{
		sts = inb(BT_CTRL_STAT_PORT);
		for (i=0; i< wait; i++)
		{
			sts = inb(BT_CTRL_STAT_PORT);
			if (!(sts & BT_CDF))
				break;
		}
		if (i >=  wait)
		{
			printf("bt_cmd: bt742a cmd/data port full\n");
			outb(BT_CTRL_STAT_PORT, BT_SRST); 
			return(ENXIO);
		}
		outb(BT_CMD_DATA_PORT, (u_char)(*ic++));
	}
	/*******************************************************\
	* If we expect input, loop that many times, each time,	*
	* looking for the data register to have valid data	*
	\*******************************************************/
	while (ocnt--)
	{
		sts = inb(BT_CTRL_STAT_PORT);
		for (i=0; i< wait; i++)
		{
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts  & BT_DF)
				break;
		}
		if (i >=  wait)
		{
			printf("bt_cmd: bt742a cmd/data port empty %d\n",ocnt);
			return(ENXIO);
		}
		oc = inb(BT_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*******************************************************\
	* Wait for the board to report a finised instruction	*
	\*******************************************************/
	i=BT_CMD_TIMEOUT_FUDGE * delaycount;	/* 1 sec? */
	while (--i)
	{
		sts = inb(BT_INTR_PORT);
		if (sts & BT_HACC)
		{
			break;
		}
	}
	if (!i)
	{
		printf("bt_cmd: bt742a host not finished(0x%x)\n",sts);
		return(ENXIO);
	}
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return(0);
}

/*******************************************************\
* Check if the device can be found at the port given	*
* and if so, set it up ready for further work		*
* as an argument, takes the isa_dev structure from	*
* autoconf.c						*
\*******************************************************/

btprobe(dev)
struct isa_dev *dev;
{
	/***********************************************\
	* find unit and check we have that many defined	*
	\***********************************************/
	int     unit = btunit;
#if defined(OSF)
	static ihandler_t bt_handler[NBT];
	static ihandler_id_t *bt_handler_id[NBT];
	register ihandler_t *chp = &bt_handler[unit];;
#endif /* defined(OSF) */

	dev->dev_unit = unit;
	bt_base[unit] = dev->dev_addr;
	if(unit >= NBT) 
	{
		printf("bt: unit number (%d) too high\n",unit);
		return(0);
	}
	/***********************************************\
	* Try initialise a unit at this location	*
	* sets up dma and bus speed, loads bt_int[unit]*
	\***********************************************/
	if (bt_init(unit) != 0)
	{
		return(0);
	}

	/***********************************************\
	* If it's there, put in it's interrupt vectors	*
	\***********************************************/
#ifdef	MACH
#if defined(OSF)				/* OSF */
	chp->ih_level = dev->dev_pic;
	chp->ih_handler = dev->dev_intr[0];
	chp->ih_resolver = i386_resolver;
	chp->ih_rdev = dev;
	chp->ih_stats.intr_type = INTR_DEVICE;
	chp->ih_stats.intr_cnt = 0;
	chp->ih_hparam[0].intparam = unit;
	if ((bt_handler_id[unit] = handler_add(chp)) != NULL)
		handler_enable(bt_handler_id[unit]);
	else
		panic("Unable to add bt interrupt handler");
#else 						/* CMU */
	dev->dev_pic = bt_int[unit];
	take_dev_irq(dev);
#endif /* !defined(OSF) */
	printf("port=%x spl=%d\n", dev->dev_addr, dev->dev_spl);
#endif	MACH
#ifdef  __386BSD__				/* 386BSD */
        dev->id_irq = (1 << bt_int[unit]);
        dev->id_drq = bt_dma[unit];
	printf("\n  **");
#endif  __386BSD__

	btunit++;
	return(1);
}

/***********************************************\
* Attach all the sub-devices we can find	*
\***********************************************/
btattach(dev)
struct	isa_dev	*dev;
{
	int	unit = dev->dev_unit;


#ifdef  __386BSD__
	printf(" probing for scsi devices**\n");
#endif  __386BSD__

	/***********************************************\
	* ask the adapter what subunits are present	*
	\***********************************************/
	scsi_attachdevs( unit, bt_scsi_dev[unit], &bt_switch);
#if defined(OSF)
	bt_attached[unit]=1;
#endif /* defined(OSF) */
	if(!unit) /* only one for all boards */
	{
		bt_timeout(0);
	}
#ifdef  __386BSD__
	printf("bt%d",unit);
#endif  __386BSD__
	return;
}

/***********************************************\
* Return some information to the caller about   *
* the adapter and it's capabilities             *
\***********************************************/
long int bt_adapter_info(unit)
int	unit;
{
	return(2);	/* 2 outstanding requests at a time per device */
}

/***********************************************\
* Catch an interrupt from the adaptor		*
\***********************************************/
btintr(unit)
{
	struct bt_ccb *ccb;
	unsigned char stat;
	register i;

	if(scsi_debug & PRINTROUTINES)
		printf("btintr ");
	/***********************************************\
	* First acknowlege the interrupt, Then if it's	*
	* not telling about a completed operation	*
	* just return. 					*
	\***********************************************/
	stat = inb(BT_INTR_PORT);
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	if(scsi_debug & TRACEINTERRUPTS)
		printf("int = 0x%x ",stat);
	if (! (stat & BT_MBIF))
		return 1;
	if(scsi_debug & TRACEINTERRUPTS)
		printf("mbxi ");
#if defined(OSF)
	if (!bt_attached[unit])
	{
		return(1);
	}
#endif /* defined(OSF) */
	/***********************************************\
	* If it IS then process the competed operation	*
	\***********************************************/
	for (i = 0; i < BT_MBX_SIZE; i++)
	{
		if (bt_mbx[unit].mbi[i].stat != BT_MBI_FREE)
		{
			ccb = (struct bt_ccb *)PHYSTOKV(
					(bt_mbx[unit].mbi[i].ccb_addr));
			if((bt_debug & BT_SHOWCCBS) && ccb)
				printf("<int ccb(%x)>",ccb);
			if((stat =  bt_mbx[unit].mbi[i].stat) != BT_MBI_OK)
			{
				switch(stat)
				{
				case	BT_MBI_ABORT:
					if(bt_debug & BT_SHOWMISC)
						printf("abort ");
					ccb->host_stat = BT_ABORTED;
					break;

				case	BT_MBI_UNKNOWN:
					ccb = (struct bt_ccb *)0;
					if(bt_debug & BT_SHOWMISC)
						printf("unknown ccb for abort");
					break;

				case	BT_MBI_ERROR:
					break;

				default:
					panic("Impossible mbxi status");

				}
				if((bt_debug & BT_SHOWCMDS ) && ccb)
				{
					u_char	*cp;
					cp = ccb->scsi_cmd;
					printf("op=%x %x %x %x %x %x\n", 
						cp[0], cp[1], cp[2],
						cp[3], cp[4], cp[5]);
					printf("stat %x for mbi[%d]\n"
						, bt_mbx[unit].mbi[i].stat, i);
					printf("addr = 0x%x\n", ccb);
				}
			}
			if(ccb)
			{
				bt_remove_timeout(ccb);
				bt_done(unit,ccb);
			}
			bt_mbx[unit].mbi[i].stat = BT_MBI_FREE;
		}
	}
	return(1);
}

/***********************************************\
* A ccb (and hence a mbx-out is put onto the 	*
* free list.					*
\***********************************************/
bt_free_ccb(unit,ccb, flags)
struct bt_ccb *ccb;
{
	unsigned int opri;
	
	if(scsi_debug & PRINTROUTINES)
		printf("ccb%d(0x%x)> ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();

	ccb->next = bt_ccb_free[unit];
	bt_ccb_free[unit] = ccb;
	ccb->flags = CCB_FREE;
	/***********************************************\
	* If there were none, wake abybody waiting for	*
	* one to come free, starting with queued entries*
	\***********************************************/
	if (!ccb->next) {
		wakeup(&bt_ccb_free[unit]);
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
}

/***********************************************\
* Get a free ccb (and hence mbox-out entry)	*
\***********************************************/
struct bt_ccb *
bt_get_ccb(unit,flags)
{
	unsigned opri;
	struct bt_ccb *rc;

	if(scsi_debug & PRINTROUTINES)
		printf("<ccb%d(0x%x) ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();
	/***********************************************\
	* If we can and have to, sleep waiting for one	*
	* to come free					*
	\***********************************************/
	while ((!(rc = bt_ccb_free[unit])) && (!(flags & SCSI_NOSLEEP)))
	{
		sleep(&bt_ccb_free[unit], PRIBIO);
	}
	if (rc) 
	{
		bt_ccb_free[unit] = rc->next;
		rc->flags = CCB_ACTIVE;
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
	return(rc);
}
		

/***********************************************\
* We have a ccb which has been processed by the	*
* adaptor, now we look to see how the operation	*
* went. Wake up the owner if waiting		*
\***********************************************/
bt_done(unit,ccb)
struct bt_ccb *ccb;
{
	struct	scsi_sense_data *s1,*s2;
	struct	scsi_xfer *xs = ccb->xfer;

	if(scsi_debug & (PRINTROUTINES | TRACEINTERRUPTS))
		printf("bt_done ");
	/***********************************************\
	* Otherwise, put the results of the operation	*
	* into the xfer and call whoever started it	*
	\***********************************************/
	if (  	(	ccb->host_stat != BT_OK 
			|| ccb->target_stat != SCSI_OK)
	      && (!(xs->flags & SCSI_ERR_OK)))
	{

		s1 = &(ccb->scsi_sense);
		s2 = &(xs->sense);

		if(ccb->host_stat)
		{
			switch(ccb->host_stat)
			{
			case	BT_ABORTED:	/* No response */
			case	BT_SEL_TIMEOUT:	/* No response */
				if (bt_debug & BT_SHOWMISC)
				{
					printf("timeout reported back\n");
				}
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				if (bt_debug & BT_SHOWMISC)
				{
					printf("unexpected host_stat: %x\n",
						ccb->host_stat);
				}
			}

		}
		else
		{
			switch(ccb->target_stat)
			{
			case 0x02:
				/* structure copy!!!!!*/
				*s2=*s1;
				xs->error = XS_SENSE;
				break;
			case 0x08:
				xs->error = XS_BUSY;
				break;
			default:
				if (bt_debug & BT_SHOWMISC)
				{
					printf("unexpected target_stat: %x\n",
						ccb->target_stat);
				}
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
	else		/* All went correctly  OR errors expected */
	{
		xs->resid = 0;
	}
	xs->flags |= ITSDONE;
	bt_free_ccb(unit,ccb, xs->flags);
	if(xs->when_done)
		(*(xs->when_done))(xs->done_arg,xs->done_arg2);
}

/***********************************************\
* Start the board, ready for normal operation	*
\***********************************************/
bt_init(unit)
int	unit;
{
	unsigned char ad[4];
	volatile int i,sts;
	struct	bt_config conf;

	/***********************************************\
	* reset board, If it doesn't respond, assume 	*
	* that it's not there.. good for the probe	*
	\***********************************************/

	outb(BT_CTRL_STAT_PORT, BT_HRST|BT_SRST);

	for (i=0; i < BT_RESET_TIMEOUT; i++)
	{
		sts = inb(BT_CTRL_STAT_PORT) ;
		if ( sts == (BT_IDLE | BT_INIT))
			break;
	}
	if (i >= BT_RESET_TIMEOUT)
	{
		if (bt_debug & BT_SHOWMISC)
			printf("bt_init: No answer from bt742a board\n");
		return(ENXIO);
	}

	/***********************************************\
	* Assume we have a board at this stage		*
	* setup dma channel from jumpers and save int	*
	* level						*
	\***********************************************/
#ifdef	__386BSD__
	printf("bt%d reading board settings, ",unit);
#define	PRNT(x)
#else	__386BSD__
	printf("bt%d:",unit);
#define	PRNT(x) printf(x)
#endif	__386BSD__

	bt_cmd(unit,0, sizeof(conf), 0 ,&conf, BT_CONF_GET);
	switch(conf.chan)
	{
	case	EISADMA:
		bt_dma[unit] = -1;
		PRNT("eisa dma ");
		break;
	case	CHAN0:
		outb(0x0b, 0x0c);
		outb(0x0a, 0x00);
		bt_dma[unit] = 0;
		PRNT("dma=0 ");
		break;
	case	CHAN5:
		outb(0xd6, 0xc1);
		outb(0xd4, 0x01);
		bt_dma[unit] = 5;
		PRNT("dma=5 ");
		break;
	case	CHAN6:
		outb(0xd6, 0xc2);
		outb(0xd4, 0x02);
		bt_dma[unit] = 6;
		PRNT("dma=6 ");
		break;
	case	CHAN7:
		outb(0xd6, 0xc3);
		outb(0xd4, 0x03);
		bt_dma[unit] = 7;
		PRNT("dma=7 ");
		break;
	default:
		printf("illegal dma setting %x\n",conf.chan);
		return(EIO);
	}
	switch(conf.intr)
	{
	case	INT9:
		bt_int[unit] = 9;
		PRNT("int=9 ");
		break;
	case	INT10:
		bt_int[unit] = 10;
		PRNT("int=10 ");
		break;
	case	INT11:
		bt_int[unit] = 11;
		PRNT("int=11 ");
		break;
	case	INT12:
		bt_int[unit] = 12;
		PRNT("int=12 ");
		break;
	case	INT14:
		bt_int[unit] = 14;
		PRNT("int=14 ");
		break;
	case	INT15:
		bt_int[unit] = 15;
		PRNT("int=15 ");
		break;
	default:
		printf("illegal int setting\n");
		return(EIO);
	}
	/* who are we on the scsi bus */
	bt_scsi_dev[unit] = conf.scsi_dev;
	/***********************************************\
	* Initialize mail box 				*
	\***********************************************/

	*((physaddr *)ad) = KVTOPHYS(&bt_mbx[unit]);
	bt_cmd(unit,5, 0, 0, 0, BT_MBX_INIT_EXTENDED
		, BT_MBX_SIZE
		, ad[0]
		, ad[1]
		, ad[2] 
		, ad[3]);

	/***********************************************\
	* link the ccb's with the mbox-out entries and	*
	* into a free-list				*
	\***********************************************/
	for (i=0; i < BT_MBX_SIZE; i++) {
		bt_ccb[unit][i].next = bt_ccb_free[unit];
		bt_ccb_free[unit] = &bt_ccb[unit][i];
		bt_ccb_free[unit]->flags = CCB_FREE;
		bt_ccb_free[unit]->mbx = &bt_mbx[unit].mbo[i];
		bt_mbx[unit].mbo[i].ccb_addr = KVTOPHYS(bt_ccb_free[unit]) ;
	}

	/***********************************************\
	* Note that we are going and return (to probe)	*
	\***********************************************/
	bt_initialized[unit]++;
	return( 0 );
}


#ifndef	min
#define min(x,y) (x < y ? x : y)
#endif	min


void btminphys(bp)
struct	buf *bp;
{
#ifdef	MACH
#if	!defined(OSF)
	bp->b_flags |= B_NPAGES;		/* can support scat/gather */
#endif	/* defined(OSF) */
#endif	MACH
	if(bp->b_bcount > ((BT_NSEG-1) * PAGESIZ))
	{
		bp->b_bcount = ((BT_NSEG-1) * PAGESIZ);
	}
}
	
/***********************************************\
* start a scsi operation given the command and	*
* the data address. Also needs the unit, target	*
* and lu					*
\***********************************************/
int	bt_scsi_cmd(xs)
struct scsi_xfer *xs;
{
	struct	scsi_sense_data *s1,*s2;
	struct bt_ccb *ccb;
	struct bt_scat_gath *sg;
	int	seg;	/* scatter gather seg being worked on */
	int i	= 0;
	int rc	=  0;
	int	thiskv;
	physaddr	thisphys,nextphys;
	int	unit =xs->adapter;
	int	bytes_this_seg,bytes_this_page,datalen,flags;
	struct	iovec	*iovp;

	if(scsi_debug & PRINTROUTINES)
		printf("bt_scsi_cmd ");
	/***********************************************\
	* get a ccb (mbox-out) to use. If the transfer	*
	* is from a buf (possibly from interrupt time)	*
	* then we can't allow it to sleep		*
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
	if (!(ccb = bt_get_ccb(unit,flags)))
	{
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

	if(bt_debug & BT_SHOWCCBS)
				printf("<start ccb(%x)>",ccb);
	if (ccb->mbx->cmd != BT_MBO_FREE)
		printf("MBO not free\n");

	/***********************************************\
	* Put all the arguments for the xfer in the ccb	*
	\***********************************************/
	ccb->xfer		=	xs;
	if(flags & SCSI_RESET)
	{
		ccb->opcode	=	BT_RESET_CCB;
	}
	else
	{
		/* can't use S/G if zero length */
		ccb->opcode	=	(xs->datalen?
						BT_INIT_SCAT_GATH_CCB
						:BT_INITIATOR_CCB);
	}
	ccb->target		=	xs->targ;;
	ccb->data_out		=	0;
	ccb->data_in		=	0;
	ccb->lun		=	xs->lu;
	ccb->scsi_cmd_length	=	xs->cmdlen;
	ccb->sense_ptr		=	KVTOPHYS(&(ccb->scsi_sense));
	ccb->req_sense_length	=	sizeof(ccb->scsi_sense);

	if((xs->datalen) && (!(flags & SCSI_RESET)))
	{ /* can use S/G only if not zero length */
		ccb->data_addr = KVTOPHYS(ccb->scat_gath);
		sg		=	ccb->scat_gath ;
		seg 		=	0;
		if(flags & SCSI_DATA_UIO)
		{
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < BT_NSEG))
			{
				sg->seg_addr = (physaddr)iovp->iov_base;
				xs->datalen += sg->seg_len = iovp->iov_len;	
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
			* Set up the scatter gather block		*
			\***********************************************/
		
			if(scsi_debug & SHOWSCATGATH)
				printf("%d @0x%x:- ",xs->datalen,xs->data);
			datalen		=	xs->datalen;
			thiskv		=	(int)xs->data;
			thisphys	=	KVTOPHYS(thiskv);
		
			while ((datalen) && (seg < BT_NSEG))
			{
				bytes_this_seg	= 0;
	
				/* put in the base address */
				sg->seg_addr = thisphys;
		
				if(scsi_debug & SHOWSCATGATH)
					printf("0x%x",thisphys);
	
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
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x)",bytes_this_seg);
				sg->seg_len = bytes_this_seg;	
				sg++;
				seg++;
			}
		} /*end of iov/kv decision */
		ccb->data_length = seg * sizeof(struct bt_scat_gath);
		if(scsi_debug & SHOWSCATGATH)
			printf("\n");
		if (datalen)
		{ /* there's still data, must have run out of segs! */
			printf("bt_scsi_cmd%d: more than %d DMA segs\n",
				unit,BT_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			bt_free_ccb(unit,ccb,flags);
			return(HAD_ERROR);
		}

	}
	else
	{	/* No data xfer, use non S/G values */
		ccb->data_addr = (physaddr)0;
		ccb->data_length = 0;
	}
	ccb->link_id = 0;
	ccb->link_addr = (physaddr)0;
	/***********************************************\
	* Put the scsi command in the ccb and start it	*
	\***********************************************/
	if(!(flags & SCSI_RESET))
	{
		bcopy(xs->cmd, ccb->scsi_cmd, ccb->scsi_cmd_length);
	}
	if(scsi_debug & SHOWCOMMANDS)
	{
		u_char	*b = ccb->scsi_cmd;
		if(!(flags & SCSI_RESET))
		{
			int i = 0;
			printf("bt%d:%d:%d-"
				,unit
				,ccb->target
				,ccb->lun);
			while(i < ccb->scsi_cmd_length )
			{
				if(i) printf(",");
				printf("%x",b[i++]);
			}
			printf("-\n");
		}
		else
		{
			printf("bt%d:%d:%d-RESET- " 
				,unit 
				,ccb->target
				,ccb->lun
			);
		}
	}
	bt_startmbx(ccb->mbx);
	/***********************************************\
	* Usually return SUCCESSFULLY QUEUED		*
	\***********************************************/
	if(scsi_debug & TRACEINTERRUPTS)
		printf("cmd_sent ");
	if (!(flags & SCSI_NOMASK))
	{
		bt_add_timeout(ccb,xs->timeout);
		return(SUCCESSFULLY_QUEUED);
	}
	/***********************************************\
	* If we can't use interrupts, poll on completion*
	\***********************************************/
	{
		int done = 0;
		int count = delaycount * xs->timeout / BT_SCSI_TIMEOUT_FUDGE;
		if(scsi_debug & TRACEINTERRUPTS)
			printf("wait ");
		while((!done) && count)
		{
			i=0;
			while ( (!done) && i<BT_MBX_SIZE)
			{
				if ((bt_mbx[unit].mbi[i].stat != BT_MBI_FREE )
				   && (PHYSTOKV(bt_mbx[unit].mbi[i].ccb_addr)
					== (int)ccb))
				{
					bt_mbx[unit].mbi[i].stat = BT_MBI_FREE;
					bt_done(unit,ccb);
					done++;
				}
				i++;
			}
			count--;
		}
		if (!count)
		{
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			bt_abortmbx(ccb->mbx);
			count = delaycount * 2000 / BT_SCSI_TIMEOUT_FUDGE;
			while((!done) && count)
			{
				i=0;
				while ( (!done) && i<BT_MBX_SIZE)
				{
					if ((bt_mbx[unit].mbi[i].stat != BT_MBI_FREE )
				   	&& (PHYSTOKV((bt_mbx[unit].mbi[i].ccb_addr)
						== (int)ccb)))
					{
						bt_mbx[unit].mbi[i].stat = BT_MBI_FREE;
						bt_done(unit,ccb);
						done++;
					}
					i++;
				}
				count--;
			}
			if(!count)
			{
				printf("abort failed in wait\n");
				ccb->mbx->cmd = BT_MBO_FREE;
			}
			bt_free_ccb(unit,ccb,flags);
			btintr(unit);
			xs->error = XS_DRIVER_STUFFUP;
			return(HAD_ERROR);
		}
		btintr(unit);
		if(xs->error) return(HAD_ERROR);
		return(COMPLETE);
	} 
}

/*
 *               +----------+     +----------+     +----------+
 * bt_soonest--->|    later |---->|     later|---->|     later|--->0
 *               | [Delta]  |     | [Delta]  |     | [Delta]  |
 *        0<-----|sooner    |<----|sooner    |<----|sooner    |<----bt_latest
 *               +----------+     +----------+     +----------+
 *
 *     bt_furtherest = sum(Delta[1..n])
 */
bt_add_timeout(ccb,time)
struct	bt_ccb	*ccb;
int	time;
{
	int	timeprev;
	struct bt_ccb *prev;
	int	s = splbio();

	if(prev = bt_latest) /* yes, an assign */
	{
		timeprev = bt_furtherest;
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
		ccb->delta = time - timeprev;
		if( ccb->later = prev->later) /* yes an assign */
		{
			ccb->later->sooner = ccb;
			ccb->later->delta -= ccb->delta;
		}
		else
		{
			bt_furtherest = time;
			bt_latest = ccb;
		}
		ccb->sooner = prev;
		prev->later = ccb;
	}
	else
	{
		if( ccb->later = bt_soonest) /* yes, an assign*/
		{
			ccb->later->sooner = ccb;
			ccb->later->delta -= time;
		}
		else
		{
			bt_furtherest = time;
			bt_latest = ccb;
		}
		ccb->delta = time;
		ccb->sooner = (struct bt_ccb *)0;
		bt_soonest = ccb;
	}
	splx(s);
}

bt_remove_timeout(ccb)
struct	bt_ccb	*ccb;
{
	int	s = splbio();

	if(ccb->sooner)
	{
		ccb->sooner->later = ccb->later;
	}
	else
	{
		bt_soonest = ccb->later;
	}
	if(ccb->later)
	{
		ccb->later->sooner = ccb->sooner;
		ccb->later->delta += ccb->delta;
	}
	else
	{
		bt_latest = ccb->sooner;
		bt_furtherest -= ccb->delta;
	}
	ccb->sooner = ccb->later = (struct bt_ccb *)0;
	splx(s);
}


extern int 	hz;
#define ONETICK 500 /* milliseconds */
#define SLEEPTIME ((hz * 1000) / ONETICK)
bt_timeout(arg)
int	arg;
{
	struct  bt_ccb  *ccb;
	int	unit;
	int	s	= splbio();

	while( ccb = bt_soonest )
	{
		if(ccb->delta <= ONETICK)
		/***********************************************\
		* It has timed out, we need to do some work	*
		\***********************************************/
		{
			unit = ccb->xfer->adapter;
			printf("bt%d:%d device timed out\n",unit
					,ccb->xfer->targ);
			if(bt_debug & BT_SHOWCCBS)
				tfs_print_active_ccbs();

			/***************************************\
			* Unlink it from the queue		*
			\***************************************/
			bt_remove_timeout(ccb);

			/***************************************\
			* If The ccb's mbx is not free, then	*
			* the board has gone south		*
			\***************************************/
			if(ccb->mbx->cmd != BT_MBO_FREE)
			{
				printf("bt%d not taking commands!\n"
							,unit);
				Debugger();
			}
			/***************************************\
			* If it has been through before, then	*
			* a previous abort has failed, don't	*
			* try abort again			*
			\***************************************/
			if(ccb->flags == CCB_ABORTED) /* abort timed out */
			{
				printf("AGAIN");
				ccb->xfer->retries = 0; /* I MEAN IT ! */
				ccb->host_stat = BT_ABORTED;
				bt_done(unit,ccb);
			}
			else	/* abort the operation that has timed out */
			{
				printf("\n");
				bt_abortmbx(ccb->mbx);
					/* 2 secs for the abort */
				bt_add_timeout(ccb,2000 + ONETICK);
				ccb->flags = CCB_ABORTED;
			}
		}
		else
		/***********************************************\
		* It has not timed out, adjust and leave	*
		\***********************************************/
		{
			ccb->delta -= ONETICK;
			bt_furtherest -= ONETICK;
			break;
		}
	}
	splx(s);
	timeout(bt_timeout,arg,SLEEPTIME);
}

tfs_print_ccb(ccb)
struct	bt_ccb *ccb;
{
	printf("ccb:%x op:%x cmdlen:%d senlen:%d\n"
		,ccb
		,ccb->opcode
		,ccb->scsi_cmd_length
		,ccb->req_sense_length);
	printf("	datlen:%d hstat:%x tstat:%x delta:%d flags:%x\n"
		,ccb->data_length
		,ccb->host_stat
		,ccb->target_stat
		,ccb->delta
		,ccb->flags);
}

tfs_print_active_ccbs()
{
	struct	bt_ccb *ccb;
	ccb = bt_soonest;

	while(ccb)
	{
		tfs_print_ccb(ccb);
		ccb = ccb->later;
	}
	printf("Furtherest = %d\n",bt_furtherest);
}

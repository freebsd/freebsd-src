/*
 * (Mostly) Written by Julian Elischer (julian@tfs.com)
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
 *	$Id: aha1542.c,v 1.13 1993/10/28 02:38:36 rgrimes Exp $
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

/*
 * a FEW lines in this driver come from a MACH adaptec-disk driver
 * so the copyright below is included:
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 * 
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/types.h>
#include <aha.h>

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

#include <i386/isa/isa_device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif  __386BSD__


#ifdef	__386BSD__
#include "ddb.h"
#if	NDDB > 0
int	Debugger();
#else	NDDB
#define Debugger() panic("should call debugger here (adaptec.c)")
#endif	NDDB
#endif	__386BSD__
extern int 	hz;
extern int delaycount;  /* from clock setup code */

/************************** board definitions *******************************/
/*
 * I/O Port Interface
 */

#define	AHA_BASE		aha_base[unit]
#define	AHA_CTRL_STAT_PORT	(AHA_BASE + 0x0)	/* control & status */
#define	AHA_CMD_DATA_PORT	(AHA_BASE + 0x1)	/* cmds and datas */
#define	AHA_INTR_PORT		(AHA_BASE + 0x2)	/* Intr. stat */

/*
 * AHA_CTRL_STAT bits (write)
 */

#define AHA_HRST		0x80	/* Hardware reset */
#define AHA_SRST		0x40	/* Software reset */
#define AHA_IRST		0x20	/* Interrupt reset */
#define AHA_SCRST		0x10	/* SCSI bus reset */

/*
 * AHA_CTRL_STAT bits (read)
 */

#define AHA_STST		0x80	/* Self test in Progress */
#define AHA_DIAGF		0x40	/* Diagnostic Failure */
#define AHA_INIT		0x20	/* Mbx Init required */
#define AHA_IDLE		0x10	/* Host Adapter Idle */
#define AHA_CDF			0x08	/* cmd/data out port full */
#define AHA_DF			0x04	/* Data in port full */
#define AHA_INVDCMD		0x01	/* Invalid command */

/*
 * AHA_CMD_DATA bits (write)
 */

#define	AHA_NOP			0x00	/* No operation */
#define AHA_MBX_INIT		0x01	/* Mbx initialization */
#define AHA_START_SCSI		0x02	/* start scsi command */
#define AHA_START_BIOS		0x03	/* start bios command */
#define AHA_INQUIRE		0x04	/* Adapter Inquiry */
#define AHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define AHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define AHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define AHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define AHA_SPEED_SET		0x09	/* set transfer speed */
#define AHA_DEV_GET		0x0a	/* return installed devices */
#define AHA_CONF_GET		0x0b	/* return configuration data */
#define AHA_TARGET_EN		0x0c	/* enable target mode */
#define AHA_SETUP_GET		0x0d	/* return setup data */
#define AHA_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define AHA_READ_CH2		0x1b	/* read channel 2 buffer */
#define AHA_WRITE_FIFO		0x1c	/* write fifo buffer */
#define AHA_READ_FIFO		0x1d	/* read fifo buffer */
#define AHA_ECHO		0x1e	/* Echo command data */
#define AHA_EXT_BIOS		0x28	/* return extended bios info */
#define AHA_MBX_ENABLE		0x29	/* enable mail box interface */

struct aha_cmd_buf {
	 u_char byte[16];	
};

/*
 * AHA_INTR_PORT bits (read)
 */

#define AHA_ANY_INTR		0x80	/* Any interrupt */
#define AHA_SCRD		0x08	/* SCSI reset detected */
#define AHA_HACC		0x04	/* Command complete */
#define AHA_MBOA		0x02	/* MBX out empty */
#define AHA_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs 
 */

#define AHA_MBX_SIZE		16	/* mail box size */

struct aha_mbx {
	struct aha_mbx_out {
		unsigned char cmd;
		unsigned char ccb_addr[3];
	} mbo [AHA_MBX_SIZE];
	struct aha_mbx_in{
		unsigned char stat;
		unsigned char ccb_addr[3];
	} mbi[AHA_MBX_SIZE];
};

/*
 * mbo.cmd values
 */

#define AHA_MBO_FREE	0x0	/* MBO entry is free */
#define AHA_MBO_START	0x1	/* MBO activate entry */
#define AHA_MBO_ABORT	0x2	/* MBO abort entry */

#define AHA_MBI_FREE	0x0	/* MBI entry is free */
#define AHA_MBI_OK	0x1	/* completed without error */
#define AHA_MBI_ABORT	0x2	/* aborted ccb */
#define AHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define AHA_MBI_ERROR	0x4	/* Completed with error */

extern struct aha_mbx aha_mbx[];

/* FOR OLD VERSIONS OF THE !%$@ this may have to be 16 (yuk) */
#define	AHA_NSEG	17	/* Number of scatter gather segments <= 16 */
				/* allow 64 K i/o (min) */

struct aha_ccb {
	unsigned char	opcode;
	unsigned char	lun:3;
	unsigned char	data_in:1;		/* must be 0 */
	unsigned char	data_out:1;		/* must be 0 */
	unsigned char	target:3;
	unsigned char	scsi_cmd_length;
	unsigned char	req_sense_length;
	unsigned char	data_length[3];
	unsigned char	data_addr[3];
	unsigned char	link_addr[3];
	unsigned char	link_id;
	unsigned char	host_stat;
	unsigned char	target_stat;
	unsigned char	reserved[2];
	struct	scsi_generic	scsi_cmd;
	struct	scsi_sense_data	scsi_sense;
	struct	aha_scat_gath {
		unsigned char seg_len[3];
		unsigned char seg_addr[3];
	} scat_gath[AHA_NSEG];
	struct	aha_ccb	*next;
	struct	scsi_xfer	*xfer;		/* the scsi_xfer for this cmd */
	struct	aha_mbx_out	*mbx;		/* pointer to mail box */
	int	flags;
#define CCB_FREE        0
#define CCB_ACTIVE      1
#define CCB_ABORTED     2

};


/*
 * opcode fields
 */

#define AHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define AHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define AHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scattter gather*/
#define AHA_RESET_CCB		0x81	/* SCSI Bus reset */


/*
 * aha_ccb.host_stat values
 */

#define AHA_OK		0x00	/* cmd ok */
#define AHA_LINK_OK	0x0a	/* Link cmd ok */
#define AHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define AHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define AHA_OVER_UNDER	0x12	/* Data over/under run */
#define AHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define AHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define AHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define AHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define AHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define AHA_INV_TARGET	0x18	/* Invalid target direction */
#define AHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define AHA_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define AHA_ABORTED      42




struct aha_setup
{
	u_char	sync_neg:1;
	u_char	parity:1;
	u_char	:6;
	u_char	speed;
	u_char	bus_on;
	u_char	bus_off;
	u_char	num_mbx;
	u_char	mbx[3];
	struct
	{
		u_char	offset:4;
		u_char	period:3;
		u_char	valid:1;
	}sync[8];
	u_char	disc_sts;
};

struct	aha_config
{
	u_char	chan;
	u_char	intr;
	u_char	scsi_dev:3;
	u_char	:5;
};

struct	aha_inquire
{
	u_char	boardid;		/* type of board */
					/* 0x20 = BusLogic 545, but it gets
					   the command wrong, only returns
					   one byte */
					/* 0x31 = AHA-1540 */
					/* 0x41 = AHA-1540A/1542A/1542B */
					/* 0x42 = AHA-1640 */
					/* 0x43 = AHA-1542C */
					/* 0x44 = AHA-1542CF */
	u_char	spec_opts;		/* special options ID */
					/* 0x41 = Board is standard model */
	u_char	revision_1;		/* firmware revision [0-9A-Z] */
	u_char	revision_2;		/* firmware revision [0-9A-Z] */
};

struct	aha_extbios
{
	u_char	flags;			/* Bit 3 == 1 extended bios enabled */
	u_char	mailboxlock;		/* mail box lock code to unlock it */
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80


/*********************************** end of board definitions***************/


#ifdef	MACH
#define PHYSTOKV(x)	phystokv(x)
#define KVTOPHYS(x)	kvtophys(x)
#else	MACH
#ifdef	__386BSD__
#define KVTOPHYS(x)	vtophys(x)
#else	__386BSD__
#endif	__386BSD__
#endif	MACH
#define	AHA_DMA_PAGES	AHA_NSEG

#define PAGESIZ 	4096
#define INVALIDATE_CACHE {asm volatile( ".byte	0x0F ;.byte 0x08" ); }


u_char			aha_scratch_buf[256];
#ifdef	MACH
caddr_t			aha_base[NAHA];		/* base port for each board */
#else
short			aha_base[NAHA];		/* base port for each board */
#endif
struct	aha_mbx		aha_mbx[NAHA];
struct	aha_ccb		*aha_ccb_free[NAHA];
struct	aha_ccb		aha_ccb[NAHA][AHA_MBX_SIZE];
struct	scsi_xfer	aha_scsi_xfer[NAHA];
struct	isa_dev		*ahainfo[NAHA];
struct	aha_ccb		*aha_get_ccb();
int			aha_int[NAHA];
int			aha_dma[NAHA];
int			aha_scsi_dev[NAHA];
int			aha_initialized[NAHA];
#ifdef	OSF
int			aha_attached[NAHA];
#endif	OSF
#ifdef	AHADEBUG
int			aha_debug = 1;
#endif	/*AHADEBUG*/

int ahaprobe(), ahaattach(), ahaintr();
#ifdef	MACH
struct	isa_driver	ahadriver = { ahaprobe, 0, ahaattach, "aha", 0, 0, 0};
int			(*ahaintrs[])() = {ahaintr, 0};
#endif
#ifdef	__386BSD__
struct	isa_driver	ahadriver = { ahaprobe, ahaattach, "aha",};
#endif	__386BSD__
static int		ahaunit = 0;


#define aha_abortmbx(mbx) \
	(mbx)->cmd = AHA_MBO_ABORT; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);
#define aha_startmbx(mbx) \
	(mbx)->cmd = AHA_MBO_START; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);



int	aha_scsi_cmd();
int	aha_timeout();
void	ahaminphys();
long int	aha_adapter_info();

struct	scsi_switch	aha_switch =
{
	aha_scsi_cmd,
	ahaminphys,
	0,
	0,
	aha_adapter_info,
	"aha",
	0,0
};	
#define AHA_CMD_TIMEOUT_FUDGE	200	/* multiplied to get Secs	*/
#define AHA_RESET_TIMEOUT	1000000 /* time to wait for reset	*/
#define AHA_SCSI_TIMEOUT_FUDGE	20	/* divided by for mSecs		*/


/***********************************************************************\
* aha_cmd(unit,icnt, ocnt,wait, retval, opcode, args)			*
* Activate Adapter command						*
*	icnt:	number of args (outbound bytes written after opcode)	*
*	ocnt:	number of expected returned bytes			*
*	wait:	number of seconds to wait for response			*
*	retval:	buffer where to place returned bytes			*
*	opcode:	opcode AHA_NOP, AHA_MBX_INIT, AHA_START_SCSI ...	*
*	args:	parameters						*
*									*
* Performs an adapter command through the ports. Not to be confused	*
*	with a scsi command, which is read in via the dma		*
* One of the adapter commands tells it to read in a scsi command	*
\***********************************************************************/


aha_cmd(unit,icnt, ocnt, wait,retval, opcode, args)

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
		wait = AHA_CMD_TIMEOUT_FUDGE * delaycount; 
	else
		wait *= AHA_CMD_TIMEOUT_FUDGE * delaycount; 
	/*******************************************************\
	* Wait for the adapter to go idle, unless it's one of	*
	* the commands which don't need this			*
	\*******************************************************/
	if (opcode != AHA_MBX_INIT && opcode != AHA_START_SCSI)
	{
		i = AHA_CMD_TIMEOUT_FUDGE * delaycount; /* 1 sec?*/
		while (--i)
		{
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_IDLE)
			{
				break;
			}
		}
		if (!i)
		{
			printf("aha%d: aha_cmd, host not idle(0x%x)\n",
				unit,sts);
			return(ENXIO);
		}
	}
	/*******************************************************\
	* Now that it is idle, if we expect output, preflush the*
	* queue feeding to us.					*
	\*******************************************************/
	if (ocnt)
	{
		while((inb(AHA_CTRL_STAT_PORT)) & AHA_DF)
			inb(AHA_CMD_DATA_PORT);
	}
			
	/*******************************************************\
	* Output the command and the number of arguments given	*
	* for each byte, first check the port is empty.		*
	\*******************************************************/
	icnt++;		/* include the command */
	while (icnt--)
	{
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i=0; i< wait; i++)
		{
			sts = inb(AHA_CTRL_STAT_PORT);
			if (!(sts & AHA_CDF))
				break;
		}
		if (i >=  wait)
		{
			printf("aha%d: aha_cmd, cmd/data port full\n",unit);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST); 
			return(ENXIO);
		}
		outb(AHA_CMD_DATA_PORT, (u_char)(*ic++));
	}
	/*******************************************************\
	* If we expect input, loop that many times, each time,	*
	* looking for the data register to have valid data	*
	\*******************************************************/
	while (ocnt--)
	{
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i=0; i< wait; i++)
		{
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts  & AHA_DF)
				break;
		}
		if (i >=  wait)
		{
			printf("aha%d: aha_cmd, cmd/data port empty %d\n",
				unit,ocnt);
			return(ENXIO);
		}
		oc = inb(AHA_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*******************************************************\
	* Wait for the board to report a finised instruction	*
	\*******************************************************/
	i=AHA_CMD_TIMEOUT_FUDGE * delaycount;	/* 1 sec? */
	while (--i)
	{
		sts = inb(AHA_INTR_PORT);
		if (sts & AHA_HACC)
		{
			break;
		}
	}
	if (!i)
	{
		printf("aha%d: aha_cmd, host not finished(0x%x)\n",unit,sts);
		return(ENXIO);
	}
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	return(0);
}

/*******************************************************\
* Check if the device can be found at the port given	*
* and if so, set it up ready for further work		*
* as an argument, takes the isa_dev structure from	*
* autoconf.c						*
\*******************************************************/
ahaprobe(dev)
struct isa_dev *dev;
{
 	int	unit = ahaunit;
#if defined(OSF)
 	static ihandler_t aha_handler[NAHA];
 	static ihandler_id_t *aha_handler_id[NAHA];
 	register ihandler_t *chp = &aha_handler[unit];;
#endif /* defined(OSF) */

	/***********************************************\
	* find unit and check we have that many defined	*
	\***********************************************/
	dev->dev_unit = unit;
	aha_base[unit] = dev->dev_addr;
	if(unit >= NAHA) 
	{
		printf("aha%d: unit number too high\n",unit);
		return(0);
	}
	/***********************************************\
	* Try initialise a unit at this location	*
	* sets up dma and bus speed, loads aha_int[unit]*
	\***********************************************/
	if (aha_init(unit) != 0)
	{
		return(0);
	}

	/***********************************************\
	* If it's there, put in it's interrupt vectors	*
	\***********************************************/
#if !defined(OSF)
#if	defined MACH
	iunit[aha_int[unit]] =unit;
	ivect[aha_int[unit]] = ahaintr;
	intpri[aha_int[unit]] = dev->dev_spl;
	form_pic_mask();
	/*take_dev_irq(dev);*/
#else
#ifdef	__386BSD__
	dev->id_irq = (1 << aha_int[unit]);
	dev->id_drq = aha_dma[unit];
#endif	__386BSD__
#endif
#else /* !defined(OSF) */
 
	dev->dev_pic = aha_dma[unit];
 	chp->ih_level = dev->dev_pic;
 	chp->ih_handler = dev->dev_intr[0];
 	chp->ih_resolver = i386_resolver;
 	chp->ih_rdev = dev;
 	chp->ih_stats.intr_type = INTR_DEVICE;
 	chp->ih_stats.intr_cnt = 0;
 	chp->ih_hparam[0].intparam = unit;
 	if ((aha_handler_id[unit] = handler_add(chp)) != NULL)
 		handler_enable(aha_handler_id[unit]);
 	else
 		panic("Unable to add aha interrupt handler");
#endif /* !defined(OSF) */
#ifndef	__386BSD__
	printf("port=%x spl=%d\n", dev->dev_addr, dev->dev_spl);
#endif	__386BSD__
	ahaunit ++;
	return(1);
}

/***********************************************\
* Attach all the sub-devices we can find	*
\***********************************************/
ahaattach(dev)
struct	isa_dev	*dev;
{
	int	unit = dev->dev_unit;

	/***********************************************\
	* ask the adapter what subunits are present	*
	\***********************************************/
	scsi_attachdevs( unit, aha_scsi_dev[unit], &aha_switch);
#if defined(OSF)
	aha_attached[unit]=1;
#endif /* defined(OSF) */
	return;
}

/***********************************************\
* Return some information to the caller about	*
* the adapter and it's capabilities		*
\***********************************************/
long int aha_adapter_info(unit)
int	unit;
{
	return(2);	/* 2 outstanding requests at a time per device */
}

/***********************************************\
* Catch an interrupt from the adaptor		*
\***********************************************/
ahaintr(unit)
{
	struct aha_ccb *ccb;
	unsigned char stat;
	register i;

#ifdef	AHADEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("ahaintr ");
#endif	/*AHADEBUG*/
	/***********************************************\
	* First acknowlege the interrupt, Then if it's	*
	* not telling about a completed operation	*
	* just return. 					*
	\***********************************************/
	stat = inb(AHA_INTR_PORT);
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
#ifdef	AHADEBUG
	if(scsi_debug & TRACEINTERRUPTS)
		printf("int ");
#endif	/*AHADEBUG*/
	if (! (stat & AHA_MBIF))
		return(1);
#ifdef	AHADEBUG
	if(scsi_debug & TRACEINTERRUPTS)
		printf("b ");
#endif	/*AHADEBUG*/
#if defined(OSF)
	if (!aha_attached[unit])
	{
		return(1);
	}
#endif /* defined(OSF) */
	/***********************************************\
	* If it IS then process the competed operation	*
	\***********************************************/
	for (i = 0; i < AHA_MBX_SIZE; i++)
	{
		if (aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE) 
		{
			ccb = (struct aha_ccb *)PHYSTOKV(
				(_3btol(aha_mbx[unit].mbi[i].ccb_addr)));

			if((stat =  aha_mbx[unit].mbi[i].stat) != AHA_MBI_OK)
			{
				switch(stat)
				{
				case	AHA_MBI_ABORT:
#ifdef	AHADEBUG
					if(aha_debug)
					    printf("abort");
#endif	/*AHADEBUG*/
					ccb->host_stat = AHA_ABORTED;
					break;

				case	AHA_MBI_UNKNOWN:
					ccb = (struct aha_ccb *)0;
#ifdef	AHADEBUG
					if(aha_debug)
					     printf("unknown ccb for abort ");
#endif	/*AHADEBUG*/
					/* may have missed it */
					/* no such ccb known for abort */

				case	AHA_MBI_ERROR:
					break;

				default:
					panic("Impossible mbxi status");

				}
#ifdef	AHADEBUG
				if( aha_debug && ccb )
				{
					u_char	*cp;
					cp = (u_char *)(&(ccb->scsi_cmd));
					printf("op=%x %x %x %x %x %x\n", 
						cp[0], cp[1], cp[2],
						cp[3], cp[4], cp[5]);
					printf("stat %x for mbi[%d]\n"
						, aha_mbx[unit].mbi[i].stat, i);
					printf("addr = 0x%x\n", ccb);
				}
#endif	/*AHADEBUG*/
			}
			if(ccb)
			{
				untimeout(aha_timeout,ccb);
				aha_done(unit,ccb);
			}
			aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
		}
	}
	return(1);
}

/***********************************************\
* A ccb (and hence a mbx-out is put onto the 	*
* free list.					*
\***********************************************/
aha_free_ccb(unit,ccb, flags)
struct aha_ccb *ccb;
{
	unsigned int opri;
	
#ifdef	AHADEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("ccb%d(0x%x)> ",unit,flags);
#endif	/*AHADEBUG*/
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();

	ccb->next = aha_ccb_free[unit];
	aha_ccb_free[unit] = ccb;
	ccb->flags = CCB_FREE;
	/***********************************************\
	* If there were none, wake abybody waiting for	*
	* one to come free, starting with queued entries*
	\***********************************************/
	if (!ccb->next) {
		wakeup(&aha_ccb_free[unit]);
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
}

/***********************************************\
* Get a free ccb (and hence mbox-out entry)	*
\***********************************************/
struct aha_ccb *
aha_get_ccb(unit,flags)
{
	unsigned opri;
	struct aha_ccb *rc;

#ifdef	AHADEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("<ccb%d(0x%x) ",unit,flags);
#endif	/*AHADEBUG*/
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();
	/***********************************************\
	* If we can and have to, sleep waiting for one	*
	* to come free					*
	\***********************************************/
	while ((!(rc = aha_ccb_free[unit])) && (!(flags & SCSI_NOSLEEP)))
	{
		sleep(&aha_ccb_free[unit], PRIBIO);
	}
	if (rc) 
	{
		aha_ccb_free[unit] = aha_ccb_free[unit]->next;
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
aha_done(unit,ccb)
struct aha_ccb *ccb;
{
	struct	scsi_sense_data *s1,*s2;
	struct	scsi_xfer *xs = ccb->xfer;

#ifdef	AHADEBUG
	if(scsi_debug & PRINTROUTINES )
		printf("aha_done ");
#endif	/*AHADEBUG*/
	/***********************************************\
	* Otherwise, put the results of the operation	*
	* into the xfer and call whoever started it	*
	\***********************************************/
	if(!(xs->flags & INUSE))
	{
		printf("aha%d: exiting but not in use!\n",unit);
		Debugger();
	}
	if (  	(	ccb->host_stat != AHA_OK 
			|| ccb->target_stat != SCSI_OK)
	      && (!(xs->flags & SCSI_ERR_OK)))
	{
		s1 = (struct scsi_sense_data *)(((char *)(&ccb->scsi_cmd)) 
				+ ccb->scsi_cmd_length);
		s2 = &(xs->sense);

		if(ccb->host_stat)
		{
			switch(ccb->host_stat)
			{
			case	AHA_ABORTED:
			case	AHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
#ifdef	AHADEBUG
				if (aha_debug > 1)
				{
					printf("host_stat%x\n",
						ccb->host_stat);
				}
#endif	/*AHADEBUG*/
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
#ifdef	AHADEBUG
				if (aha_debug > 1)
				{
					printf("target_stat%x\n",
						ccb->target_stat);
				}
#endif	/*AHADEBUG*/
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
	else		/* All went correctly  OR errors expected */
	{
		xs->resid = 0;
	}
	xs->flags |= ITSDONE;
	aha_free_ccb(unit,ccb, xs->flags);
	if(xs->when_done)
		(*(xs->when_done))(xs->done_arg,xs->done_arg2);
}


/***********************************************\
* Start the board, ready for normal operation	*
\***********************************************/
aha_init(unit)
int	unit;
{
	unsigned char ad[3];
	volatile int i,sts;
	struct	aha_config conf;
	struct	aha_inquire inquire;
	struct	aha_extbios extbios;

	/***********************************************\
	* reset board, If it doesn't respond, assume 	*
	* that it's not there.. good for the probe	*
	\***********************************************/

	outb(AHA_CTRL_STAT_PORT, AHA_HRST|AHA_SRST);

	for (i=0; i < AHA_RESET_TIMEOUT; i++)
	{
		sts = inb(AHA_CTRL_STAT_PORT) ;
		if ( sts == (AHA_IDLE | AHA_INIT))
			break;
	}
	if (i >= AHA_RESET_TIMEOUT)
	{
#ifdef	AHADEBUG
		if (aha_debug)
			printf("aha_init: No answer from adaptec board\n");
#endif	/*AHADEBUG*/
		return(ENXIO);
	}
	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is
	 */
	aha_cmd(unit, 0, sizeof(inquire), 1 ,&inquire, AHA_INQUIRE);
#ifdef	AHADEBUG
	printf("aha%d: inquire %x, %x, %x, %x\n",
		unit,
		inquire.boardid, inquire.spec_opts,
		inquire.revision_1, inquire.revision_2);
#endif	/* AHADEBUG */
	/*
	 * XXX The Buslogic 545S gets the AHA_INQUIRE command wrong,
	 * they only return one byte which causes us to print an error,
	 * so if the boardid comes back as 0x20, tell the user why they
	 * get the "cmd/data port empty" message
	 */
	if (inquire.boardid == 0x20) {
		/* looks like a Buslogic 545 */
		printf ("aha%d: above cmd/data port empty do to Buslogic 545\n",
			unit);
	}
	/*
	 * If we are a 1542C or 1542CF disable the extended bios so that the
	 * mailbox interface is unlocked.
	 * No need to check the extended bios flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	if ((inquire.boardid == 0x43) || (inquire.boardid == 0x44)) {
		aha_cmd(unit, 0, sizeof(extbios), 0, &extbios, AHA_EXT_BIOS);
#ifdef	AHADEBUG
		printf("aha%d: extended bios flags %x\n", unit, extbios.flags);
#endif	/* AHADEBUG */
		printf("aha%d: 1542C/CF detected, unlocking mailbox\n");
		aha_cmd(unit, 2, 0, 0, 0, AHA_MBX_ENABLE,
			0, extbios.mailboxlock);
	}
	/***********************************************\
	* Setup dma channel from jumpers and save int	*
	* level						*
	\***********************************************/
#ifdef	__386BSD__
	printf("aha%d: reading board settings, ",unit);
#define	PRNT(x) printf(x)
#else	__386BSD__
	printf("aha%d:",unit);
#define	PRNT(x) printf(x)
#endif	__386BSD__
	DELAY(10000);	/* for Bustek 545 */
	aha_cmd(unit,0, sizeof(conf), 0 ,&conf, AHA_CONF_GET);
	switch(conf.chan)
	{
	case	CHAN0:
		outb(0x0b, 0x0c);
		outb(0x0a, 0x00);
		aha_dma[unit] = 0;
		PRNT("dma=0 ");
		break;
	case	CHAN5:
		outb(0xd6, 0xc1);
		outb(0xd4, 0x01);
		aha_dma[unit] = 5;
		PRNT("dma=5 ");
		break;
	case	CHAN6:
		outb(0xd6, 0xc2);
		outb(0xd4, 0x02);
		aha_dma[unit] = 6;
		PRNT("dma=6 ");
		break;
	case	CHAN7:
		outb(0xd6, 0xc3);
		outb(0xd4, 0x03);
		aha_dma[unit] = 7;
		PRNT("dma=7 ");
		break;
	default:
		printf("illegal dma jumper setting\n");
		return(EIO);
	}
	switch(conf.intr)
	{
	case	INT9:
		aha_int[unit] = 9;
		PRNT("int=9 ");
		break;
	case	INT10:
		aha_int[unit] = 10;
		PRNT("int=10 ");
		break;
	case	INT11:
		aha_int[unit] = 11;
		PRNT("int=11 ");
		break;
	case	INT12:
		aha_int[unit] = 12;
		PRNT("int=12 ");
		break;
	case	INT14:
		aha_int[unit] = 14;
		PRNT("int=14 ");
		break;
	case	INT15:
		aha_int[unit] = 15;
		PRNT("int=15 ");
		break;
	default:
		printf("illegal int jumper setting\n");
		return(EIO);
	}
	/* who are we on the scsi bus */
	aha_scsi_dev[unit] = conf.scsi_dev;


	/***********************************************\
	* Initialize memory transfer speed		*
	\***********************************************/
/*
 * XXX This code seems to BREAK more boards than it makes
 * work right, we are just going to NOP this here...
 */ 
#if 0
	if(!(aha_set_bus_speed(unit)))
	{
		return(EIO);
	}
#else
	printf ("\n");
#endif
	

	/***********************************************\
	* Initialize mail box 				*
	\***********************************************/

	lto3b(KVTOPHYS(&aha_mbx[unit]), ad);

	aha_cmd(unit,4, 0, 0, 0, AHA_MBX_INIT,
			AHA_MBX_SIZE,
			ad[0],
			ad[1],
			ad[2]);


	/***********************************************\
	* link the ccb's with the mbox-out entries and	*
	* into a free-list				*
	\***********************************************/
	for (i=0; i < AHA_MBX_SIZE; i++) {
		aha_ccb[unit][i].next = aha_ccb_free[unit];
		aha_ccb_free[unit] = &aha_ccb[unit][i];
		aha_ccb_free[unit]->flags = CCB_FREE;
		aha_ccb_free[unit]->mbx = &aha_mbx[unit].mbo[i];
		lto3b(KVTOPHYS(aha_ccb_free[unit]), aha_mbx[unit].mbo[i].ccb_addr);
	}

	/***********************************************\
	* Note that we are going and return (to probe)	*
	\***********************************************/
	aha_initialized[unit]++;
	return(0);
}





void ahaminphys(bp)
struct	buf *bp;
{
#ifdef	MACH
#if !defined(OSF)
	bp->b_flags |= B_NPAGES;		/* can support scat/gather */
#endif /* !defined(OSF) */
#endif	MACH
/*	aha seems to explode with 17 segs (64k may require 17 segs) */
/* 	on old boards so use a max of 16 segs if you have problems here*/
	if(bp->b_bcount > ((AHA_NSEG - 1) * PAGESIZ))
	{
		bp->b_bcount = ((AHA_NSEG - 1) * PAGESIZ);
	}
}
	
/***********************************************\
* start a scsi operation given the command and	*
* the data address. Also needs the unit, target	*
* and lu					*
\***********************************************/
int	aha_scsi_cmd(xs)
struct scsi_xfer *xs;
{
	struct	scsi_sense_data *s1,*s2;
	struct aha_ccb *ccb;
	struct aha_scat_gath *sg;
	int	seg;	/* scatter gather seg being worked on */
	int i	= 0;
	int rc	=  0;
	int	thiskv;
	int	thisphys,nextphys;
	int	unit =xs->adapter;
	int	bytes_this_seg,bytes_this_page,datalen,flags;
	struct	iovec	*iovp;
	int	s;

#ifdef	AHADEBUG
	if(scsi_debug & PRINTROUTINES)
		printf("aha_scsi_cmd ");
#endif	/*AHADEBUG*/
	/***********************************************\
	* get a ccb (mbox-out) to use. If the transfer	*
	* is from a buf (possibly from interrupt time)	*
	* then we can't allow it to sleep		*
	\***********************************************/
	flags = xs->flags;
	if(!(flags & INUSE))
	{
		printf("aha%d: not in use!\n",unit);
		Debugger();
		xs->flags |= INUSE;
	}
	if(flags & ITSDONE)
	{
		printf("aha%d: Already done! check device retry code\n",unit);
		Debugger();
		xs->flags &= ~ITSDONE;
	}
	if(xs->bp) flags |= (SCSI_NOSLEEP); /* just to be sure */
	if (!(ccb = aha_get_ccb(unit,flags)))
	{
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

	if (ccb->mbx->cmd != AHA_MBO_FREE)
		printf("aha%d: MBO not free\n",unit);

	/***********************************************\
	* Put all the arguments for the xfer in the ccb	*
	\***********************************************/
	ccb->xfer		=	xs;
	if(flags & SCSI_RESET)
	{
		ccb->opcode	=	AHA_RESET_CCB;
	}
	else
	{
		/* can't use S/G if zero length */
		ccb->opcode	=	(xs->datalen?
						AHA_INIT_SCAT_GATH_CCB
						:AHA_INITIATOR_CCB);
	}
	ccb->target		=	xs->targ;;
	ccb->data_out		=	0;
	ccb->data_in		=	0;
	ccb->lun		=	xs->lu;
	ccb->scsi_cmd_length	=	xs->cmdlen;
	ccb->req_sense_length	=	sizeof(ccb->scsi_sense);

	if((xs->datalen) && (!(flags & SCSI_RESET)))
	{ /* can use S/G only if not zero length */
		lto3b(KVTOPHYS(ccb->scat_gath), ccb->data_addr );
		sg		=	ccb->scat_gath ;
		seg 		=	0;
		if(flags & SCSI_DATA_UIO)
		{
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			while ((datalen) && (seg < AHA_NSEG))
			{
				lto3b(iovp->iov_base,&(sg->seg_addr));
				lto3b(iovp->iov_len,&(sg->seg_len));
#ifdef	AHADEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x@0x%x)"
							,iovp->iov_len
							,iovp->iov_base);
#endif	/*AHADEBUG*/
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
		
#ifdef	AHADEBUG
			if(scsi_debug & SHOWSCATGATH)
				printf("%d @0x%x:- ",xs->datalen,xs->data);
#endif	/*AHADEBUG*/
			datalen		=	xs->datalen;
			thiskv		=	(int)xs->data;
			thisphys	=	KVTOPHYS(thiskv);
		
			while ((datalen) && (seg < AHA_NSEG))
			{
				bytes_this_seg	= 0;
	
				/* put in the base address */
				lto3b(thisphys,&(sg->seg_addr));
		
#ifdef	AHADEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("0x%x",thisphys);
#endif	/*AHADEBUG*/
	
				/* do it at least once */
				nextphys = thisphys;	
				while ((datalen) && (thisphys == nextphys))
				/***************************************\
				* This page is contiguous (physically)	*
				* with the the last, just extend the	*
				* length				*
				\***************************************/
				{
					/** how far to the end of the page ***/
					nextphys = (thisphys & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					bytes_this_page	= nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page	= min(bytes_this_page
								,datalen);
					bytes_this_seg	+= bytes_this_page;
					datalen		-= bytes_this_page;
		
					/**** get more ready for the next page ****/
					thiskv	= (thiskv & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					if(datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/***************************************\
				* next page isn't contiguous, finish the seg*
				\***************************************/
#ifdef	AHADEBUG
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x)",bytes_this_seg);
#endif	/*AHADEBUG*/
				lto3b(bytes_this_seg,&(sg->seg_len));	
				sg++;
				seg++;
			}
		}
		lto3b(seg * sizeof(struct aha_scat_gath),ccb->data_length);
#ifdef	AHADEBUG
		if(scsi_debug & SHOWSCATGATH)
			printf("\n");
#endif	/*AHADEBUG*/
		if (datalen)
		{ /* there's still data, must have run out of segs! */
			printf("aha%d: aha_scsi_cmd, more than %d DMA segs\n",
				unit,AHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			aha_free_ccb(unit,ccb,flags);
			return(HAD_ERROR);
		}

	}
	else
	{	/* No data xfer, use non S/G values */
		lto3b(0, ccb->data_addr );
		lto3b(0,ccb->data_length);
	}
	lto3b(0, ccb->link_addr );
	/***********************************************\
	* Put the scsi command in the ccb and start it	*
	\***********************************************/
	if(!(flags & SCSI_RESET))
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmd_length);
#ifdef	AHADEBUG
	if(scsi_debug & SHOWCOMMANDS)
	{
		u_char	*b = (u_char *)&ccb->scsi_cmd;
		if(!(flags & SCSI_RESET))
		{
			int i = 0;
			printf("aha%d:%d:%d-"
				,unit
				,ccb->target
				,ccb->lun );
				while(i < ccb->scsi_cmd_length )
				{
					if(i) printf(",");
					 printf("%x",b[i++]);
				}
		}
		else
		{
			printf("aha%d:%d:%d-RESET- " 
				,unit 
				,ccb->target
				,ccb->lun
			);
		}
	}
#endif	/*AHADEBUG*/
	if (!(flags & SCSI_NOMASK))
	{
		s= splbio(); /* stop instant timeouts */
		timeout(aha_timeout,ccb,(xs->timeout * hz) / 1000);
		aha_startmbx(ccb->mbx);
		/***********************************************\
		* Usually return SUCCESSFULLY QUEUED		*
		\***********************************************/
		splx(s);
#ifdef	AHADEBUG
		if(scsi_debug & TRACEINTERRUPTS)
			printf("sent ");
#endif	/*AHADEBUG*/
		return(SUCCESSFULLY_QUEUED);
	}
	aha_startmbx(ccb->mbx);
#ifdef	AHADEBUG
	if(scsi_debug & TRACEINTERRUPTS)
		printf("cmd_sent, waiting ");
#endif	/*AHADEBUG*/
	/***********************************************\
	* If we can't use interrupts, poll on completion*
	\***********************************************/
	{
		int done = 0;
		int count = delaycount * xs->timeout / AHA_SCSI_TIMEOUT_FUDGE;
		while((!done) && count)
		{
			i=0;
			while ( (!done) && i<AHA_MBX_SIZE)
			{
				if ((aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE )
				   && (PHYSTOKV(_3btol(aha_mbx[unit].mbi[i].ccb_addr)
					== (int)ccb)))
				{
					aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
					aha_done(unit,ccb);
					done++;
				}
				i++;
			}
			count--;
		}
		if (!count)
		{
			if (!(xs->flags & SCSI_SILENT))
				printf("aha%d: cmd fail\n",unit);
			aha_abortmbx(ccb->mbx);
			count = delaycount * 2000 / AHA_SCSI_TIMEOUT_FUDGE;
			while((!done) && count)
			{
				i=0;
				while ( (!done) && i<AHA_MBX_SIZE)
				{
					if ((aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE )
				   	&& (PHYSTOKV(_3btol(aha_mbx[unit].mbi[i].ccb_addr)
						== (int)ccb)))
					{
						aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
						aha_done(unit,ccb);
						done++;
					}
					i++;
				}
				count--;
			}
			if(!count)
			{
				printf("aha%d: abort failed in wait\n",unit);
				ccb->mbx->cmd = AHA_MBO_FREE;
			}
			aha_free_ccb(unit,ccb,flags);
			ahaintr(unit);
			xs->error = XS_DRIVER_STUFFUP;
			return(HAD_ERROR);
		}
		ahaintr(unit);
		if(xs->error) return(HAD_ERROR);
		return(COMPLETE);

	} 
}
/***************************************************************\
* try each speed in turn, when we find one that works, use	*
* the NEXT one for a safety margin, unless that doesn't exist	*
* or doesn't work. returns the nSEC value of the time used	*
* or 0 if it could get a working speed ( or the NEXT speed 	*
* failed)							*
\***************************************************************/

int	aha_set_bus_speed(unit)
int	unit;
{
	int	speed;
	int	retval,retval2;

#ifdef	EISA
	speed = 0; /* start at the fastest */
#else	EISA
	speed = 1; /* 100 ns can crash some ISA busses (!?!) */
#endif	EISA
	while (1)
	{
		retval = aha_bus_speed_check(unit,speed);
		if(retval == HAD_ERROR) 
		{
			printf("no working bus speed!!!\n");
			return(0);
		}
		if(retval == 0)
		{
			speed++;
		}
		else	/* Go one slower to be safe */
		{	/* unless eisa at 100 ns.. trust it */
			if(speed != 0)
			{
				speed++;
			}
			printf("%d nSEC ok, using ",retval);
			retval2 = aha_bus_speed_check(unit,speed);
			if(retval2 == HAD_ERROR) /* retval is slowest already */
			{
				printf("marginal ");
				retval2 = retval;
			}
			if(retval2)
			{
				printf("%d nSEC\n",retval2);
				return(retval2);
			}
			else
			{
				printf(".. slower failed, abort\n",retval);
				return(0);
			}

		}
	}
}

/***************************************************************\
* Set the DMA speed to the Nth speed and try an xfer. If it	*
* fails return 0, if it succeeds return the nSec value selected	*
* If there is no such speed return HAD_ERROR.			*
\***************************************************************/
static	struct bus_speed
{
	char	arg;
	int	nsecs;
}aha_bus_speeds[] =
{
	{0x88,100},
	{0x99,150},
	{0xaa,200},
	{0xbb,250},
	{0xcc,300},
	{0xdd,350},
	{0xee,400},
	{0xff,450}
};
static	char aha_test_string[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz!@";

int aha_bus_speed_check(unit,speed)
int	unit,speed;
{
	int	numspeeds = sizeof(aha_bus_speeds)/sizeof(struct bus_speed);
	u_char	ad[3];

	/*******************************************************\
	* Check we have such an entry				*
	\*******************************************************/
	if(speed >= numspeeds) return(HAD_ERROR);	/* illegal speed */
	
	/*******************************************************\
	* Set the dma-speed					*
	\*******************************************************/
	aha_cmd(unit,1, 0, 0, 0, AHA_SPEED_SET,aha_bus_speeds[speed].arg);

	/*******************************************************\
	* put the test data into the buffer and calculate	*
	* it's address. Read it onto the board			*
	\*******************************************************/
	strcpy(aha_scratch_buf,aha_test_string);
	lto3b(KVTOPHYS(aha_scratch_buf),ad);

	aha_cmd(unit,3, 0, 0, 0, AHA_WRITE_FIFO, ad[0], ad[1], ad[2]);

	/*******************************************************\
	* clear the buffer then copy the contents back from the	*
	* board.						*
	\*******************************************************/
	bzero(aha_scratch_buf,54);	/* 54 bytes transfered by test */

	aha_cmd(unit,3, 0, 0, 0, AHA_READ_FIFO, ad[0], ad[1], ad[2]);

	/*******************************************************\
	* Compare the original data and the final data and	*
	* return the correct value depending upon the result	*
	\*******************************************************/
	if(strcmp(aha_test_string,aha_scratch_buf))
	{	/* copy failed.. assume too fast */
		return(0);
	}
	else
	{	/* copy succeded assume speed ok */
		return(aha_bus_speeds[speed].nsecs);
	}
}



aha_timeout(struct aha_ccb *ccb)
{
	int	unit;
	int	s	= splbio();

	unit = ccb->xfer->adapter;
	printf("aha%d: device %d timed out ",unit ,ccb->xfer->targ);

	/***************************************\
	* If The ccb's mbx is not free, then	*
	* the board has gone south		*
	\***************************************/
	if(ccb->mbx->cmd != AHA_MBO_FREE)
	{
		printf("aha%d: not taking commands!\n",unit);
		Debugger();
	}
	/***************************************\
	* If it has been through before, then	*
	* a previous abort has failed, don't	*
	* try abort again			*
	\***************************************/
	if(ccb->flags == CCB_ABORTED) /* abort timed out */
	{
		printf(" AGAIN\n");
		ccb->xfer->retries = 0;	/* I MEAN IT ! */
		ccb->host_stat = AHA_ABORTED;
		aha_done(unit,ccb);
	}
	else	/* abort the operation that has timed out */
	{
		printf("\n");
		aha_abortmbx(ccb->mbx);
				/* 2 secs for the abort */
		timeout(aha_timeout,ccb,2 * hz);
		ccb->flags = CCB_ABORTED;
	}
	splx(s);
}

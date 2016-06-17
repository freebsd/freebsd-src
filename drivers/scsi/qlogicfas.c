/*----------------------------------------------------------------*/
/*
   Qlogic linux driver - work in progress. No Warranty express or implied.
   Use at your own risk.  Support Tort Reform so you won't have to read all
   these silly disclaimers.

   Copyright 1994, Tom Zerucha.   
   tz@execpc.com
   
   Additional Code, and much appreciated help by
   Michael A. Griffith
   grif@cs.ucr.edu

   Thanks to Eric Youngdale and Dave Hinds for loadable module and PCMCIA
   help respectively, and for suffering through my foolishness during the
   debugging process.

   Reference Qlogic FAS408 Technical Manual, 53408-510-00A, May 10, 1994
   (you can reference it, but it is incomplete and inaccurate in places)

   Version 0.46 1/30/97 - kernel 1.2.0+

   Functions as standalone, loadable, and PCMCIA driver, the latter from
   Dave Hinds' PCMCIA package.

   Redistributable under terms of the GNU General Public License

*/
/*----------------------------------------------------------------*/
/* Configuration */

/* Set the following to 2 to use normal interrupt (active high/totempole-
   tristate), otherwise use 0 (REQUIRED FOR PCMCIA) for active low, open
   drain */
#define QL_INT_ACTIVE_HIGH 2

/* Set the following to 1 to enable the use of interrupts.  Note that 0 tends
   to be more stable, but slower (or ties up the system more) */
#define QL_USE_IRQ 1

/* Set the following to max out the speed of the PIO PseudoDMA transfers,
   again, 0 tends to be slower, but more stable.  */
#define QL_TURBO_PDMA 1

/* This should be 1 to enable parity detection */
#define QL_ENABLE_PARITY 1

/* This will reset all devices when the driver is initialized (during bootup).
   The other linux drivers don't do this, but the DOS drivers do, and after
   using DOS or some kind of crash or lockup this will bring things back
   without requiring a cold boot.  It does take some time to recover from a
   reset, so it is slower, and I have seen timeouts so that devices weren't
   recognized when this was set. */
#define QL_RESET_AT_START 0

/* crystal frequency in megahertz (for offset 5 and 9)
   Please set this for your card.  Most Qlogic cards are 40 Mhz.  The
   Control Concepts ISA (not VLB) is 24 Mhz */
#define XTALFREQ	40

/**********/
/* DANGER! modify these at your own risk */
/* SLOWCABLE can usually be reset to zero if you have a clean setup and
   proper termination.  The rest are for synchronous transfers and other
   advanced features if your device can transfer faster than 5Mb/sec.
   If you are really curious, email me for a quick howto until I have
   something official */
/**********/

/*****/
/* config register 1 (offset 8) options */
/* This needs to be set to 1 if your cabling is long or noisy */
#define SLOWCABLE 1

/*****/
/* offset 0xc */
/* This will set fast (10Mhz) synchronous timing when set to 1
   For this to have an effect, FASTCLK must also be 1 */
#define FASTSCSI 0

/* This when set to 1 will set a faster sync transfer rate */
#define FASTCLK 0
/*(XTALFREQ>25?1:0)*/

/*****/
/* offset 6 */
/* This is the sync transfer divisor, XTALFREQ/X will be the maximum
   achievable data rate (assuming the rest of the system is capable
   and set properly) */
#define SYNCXFRPD 5
/*(XTALFREQ/5)*/

/*****/
/* offset 7 */
/* This is the count of how many synchronous transfers can take place
	i.e. how many reqs can occur before an ack is given.
	The maximum value for this is 15, the upper bits can modify
	REQ/ACK assertion and deassertion during synchronous transfers
	If this is 0, the bus will only transfer asynchronously */
#define SYNCOFFST 0
/* for the curious, bits 7&6 control the deassertion delay in 1/2 cycles
	of the 40Mhz clock. If FASTCLK is 1, specifying 01 (1/2) will
	cause the deassertion to be early by 1/2 clock.  Bits 5&4 control
	the assertion delay, also in 1/2 clocks (FASTCLK is ignored here). */

/*----------------------------------------------------------------*/
#ifdef PCMCIA
#undef QL_INT_ACTIVE_HIGH
#define QL_INT_ACTIVE_HIGH 0
#endif 

#include <linux/module.h>

#include <linux/blk.h>	/* to get disk capacity */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/irq.h>
#include "sd.h"
#include "hosts.h"
#include "qlogicfas.h"
#include <linux/stat.h>

#ifdef PCMCIA
#undef MODULE
#endif 

/*----------------------------------------------------------------*/
/* driver state info, local to driver */
static int	    qbase;	/* Port */
static int	    qinitid;	/* initiator ID */
static int	    qabort;	/* Flag to cause an abort */
static int	    qlirq = -1;	/* IRQ being used */
static char	    qinfo[80];	/* description */
static Scsi_Cmnd   *qlcmd;	/* current command being processed */

static int	    qlcfg5 = ( XTALFREQ << 5 );	/* 15625/512 */
static int	    qlcfg6 = SYNCXFRPD;
static int	    qlcfg7 = SYNCOFFST;
static int	    qlcfg8 = ( SLOWCABLE << 7 ) | ( QL_ENABLE_PARITY << 4 );
static int	    qlcfg9 = ( ( XTALFREQ + 4 ) / 5 );
static int	    qlcfgc = ( FASTCLK << 3 ) | ( FASTSCSI << 4 );

struct	Scsi_Host	*hreg;	/* registered host structure */

/*----------------------------------------------------------------*/
/* The qlogic card uses two register maps - These macros select which one */
#define REG0 ( outb( inb( qbase + 0xd ) & 0x7f , qbase + 0xd ), outb( 4 , qbase + 0xd ))
#define REG1 ( outb( inb( qbase + 0xd ) | 0x80 , qbase + 0xd ), outb( 0xb4 | QL_INT_ACTIVE_HIGH , qbase + 0xd ))

/* following is watchdog timeout in microseconds */
#define WATCHDOG 5000000

/*----------------------------------------------------------------*/
/* the following will set the monitor border color (useful to find
   where something crashed or gets stuck at and as a simple profiler) */

#if 0
#define rtrc(i) {inb(0x3da);outb(0x31,0x3c0);outb((i),0x3c0);}
#else
#define rtrc(i) {}
#endif

/*----------------------------------------------------------------*/
/* local functions */
/*----------------------------------------------------------------*/
static void	ql_zap(void);
/* error recovery - reset everything */
void	ql_zap()
{
int	x;
unsigned long	flags;
	save_flags( flags );
	cli();
	x = inb(qbase + 0xd);
	REG0;
	outb(3, qbase + 3);				/* reset SCSI */
	outb(2, qbase + 3);				/* reset chip */
	if (x & 0x80)
		REG1;
	restore_flags( flags );
}

/*----------------------------------------------------------------*/
/* do pseudo-dma */
static int	ql_pdma(int phase, char *request, int reqlen)
{
int	j;
	j = 0;
	if (phase & 1) {	/* in */
#if QL_TURBO_PDMA
rtrc(4)
		/* empty fifo in large chunks */
		if( reqlen >= 128 && (inb( qbase + 8 ) & 2) ) { /* full */
			insl( qbase + 4, request, 32 );
			reqlen -= 128;
			request += 128;
		}
		while( reqlen >= 84 && !( j & 0xc0 ) ) /* 2/3 */
			if( (j=inb( qbase + 8 )) & 4 ) {
				insl( qbase + 4, request, 21 );
				reqlen -= 84;
				request += 84;
			}
		if( reqlen >= 44 && (inb( qbase + 8 ) & 8) ) {	/* 1/3 */
			insl( qbase + 4, request, 11 );
			reqlen -= 44;
			request += 44;
		}
#endif
		/* until both empty and int (or until reclen is 0) */
rtrc(7)
		j = 0;
		while( reqlen && !( (j & 0x10) && (j & 0xc0) ) ) {
			/* while bytes to receive and not empty */
			j &= 0xc0;
			while ( reqlen && !( (j=inb(qbase + 8)) & 0x10 ) ) {
				*request++ = inb(qbase + 4);
				reqlen--;
			}
			if( j & 0x10 )
				j = inb(qbase+8);

		}
	}
	else {	/* out */
#if QL_TURBO_PDMA
rtrc(4)
		if( reqlen >= 128 && inb( qbase + 8 ) & 0x10 ) { /* empty */
			outsl(qbase + 4, request, 32 );
			reqlen -= 128;
			request += 128;
		}
		while( reqlen >= 84 && !( j & 0xc0 ) ) /* 1/3 */
			if( !((j=inb( qbase + 8 )) & 8) ) {
				outsl( qbase + 4, request, 21 );
				reqlen -= 84;
				request += 84;
			}
		if( reqlen >= 40 && !(inb( qbase + 8 ) & 4 ) ) { /* 2/3 */
			outsl( qbase + 4, request, 10 );
			reqlen -= 40;
			request += 40;
		}
#endif
		/* until full and int (or until reclen is 0) */
rtrc(7)
		j = 0;
		while( reqlen && !( (j & 2) && (j & 0xc0) ) ) {
			/* while bytes to send and not full */
			while ( reqlen && !( (j=inb(qbase + 8)) & 2 ) ) {
				outb(*request++, qbase + 4);
				reqlen--;
			}
			if( j & 2 )
				j = inb(qbase+8);
		}
	}
/* maybe return reqlen */
	return inb( qbase + 8 ) & 0xc0;
}

/*----------------------------------------------------------------*/
/* wait for interrupt flag (polled - not real hardware interrupt) */
static int	ql_wai(void)
{
int	i,k;
	k = 0;
	i = jiffies + WATCHDOG;
	while (time_before(jiffies, i) && !qabort && !((k = inb(qbase + 4)) & 0xe0)) {
		barrier();
		cpu_relax();
	}
	if (time_after_eq(jiffies, i))
		return (DID_TIME_OUT);
	if (qabort)
		return (qabort == 1 ? DID_ABORT : DID_RESET);
	if (k & 0x60)
		ql_zap();
	if (k & 0x20)
		return (DID_PARITY);
	if (k & 0x40)
		return (DID_ERROR);
	return 0;
}

/*----------------------------------------------------------------*/
/* initiate scsi command - queueing handler */
static void	ql_icmd(Scsi_Cmnd * cmd)
{
unsigned int	    i;
unsigned long	flags;

	qabort = 0;

	save_flags( flags );
	cli();
	REG0;
/* clearing of interrupts and the fifo is needed */
	inb(qbase + 5); 			/* clear interrupts */
	if (inb(qbase + 5))			/* if still interrupting */
		outb(2, qbase + 3);		/* reset chip */
	else if (inb(qbase + 7) & 0x1f)
		outb(1, qbase + 3);		/* clear fifo */
	while (inb(qbase + 5)); 		/* clear ints */
	REG1;
	outb(1, qbase + 8);			/* set for PIO pseudo DMA */
	outb(0, qbase + 0xb);			/* disable ints */
	inb(qbase + 8); 			/* clear int bits */
	REG0;
	outb(0x40, qbase + 0xb);		/* enable features */

/* configurables */
	outb( qlcfgc , qbase + 0xc);
/* config: no reset interrupt, (initiator) bus id */
	outb( 0x40 | qlcfg8 | qinitid, qbase + 8);
	outb( qlcfg7 , qbase + 7 );
	outb( qlcfg6 , qbase + 6 );
/**/
	outb(qlcfg5, qbase + 5);		/* select timer */
	outb(qlcfg9 & 7, qbase + 9);			/* prescaler */
/*	outb(0x99, qbase + 5);	*/
	outb(cmd->target, qbase + 4);

	for (i = 0; i < cmd->cmd_len; i++)
		outb(cmd->cmnd[i], qbase + 2);
	qlcmd = cmd;
	outb(0x41, qbase + 3);	/* select and send command */
	restore_flags( flags );
}
/*----------------------------------------------------------------*/
/* process scsi command - usually after interrupt */
static unsigned int	ql_pcmd(Scsi_Cmnd * cmd)
{
unsigned int	i, j, k;
unsigned int	result; 		/* ultimate return result */
unsigned int	status; 		/* scsi returned status */
unsigned int	message;		/* scsi returned message */
unsigned int	phase;			/* recorded scsi phase */
unsigned int	reqlen; 		/* total length of transfer */
struct scatterlist	*sglist;	/* scatter-gather list pointer */
unsigned int	sgcount;		/* sg counter */

rtrc(1)
	j = inb(qbase + 6);
	i = inb(qbase + 5);
	if (i == 0x20) {
		return (DID_NO_CONNECT << 16);
	}
	i |= inb(qbase + 5);	/* the 0x10 bit can be set after the 0x08 */
	if (i != 0x18) {
		printk("Ql:Bad Interrupt status:%02x\n", i);
		ql_zap();
		return (DID_BAD_INTR << 16);
	}
	j &= 7; /* j = inb( qbase + 7 ) >> 5; */
/* correct status is supposed to be step 4 */
/* it sometimes returns step 3 but with 0 bytes left to send */
/* We can try stuffing the FIFO with the max each time, but we will get a
   sequence of 3 if any bytes are left (but we do flush the FIFO anyway */
	if(j != 3 && j != 4) {
		printk("Ql:Bad sequence for command %d, int %02X, cmdleft = %d\n", j, i, inb( qbase+7 ) & 0x1f );
		ql_zap();
		return (DID_ERROR << 16);
	}
	result = DID_OK;
	if (inb(qbase + 7) & 0x1f)	/* if some bytes in fifo */
		outb(1, qbase + 3);		/* clear fifo */
/* note that request_bufflen is the total xfer size when sg is used */
	reqlen = cmd->request_bufflen;
/* note that it won't work if transfers > 16M are requested */
	if (reqlen && !((phase = inb(qbase + 4)) & 6)) {	/* data phase */
rtrc(2)
		outb(reqlen, qbase);			/* low-mid xfer cnt */
		outb(reqlen >> 8, qbase+1);			/* low-mid xfer cnt */
		outb(reqlen >> 16, qbase + 0xe);	/* high xfer cnt */
		outb(0x90, qbase + 3);			/* command do xfer */
/* PIO pseudo DMA to buffer or sglist */
		REG1;
		if (!cmd->use_sg)
			ql_pdma(phase, cmd->request_buffer, cmd->request_bufflen);
		else {
			sgcount = cmd->use_sg;
			sglist = cmd->request_buffer;
			while (sgcount--) {
				if (qabort) {
					REG0;
					return ((qabort == 1 ? DID_ABORT : DID_RESET) << 16);
				}
				if (ql_pdma(phase, sglist->address, sglist->length))
					break;
				sglist++;
			}
		}
		REG0;
rtrc(2)
/* wait for irq (split into second state of irq handler if this can take time) */
		if ((k = ql_wai()))
			return (k << 16);
		k = inb(qbase + 5);	/* should be 0x10, bus service */
	}
/*** Enter Status (and Message In) Phase ***/
	k = jiffies + WATCHDOG;
	while ( time_before(jiffies, k) && !qabort && !(inb(qbase + 4) & 6));	/* wait for status phase */
	if ( time_after_eq(jiffies, k) ) {
		ql_zap();
		return (DID_TIME_OUT << 16);
	}
	while (inb(qbase + 5)); 				/* clear pending ints */
	if (qabort)
		return ((qabort == 1 ? DID_ABORT : DID_RESET) << 16);
	outb(0x11, qbase + 3);					/* get status and message */
	if ((k = ql_wai()))
		return (k << 16);
	i = inb(qbase + 5);					/* get chip irq stat */
	j = inb(qbase + 7) & 0x1f;				/* and bytes rec'd */
	status = inb(qbase + 2);
	message = inb(qbase + 2);
/* should get function complete int if Status and message, else bus serv if only status */
	if (!((i == 8 && j == 2) || (i == 0x10 && j == 1))) {
		printk("Ql:Error during status phase, int=%02X, %d bytes recd\n", i, j);
		result = DID_ERROR;
	}
	outb(0x12, qbase + 3);	/* done, disconnect */
rtrc(1)
	if ((k = ql_wai()))
		return (k << 16);
/* should get bus service interrupt and disconnect interrupt */
	i = inb(qbase + 5);	/* should be bus service */
	while (!qabort && ((i & 0x20) != 0x20)) {
		barrier();
		cpu_relax();
		i |= inb(qbase + 5);
	}
rtrc(0)
	if (qabort)
		return ((qabort == 1 ? DID_ABORT : DID_RESET) << 16);
	return (result << 16) | (message << 8) | (status & STATUS_MASK);
}

#if QL_USE_IRQ
/*----------------------------------------------------------------*/
/* interrupt handler */
static void	    ql_ihandl(int irq, void *dev_id, struct pt_regs * regs)
{
Scsi_Cmnd	   *icmd;
	REG0;
	if (!(inb(qbase + 4) & 0x80))	/* false alarm? */
		return;
	if (qlcmd == NULL) {		/* no command to process? */
		int	i;
		i = 16;
		while (i-- && inb(qbase + 5)); /* maybe also ql_zap() */
		return;
	}
	icmd = qlcmd;
	icmd->result = ql_pcmd(icmd);
	qlcmd = NULL;
/* if result is CHECK CONDITION done calls qcommand to request sense */
	(icmd->scsi_done) (icmd);
}

static void	    do_ql_ihandl(int irq, void *dev_id, struct pt_regs * regs)
{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);
	ql_ihandl(irq, dev_id, regs);
	spin_unlock_irqrestore(&io_request_lock, flags);
}
#endif

/*----------------------------------------------------------------*/
/* global functions */
/*----------------------------------------------------------------*/
/* non queued command */
#if QL_USE_IRQ
static void	qlidone(Scsi_Cmnd * cmd) {};		/* null function */
#endif

/* command process */
int	qlogicfas_command(Scsi_Cmnd * cmd)
{
int	k;
#if QL_USE_IRQ
	if (qlirq >= 0) {
		qlogicfas_queuecommand(cmd, qlidone);
		while (qlcmd != NULL);
		return cmd->result;
	}
#endif
/* non-irq version */
	if (cmd->target == qinitid)
		return (DID_BAD_TARGET << 16);
	ql_icmd(cmd);
	if ((k = ql_wai()))
		return (k << 16);
	return ql_pcmd(cmd);

}

#if QL_USE_IRQ
/*----------------------------------------------------------------*/
/* queued command */
int	qlogicfas_queuecommand(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
	if(cmd->target == qinitid) {
		cmd->result = DID_BAD_TARGET << 16;
		done(cmd);
		return 0;
	}

	cmd->scsi_done = done;
/* wait for the last command's interrupt to finish */
	while (qlcmd != NULL) {
		barrier();
		cpu_relax();
	}
	ql_icmd(cmd);
	return 0;
}
#else
int	qlogicfas_queuecommand(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
	return 1;
}
#endif

#ifdef PCMCIA
/*----------------------------------------------------------------*/
/* allow PCMCIA code to preset the port */
/* port should be 0 and irq to -1 respectively for autoprobing */
void	qlogicfas_preset(int port, int irq)
{
	qbase=port;
	qlirq=irq;
}
#endif

int	qlogicfas_release(struct Scsi_Host *hreg)
{
	release_region(qbase, 0x10);

	if (qlirq >= 0)
		free_irq(qlirq, hreg);

	scsi_unregister(hreg);

	return 0;
}

/*----------------------------------------------------------------*/
/* look for qlogic card and init if found */
int __QLINIT qlogicfas_detect(Scsi_Host_Template * host)
{
int	i, j;			/* these are only used by IRQ detect */
int	qltyp;			/* type of chip */
unsigned long	flags;

host->proc_name =  "qlogicfas";

/* Qlogic Cards only exist at 0x230 or 0x330 (the chip itself decodes the
   address - I check 230 first since MIDI cards are typically at 330

   Theoretically, two Qlogic cards can coexist in the same system.  This
   should work by simply using this as a loadable module for the second
   card, but I haven't tested this.
*/

	if( !qbase ) {
		for (qbase = 0x230; qbase < 0x430; qbase += 0x100) {
			if( !request_region( qbase , 0x10, "qlogicfas" ) )
				continue;
			REG1;
			if ( ( (inb(qbase + 0xe) ^ inb(qbase + 0xe)) == 7 )
			  && ( (inb(qbase + 0xe) ^ inb(qbase + 0xe)) == 7 ) )
				break;
			release_region(qbase, 0x10 );
		}
		if (qbase == 0x430)
			return 0;
	}
	else
		printk( "Ql: Using preset base address of %03x\n", qbase );

	qltyp = inb(qbase + 0xe) & 0xf8;
	qinitid = host->this_id;
	if (qinitid < 0)
		qinitid = 7;			/* if no ID, use 7 */
	outb(1, qbase + 8);			/* set for PIO pseudo DMA */
	REG0;
	outb(0x40 | qlcfg8 | qinitid, qbase + 8);	/* (ini) bus id, disable scsi rst */
	outb(qlcfg5, qbase + 5);		/* select timer */
	outb(qlcfg9, qbase + 9);			/* prescaler */
#if QL_RESET_AT_START
	outb( 3 , qbase + 3 );
	REG1;
	while( inb( qbase + 0xf ) & 4 );
	REG0;
#endif
#if QL_USE_IRQ
/* IRQ probe - toggle pin and check request pending */

	if( qlirq == -1 ) {
		save_flags( flags );
		cli();
		i = 0xffff;
		j = 3;
		outb(0x90, qbase + 3);	/* illegal command - cause interrupt */
		REG1;
		outb(10, 0x20); /* access pending interrupt map */
		outb(10, 0xa0);
		while (j--) {
			outb(0xb0 | QL_INT_ACTIVE_HIGH , qbase + 0xd);	/* int pin off */
			i &= ~(inb(0x20) | (inb(0xa0) << 8));	/* find IRQ off */
			outb(0xb4 | QL_INT_ACTIVE_HIGH , qbase + 0xd);	/* int pin on */
			i &= inb(0x20) | (inb(0xa0) << 8);	/* find IRQ on */
		}
		REG0;
		while (inb(qbase + 5)); 			/* purge int */
		j = -1;
		while (i)					/* find on bit */
			i >>= 1, j++;	/* should check for exactly 1 on */
		qlirq = j;
		restore_flags( flags );
	}
	else
		printk( "Ql: Using preset IRQ %d\n", qlirq );

	if (qlirq >= 0)
		host->can_queue = 1;
#endif
	hreg = scsi_register( host , 0 );	/* no host data */
	if (!hreg)
		goto err_release_mem;

#if QL_USE_IRQ
#ifdef PCMCIA
	if(request_irq(qlirq, do_ql_ihandl, SA_SHIRQ, "qlogicfas", hreg) < 0)
#else	
	if(request_irq(qlirq, do_ql_ihandl, SA_SHIRQ, "qlogicfas", hreg) < 0)
#endif	
	{
		scsi_unregister(hreg);
		goto err_release_mem;
	}
#endif
	hreg->io_port = qbase;
	hreg->n_io_port = 16;
	hreg->dma_channel = -1;
	if( qlirq >= 0 )
		hreg->irq = qlirq;

	sprintf(qinfo, "Qlogicfas Driver version 0.46, chip %02X at %03X, IRQ %d, TPdma:%d",
	    qltyp, qbase, qlirq, QL_TURBO_PDMA );
	host->name = qinfo;

	return 1;

 err_release_mem:
	release_region(qbase, 0x10);
	if (qlirq >= 0)
		free_irq(qlirq, hreg);
	return 0;

}

/*----------------------------------------------------------------*/
/* return bios parameters */
int	qlogicfas_biosparam(Disk * disk, kdev_t dev, int ip[])
{
/* This should mimic the DOS Qlogic driver's behavior exactly */
	ip[0] = 0x40;
	ip[1] = 0x20;
	ip[2] = disk->capacity / (ip[0] * ip[1]);
	if (ip[2] > 1024) {
		ip[0] = 0xff;
		ip[1] = 0x3f;
		ip[2] = disk->capacity / (ip[0] * ip[1]);
#if 0
		if (ip[2] > 1023)
			ip[2] = 1023;
#endif
	}
	return 0;
}

/*----------------------------------------------------------------*/
/* abort command in progress */
int	qlogicfas_abort(Scsi_Cmnd * cmd)
{
	qabort = 1;
	ql_zap();
	return 0;
}

/*----------------------------------------------------------------*/
/* reset SCSI bus */
int	qlogicfas_reset(Scsi_Cmnd * cmd, unsigned int ignored)
{
	qabort = 2;
	ql_zap();
	return 1;
}

/*----------------------------------------------------------------*/
/* return info string */
const char	*qlogicfas_info(struct Scsi_Host * host)
{
	return qinfo;
}
MODULE_LICENSE("GPL");

#ifndef PCMCIA
/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = QLOGICFAS;
#include "scsi_module.c"
#endif


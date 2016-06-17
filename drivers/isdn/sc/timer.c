/* $Id: timer.c,v 1.1.4.1 2001/11/20 14:19:37 kai Exp $
 *
 * Copyright (C) 1996  SpellCaster Telecommunications Inc.
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For more information, please contact gpl-info@spellcast.com or write:
 *
 *     SpellCaster Telecommunications Inc.
 *     5621 Finch Avenue East, Unit #3
 *     Scarborough, Ontario  Canada
 *     M1B 2T9
 *     +1 (416) 297-8565
 *     +1 (416) 297-6433 Facsimile
 */

#define __NO_VERSION__
#include "includes.h"
#include "hardware.h"
#include "message.h"
#include "card.h"

extern board *adapter[];

extern void flushreadfifo(int);
extern int  startproc(int);
extern int  indicate_status(int, int, unsigned long, char *);
extern int  sendmessage(int, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int *);


/*
 * Write the proper values into the I/O ports following a reset
 */
void setup_ports(int card)
{

	outb((adapter[card]->rambase >> 12), adapter[card]->ioport[EXP_BASE]);

	/* And the IRQ */
	outb((adapter[card]->interrupt | 0x80), 
		adapter[card]->ioport[IRQ_SELECT]);
}

/*
 * Timed function to check the status of a previous reset
 * Must be very fast as this function runs in the context of
 * an interrupt handler.
 *
 * Setup the ioports for the board that were cleared by the reset.
 * Then, check to see if the signate has been set. Next, set the
 * signature to a known value and issue a startproc if needed.
 */
void check_reset(unsigned long data)
{
	unsigned long flags;
	unsigned long sig;
	int card = (unsigned int) data;

	pr_debug("%s: check_timer timer called\n", adapter[card]->devicename);

	/* Setup the io ports */
	setup_ports(card);

  	save_flags(flags);
	cli();
	outb(adapter[card]->ioport[adapter[card]->shmem_pgport],
		(adapter[card]->shmem_magic>>14) | 0x80);	
	sig = (unsigned long) *((unsigned long *)(adapter[card]->rambase + SIG_OFFSET));	

	/* check the signature */
	if(sig == SIGNATURE) {
		flushreadfifo(card);
		restore_flags(flags);
		/* See if we need to do a startproc */
		if (adapter[card]->StartOnReset)
			startproc(card);
	}
	else  {
		pr_debug("%s: No signature yet, waiting another %d jiffies.\n", 
			adapter[card]->devicename, CHECKRESET_TIME);
		mod_timer(&adapter[card]->reset_timer, jiffies+CHECKRESET_TIME);
	}
	restore_flags(flags);
		
}

/*
 * Timed function to check the status of a previous reset
 * Must be very fast as this function runs in the context of
 * an interrupt handler.
 *
 * Send check adapter->phystat to see if the channels are up
 * If they are, tell ISDN4Linux that the board is up. If not,
 * tell IADN4Linux that it is up. Always reset the timer to
 * fire again (endless loop).
 */
void check_phystat(unsigned long data)
{
	unsigned long flags;
	int card = (unsigned int) data;

	pr_debug("%s: Checking status...\n", adapter[card]->devicename);
	/* 
	 * check the results of the last PhyStat and change only if
	 * has changed drastically
	 */
	if (adapter[card]->nphystat && !adapter[card]->phystat) {   /* All is well */
		pr_debug("PhyStat transition to RUN\n");
		pr_info("%s: Switch contacted, transmitter enabled\n", 
			adapter[card]->devicename);
		indicate_status(card, ISDN_STAT_RUN, 0, NULL);
	}
	else if (!adapter[card]->nphystat && adapter[card]->phystat) {   /* All is not well */
		pr_debug("PhyStat transition to STOP\n");
		pr_info("%s: Switch connection lost, transmitter disabled\n", 
			adapter[card]->devicename);

		indicate_status(card, ISDN_STAT_STOP, 0, NULL);
	}

	adapter[card]->phystat = adapter[card]->nphystat;

	/* Reinitialize the timer */
	save_flags(flags);
	cli();
	mod_timer(&adapter[card]->stat_timer, jiffies+CHECKSTAT_TIME);
	restore_flags(flags);

	/* Send a new cePhyStatus message */
	sendmessage(card, CEPID,ceReqTypePhy,ceReqClass2,
		ceReqPhyStatus,0,0,NULL);
}

/*
 * When in trace mode, this callback is used to swap the working shared
 * RAM page to the trace page(s) and process all received messages. It
 * must be called often enough to get all of the messages out of RAM before
 * it loops around.
 * Trace messages are \n terminated strings.
 * We output the messages in 64 byte chunks through readstat. Each chunk
 * is scanned for a \n followed by a time stamp. If the timerstamp is older
 * than the current time, scanning stops and the page and offset are recorded
 * as the starting point the next time the trace timer is called. The final
 * step is to restore the working page and reset the timer.
 */
void trace_timer(unsigned long data)
{
	unsigned long flags;

	/*
	 * Disable interrupts and swap the first page
	 */
	save_flags(flags);
	cli();
}

/*
 *	AX.25 release 037
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	AX.25 028a	Jonathan(G4KLX)	New state machine based on SDL diagrams.
 *	AX.25 028b	Jonathan(G4KLX)	Extracted AX25 control block from the
 *					sock structure.
 *	AX.25 029	Alan(GW4PTS)	Switched to KA9Q constant names.
 *	AX.25 031	Joerg(DL1BKE)	Added DAMA support
 *	AX.25 032	Joerg(DL1BKE)	Fixed DAMA timeout bug
 *	AX.25 033	Jonathan(G4KLX)	Modularisation functions.
 *	AX.25 035	Frederic(F1OAT)	Support for pseudo-digipeating.
 *	AX.25 036	Jonathan(G4KLX)	Split Standard and DAMA code into separate files.
 *			Joerg(DL1BKE)	Fixed DAMA Slave. We are *required* to start with
 *					standard AX.25 mode.
 *	AX.25 037	Jonathan(G4KLX)	New timer architecture.
 *                      Tomi(OH2BNS)    Fixed heartbeat expiry (check ax25_dev).
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

static void ax25_heartbeat_expiry(unsigned long);
static void ax25_t1timer_expiry(unsigned long);
static void ax25_t2timer_expiry(unsigned long);
static void ax25_t3timer_expiry(unsigned long);
static void ax25_idletimer_expiry(unsigned long);

void ax25_start_heartbeat(ax25_cb *ax25)
{
	del_timer(&ax25->timer);

	ax25->timer.data     = (unsigned long)ax25;
	ax25->timer.function = &ax25_heartbeat_expiry;
	ax25->timer.expires  = jiffies + 5 * HZ;

	add_timer(&ax25->timer);
}

void ax25_start_t1timer(ax25_cb *ax25)
{
	del_timer(&ax25->t1timer);

	ax25->t1timer.data     = (unsigned long)ax25;
	ax25->t1timer.function = &ax25_t1timer_expiry;
	ax25->t1timer.expires  = jiffies + ax25->t1;

	add_timer(&ax25->t1timer);
}

void ax25_start_t2timer(ax25_cb *ax25)
{
	del_timer(&ax25->t2timer);

	ax25->t2timer.data     = (unsigned long)ax25;
	ax25->t2timer.function = &ax25_t2timer_expiry;
	ax25->t2timer.expires  = jiffies + ax25->t2;

	add_timer(&ax25->t2timer);
}

void ax25_start_t3timer(ax25_cb *ax25)
{
	del_timer(&ax25->t3timer);

	if (ax25->t3 > 0) {
		ax25->t3timer.data     = (unsigned long)ax25;
		ax25->t3timer.function = &ax25_t3timer_expiry;
		ax25->t3timer.expires  = jiffies + ax25->t3;

		add_timer(&ax25->t3timer);
	}
}

void ax25_start_idletimer(ax25_cb *ax25)
{
	del_timer(&ax25->idletimer);

	if (ax25->idle > 0) {
		ax25->idletimer.data     = (unsigned long)ax25;
		ax25->idletimer.function = &ax25_idletimer_expiry;
		ax25->idletimer.expires  = jiffies + ax25->idle;

		add_timer(&ax25->idletimer);
	}
}

void ax25_stop_heartbeat(ax25_cb *ax25)
{
	del_timer(&ax25->timer);
}

void ax25_stop_t1timer(ax25_cb *ax25)
{
	del_timer(&ax25->t1timer);
}

void ax25_stop_t2timer(ax25_cb *ax25)
{
	del_timer(&ax25->t2timer);
}

void ax25_stop_t3timer(ax25_cb *ax25)
{
	del_timer(&ax25->t3timer);
}

void ax25_stop_idletimer(ax25_cb *ax25)
{
	del_timer(&ax25->idletimer);
}

int ax25_t1timer_running(ax25_cb *ax25)
{
	return timer_pending(&ax25->t1timer);
}

unsigned long ax25_display_timer(struct timer_list *timer)
{
	if (!timer_pending(timer))
		return 0;

	return timer->expires - jiffies;
}

static void ax25_heartbeat_expiry(unsigned long param)
{
	ax25_cb *ax25 = (ax25_cb *)param;
	int proto = AX25_PROTO_STD_SIMPLEX;

	if (ax25->ax25_dev)
		proto = ax25->ax25_dev->values[AX25_VALUES_PROTOCOL];

	switch (proto) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_heartbeat_expiry(ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (ax25->ax25_dev->dama.slave)
				ax25_ds_heartbeat_expiry(ax25);
			else
				ax25_std_heartbeat_expiry(ax25);
			break;
#endif
	}
}

static void ax25_t1timer_expiry(unsigned long param)
{
	ax25_cb *ax25 = (ax25_cb *)param;

	switch (ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_t1timer_expiry(ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (!ax25->ax25_dev->dama.slave)
				ax25_std_t1timer_expiry(ax25);
			break;
#endif
	}
}

static void ax25_t2timer_expiry(unsigned long param)
{
	ax25_cb *ax25 = (ax25_cb *)param;

	switch (ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_t2timer_expiry(ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (!ax25->ax25_dev->dama.slave)
				ax25_std_t2timer_expiry(ax25);
			break;
#endif
	}
}

static void ax25_t3timer_expiry(unsigned long param)
{
	ax25_cb *ax25 = (ax25_cb *)param;

	switch (ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_t3timer_expiry(ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (ax25->ax25_dev->dama.slave)
				ax25_ds_t3timer_expiry(ax25);
			else
				ax25_std_t3timer_expiry(ax25);
			break;
#endif
	}
}

static void ax25_idletimer_expiry(unsigned long param)
{
	ax25_cb *ax25 = (ax25_cb *)param;

	switch (ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_idletimer_expiry(ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (ax25->ax25_dev->dama.slave)
				ax25_ds_idletimer_expiry(ax25);
			else
				ax25_std_idletimer_expiry(ax25);
			break;
#endif
	}
}

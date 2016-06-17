/*********************************************************************
 *                
 * Filename:      timer.h
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Aug 16 00:59:29 1997
 * Modified at:   Thu Oct  7 12:25:24 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1997, 1998-1999 Dag Brattli <dagb@cs.uit.no>, 
 *     All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/

#ifndef TIMER_H
#define TIMER_H

#include <linux/netdevice.h>

#include <asm/param.h>  /* for HZ */

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap.h>
#include <net/irda/irlmp.h>
#include <net/irda/irda_device.h>

/* 
 *  Timeout definitions, some defined in IrLAP p. 92
 */
#define POLL_TIMEOUT        (450*HZ/1000)    /* Must never exceed 500 ms */
#define FINAL_TIMEOUT       (500*HZ/1000)    /* Must never exceed 500 ms */

/* 
 *  Normally twice of p-timer. Note 3, IrLAP p. 60 suggests at least twice 
 *  duration of the P-timer.
 */
#define WD_TIMEOUT          (POLL_TIMEOUT*2)

#define MEDIABUSY_TIMEOUT   (500*HZ/1000)    /* 500 msec */
#define SMALLBUSY_TIMEOUT   (100*HZ/1000)    /* 100 msec - IrLAP 6.13.4 */

/*
 *  Slot timer must never exceed 85 ms, and must always be at least 25 ms, 
 *  suggested to  75-85 msec by IrDA lite. This doesn't work with a lot of
 *  devices, and other stackes uses a lot more, so it's best we do it as well
 */
#define SLOT_TIMEOUT            (90*HZ/1000)

/* 
 *  We set the query timeout to 100 ms and then expect the value to be 
 *  multiplied with the number of slots to product the actual timeout value
 */
#define QUERY_TIMEOUT           (HZ/10)       

#define WATCHDOG_TIMEOUT        (20*HZ)       /* 20 sec */

typedef void (*TIMER_CALLBACK)(void *);

void irda_start_timer(struct timer_list *ptimer, int timeout, void* data,
		      TIMER_CALLBACK callback);

inline void irlap_start_slot_timer(struct irlap_cb *self, int timeout);
inline void irlap_start_query_timer(struct irlap_cb *self, int timeout);
inline void irlap_start_final_timer(struct irlap_cb *self, int timeout);
inline void irlap_start_wd_timer(struct irlap_cb *self, int timeout);
inline void irlap_start_backoff_timer(struct irlap_cb *self, int timeout);

void irlap_start_mbusy_timer(struct irlap_cb *self, int timeout);
void irlap_stop_mbusy_timer(struct irlap_cb *);

struct lsap_cb;
struct lap_cb;
inline void irlmp_start_watchdog_timer(struct lsap_cb *, int timeout);
inline void irlmp_start_discovery_timer(struct irlmp_cb *, int timeout);
inline void irlmp_start_idle_timer(struct lap_cb *, int timeout);
inline void irlmp_stop_idle_timer(struct lap_cb *self); 

#endif


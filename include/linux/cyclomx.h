/*
* cyclomx.h	Cyclom 2X WAN Link Driver.
*		User-level API definitions.
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2000 Arnaldo Carvalho de Melo
*
* Based on wanpipe.h by Gene Kozin <genek@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 2000/07/13    acme		remove crap #if KERNEL_VERSION > blah
* 2000/01/21    acme            rename cyclomx_open to cyclomx_mod_inc_use_count
*                               and cyclomx_close to cyclomx_mod_dec_use_count
* 1999/05/19	acme		wait_queue_head_t wait_stats(support for 2.3.*)
* 1999/01/03	acme		judicious use of data types
* 1998/12/27	acme		cleanup: PACKED not needed
* 1998/08/08	acme		Version 0.0.1
*/
#ifndef	_CYCLOMX_H
#define	_CYCLOMX_H

#include <linux/config.h>
#include <linux/wanrouter.h>
#include <linux/spinlock.h>

#ifdef	__KERNEL__
/* Kernel Interface */

#include <linux/cycx_drv.h>	/* Cyclom 2X support module API definitions */
#include <linux/cycx_cfm.h>	/* Cyclom 2X firmware module definitions */
#ifdef CONFIG_CYCLOMX_X25
#include <linux/cycx_x25.h>
#endif

#define	is_digit(ch) (((ch)>=(unsigned)'0'&&(ch)<=(unsigned)'9')?1:0)

/* Adapter Data Space.
 * This structure is needed because we handle multiple cards, otherwise
 * static data would do it.
 */
typedef struct cycx {
	char devname[WAN_DRVNAME_SZ+1];	/* card name */
	cycxhw_t hw;			/* hardware configuration */
	wan_device_t wandev;		/* WAN device data space */
	u32 open_cnt;			/* number of open interfaces */
	u32 state_tick;			/* link state timestamp */
	spinlock_t lock;
	char in_isr;			/* interrupt-in-service flag */
	char buff_int_mode_unbusy;      /* flag for carrying out dev_tint */
	wait_queue_head_t wait_stats;  /* to wait for the STATS indication */
	u32 mbox;			/* -> mailbox */
	void (*isr)(struct cycx* card);	/* interrupt service routine */
	int (*exec)(struct cycx* card, void* u_cmd, void* u_data);
	union {
#ifdef CONFIG_CYCLOMX_X25
		struct { /* X.25 specific data */
			u32 lo_pvc;
			u32 hi_pvc;
			u32 lo_svc;
			u32 hi_svc;
			TX25Stats stats;
			spinlock_t lock;
			u32 connection_keys;
		} x;
#endif
	} u;
} cycx_t;

/* Public Functions */
void cyclomx_mod_inc_use_count (cycx_t *card);		/* cycx_main.c */
void cyclomx_mod_dec_use_count (cycx_t *card);		/* cycx_main.c */
void cyclomx_set_state (cycx_t *card, int state);	/* cycx_main.c */

#ifdef CONFIG_CYCLOMX_X25
int cyx_init (cycx_t *card, wandev_conf_t *conf);	/* cycx_x25.c */
#endif
#endif	/* __KERNEL__ */
#endif	/* _CYCLOMX_H */

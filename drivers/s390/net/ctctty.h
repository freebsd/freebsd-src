/*
 * $Id: ctctty.h,v 1.4 2001/03/22 12:46:01 felfert Exp $
 *
 * CTC / ESCON network driver, tty interface.
 *
 * Copyright (C) 2001 IBM Deutschland Entwicklung GmbH, IBM Corporation
 * Author(s): Fritz Elfert (elfert@de.ibm.com, felfert@millenux.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CTCTTY_H_
#define _CTCTTY_H_

#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#if LINUX_VERSION_CODE < 0x020300
typedef struct device      net_device;
#else
typedef struct net_device  net_device;
#endif

extern int  ctc_tty_register_netdev(net_device *);
extern void ctc_tty_unregister_netdev(net_device *);
extern void ctc_tty_netif_rx(struct sk_buff *);
extern int  ctc_tty_init(void);
extern void ctc_tty_cleanup(int);
extern void ctc_tty_setcarrier(net_device *, int);

#endif

/* 
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2000-2001 Qualcomm Incorporated

   Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 * BlueZ symbols.
 *
 * $Id: syms.c,v 1.1 2002/03/08 21:06:59 maxk Exp $
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/socket.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

/* HCI Core */
EXPORT_SYMBOL(hci_register_dev);
EXPORT_SYMBOL(hci_unregister_dev);
EXPORT_SYMBOL(hci_suspend_dev);
EXPORT_SYMBOL(hci_resume_dev);

EXPORT_SYMBOL(hci_register_proto);
EXPORT_SYMBOL(hci_unregister_proto);

EXPORT_SYMBOL(hci_get_route);
EXPORT_SYMBOL(hci_connect);
EXPORT_SYMBOL(hci_dev_get);
EXPORT_SYMBOL(hci_conn_auth);
EXPORT_SYMBOL(hci_conn_encrypt);

EXPORT_SYMBOL(hci_recv_frame);
EXPORT_SYMBOL(hci_send_acl);
EXPORT_SYMBOL(hci_send_sco);
EXPORT_SYMBOL(hci_send_cmd);
EXPORT_SYMBOL(hci_si_event);

/* BlueZ lib */
EXPORT_SYMBOL(bluez_dump);
EXPORT_SYMBOL(baswap);
EXPORT_SYMBOL(batostr);
EXPORT_SYMBOL(bterr);

/* BlueZ sockets */
EXPORT_SYMBOL(bluez_sock_register);
EXPORT_SYMBOL(bluez_sock_unregister);
EXPORT_SYMBOL(bluez_sock_init);
EXPORT_SYMBOL(bluez_sock_link);
EXPORT_SYMBOL(bluez_sock_unlink);
EXPORT_SYMBOL(bluez_sock_recvmsg);
EXPORT_SYMBOL(bluez_sock_poll);
EXPORT_SYMBOL(bluez_accept_enqueue);
EXPORT_SYMBOL(bluez_accept_dequeue);
EXPORT_SYMBOL(bluez_sock_wait_state);

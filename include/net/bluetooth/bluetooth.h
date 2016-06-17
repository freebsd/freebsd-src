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
 *  $Id: bluetooth.h,v 1.9 2002/05/06 21:11:55 maxk Exp $
 */

#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include <asm/types.h>
#include <asm/byteorder.h>
#include <linux/poll.h>
#include <net/sock.h>

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH	31
#define PF_BLUETOOTH	AF_BLUETOOTH
#endif

/* Reserv for core and drivers use */
#define BLUEZ_SKB_RESERVE       8

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define BTPROTO_L2CAP   0
#define BTPROTO_HCI     1
#define BTPROTO_SCO   	2
#define BTPROTO_RFCOMM	3
#define BTPROTO_BNEP	4
#define BTPROTO_CMTP	5

#define SOL_HCI     0
#define SOL_L2CAP   6
#define SOL_SCO     17
#define SOL_RFCOMM  18

/* Debugging */
#ifdef CONFIG_BLUEZ_DEBUG

#define HCI_CORE_DEBUG		1
#define HCI_SOCK_DEBUG		1
#define HCI_UART_DEBUG		1
#define HCI_USB_DEBUG		1
//#define HCI_DATA_DUMP		1

#define L2CAP_DEBUG		1
#define SCO_DEBUG		1
#define AF_BLUETOOTH_DEBUG	1

#endif /* CONFIG_BLUEZ_DEBUG */

extern void bluez_dump(char *pref, __u8 *buf, int count);

#if __GNUC__ <= 2 && __GNUC_MINOR__ < 95
#define __func__ __FUNCTION__
#endif

#define BT_INFO(fmt, arg...) printk(KERN_INFO fmt "\n" , ## arg)
#define BT_DBG(fmt, arg...)  printk(KERN_INFO "%s: " fmt "\n" , __func__ , ## arg)
#define BT_ERR(fmt, arg...)  printk(KERN_ERR  "%s: " fmt "\n" , __func__ , ## arg)

#ifdef HCI_DATA_DUMP
#define BT_DMP(buf, len)    bluez_dump(__func__, buf, len)
#else
#define BT_DMP(D...)
#endif

/* Connection and socket states */
enum {
	BT_CONNECTED = 1, /* Equal to TCP_ESTABLISHED to make net code happy */
	BT_OPEN,
	BT_BOUND,
	BT_LISTEN,
	BT_CONNECT,
	BT_CONNECT2,
	BT_CONFIG,
	BT_DISCONN,
	BT_CLOSED
};

/* Endianness conversions */
#define htobs(a)	__cpu_to_le16(a)
#define htobl(a)	__cpu_to_le32(a)
#define btohs(a)	__le16_to_cpu(a)
#define btohl(a)	__le32_to_cpu(a)

/* BD Address */
typedef struct {
	__u8 b[6];
} __attribute__((packed)) bdaddr_t;

#define BDADDR_ANY   (&(bdaddr_t) {{0, 0, 0, 0, 0, 0}})
#define BDADDR_LOCAL (&(bdaddr_t) {{0, 0, 0, 0xff, 0xff, 0xff}})

/* Copy, swap, convert BD Address */
static inline int bacmp(bdaddr_t *ba1, bdaddr_t *ba2)
{
	return memcmp(ba1, ba2, sizeof(bdaddr_t));
}
static inline void bacpy(bdaddr_t *dst, bdaddr_t *src)
{
	memcpy(dst, src, sizeof(bdaddr_t));
}

void baswap(bdaddr_t *dst, bdaddr_t *src);
char *batostr(bdaddr_t *ba);
bdaddr_t *strtoba(char *str);

/* Common socket structures and functions */

#define bluez_pi(sk) ((struct bluez_pinfo *) &sk->protinfo)
#define bluez_sk(pi) ((struct sock *) \
	((void *)pi - (unsigned long)(&((struct sock *)0)->protinfo)))

struct bluez_pinfo {
	bdaddr_t	src;
	bdaddr_t	dst;

	struct list_head accept_q;
	struct sock *parent;
};

struct bluez_sock_list {
	struct sock *head;
	rwlock_t     lock;
};

int  bluez_sock_register(int proto, struct net_proto_family *ops);
int  bluez_sock_unregister(int proto);
void bluez_sock_init(struct socket *sock, struct sock *sk);
void bluez_sock_link(struct bluez_sock_list *l, struct sock *s);
void bluez_sock_unlink(struct bluez_sock_list *l, struct sock *s);
int  bluez_sock_recvmsg(struct socket *sock, struct msghdr *msg, int len, int flags, struct scm_cookie *scm);
uint bluez_sock_poll(struct file * file, struct socket *sock, poll_table *wait);
int  bluez_sock_wait_state(struct sock *sk, int state, unsigned long timeo);

void bluez_accept_enqueue(struct sock *parent, struct sock *sk);
struct sock * bluez_accept_dequeue(struct sock *parent, struct socket *newsock);

/* Skb helpers */
struct bluez_skb_cb {
	int    incomming;
};
#define bluez_cb(skb)	((struct bluez_skb_cb *)(skb->cb)) 

static inline struct sk_buff *bluez_skb_alloc(unsigned int len, int how)
{
	struct sk_buff *skb;

	if ((skb = alloc_skb(len + BLUEZ_SKB_RESERVE, how))) {
		skb_reserve(skb, BLUEZ_SKB_RESERVE);
		bluez_cb(skb)->incomming  = 0;
	}
	return skb;
}

static inline struct sk_buff *bluez_skb_send_alloc(struct sock *sk, unsigned long len, 
						       int nb, int *err)
{
	struct sk_buff *skb;

	if ((skb = sock_alloc_send_skb(sk, len + BLUEZ_SKB_RESERVE, nb, err))) {
		skb_reserve(skb, BLUEZ_SKB_RESERVE);
		bluez_cb(skb)->incomming  = 0;
	}

	return skb;
}

static inline int skb_frags_no(struct sk_buff *skb)
{
	register struct sk_buff *frag = skb_shinfo(skb)->frag_list;
	register int n = 1;

	for (; frag; frag=frag->next, n++);
	return n;
}

int hci_core_init(void);
int hci_core_cleanup(void);
int hci_sock_init(void);
int hci_sock_cleanup(void);

int bterr(__u16 code);

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)
#endif

#endif /* __BLUETOOTH_H */

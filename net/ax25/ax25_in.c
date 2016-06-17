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
 *	Most of this code is based on the SDL diagrams published in the 7th
 *	ARRL Computer Networking Conference papers. The diagrams have mistakes
 *	in them, but are mostly correct. Before you modify the code could you
 *	read the SDL diagrams as the code is not obvious and probably very
 *	easy to break;
 *
 *	History
 *	AX.25 028a	Jonathan(G4KLX)	New state machine based on SDL diagrams.
 *	AX.25 028b	Jonathan(G4KLX) Extracted AX25 control block from
 *					the sock structure.
 *	AX.25 029	Alan(GW4PTS)	Switched to KA9Q constant names.
 *			Jonathan(G4KLX)	Added IP mode registration.
 *	AX.25 030	Jonathan(G4KLX)	Added AX.25 fragment reception.
 *					Upgraded state machine for SABME.
 *					Added arbitrary protocol id support.
 *	AX.25 031	Joerg(DL1BKE)	Added DAMA support
 *			HaJo(DD8NE)	Added Idle Disc Timer T5
 *			Joerg(DL1BKE)   Renamed it to "IDLE" with a slightly
 *					different behaviour. Fixed defrag
 *					routine (I hope)
 *	AX.25 032	Darryl(G7LED)	AX.25 segmentation fixed.
 *	AX.25 033	Jonathan(G4KLX)	Remove auto-router.
 *					Modularisation changes.
 *	AX.25 035	Hans(PE1AYX)	Fixed interface to IP layer.
 *	AX.25 036	Jonathan(G4KLX)	Move DAMA code into own file.
 *			Joerg(DL1BKE)	Fixed DAMA Slave.
 *	AX.25 037	Jonathan(G4KLX)	New timer architecture.
 *			Thomas(DL9SAU)  Fixed missing initialization of skb->protocol.
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
#include <linux/netfilter.h>
#include <net/sock.h>
#include <net/ip.h>			/* For ip_rcv */
#include <net/arp.h>			/* For arp_rcv */
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

/*
 *	Given a fragment, queue it on the fragment queue and if the fragment
 *	is complete, send it back to ax25_rx_iframe.
 */
static int ax25_rx_fragment(ax25_cb *ax25, struct sk_buff *skb)
{
	struct sk_buff *skbn, *skbo;

	if (ax25->fragno != 0) {
		if (!(*skb->data & AX25_SEG_FIRST)) {
			if ((ax25->fragno - 1) == (*skb->data & AX25_SEG_REM)) {
				/* Enqueue fragment */
				ax25->fragno = *skb->data & AX25_SEG_REM;
				skb_pull(skb, 1);	/* skip fragno */
				ax25->fraglen += skb->len;
				skb_queue_tail(&ax25->frag_queue, skb);

				/* Last fragment received ? */
				if (ax25->fragno == 0) {
					skbn = alloc_skb(AX25_MAX_HEADER_LEN +
							 ax25->fraglen,
							 GFP_ATOMIC);
					if (!skbn) {
						skb_queue_purge(&ax25->frag_queue);
						return 1;
					}

					skb_reserve(skbn, AX25_MAX_HEADER_LEN);

					skbn->dev   = ax25->ax25_dev->dev;
					skbn->h.raw = skbn->data;
					skbn->nh.raw = skbn->data;

					/* Copy data from the fragments */
					while ((skbo = skb_dequeue(&ax25->frag_queue)) != NULL) {
						memcpy(skb_put(skbn, skbo->len), skbo->data, skbo->len);
						kfree_skb(skbo);
					}

					ax25->fraglen = 0;

					if (ax25_rx_iframe(ax25, skbn) == 0)
						kfree_skb(skbn);
				}

				return 1;
			}
		}
	} else {
		/* First fragment received */
		if (*skb->data & AX25_SEG_FIRST) {
			skb_queue_purge(&ax25->frag_queue);
			ax25->fragno = *skb->data & AX25_SEG_REM;
			skb_pull(skb, 1);		/* skip fragno */
			ax25->fraglen = skb->len;
			skb_queue_tail(&ax25->frag_queue, skb);
			return 1;
		}
	}

	return 0;
}

/*
 *	This is where all valid I frames are sent to, to be dispatched to
 *	whichever protocol requires them.
 */
int ax25_rx_iframe(ax25_cb *ax25, struct sk_buff *skb)
{
	int (*func)(struct sk_buff *, ax25_cb *);
	volatile int queued = 0;
	unsigned char pid;

	if (skb == NULL) return 0;

	ax25_start_idletimer(ax25);

	pid = *skb->data;

#ifdef CONFIG_INET
	if (pid == AX25_P_IP) {
		/* working around a TCP bug to keep additional listeners 
		 * happy. TCP re-uses the buffer and destroys the original
		 * content.
		 */
		struct sk_buff *skbn = skb_copy(skb, GFP_ATOMIC);
		if (skbn != NULL) {
			kfree_skb(skb);
			skb = skbn;
		}

		skb_pull(skb, 1);	/* Remove PID */
		skb->h.raw    = skb->data;
		skb->nh.raw   = skb->data;
		skb->dev      = ax25->ax25_dev->dev;
		skb->pkt_type = PACKET_HOST;
		skb->protocol = htons(ETH_P_IP);
		ip_rcv(skb, skb->dev, NULL);	/* Wrong ptype */
		return 1;
	}
#endif
	if (pid == AX25_P_SEGMENT) {
		skb_pull(skb, 1);	/* Remove PID */
		return ax25_rx_fragment(ax25, skb);
	}

	if ((func = ax25_protocol_function(pid)) != NULL) {
		skb_pull(skb, 1);	/* Remove PID */
		return (*func)(skb, ax25);
	}

	if (ax25->sk != NULL && ax25->ax25_dev->values[AX25_VALUES_CONMODE] == 2) {
		if ((!ax25->pidincl && ax25->sk->protocol == pid) || ax25->pidincl) {
			if (sock_queue_rcv_skb(ax25->sk, skb) == 0)
				queued = 1;
			else
				ax25->condition |= AX25_COND_OWN_RX_BUSY;
		}
	}

	return queued;
}

/*
 *	Higher level upcall for a LAPB frame
 */
static int ax25_process_rx_frame(ax25_cb *ax25, struct sk_buff *skb, int type, int dama)
{
	int queued = 0;

	if (ax25->state == AX25_STATE_0)
		return 0;

	switch (ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			queued = ax25_std_frame_in(ax25, skb, type);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			if (dama || ax25->ax25_dev->dama.slave)
				queued = ax25_ds_frame_in(ax25, skb, type);
			else
				queued = ax25_std_frame_in(ax25, skb, type);
			break;
#endif
	}

	return queued;
}

static int ax25_rcv(struct sk_buff *skb, struct net_device *dev, ax25_address *dev_addr, struct packet_type *ptype)
{
	struct sock *make;
	struct sock *sk;
	int type = 0;
	ax25_digi dp, reverse_dp;
	ax25_cb *ax25;
	ax25_address src, dest;
	ax25_address *next_digi = NULL;
	ax25_dev *ax25_dev;
	struct sock *raw;
	int mine = 0;
	int dama;

	/*
	 *	Process the AX.25/LAPB frame.
	 */

	skb->h.raw = skb->data;

	if ((ax25_dev = ax25_dev_ax25dev(dev)) == NULL) {
		kfree_skb(skb);
		return 0;
	}

	/*
	 *	Parse the address header.
	 */

	if (ax25_addr_parse(skb->data, skb->len, &src, &dest, &dp, &type, &dama) == NULL) {
		kfree_skb(skb);
		return 0;
	}

	/*
	 *	Ours perhaps ?
	 */
	if (dp.lastrepeat + 1 < dp.ndigi)		/* Not yet digipeated completely */
		next_digi = &dp.calls[dp.lastrepeat + 1];

	/*
	 *	Pull of the AX.25 headers leaving the CTRL/PID bytes
	 */
	skb_pull(skb, ax25_addr_size(&dp));

	/* For our port addresses ? */
	if (ax25cmp(&dest, dev_addr) == 0 && dp.lastrepeat + 1 == dp.ndigi)
		mine = 1;

	/* Also match on any registered callsign from L3/4 */
	if (!mine && ax25_listen_mine(&dest, dev) && dp.lastrepeat + 1 == dp.ndigi)
		mine = 1;

	/* UI frame - bypass LAPB processing */
	if ((*skb->data & ~0x10) == AX25_UI && dp.lastrepeat + 1 == dp.ndigi) {
		skb->h.raw = skb->data + 2;		/* skip control and pid */

		if ((raw = ax25_addr_match(&dest)) != NULL)
			ax25_send_to_raw(raw, skb, skb->data[1]);

		if (!mine && ax25cmp(&dest, (ax25_address *)dev->broadcast) != 0) {
			kfree_skb(skb);
			return 0;
		}

		/* Now we are pointing at the pid byte */
		switch (skb->data[1]) {
#ifdef CONFIG_INET
			case AX25_P_IP:
				skb_pull(skb,2);		/* drop PID/CTRL */
				skb->h.raw    = skb->data;
				skb->nh.raw   = skb->data;
				skb->dev      = dev;
				skb->pkt_type = PACKET_HOST;
				skb->protocol = htons(ETH_P_IP);
				ip_rcv(skb, dev, ptype);	/* Note ptype here is the wrong one, fix me later */
				break;

			case AX25_P_ARP:
				skb_pull(skb,2);
				skb->h.raw    = skb->data;
				skb->nh.raw   = skb->data;
				skb->dev      = dev;
				skb->pkt_type = PACKET_HOST;
				skb->protocol = htons(ETH_P_ARP);
				arp_rcv(skb, dev, ptype);	/* Note ptype here is wrong... */
				break;
#endif
			case AX25_P_TEXT:
				/* Now find a suitable dgram socket */
				if ((sk = ax25_find_socket(&dest, &src, SOCK_DGRAM)) != NULL) {
					if (atomic_read(&sk->rmem_alloc) >= sk->rcvbuf) {
						kfree_skb(skb);
					} else {
						/*
						 *	Remove the control and PID.
						 */
						skb_pull(skb, 2);
						if (sock_queue_rcv_skb(sk, skb) != 0)
							kfree_skb(skb);
					}
				} else {
					kfree_skb(skb);
				}
				break;

			default:
				kfree_skb(skb);	/* Will scan SOCK_AX25 RAW sockets */
				break;
		}

		return 0;
	}

	/*
	 *	Is connected mode supported on this device ?
	 *	If not, should we DM the incoming frame (except DMs) or
	 *	silently ignore them. For now we stay quiet.
	 */
	if (ax25_dev->values[AX25_VALUES_CONMODE] == 0) {
		kfree_skb(skb);
		return 0;
	}

	/* LAPB */

	/* AX.25 state 1-4 */

	ax25_digi_invert(&dp, &reverse_dp);

	if ((ax25 = ax25_find_cb(&dest, &src, &reverse_dp, dev)) != NULL) {
		/*
		 *	Process the frame. If it is queued up internally it returns one otherwise we
		 *	free it immediately. This routine itself wakes the user context layers so we
		 *	do no further work
		 */
		if (ax25_process_rx_frame(ax25, skb, type, dama) == 0)
			kfree_skb(skb);

		return 0;
	}

	/* AX.25 state 0 (disconnected) */

	/* a) received not a SABM(E) */

	if ((*skb->data & ~AX25_PF) != AX25_SABM && (*skb->data & ~AX25_PF) != AX25_SABME) {
		/*
		 *	Never reply to a DM. Also ignore any connects for
		 *	addresses that are not our interfaces and not a socket.
		 */
		if ((*skb->data & ~AX25_PF) != AX25_DM && mine)
			ax25_return_dm(dev, &src, &dest, &dp);

		kfree_skb(skb);
		return 0;
	}

	/* b) received SABM(E) */

	if (dp.lastrepeat + 1 == dp.ndigi)
		sk = ax25_find_listener(&dest, 0, dev, SOCK_SEQPACKET);
	else
		sk = ax25_find_listener(next_digi, 1, dev, SOCK_SEQPACKET);

	if (sk != NULL) {
		if (sk->ack_backlog == sk->max_ack_backlog || (make = ax25_make_new(sk, ax25_dev)) == NULL) {
			if (mine) ax25_return_dm(dev, &src, &dest, &dp);
			kfree_skb(skb);
			return 0;
		}

		ax25 = make->protinfo.ax25;
		skb_set_owner_r(skb, make);
		skb_queue_head(&sk->receive_queue, skb);

		make->state = TCP_ESTABLISHED;
		make->pair  = sk;

		sk->ack_backlog++;
	} else {
		if (!mine) {
			kfree_skb(skb);
			return 0;
		}

		if ((ax25 = ax25_create_cb()) == NULL) {
			ax25_return_dm(dev, &src, &dest, &dp);
			kfree_skb(skb);
			return 0;
		}

		ax25_fillin_cb(ax25, ax25_dev);
	}

	ax25->source_addr = dest;
	ax25->dest_addr   = src;

	/*
	 *	Sort out any digipeated paths.
	 */
	if (dp.ndigi && !ax25->digipeat &&
	    (ax25->digipeat = kmalloc(sizeof(ax25_digi), GFP_ATOMIC)) == NULL) {
		kfree_skb(skb);
		ax25_destroy_socket(ax25);
		return 0;
	}

	if (dp.ndigi == 0) {
		if (ax25->digipeat != NULL) {
			kfree(ax25->digipeat);
			ax25->digipeat = NULL;
		}
	} else {
		/* Reverse the source SABM's path */
		memcpy(ax25->digipeat, &reverse_dp, sizeof(ax25_digi));
	}

	if ((*skb->data & ~AX25_PF) == AX25_SABME) {
		ax25->modulus = AX25_EMODULUS;
		ax25->window  = ax25_dev->values[AX25_VALUES_EWINDOW];
	} else {
		ax25->modulus = AX25_MODULUS;
		ax25->window  = ax25_dev->values[AX25_VALUES_WINDOW];
	}

	ax25_send_control(ax25, AX25_UA, AX25_POLLON, AX25_RESPONSE);

#ifdef CONFIG_AX25_DAMA_SLAVE
	if (dama && ax25->ax25_dev->values[AX25_VALUES_PROTOCOL] == AX25_PROTO_DAMA_SLAVE)
		ax25_dama_on(ax25);
#endif

	ax25->state = AX25_STATE_3;

	ax25_insert_socket(ax25);

	ax25_start_heartbeat(ax25);
	ax25_start_t3timer(ax25);
	ax25_start_idletimer(ax25);

	if (sk != NULL) {
		if (!sk->dead)
			sk->data_ready(sk, skb->len);
	} else {
		kfree_skb(skb);
	}

	return 0;
}

/*
 *	Receive an AX.25 frame via a SLIP interface.
 */
int ax25_kiss_rcv(struct sk_buff *skb, struct net_device *dev,
		  struct packet_type *ptype)
{
	skb->sk = NULL;		/* Initially we don't know who it's for */
	skb->destructor = NULL;	/* Who initializes this, dammit?! */

	if ((*skb->data & 0x0F) != 0) {
		kfree_skb(skb);	/* Not a KISS data frame */
		return 0;
	}

	skb_pull(skb, AX25_KISS_HEADER_LEN);	/* Remove the KISS byte */

	return ax25_rcv(skb, dev, (ax25_address *)dev->dev_addr, ptype);
}


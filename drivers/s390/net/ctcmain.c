/*
 * $Id: ctcmain.c,v 1.63 2003/10/22 19:32:57 felfert Exp $
 *
 * CTC / ESCON network driver
 *
 * Copyright (C) 2001 IBM Deutschland Entwicklung GmbH, IBM Corporation
 * Author(s): Fritz Elfert (elfert@de.ibm.com, felfert@millenux.com)
 * Fixes by : Jochen Röhrig (roehrig@de.ibm.com)
 *            Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * Documentation used:
 *  - Principles of Operation (IBM doc#: SA22-7201-06)
 *  - Common IO/-Device Commands and Self Description (IBM doc#: SA22-7204-02)
 *  - Common IO/-Device Commands and Self Description (IBM doc#: SN22-5535)
 *  - ESCON Channel-to-Channel Adapter (IBM doc#: SA22-7203-00)
 *  - ESCON I/O Interface (IBM doc#: SA22-7202-029
 *
 * and the source of the original CTC driver by:
 *  Dieter Wellerdiek (wel@de.ibm.com)
 *  Martin Schwidefsky (schwidefsky@de.ibm.com)
 *  Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *  Jochen Röhrig (roehrig@de.ibm.com)
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
 *
 * RELEASE-TAG: CTC/ESCON network driver $Revision: 1.63 $
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>

#include <linux/signal.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

#include <linux/ip.h>
#include <linux/if_arp.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/dst.h>

#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#ifdef CONFIG_CHANDEV
#define CTC_CHANDEV
#endif

#ifdef CTC_CHANDEV
#include <asm/chandev.h>
#define REQUEST_IRQ chandev_request_irq
#define FREE_IRQ chandev_free_irq
#else
#define REQUEST_IRQ request_irq
#define FREE_IRQ free_irq
#endif

#if LINUX_VERSION_CODE >= 0x020213
#  include <asm/idals.h>
#else
#  define set_normalized_cda(ccw, addr) ((ccw)->cda = (addr),0)
#  define clear_normalized_cda(ccw)
#endif
#if LINUX_VERSION_CODE < 0x020400
#  define s390_dev_info_t dev_info_t
#  define dev_kfree_skb_irq(a) dev_kfree_skb(a)
#endif

#include <asm/irq.h>

#include "ctctty.h"
#include "fsm.h"

#ifdef MODULE
MODULE_AUTHOR("(C) 2000 IBM Corp. by Fritz Elfert (felfert@millenux.com)");
MODULE_DESCRIPTION("Linux for S/390 CTC/Escon Driver");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,12))
MODULE_LICENSE("GPL");
#endif
#ifndef CTC_CHANDEV
MODULE_PARM(ctc, "s");
MODULE_PARM_DESC(ctc,
"One or more definitions in the same format like the kernel param for ctc.\n"
"E.g.: ctc0:0x700:0x701:0:ctc1:0x702:0x703:0\n");

char *ctc = NULL;
#endif
#else
/**
 * Number of devices in monolithic (not module) driver version.
 */
#define MAX_STATIC_DEVICES 16
#endif /* MODULE */

#undef DEBUG

/**
 * CCW commands, used in this driver.
 */
#define CCW_CMD_WRITE		0x01
#define CCW_CMD_READ		0x02
#define CCW_CMD_SET_EXTENDED	0xc3
#define CCW_CMD_PREPARE		0xe3

#define CTC_PROTO_S390          0
#define CTC_PROTO_LINUX         1
#define CTC_PROTO_LINUX_TTY     2
#define CTC_PROTO_OS390         3
#define CTC_PROTO_MAX           3

#define CTC_BUFSIZE_LIMIT       65535
#define CTC_BUFSIZE_DEFAULT     32768

#define CTC_TIMEOUT_5SEC        5000

#define CTC_INITIAL_BLOCKLEN    2

#define READ			0
#define WRITE			1

/**
 * Enum for classifying detected devices.
 */
enum channel_types {
        /**
	 * Device is not a channel.
	 */
	channel_type_none,

	/**
	 * Device is a channel, but we don't know
	 * anything about it.
	 */
	channel_type_unknown,

        /**
	 * Device is a CTC/A.
	 */
	channel_type_ctca,

	/**
	 * Device is a ESCON channel.
	 */
	channel_type_escon,
	/**
	 * Device is an unsupported model.
	 */
	channel_type_unsupported
};

typedef enum channel_types channel_type_t;

#ifndef CTC_CHANDEV
static int ctc_no_auto = 0;
#endif

/**
 * If running on 64 bit, this must be changed. XXX Why? (bird)
 */
typedef unsigned long intparm_t;

#ifndef CTC_CHANDEV
/**
 * Definition of a per device parameter block
 */
#define MAX_PARAM_NAME_LEN 11
typedef struct param_t {
	struct param_t *next;
	int            read_dev;
	int            write_dev;
	__u16          proto;
	char           name[MAX_PARAM_NAME_LEN];
} param;

static param *params = NULL;
#endif

typedef struct {
	unsigned long maxmulti;
	unsigned long maxcqueue;
	unsigned long doios_single;
	unsigned long doios_multi;
	unsigned long txlen;
	unsigned long tx_time;
	struct timeval send_stamp;
} ctc_profile;

/**
 * Definition of one channel
 */
typedef struct channel_t {

        /**
	 * Pointer to next channel in list.
	 */
	struct channel_t    *next;
	__u16               devno;
	int                 irq;

	/**
	 * Type of this channel.
	 * CTC/A or Escon for valid channels.
	 */
	channel_type_t      type;

        /**
	 * Misc. flags. See CHANNEL_FLAGS_... below
	 */
	__u32               flags;

	/**
	 * The protocol of this channel
	 */
	__u16               protocol;

	/**
	 * I/O and irq related stuff
	 */
	ccw1_t              *ccw;
	devstat_t           *devstat;

	/**
	 * Bottom half task queue.
	 */
	struct tq_struct    tq;

	/**
	 * RX/TX buffer size
	 */
	int                 max_bufsize;

	/**
	 * Transmit/Receive buffer.
	 */
	struct sk_buff      *trans_skb;

	/**
	 * Universal I/O queue.
	 */
	struct sk_buff_head io_queue;

	/**
	 * TX queue for collecting skb's during busy.
	 */
	struct sk_buff_head collect_queue;

	/**
	 * Amount of data in collect_queue.
	 */
	int                 collect_len;

	/**
	 * spinlock for collect_queue and collect_len
	 */
	spinlock_t          collect_lock;

	/**
	 * Timer for detecting unresposive
	 * I/O operations.
	 */
	fsm_timer           timer;

	/**
	 * Retry counter for misc. operations.
	 */
	int                 retry;

	/**
	 * The finite state machine of this channel
	 */
	fsm_instance        *fsm;

	/**
	 * The corresponding net_device this channel
	 * belongs to.
	 */
	net_device          *netdev;

	ctc_profile         prof;

	unsigned char       *trans_skb_data;

        __u16               logflags;

} channel;

#define CHANNEL_FLAGS_READ            0
#define CHANNEL_FLAGS_WRITE           1
#define CHANNEL_FLAGS_INUSE           2
#define CHANNEL_FLAGS_BUFSIZE_CHANGED 4
#define CHANNEL_FLAGS_FAILED          8
#define CHANNEL_FLAGS_RWMASK 1
#define CHANNEL_DIRECTION(f) (f & CHANNEL_FLAGS_RWMASK)

#define LOG_FLAG_ILLEGALPKT  1
#define LOG_FLAG_ILLEGALSIZE 2
#define LOG_FLAG_OVERRUN     4
#define LOG_FLAG_NOMEM       8

/**
 * Linked list of all detected channels.
 */
static channel *channels = NULL;

#define CTC_LOGLEVEL_INFO     1
#define CTC_LOGLEVEL_NOTICE   2
#define CTC_LOGLEVEL_WARN     4
#define CTC_LOGLEVEL_EMERG    8  
#define CTC_LOGLEVEL_ERR     16
#define CTC_LOGLEVEL_DEBUG   32
#define CTC_LOGLEVEL_CRIT    64

#define CTC_LOGLEVEL_DEFAULT \
(CTC_LOGLEVEL_INFO | CTC_LOGLEVEL_NOTICE | CTC_LOGLEVEL_WARN | CTC_LOGLEVEL_CRIT)

#define CTC_LOGLEVEL_MAX     ((CTC_LOGLEVEL_CRIT<<1)-1)

static int loglevel = CTC_LOGLEVEL_DEFAULT;

#ifdef CTC_CHANDEV
static int activated;
#endif

typedef struct ctc_priv_t {
	struct net_device_stats stats;
#if LINUX_VERSION_CODE >= 0x02032D
	unsigned long           tbusy;
#endif
	/**
	 * The finite state machine of this interface.
	 */
	fsm_instance            *fsm;
	/**
	 * The protocol of this device
	 */
	__u16                   protocol;
	channel                 *channel[2];
	struct proc_dir_entry   *proc_dentry;
	struct proc_dir_entry   *proc_stat_entry;
	struct proc_dir_entry   *proc_ctrl_entry;
	struct proc_dir_entry   *proc_loglevel_entry;
	int                     proc_registered;

	/**
	 * Timer for restarting after I/O Errors
	 */
	fsm_timer               restart_timer;
} ctc_priv;

/**
 * Definition of our link level header.
 */
typedef struct ll_header_t {
	__u16	      length;
	__u16	      type;
	__u16	      unused;
} ll_header;
#define LL_HEADER_LENGTH (sizeof(ll_header))

/**
 * Compatibility macros for busy handling
 * of network devices.
 */
#if LINUX_VERSION_CODE < 0x02032D
static __inline__ void ctc_clear_busy(net_device *dev)
{
	clear_bit(0 ,(void *)&dev->tbusy);
	mark_bh(NET_BH);
}

static __inline__ int ctc_test_and_set_busy(net_device *dev)
{
	return(test_and_set_bit(0, (void *)&dev->tbusy));
}

#define SET_DEVICE_START(device, value) dev->start = value
#else
static __inline__ void ctc_clear_busy(net_device *dev)
{
	clear_bit(0, &(((ctc_priv *)dev->priv)->tbusy));
	if (((ctc_priv *)dev->priv)->protocol != CTC_PROTO_LINUX_TTY)
		netif_wake_queue(dev);
}

static __inline__ int ctc_test_and_set_busy(net_device *dev)
{
	if (((ctc_priv *)dev->priv)->protocol != CTC_PROTO_LINUX_TTY)
		netif_stop_queue(dev);
	return test_and_set_bit(0, &((ctc_priv *)dev->priv)->tbusy);
}

#define SET_DEVICE_START(device, value)
#endif

/**
 * Print Banner.
 */
static void print_banner(void) {
	static int printed = 0;
	char vbuf[] = "$Revision: 1.63 $";
	char *version = vbuf;

	if (printed)
		return;
	if ((version = strchr(version, ':'))) {
		char *p = strchr(version + 1, '$');
		if (p)
			*p = '\0';
	} else
		version = " ??? ";
	if (loglevel & CTC_LOGLEVEL_INFO)
		printk(KERN_INFO
		       "CTC driver Version%swith"
#ifndef CTC_CHANDEV
		       "out"
#endif
		       " CHANDEV support"
#ifdef DEBUG
		       " (DEBUG-VERSION, " __DATE__ __TIME__ ")"
#endif
		       " initialized\n", version);
	printed = 1;
}


#ifndef CTC_CHANDEV
/**
 * Return type of a detected device.
 */
static channel_type_t channel_type (senseid_t *id) {
	channel_type_t type = channel_type_none;

	switch (id->cu_type) {
		case 0x3088:
			switch (id->cu_model) {
				case 0x08:
					/**
					 * 3088-08 = CTCA
					 */
					type = channel_type_ctca;
					break;

				case 0x1F:
					/**
					 * 3088-1F = ESCON channel
					 */
					type = channel_type_escon;
					break;

					/**
					 * 3088-01 = P390 OSA emulation
					 */
				case 0x01:
					/* fall thru */

					/**
					 * 3088-60 = OSA/2 adapter
					 */
				case 0x60:
					/* fall thru */

					/**
					 * 3088-61 = CISCO 7206 CLAW proto
					 * on ESCON
					 */
				case 0x61:
					/* fall thru */

					/**
					 * 3088-62 = OSA/D device
					 */
				case 0x62:
					type = channel_type_unsupported;
					break;

				default:
					type = channel_type_unknown;
					if (loglevel & CTC_LOGLEVEL_INFO)
						printk(KERN_INFO
						       "channel: Unknown model found "
						       "3088-%02x\n", id->cu_model);
			}
			break;

		default:
			type = channel_type_none;
	}
	return type;
}
#endif


/**
 * States of the interface statemachine.
 */
enum dev_states {
	DEV_STATE_STOPPED,
	DEV_STATE_STARTWAIT_RXTX,
	DEV_STATE_STARTWAIT_RX,
	DEV_STATE_STARTWAIT_TX,
	DEV_STATE_STOPWAIT_RXTX,
	DEV_STATE_STOPWAIT_RX,
	DEV_STATE_STOPWAIT_TX,
	DEV_STATE_RUNNING,
	/**
	 * MUST be always the last element!!
	 */
	NR_DEV_STATES
};

static const char *dev_state_names[] = {
	"Stopped",
	"StartWait RXTX",
	"StartWait RX",
	"StartWait TX",
	"StopWait RXTX",
	"StopWait RX",
	"StopWait TX",
	"Running",
};

/**
 * Events of the interface statemachine.
 */
enum dev_events {
	DEV_EVENT_START,
	DEV_EVENT_STOP,
	DEV_EVENT_RXUP,
	DEV_EVENT_TXUP,
	DEV_EVENT_RXDOWN,
	DEV_EVENT_TXDOWN,
	DEV_EVENT_RESTART,
	/**
	 * MUST be always the last element!!
	 */
	NR_DEV_EVENTS
};

static const char *dev_event_names[] = {
	"Start",
	"Stop",
	"RX up",
	"TX up",
	"RX down",
	"TX down",
	"Restart",
};

/**
 * Events of the channel statemachine
 */
enum ch_events {
	/**
	 * Events, representing return code of
	 * I/O operations (do_IO, halt_IO et al.)
	 */
	CH_EVENT_IO_SUCCESS,
	CH_EVENT_IO_EBUSY,
	CH_EVENT_IO_ENODEV,
	CH_EVENT_IO_EIO,
	CH_EVENT_IO_UNKNOWN,

	CH_EVENT_ATTNBUSY,
	CH_EVENT_ATTN,
	CH_EVENT_BUSY,

	/**
	 * Events, representing unit-check
	 */
	CH_EVENT_UC_RCRESET,
	CH_EVENT_UC_RSRESET,
	CH_EVENT_UC_TXTIMEOUT,
	CH_EVENT_UC_TXPARITY,
	CH_EVENT_UC_HWFAIL,
	CH_EVENT_UC_RXPARITY,
	CH_EVENT_UC_ZERO,
	CH_EVENT_UC_UNKNOWN,

	/**
	 * Events, representing subchannel-check
	 */
	CH_EVENT_SC_UNKNOWN,

	/**
	 * Events, representing machine checks
	 */
	CH_EVENT_MC_FAIL,
	CH_EVENT_MC_GOOD,

	/**
	 * Event, representing normal IRQ
	 */
	CH_EVENT_IRQ,
	CH_EVENT_FINSTAT,

	/**
	 * Event, representing timer expiry.
	 */
	CH_EVENT_TIMER,

	/**
	 * Events, representing commands from upper levels.
	 */
	CH_EVENT_START,
	CH_EVENT_STOP,

	/**
	 * MUST be always the last element!!
	 */
	NR_CH_EVENTS,
};

static const char *ch_event_names[] = {
	"do_IO success",
	"do_IO busy",
	"do_IO enodev",
	"do_IO ioerr",
	"do_IO unknown",

	"Status ATTN & BUSY",
	"Status ATTN",
	"Status BUSY",

	"Unit check remote reset",
	"Unit check remote system reset",
	"Unit check TX timeout",
	"Unit check TX parity",
	"Unit check Hardware failure",
	"Unit check RX parity",
	"Unit check ZERO",
	"Unit check Unknown",

	"SubChannel check Unknown",

	"Machine check failure",
	"Machine check operational",

	"IRQ normal",
	"IRQ final",

	"Timer",

	"Start",
	"Stop",
};

/**
 * States of the channel statemachine.
 */
enum ch_states {
	/**
	 * Channel not assigned to any device,
	 * initial state, direction invalid
	 */
	CH_STATE_IDLE,

	/**
	 * Channel assigned but not operating
	 */
	CH_STATE_STOPPED,
	CH_STATE_STARTWAIT,
	CH_STATE_STARTRETRY,
	CH_STATE_SETUPWAIT,
	CH_STATE_RXINIT,
	CH_STATE_TXINIT,
	CH_STATE_RX,
	CH_STATE_TX,
	CH_STATE_RXIDLE,
	CH_STATE_TXIDLE,
	CH_STATE_RXERR,
	CH_STATE_TXERR,
	CH_STATE_TERM,
	CH_STATE_DTERM,
	CH_STATE_NOTOP,

	/**
	 * MUST be always the last element!!
	 */
	NR_CH_STATES,
};

static const char *ch_state_names[] = {
	"Idle",
	"Stopped",
	"StartWait",
	"StartRetry",
	"SetupWait",
	"RX init",
	"TX init",
	"RX",
	"TX",
	"RX idle",
	"TX idle",
	"RX error",
	"TX error",
	"Terminating",
	"Restarting",
	"Not operational",
};


#ifdef DEBUG
/**
 * Dump header and first 16 bytes of an sk_buff for debugging purposes.
 *
 * @param skb    The sk_buff to dump.
 * @param offset Offset relative to skb-data, where to start the dump.
 */
static void ctc_dump_skb(struct sk_buff *skb, int offset)
{
	unsigned char *p = skb->data;
	__u16 bl;
	ll_header *header;
	int i;

	if (!(loglevel & CTC_LOGLEVEL_DEBUG))
		return;
	p += offset;
	bl = *((__u16*)p);
	p += 2;
	header = (ll_header *)p;
	p -= 2;
	
	printk(KERN_DEBUG "dump:\n");
	printk(KERN_DEBUG "blocklen=%d %04x\n", bl, bl);

	printk(KERN_DEBUG "h->length=%d %04x\n", header->length,
	       header->length); 
	printk(KERN_DEBUG "h->type=%04x\n", header->type); 
	printk(KERN_DEBUG "h->unused=%04x\n", header->unused);
	if (bl > 16)
		bl = 16;
	printk(KERN_DEBUG "data: ");
	for (i = 0; i < bl; i++)
		printk("%02x%s", *p++, (i % 16) ? " " : "\n<7>");
	printk("\n");
}
#endif

/**
 * Unpack a just received skb and hand it over to
 * upper layers.
 *
 * @param ch The channel where this skb has been received.
 * @param pskb The received skb.
 */
static __inline__ void ctc_unpack_skb(channel *ch, struct sk_buff *pskb)
{
	net_device     *dev = ch->netdev;
	ctc_priv       *privptr = (ctc_priv *)dev->priv;

	__u16 len = *((__u16*)pskb->data);
	skb_put(pskb, 2 + LL_HEADER_LENGTH);
	skb_pull(pskb, 2);
	pskb->dev = dev;
	pskb->ip_summed = CHECKSUM_UNNECESSARY;
	while (len > 0) {
		struct sk_buff *skb;
		ll_header *header = (ll_header *)pskb->data;

		skb_pull(pskb, LL_HEADER_LENGTH);
		if ((ch->protocol == CTC_PROTO_S390) &&
		    (header->type != ETH_P_IP)) {
#ifndef DEBUG
		        if (!(ch->logflags & LOG_FLAG_ILLEGALPKT)) {
#endif
				/**
				 * Check packet type only if we stick strictly
				 * to S/390's protocol of OS390. This only
				 * supports IP. Otherwise allow any packet
				 * type.
				 */
				if (loglevel & CTC_LOGLEVEL_WARN)
					printk(KERN_WARNING
					       "%s Illegal packet type 0x%04x "
					       "received, dropping\n",
				       dev->name, header->type);
				ch->logflags |= LOG_FLAG_ILLEGALPKT;
#ifndef DEBUG
			}
#endif

#ifdef DEBUG
			ctc_dump_skb(pskb, -6);
#endif
			privptr->stats.rx_dropped++;
			privptr->stats.rx_frame_errors++;
			return;
		}
		pskb->protocol = ntohs(header->type);
		if (header->length <= LL_HEADER_LENGTH) {
#ifndef DEBUG
		        if (!(ch->logflags & LOG_FLAG_ILLEGALSIZE)) {
#endif
				if (loglevel & CTC_LOGLEVEL_WARN)
					printk(KERN_WARNING
					       "%s Illegal packet size %d "
					       "received (MTU=%d blocklen=%d), "
					       "dropping\n", dev->name, header->length,
					       dev->mtu, len);
				ch->logflags |= LOG_FLAG_ILLEGALSIZE;
#ifndef DEBUG
			}
#endif
#ifdef DEBUG
			ctc_dump_skb(pskb, -6);
#endif
			privptr->stats.rx_dropped++;
			privptr->stats.rx_length_errors++;
			return;
		}
		header->length -= LL_HEADER_LENGTH;
		len -= LL_HEADER_LENGTH;
		if ((header->length > skb_tailroom(pskb)) ||
		    (header->length > len)) {
#ifndef DEBUG
		        if (!(ch->logflags & LOG_FLAG_OVERRUN)) {
#endif
				if (loglevel & CTC_LOGLEVEL_WARN)
					printk(KERN_WARNING
					       "%s Illegal packet size %d "
					       "(beyond the end of received data), "
					       "dropping\n", dev->name, header->length);
				ch->logflags |= LOG_FLAG_OVERRUN;
#ifndef DEBUG
			}
#endif
#ifdef DEBUG
			ctc_dump_skb(pskb, -6);
#endif
			privptr->stats.rx_dropped++;
			privptr->stats.rx_length_errors++;
			return;
		}
		skb_put(pskb, header->length);
		pskb->mac.raw = pskb->data;
		len -= header->length;
		skb = dev_alloc_skb(pskb->len);
		if (!skb) {
#ifndef DEBUG
		        if (!(ch->logflags & LOG_FLAG_NOMEM)) {
#endif
				if (loglevel & CTC_LOGLEVEL_WARN)
					printk(KERN_WARNING
					       "%s Out of memory in ctc_unpack_skb\n",
					       dev->name);
				ch->logflags |= LOG_FLAG_NOMEM;
#ifndef DEBUG
			}
#endif
			privptr->stats.rx_dropped++;
			return;
		}
		memcpy(skb_put(skb, pskb->len), pskb->data, pskb->len);
		skb->mac.raw = skb->data;
		skb->dev = pskb->dev;
		skb->protocol = pskb->protocol;
		pskb->ip_summed = CHECKSUM_UNNECESSARY;
		if (ch->protocol == CTC_PROTO_LINUX_TTY)
			ctc_tty_netif_rx(skb);
		else
			netif_rx(skb);
		/**
		 * Successful rx; reset logflags
		 */
		ch->logflags = 0;
		privptr->stats.rx_packets++;
		privptr->stats.rx_bytes += skb->len;
		if (len > 0) {
			skb_pull(pskb, header->length);
			if (skb_tailroom(pskb) < LL_HEADER_LENGTH) {
#ifndef DEBUG
				if (!(ch->logflags & LOG_FLAG_OVERRUN)) {
#endif
					if (loglevel & CTC_LOGLEVEL_WARN)
						printk(KERN_WARNING
						       "%s Buffer overrun in ctc_unpack_skb\n",
						       dev->name);
					ch->logflags |= LOG_FLAG_OVERRUN;
#ifndef DEBUG
				}
#endif
				return;
			}
	                skb_put(pskb, LL_HEADER_LENGTH);
		}
	}
}

/**
 * Bottom half routine.
 *
 * @param ch The channel to work on.
 */
static void ctc_bh(channel *ch)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&ch->io_queue)))
		ctc_unpack_skb(ch, skb);
}

/**
 * Check return code of a preceeding do_IO, halt_IO etc...
 *
 * @param ch          The channel, the error belongs to.
 * @param return_code The error code to inspect.
 */
static void inline ccw_check_return_code (channel *ch, int return_code)
{
	switch (return_code) {
		case 0:
			fsm_event(ch->fsm, CH_EVENT_IO_SUCCESS, ch);
			break;
		case -EBUSY:
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING "ch-%04x: Busy !\n", ch->devno);
			fsm_event(ch->fsm, CH_EVENT_IO_EBUSY, ch);
			break;
		case -ENODEV:
			if (loglevel & CTC_LOGLEVEL_EMERG)
				printk(KERN_EMERG
				       "ch-%04x: Invalid device called for IO\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_IO_ENODEV, ch);
			break;
		case -EIO:
			if (loglevel & CTC_LOGLEVEL_EMERG)
				printk(KERN_EMERG
				       "ch-%04x: Status pending... \n", ch->devno);
			fsm_event(ch->fsm, CH_EVENT_IO_EIO, ch);
			break;
		default:
			if (loglevel & CTC_LOGLEVEL_EMERG)
				printk(KERN_EMERG
				       "ch-%04x: Unknown error in do_IO %04x\n",
				       ch->devno, return_code);
			fsm_event(ch->fsm, CH_EVENT_IO_UNKNOWN, ch);
	}
}

/**
 * Check sense of a unit check.
 *
 * @param ch    The channel, the sense code belongs to.
 * @param sense The sense code to inspect.
 */
static void inline ccw_unit_check (channel *ch, unsigned char sense) {
	if (sense & SNS0_INTERVENTION_REQ) {
		if (sense & 0x01)  {
			if (ch->protocol != CTC_PROTO_LINUX_TTY)
				if (loglevel & CTC_LOGLEVEL_DEBUG)
					printk(KERN_DEBUG
					       "ch-%04x: Interface disc. or Sel. reset "
					       "(remote)\n", ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_RCRESET, ch);
		} else {
			if (loglevel & CTC_LOGLEVEL_DEBUG)
				printk(KERN_DEBUG "ch-%04x: System reset (remote)\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_RSRESET, ch);
		}
	} else if (sense & SNS0_EQUIPMENT_CHECK) {
		if (sense & SNS0_BUS_OUT_CHECK) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ch-%04x: Hardware malfunction (remote)\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_HWFAIL, ch);
		} else {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ch-%04x: Read-data parity error (remote)\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_RXPARITY, ch);
		}
	} else if (sense & SNS0_BUS_OUT_CHECK) {
		if (sense & 0x04) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ch-%04x: Data-streaming timeout)\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_TXTIMEOUT, ch);
		} else {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ch-%04x: Data-transfer parity error\n",
				       ch->devno);
			fsm_event(ch->fsm, CH_EVENT_UC_TXPARITY, ch);
		}
	} else if (sense & SNS0_CMD_REJECT) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING "ch-%04x: Command reject\n",
				       ch->devno);
	} else if (sense == 0) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "ch-%04x: Unit check ZERO\n", ch->devno);
		fsm_event(ch->fsm, CH_EVENT_UC_ZERO, ch);
	} else {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING
			       "ch-%04x: Unit Check with sense code: %02x\n",
			       ch->devno, sense);
		fsm_event(ch->fsm, CH_EVENT_UC_UNKNOWN, ch);
	}
}

static void ctc_purge_skb_queue(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(q))) {
		atomic_dec(&skb->users);
		dev_kfree_skb_irq(skb);
	}
}

static __inline__ int ctc_checkalloc_buffer(channel *ch, int warn) {
	if ((ch->trans_skb == NULL) ||
	    (ch->flags & CHANNEL_FLAGS_BUFSIZE_CHANGED)) {
		if (ch->trans_skb != NULL)
			dev_kfree_skb(ch->trans_skb);
		clear_normalized_cda(&ch->ccw[1]);
		ch->trans_skb = __dev_alloc_skb(ch->max_bufsize,
						GFP_ATOMIC|GFP_DMA);
		if (ch->trans_skb == NULL) {
			if (warn && (loglevel & CTC_LOGLEVEL_WARN))
				printk(KERN_WARNING
				       "ch-%04x: Couldn't alloc %s trans_skb\n",
				       ch->devno,
				       (CHANNEL_DIRECTION(ch->flags) == READ) ?
				       "RX" : "TX");
			return -ENOMEM;
		}
		ch->ccw[1].count = ch->max_bufsize;
		if (set_normalized_cda(&ch->ccw[1],
				       virt_to_phys(ch->trans_skb->data))) {
			dev_kfree_skb(ch->trans_skb);
			ch->trans_skb = NULL;
			if (warn && (loglevel & CTC_LOGLEVEL_WARN))
				printk(KERN_WARNING
				       "ch-%04x: set_normalized_cda for %s "
				       "trans_skb failed, dropping packets\n",
				       ch->devno,
				       (CHANNEL_DIRECTION(ch->flags) == READ) ?
				       "RX" : "TX");
			return -ENOMEM;
		}
		ch->ccw[1].count = 0;
		ch->trans_skb_data = ch->trans_skb->data;
		ch->flags &= ~CHANNEL_FLAGS_BUFSIZE_CHANGED;
	}
	return 0;
}

/**
 * Dummy NOP action for statemachines
 */
static void fsm_action_nop(fsm_instance *fi, int event, void *arg)
{
}

/**
 * Actions for channel - statemachines.
 *****************************************************************************/

/**
 * Normal data has been send. Free the corresponding
 * skb (it's in io_queue), reset dev->tbusy and
 * revert to idle state.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_txdone(fsm_instance *fi, int event, void *arg)
{
	channel        *ch = (channel *)arg;
	net_device     *dev = ch->netdev;
	ctc_priv       *privptr = dev->priv;
	struct sk_buff *skb;
	int            first = 1;
	int            i;

	struct timeval done_stamp = xtime;
	unsigned long duration = 
		(done_stamp.tv_sec - ch->prof.send_stamp.tv_sec) * 1000000 +
		done_stamp.tv_usec - ch->prof.send_stamp.tv_usec;
	if (duration > ch->prof.tx_time)
		ch->prof.tx_time = duration;

	if ((ch->devstat->rescnt != 0) && (loglevel & CTC_LOGLEVEL_DEBUG))
		printk(KERN_DEBUG "%s: TX not complete, remaining %d bytes\n",
		       dev->name, ch->devstat->rescnt);
	
	fsm_deltimer(&ch->timer);
	while ((skb = skb_dequeue(&ch->io_queue))) {
		privptr->stats.tx_packets++;
		privptr->stats.tx_bytes += skb->len - LL_HEADER_LENGTH;
		if (first) {
			privptr->stats.tx_bytes += 2;
			first = 0;
		}
		atomic_dec(&skb->users);
		dev_kfree_skb_irq(skb);
	}
	spin_lock(&ch->collect_lock);
	clear_normalized_cda(&ch->ccw[4]);
	if (ch->collect_len > 0) {
		int rc;

		if (ctc_checkalloc_buffer(ch, 1)) {
			spin_unlock(&ch->collect_lock);
			return;
		}
		ch->trans_skb->tail = ch->trans_skb->data = ch->trans_skb_data;
		ch->trans_skb->len = 0;
		if (ch->prof.maxmulti < (ch->collect_len + 2))
			ch->prof.maxmulti = ch->collect_len + 2;
		if (ch->prof.maxcqueue < skb_queue_len(&ch->collect_queue))
			ch->prof.maxcqueue = skb_queue_len(&ch->collect_queue);
		*((__u16 *)skb_put(ch->trans_skb, 2)) = ch->collect_len + 2;
		i = 0;
		while ((skb = skb_dequeue(&ch->collect_queue))) {
			memcpy(skb_put(ch->trans_skb, skb->len), skb->data,
			       skb->len);
			privptr->stats.tx_packets++;
			privptr->stats.tx_bytes += skb->len - LL_HEADER_LENGTH;
			atomic_dec(&skb->users);
			dev_kfree_skb_irq(skb);
			i++;
		}
		ch->collect_len = 0;
		spin_unlock(&ch->collect_lock);
		ch->ccw[1].count = ch->trans_skb->len;
		fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);
		ch->prof.send_stamp = xtime;
		rc = do_IO(ch->irq, &ch->ccw[0], (intparm_t)ch, 0xff, 0);
		ch->prof.doios_multi++;
		if (rc != 0) {
			privptr->stats.tx_dropped += i;
			privptr->stats.tx_errors += i;
			fsm_deltimer(&ch->timer);
			ccw_check_return_code(ch, rc);
		}
	} else {
		spin_unlock(&ch->collect_lock);
		fsm_newstate(fi, CH_STATE_TXIDLE);
	}
	ctc_clear_busy(dev);
}

/**
 * Initial data is sent.
 * Notify device statemachine that we are up and
 * running.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_txidle(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;

	fsm_deltimer(&ch->timer);
	fsm_newstate(fi, CH_STATE_TXIDLE);
	fsm_event(((ctc_priv *)ch->netdev->priv)->fsm, DEV_EVENT_TXUP,
		  ch->netdev);
}

/**
 * Got normal data, check for sanity, queue it up, allocate new buffer
 * trigger bottom half, and initiate next read.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_rx(fsm_instance *fi, int event, void *arg)
{
	channel        *ch = (channel *)arg;
	net_device     *dev = ch->netdev;
	ctc_priv       *privptr = dev->priv;
	int            len = ch->max_bufsize - ch->devstat->rescnt;
	struct sk_buff *skb = ch->trans_skb;
	__u16          block_len = *((__u16*)skb->data);
	int            check_len;
	int            rc;

	fsm_deltimer(&ch->timer);
	if (len < 8) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: got packet with length %d < 8\n",
			       dev->name, len);
		privptr->stats.rx_dropped++;
		privptr->stats.rx_length_errors++;
		goto again;
	}
	if (len > ch->max_bufsize) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: got packet with length %d > %d\n",
			       dev->name, len, ch->max_bufsize);
		privptr->stats.rx_dropped++;
		privptr->stats.rx_length_errors++;
		goto again;
	}

	/**
	 * VM TCP seems to have a bug sending 2 trailing bytes of garbage.
	 */
	switch (ch->protocol) {
		case CTC_PROTO_S390:
		case CTC_PROTO_OS390:
			check_len = block_len + 2;
			break;
		default:
			check_len = block_len;
			break;
	}
	if ((len < block_len) || (len > check_len)) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: got block length %d != rx length %d\n",
			       dev->name, block_len, len);
#ifdef DEBUG
		ctc_dump_skb(skb, 0);
#endif
		*((__u16*)skb->data) = len;
		privptr->stats.rx_dropped++;
		privptr->stats.rx_length_errors++;
		goto again;
	}
	block_len -= 2;
	if (block_len > 0) {
		*((__u16*)skb->data) = block_len;
		ctc_unpack_skb(ch, skb);
	}
 again:
	skb->data = skb->tail = ch->trans_skb_data;
	skb->len = 0;
	if (ctc_checkalloc_buffer(ch, 1))
		return;
	ch->ccw[1].count = ch->max_bufsize;
	rc = do_IO(ch->irq, &ch->ccw[0], (intparm_t)ch, 0xff, 0);
	if (rc != 0)
		ccw_check_return_code(ch, rc);
}

static void ch_action_rxidle(fsm_instance *fi, int event, void *arg);

/**
 * Initialize connection by sending a __u16 of value 0.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_firstio(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	int     rc;

	if (fsm_getstate(fi) == CH_STATE_TXIDLE) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "ch-%04x: remote side issued READ?, "
			       "init ...\n", ch->devno);
	}
	fsm_deltimer(&ch->timer);
	if (ctc_checkalloc_buffer(ch, 1))
		return;
	if ((fsm_getstate(fi) == CH_STATE_SETUPWAIT) &&
	    (ch->protocol == CTC_PROTO_OS390)) {
		/* OS/390 resp. z/OS */
		if (CHANNEL_DIRECTION(ch->flags) == READ) {
			*((__u16 *)ch->trans_skb->data) = CTC_INITIAL_BLOCKLEN;
			fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC,
				     CH_EVENT_TIMER, ch);
			ch_action_rxidle(fi, event, arg);
		} else {
			net_device *dev = ch->netdev;
			fsm_newstate(fi, CH_STATE_TXIDLE);
			fsm_event(((ctc_priv *)dev->priv)->fsm,
				  DEV_EVENT_TXUP, dev);
		}
		return;
	}

	/**
	 * Don´t setup a timer for receiving the initial RX frame
	 * if in compatibility mode, since VM TCP delays the initial
	 * frame until it has some data to send.
	 */
	if ((CHANNEL_DIRECTION(ch->flags) == WRITE) ||
	    (ch->protocol != CTC_PROTO_S390))
		fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);

	*((__u16 *)ch->trans_skb->data) = CTC_INITIAL_BLOCKLEN;
	ch->ccw[1].count = 2; /* Transfer only length */

	fsm_newstate(fi, (CHANNEL_DIRECTION(ch->flags) == READ)
		     ? CH_STATE_RXINIT : CH_STATE_TXINIT);
	rc = do_IO(ch->irq, &ch->ccw[0], (intparm_t)ch, 0xff, 0);
	if (rc != 0) {
		fsm_deltimer(&ch->timer);
		fsm_newstate(fi, CH_STATE_SETUPWAIT);
		ccw_check_return_code(ch, rc);
	}
	/**
	 * If in compatibility mode since we don´t setup a timer, we
	 * also signal RX channel up immediately. This enables us
	 * to send packets early which in turn usually triggers some
	 * reply from VM TCP which brings up the RX channel to it´s
	 * final state.
	 */
	if ((CHANNEL_DIRECTION(ch->flags) == READ) &&
	    (ch->protocol == CTC_PROTO_S390)) {
		net_device *dev = ch->netdev;
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXUP, dev);
	}
}

/**
 * Got initial data, check it. If OK,
 * notify device statemachine that we are up and
 * running.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_rxidle(fsm_instance *fi, int event, void *arg)
{
	channel    *ch = (channel *)arg;
	net_device *dev = ch->netdev;
	__u16      buflen;
	int        rc;

	fsm_deltimer(&ch->timer);
	buflen = *((__u16 *)ch->trans_skb->data);
#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "%s: Initial RX count %d\n", dev->name, buflen);
#endif
	if (buflen >= CTC_INITIAL_BLOCKLEN) {
		if (ctc_checkalloc_buffer(ch, 1))
			return;
		ch->ccw[1].count = ch->max_bufsize;
		fsm_newstate(fi, CH_STATE_RXIDLE);
		rc = do_IO(ch->irq, &ch->ccw[0], (intparm_t)ch, 0xff, 0);
		if (rc != 0) {
			fsm_newstate(fi, CH_STATE_RXINIT);
			ccw_check_return_code(ch, rc);
		} else
			fsm_event(((ctc_priv *)dev->priv)->fsm,
				  DEV_EVENT_RXUP, dev);
	} else {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: Initial RX count %d not %d\n",
			       dev->name, buflen, CTC_INITIAL_BLOCKLEN);
		ch_action_firstio(fi, event, arg);
	}
}

/**
 * Set channel into extended mode.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_setmode(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	int     rc;
	unsigned long saveflags;

	fsm_deltimer(&ch->timer);
	fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);
	fsm_newstate(fi, CH_STATE_SETUPWAIT);
	if (event == CH_EVENT_TIMER)
		s390irq_spin_lock_irqsave(ch->irq, saveflags);
	rc = do_IO(ch->irq, &ch->ccw[6], (intparm_t)ch, 0xff, 0);
	if (event == CH_EVENT_TIMER)
		s390irq_spin_unlock_irqrestore(ch->irq, saveflags);
	if (rc != 0) {
		fsm_deltimer(&ch->timer);
		fsm_newstate(fi, CH_STATE_STARTWAIT);
		ccw_check_return_code(ch, rc);
	} else
		ch->retry = 0;
}

/**
 * Setup channel.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_start(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	unsigned long saveflags;
	int     rc;
	net_device *dev;

	if (ch == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ch_action_start ch=NULL\n");
		return;
	}
	if (ch->netdev == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ch_action_start dev=NULL, irq=%d\n",
			       ch->irq);
		return;
	}
	dev = ch->netdev;

#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "%s: %s channel start\n", dev->name,
		       (CHANNEL_DIRECTION(ch->flags) == READ) ? "RX" : "TX");
#endif

	if (ch->trans_skb != NULL) {
		clear_normalized_cda(&ch->ccw[1]);
		dev_kfree_skb(ch->trans_skb);
		ch->trans_skb = NULL;
	}
	if (CHANNEL_DIRECTION(ch->flags) == READ) {
		ch->ccw[1].cmd_code = CCW_CMD_READ;
		ch->ccw[1].flags    = CCW_FLAG_SLI;
		ch->ccw[1].count    = 0;
	} else {
		ch->ccw[1].cmd_code = CCW_CMD_WRITE;
		ch->ccw[1].flags    = CCW_FLAG_SLI | CCW_FLAG_CC;
		ch->ccw[1].count    = 0;
	}
	if (ctc_checkalloc_buffer(ch, 0)) {
		if (loglevel & CTC_LOGLEVEL_NOTICE)
			printk(KERN_NOTICE
			       "%s: Could not allocate %s trans_skb, delaying "
			       "allocation until first transfer\n",
			       dev->name, 
			       (CHANNEL_DIRECTION(ch->flags) == READ) ? "RX" : "TX");
	}

#if LINUX_VERSION_CODE >= 0x020400
	INIT_LIST_HEAD(&ch->tq.list);
#else
	ch->tq.next = NULL;
#endif
	ch->tq.sync    = 0;
	ch->tq.routine = (void *)(void *)ctc_bh;
	ch->tq.data    = ch;

	ch->ccw[0].cmd_code = CCW_CMD_PREPARE;
	ch->ccw[0].flags    = CCW_FLAG_SLI | CCW_FLAG_CC;
	ch->ccw[0].count    = 0;
	ch->ccw[0].cda	    = 0;
	ch->ccw[2].cmd_code = CCW_CMD_NOOP;	 /* jointed CE + DE */
	ch->ccw[2].flags    = CCW_FLAG_SLI;
	ch->ccw[2].count    = 0;
	ch->ccw[2].cda	    = 0;
	memcpy(&ch->ccw[3], &ch->ccw[0], sizeof(ccw1_t) * 3);
	ch->ccw[4].cda	    = 0;
	ch->ccw[4].flags    &= ~CCW_FLAG_IDA;

	fsm_newstate(fi, CH_STATE_STARTWAIT);
	fsm_addtimer(&ch->timer, 1000, CH_EVENT_TIMER, ch);
	s390irq_spin_lock_irqsave(ch->irq, saveflags);
	rc = halt_IO(ch->irq, (intparm_t)ch, 0);
	s390irq_spin_unlock_irqrestore(ch->irq, saveflags);
	if (rc != 0) {
		fsm_deltimer(&ch->timer);
		ccw_check_return_code(ch, rc);
	}
#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "ctc: %s(): leaving\n", __FUNCTION__);
#endif
}

/**
 * Shutdown a channel.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_haltio(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	unsigned long saveflags;
	int     rc;
	int     oldstate;

	fsm_deltimer(&ch->timer);
	fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);
	if (event == CH_EVENT_STOP)
		s390irq_spin_lock_irqsave(ch->irq, saveflags);
	oldstate = fsm_getstate(fi);
	fsm_newstate(fi, CH_STATE_TERM);
	rc = halt_IO (ch->irq, (intparm_t)ch, 0);
	if (event == CH_EVENT_STOP)
		s390irq_spin_unlock_irqrestore(ch->irq, saveflags);
	if (rc != 0) {
		fsm_deltimer(&ch->timer);
		fsm_newstate(fi, oldstate);
		ccw_check_return_code(ch, rc);
	}
}

/**
 * A channel has successfully been halted.
 * Cleanup it's queue and notify interface statemachine.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_stopped(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	fsm_deltimer(&ch->timer);
	fsm_newstate(fi, CH_STATE_STOPPED);
	if (ch->trans_skb != NULL) {
		clear_normalized_cda(&ch->ccw[1]);
		dev_kfree_skb(ch->trans_skb);
		ch->trans_skb = NULL;
	}
	if (CHANNEL_DIRECTION(ch->flags) == READ) {
		skb_queue_purge(&ch->io_queue);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
	} else {
		ctc_purge_skb_queue(&ch->io_queue);
		spin_lock(&ch->collect_lock);
		ctc_purge_skb_queue(&ch->collect_queue);
		ch->collect_len = 0;
		spin_unlock(&ch->collect_lock);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);
	}
}

/**
 * A stop command from device statemachine arrived and we are in
 * not operational mode. Set state to stopped.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_stop(fsm_instance *fi, int event, void *arg)
{
	fsm_newstate(fi, CH_STATE_STOPPED);
}

/**
 * A machine check for no path, not operational status or gone device has
 * happened.
 * Cleanup queue and notify interface statemachine.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_fail(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	fsm_deltimer(&ch->timer);
	fsm_newstate(fi, CH_STATE_NOTOP);
	if (CHANNEL_DIRECTION(ch->flags) == READ) {
		skb_queue_purge(&ch->io_queue);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
	} else {
		ctc_purge_skb_queue(&ch->io_queue);
		spin_lock(&ch->collect_lock);
		ctc_purge_skb_queue(&ch->collect_queue);
		ch->collect_len = 0;
		spin_unlock(&ch->collect_lock);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);
	}
}

/**
 * Handle error during setup of channel.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_setuperr(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	/**
	 * Special case: Got UC_RCRESET on setmode.
	 * This means that remote side isn't setup. In this case
	 * simply retry after some 10 secs...
	 */
	if ((fsm_getstate(fi) == CH_STATE_SETUPWAIT) &&
	    ((event == CH_EVENT_UC_RCRESET) ||
	     (event == CH_EVENT_UC_RSRESET)   )         ) {
		fsm_newstate(fi, CH_STATE_STARTRETRY);
		fsm_deltimer(&ch->timer);
		fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);
		if (CHANNEL_DIRECTION(ch->flags) == READ) {
			int rc = halt_IO (ch->irq, (intparm_t)ch, 0);
			if (rc != 0)
				ccw_check_return_code(ch, rc);
		}
		return;
	}

	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "%s: Error %s during %s channel setup state=%s\n",
		       dev->name, ch_event_names[event],
		       (CHANNEL_DIRECTION(ch->flags) == READ) ? "RX" : "TX",
		       fsm_getstate_str(fi));
	if (CHANNEL_DIRECTION(ch->flags) == READ) {
		fsm_newstate(fi, CH_STATE_RXERR);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
	} else {
		fsm_newstate(fi, CH_STATE_TXERR);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);
	}
}

/**
 * Restart a channel after an error.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_restart(fsm_instance *fi, int event, void *arg)
{
	unsigned long saveflags;
	int   oldstate;
	int   rc;

	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	fsm_deltimer(&ch->timer);
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "%s: %s channel restart\n", dev->name,
		       (CHANNEL_DIRECTION(ch->flags) == READ) ? "RX" : "TX");
	fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC, CH_EVENT_TIMER, ch);
	oldstate = fsm_getstate(fi);
	fsm_newstate(fi, CH_STATE_STARTWAIT);
	if (event == CH_EVENT_TIMER)
		s390irq_spin_lock_irqsave(ch->irq, saveflags);
	rc = halt_IO (ch->irq, (intparm_t)ch, 0);
	if (event == CH_EVENT_TIMER)
		s390irq_spin_unlock_irqrestore(ch->irq, saveflags);
	if (rc != 0) {
		fsm_deltimer(&ch->timer);
		fsm_newstate(fi, oldstate);
		ccw_check_return_code(ch, rc);
	}
}

/**
 * Handle error during RX initial handshake (exchange of
 * 0-length block header)
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_rxiniterr(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	if (event == CH_EVENT_TIMER) {
		fsm_deltimer(&ch->timer);
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: Timeout during RX init handshake\n",
			       dev->name);
		if (ch->retry++ < 3)
			ch_action_restart(fi, event, arg);
		else {
			fsm_newstate(fi, CH_STATE_RXERR);
			fsm_event(((ctc_priv *)dev->priv)->fsm,
				  DEV_EVENT_RXDOWN, dev);
		}
	} else
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "%s: Error during RX init handshake\n",
			       dev->name);
}

/**
 * Notify device statemachine if we gave up initialization
 * of RX channel.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_rxinitfail(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	fsm_newstate(fi, CH_STATE_RXERR);
	if (loglevel & CTC_LOGLEVEL_WARN) {
		printk(KERN_WARNING "%s: RX initialization failed\n", dev->name);
		printk(KERN_WARNING "%s: RX <-> RX connection detected\n", dev->name);
	}
	fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
}

/**
 * Handle RX Unit check remote reset (remote disconnected)
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_rxdisc(fsm_instance *fi, int event, void *arg)
{
	channel    *ch = (channel *)arg;
	channel    *ch2;
	net_device *dev = ch->netdev;

	fsm_deltimer(&ch->timer);
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG "%s: Got remote disconnect, re-initializing ...\n",
		       dev->name);

	/**
	 * Notify device statemachine
	 */
	fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
	fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);

	fsm_newstate(fi, CH_STATE_DTERM);
	ch2 = ((ctc_priv *)dev->priv)->channel[WRITE];
	fsm_newstate(ch2->fsm, CH_STATE_DTERM);

	halt_IO(ch->irq, (intparm_t)ch, 0);
	halt_IO(ch2->irq, (intparm_t)ch2, 0);
}

/**
 * Handle error during TX channel initialization.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_txiniterr(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	if (event == CH_EVENT_TIMER) {
		fsm_deltimer(&ch->timer);
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: Timeout during TX init handshake\n",
			       dev->name);
		if (ch->retry++ < 3)
			ch_action_restart(fi, event, arg);
		else {
			fsm_newstate(fi, CH_STATE_TXERR);
			fsm_event(((ctc_priv *)dev->priv)->fsm,
				  DEV_EVENT_TXDOWN, dev);
		}
	} else
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "%s: Error during TX init handshake\n",
			       dev->name);
}

/**
 * Handle TX timeout by retrying operation.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_txretry(fsm_instance *fi, int event, void *arg)
{
	channel    *ch = (channel *)arg;
	net_device *dev = ch->netdev;
	unsigned long saveflags;

	fsm_deltimer(&ch->timer);
	if (ch->retry++ > 3) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: TX retry failed, restarting channel\n",
			       dev->name);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);
		ch_action_restart(fi, event, arg);
	} else {
		struct sk_buff *skb;

		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: TX retry %d\n", dev->name, ch->retry);
		if ((skb = skb_peek(&ch->io_queue))) {
			int rc = 0;

			clear_normalized_cda(&ch->ccw[4]);
			ch->ccw[4].count = skb->len;
			if (set_normalized_cda(&ch->ccw[4],
					       virt_to_phys(skb->data))) {
				if (loglevel & CTC_LOGLEVEL_DEBUG)
					printk(KERN_DEBUG "%s: IDAL alloc failed, "
					       "restarting channel\n", dev->name);
				fsm_event(((ctc_priv *)dev->priv)->fsm,
					  DEV_EVENT_TXDOWN, dev);
				ch_action_restart(fi, event, arg);
				return;
			}
			fsm_addtimer(&ch->timer, 1000, CH_EVENT_TIMER, ch);
			if (event == CH_EVENT_TIMER)
				s390irq_spin_lock_irqsave(ch->irq, saveflags);
			rc = do_IO(ch->irq, &ch->ccw[3], (intparm_t)ch, 0xff, 0);
			if (event == CH_EVENT_TIMER)
				s390irq_spin_unlock_irqrestore(ch->irq,
							       saveflags);
			if (rc != 0) {
				fsm_deltimer(&ch->timer);
				ccw_check_return_code(ch, rc);
				ctc_purge_skb_queue(&ch->io_queue);
			}
		}
	}

}

/**
 * Handle fatal errors during an I/O command.
 *
 * @param fi    An instance of a channel statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from channel * upon call.
 */
static void ch_action_iofatal(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;

	fsm_deltimer(&ch->timer);
	if (CHANNEL_DIRECTION(ch->flags) == READ) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: RX I/O error\n", dev->name);
		fsm_newstate(fi, CH_STATE_RXERR);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_RXDOWN, dev);
	} else {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: TX I/O error\n", dev->name);
		fsm_newstate(fi, CH_STATE_TXERR);
		fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_TXDOWN, dev);
	}
}

static void ch_action_reinit(fsm_instance *fi, int event, void *arg)
{
	channel *ch = (channel *)arg;
	net_device *dev = ch->netdev;
	ctc_priv   *privptr = dev->priv;

	ch_action_iofatal(fi, event, arg);
	fsm_addtimer(&privptr->restart_timer, 1000, DEV_EVENT_RESTART, dev);
}

/**
 * The statemachine for a channel.
 */
static const fsm_node ch_fsm[] = {
	{ CH_STATE_STOPPED,    CH_EVENT_STOP,       fsm_action_nop       },
	{ CH_STATE_STOPPED,    CH_EVENT_START,      ch_action_start      },
	{ CH_STATE_STOPPED,    CH_EVENT_FINSTAT,    fsm_action_nop       },
	{ CH_STATE_STOPPED,    CH_EVENT_MC_FAIL,    fsm_action_nop       },

	{ CH_STATE_NOTOP,      CH_EVENT_STOP,       ch_action_stop       },
	{ CH_STATE_NOTOP,      CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_NOTOP,      CH_EVENT_FINSTAT,    fsm_action_nop       },
	{ CH_STATE_NOTOP,      CH_EVENT_MC_FAIL,    fsm_action_nop       },
	{ CH_STATE_NOTOP,      CH_EVENT_MC_GOOD,    ch_action_start      },

	{ CH_STATE_STARTWAIT,  CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_STARTWAIT,  CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_STARTWAIT,  CH_EVENT_FINSTAT,    ch_action_setmode    },
	{ CH_STATE_STARTWAIT,  CH_EVENT_TIMER,      ch_action_setuperr   },
	{ CH_STATE_STARTWAIT,  CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_STARTWAIT,  CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_STARTWAIT,  CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_STARTRETRY, CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_STARTRETRY, CH_EVENT_TIMER,      ch_action_setmode    },
	{ CH_STATE_STARTRETRY, CH_EVENT_FINSTAT,    fsm_action_nop       },
	{ CH_STATE_STARTRETRY, CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_SETUPWAIT,  CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_FINSTAT,    ch_action_firstio    },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_UC_RCRESET, ch_action_setuperr   },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_UC_RSRESET, ch_action_setuperr   },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_TIMER,      ch_action_setmode    },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_SETUPWAIT,  CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_RXINIT,     CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_RXINIT,     CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_RXINIT,     CH_EVENT_FINSTAT,    ch_action_rxidle     },
	{ CH_STATE_RXINIT,     CH_EVENT_UC_RCRESET, ch_action_rxiniterr  },
	{ CH_STATE_RXINIT,     CH_EVENT_UC_RSRESET, ch_action_rxiniterr  },
	{ CH_STATE_RXINIT,     CH_EVENT_TIMER,      ch_action_rxiniterr  },
	{ CH_STATE_RXINIT,     CH_EVENT_ATTNBUSY,   ch_action_rxinitfail },
	{ CH_STATE_RXINIT,     CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_RXINIT,     CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_RXINIT,     CH_EVENT_UC_ZERO,    ch_action_firstio    },
	{ CH_STATE_RXINIT,     CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_RXIDLE,     CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_RXIDLE,     CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_RXIDLE,     CH_EVENT_FINSTAT,    ch_action_rx         },
	{ CH_STATE_RXIDLE,     CH_EVENT_UC_RCRESET, ch_action_rxdisc     },
//	{ CH_STATE_RXIDLE,     CH_EVENT_UC_RSRESET, ch_action_rxretry    },
	{ CH_STATE_RXIDLE,     CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_RXIDLE,     CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_RXIDLE,     CH_EVENT_MC_FAIL,    ch_action_fail       },
	{ CH_STATE_RXIDLE,     CH_EVENT_UC_ZERO,    ch_action_rx         },

	{ CH_STATE_TXINIT,     CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_TXINIT,     CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_TXINIT,     CH_EVENT_FINSTAT,    ch_action_txidle     },
	{ CH_STATE_TXINIT,     CH_EVENT_UC_RCRESET, ch_action_txiniterr  },
	{ CH_STATE_TXINIT,     CH_EVENT_UC_RSRESET, ch_action_txiniterr  },
	{ CH_STATE_TXINIT,     CH_EVENT_TIMER,      ch_action_txiniterr  },
	{ CH_STATE_TXINIT,     CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_TXINIT,     CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_TXINIT,     CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_TXIDLE,     CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_TXIDLE,     CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_TXIDLE,     CH_EVENT_FINSTAT,    ch_action_firstio    },
	{ CH_STATE_TXIDLE,     CH_EVENT_UC_RCRESET, fsm_action_nop       },
	{ CH_STATE_TXIDLE,     CH_EVENT_UC_RSRESET, fsm_action_nop       },
	{ CH_STATE_TXIDLE,     CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_TXIDLE,     CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_TXIDLE,     CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_TERM,       CH_EVENT_STOP,       fsm_action_nop       },
	{ CH_STATE_TERM,       CH_EVENT_START,      ch_action_restart    },
	{ CH_STATE_TERM,       CH_EVENT_FINSTAT,    ch_action_stopped    },
	{ CH_STATE_TERM,       CH_EVENT_UC_RCRESET, fsm_action_nop       },
	{ CH_STATE_TERM,       CH_EVENT_UC_RSRESET, fsm_action_nop       },
	{ CH_STATE_TERM,       CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_DTERM,      CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_DTERM,      CH_EVENT_START,      ch_action_restart    },
	{ CH_STATE_DTERM,      CH_EVENT_FINSTAT,    ch_action_setmode    },
	{ CH_STATE_DTERM,      CH_EVENT_UC_RCRESET, fsm_action_nop       },
	{ CH_STATE_DTERM,      CH_EVENT_UC_RSRESET, fsm_action_nop       },
	{ CH_STATE_DTERM,      CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_TX,         CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_TX,         CH_EVENT_START,      fsm_action_nop       },
	{ CH_STATE_TX,         CH_EVENT_FINSTAT,    ch_action_txdone     },
	{ CH_STATE_TX,         CH_EVENT_UC_RCRESET, ch_action_txretry    },
	{ CH_STATE_TX,         CH_EVENT_UC_RSRESET, ch_action_txretry    },
	{ CH_STATE_TX,         CH_EVENT_TIMER,      ch_action_txretry    },
	{ CH_STATE_TX,         CH_EVENT_IO_ENODEV,  ch_action_iofatal    },
	{ CH_STATE_TX,         CH_EVENT_IO_EIO,     ch_action_reinit     },
	{ CH_STATE_TX,         CH_EVENT_MC_FAIL,    ch_action_fail       },

	{ CH_STATE_RXERR,      CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_TXERR,      CH_EVENT_STOP,       ch_action_haltio     },
	{ CH_STATE_TXERR,      CH_EVENT_MC_FAIL,    ch_action_fail       },
	{ CH_STATE_RXERR,      CH_EVENT_MC_FAIL,    ch_action_fail       },
};

static const int CH_FSM_LEN = sizeof(ch_fsm) / sizeof(fsm_node);

/**
 * Functions related to setup and device detection.
 *****************************************************************************/

/**
 * Add a new channel to the list of channels.
 * Keeps the channel list sorted.
 *
 * @param irq   The IRQ to be used by the new channel.
 * @param devno The device number of the new channel.
 * @param type  The type class of the new channel.
 *
 * @return 0 on success, !0 on error.
 */
static int add_channel(int irq, __u16 devno, channel_type_t type)
{
	channel **c = &channels;
	channel *ch;
	char name[10];

	if ((ch = (channel *)kmalloc(sizeof(channel), GFP_KERNEL)) == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc: Out of memory in add_channel\n");
		return -1;
	}
	memset(ch, 0, sizeof(channel));
	if ((ch->ccw = (ccw1_t *)kmalloc(sizeof(ccw1_t) * 8,
					 GFP_KERNEL|GFP_DMA)) == NULL) {
		kfree(ch);
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc: Out of memory in add_channel\n");
		return -1;
	}

	/**
	 * "static" ccws are used in the following way:
	 *
	 * ccw[0..2] (Channel program for generic I/O):
	 *           0: prepare
	 *           1: read or write (depending on direction) with fixed
	 *              buffer (idal allocated once when buffer is allocated)
	 *           2: nop
	 * ccw[3..5] (Channel program for direct write of packets)
	 *           3: prepare
	 *           4: write (idal allocated on every write).
	 *           5: nop
	 * ccw[6..7] (Channel program for initial channel setup):
	 *           3: set extended mode
	 *           4: nop
	 *
	 * ch->ccw[0..5] are initialized in ch_action_start because
	 * the channel's direction is yet unknown here.
	 */
	ch->ccw[6].cmd_code = CCW_CMD_SET_EXTENDED;
	ch->ccw[6].flags    = CCW_FLAG_SLI;
	ch->ccw[6].count    = 0;
	ch->ccw[6].cda      = 0;
	
	ch->ccw[7].cmd_code = CCW_CMD_NOOP;
	ch->ccw[7].flags    = CCW_FLAG_SLI;
	ch->ccw[7].count    = 0;
	ch->ccw[7].cda      = 0;

	ch->irq = irq;
	ch->devno = devno;
	ch->type = type;
	loglevel = CTC_LOGLEVEL_DEFAULT;
	sprintf(name, "ch-%04x", devno);
	ch->fsm = init_fsm(name, ch_state_names,
			ch_event_names, NR_CH_STATES, NR_CH_EVENTS,
			ch_fsm, CH_FSM_LEN, GFP_KERNEL);
	if (ch->fsm == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING
			       "ctc: Could not create FSM in add_channel\n");
		kfree(ch);
		return -1;
	}
	fsm_newstate(ch->fsm, CH_STATE_IDLE);
	if ((ch->devstat = (devstat_t*)kmalloc(sizeof(devstat_t), GFP_KERNEL))
	    == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc: Out of memory in add_channel\n");
		kfree_fsm(ch->fsm);
		kfree(ch);
		return -1;
	}
	memset(ch->devstat, 0, sizeof(devstat_t));
	while (*c && ((*c)->devno < devno))
		c = &(*c)->next;
	if ((*c)->devno == devno) {
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG
			       "ctc: add_channel: device %04x already in list, "
			       "using old entry\n", (*c)->devno);
		kfree(ch->devstat);
		kfree_fsm(ch->fsm);
		kfree(ch);
		return 0;
	}
	fsm_settimer(ch->fsm, &ch->timer);
	skb_queue_head_init(&ch->io_queue);
	skb_queue_head_init(&ch->collect_queue);
	ch->next = *c;
	*c = ch;
	return 0;
}

#ifndef CTC_CHANDEV
/**
 * scan for all channels and create an entry in the channels list
 * for every supported channel.
 */
static void channel_scan(void)
{
	static int      print_result = 1;
	int	        irq;
	int             nr_escon = 0;
	int             nr_ctca  = 0;
	s390_dev_info_t di;

	for (irq = 0; irq < NR_IRQS; irq++) {
		if (get_dev_info_by_irq(irq, &di) == 0) {
			if ((di.status == DEVSTAT_NOT_OPER) ||
			    (di.status == DEVSTAT_DEVICE_OWNED))
				continue;
			switch (channel_type(&di.sid_data)) {
				case channel_type_ctca:
					/* CTC/A */
					if (!add_channel(irq, di.devno,
							 channel_type_ctca))
						nr_ctca++;
					break;
				case channel_type_escon:
					/* ESCON */
					if (!add_channel(irq, di.devno,
							 channel_type_escon))
						nr_escon++;
					break;
			default:
			}
		}
	}
	if (print_result) {
		if (loglevel & CTC_LOGLEVEL_INFO) {
			if (nr_escon + nr_ctca)
				printk(KERN_INFO
				       "ctc: %d CTC/A channel%s and %d ESCON "
				       "channel%s found.\n",
				       nr_ctca, (nr_ctca == 1) ? "s" : "",
				       nr_escon, (nr_escon == 1) ? "s" : "");
			else
				printk(KERN_INFO "ctc: No channel devices found.\n");
		}
	}
	print_result = 0;
}
#endif

/**
 * Release a specific channel in the channel list.
 *
 * @param ch Pointer to channel struct to be released.
 */
static void channel_free(channel *ch)
{
	ch->flags &= ~CHANNEL_FLAGS_INUSE;
	fsm_newstate(ch->fsm, CH_STATE_IDLE);
}

/**
 * Remove a specific channel in the channel list.
 *
 * @param ch Pointer to channel struct to be released.
 */
static void channel_remove(channel *ch)
{
	channel **c = &channels;

	if (ch == NULL)
		return;

#ifndef CTC_CHANDEV
	if (ch->flags & CHANNEL_FLAGS_INUSE)
		FREE_IRQ(ch->irq, ch->devstat);
#endif
	channel_free(ch);
	while (*c) {
		if (*c == ch) {
			*c = ch->next;
			fsm_deltimer(&ch->timer);
			kfree_fsm(ch->fsm);
			clear_normalized_cda(&ch->ccw[4]);
			if (ch->trans_skb != NULL) {
				clear_normalized_cda(&ch->ccw[1]);
				dev_kfree_skb(ch->trans_skb);
			}
			kfree(ch->ccw);
			return;
		}
		c = &((*c)->next);
	}
}


/**
 * Get a specific channel from the channel list.
 *
 * @param type Type of channel we are interested in.
 * @param devno Device number of channel we are interested in.
 * @param direction Direction we want to use this channel for.
 *
 * @return Pointer to a channel or NULL if no matching channel available.
 */
static channel *channel_get(channel_type_t type, int devno, int direction)
{
	channel *ch = channels;

#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG
		       "ctc: %s(): searching for ch with devno %d and type %d\n",
		       __FUNCTION__, devno, type);
#endif

	while (ch && ((ch->devno != devno) || (ch->type != type))) {
#ifdef DEBUG
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG
			       "ctc: %s(): ch=0x%p (devno=%d, type=%d\n",
			       __FUNCTION__, ch, ch->devno, ch->type);
#endif
		ch = ch->next;
	}
#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG
		       "ctc: %s(): ch=0x%pq (devno=%d, type=%d\n",
		       __FUNCTION__, ch, ch->devno, ch->type);
#endif
	if (!ch) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc: %s(): channel with devno %d "
			       "and type %d not found in channel list\n",
			       __FUNCTION__, devno, type);
	}
	else {
		if (ch->flags & CHANNEL_FLAGS_INUSE)
			ch = NULL;
		else {
			ch->flags |= CHANNEL_FLAGS_INUSE;
			ch->flags &= ~CHANNEL_FLAGS_RWMASK;
			ch->flags |= (direction == WRITE)
				? CHANNEL_FLAGS_WRITE:CHANNEL_FLAGS_READ;
			fsm_newstate(ch->fsm, CH_STATE_STOPPED);
		}
	}
	return ch;
}

#ifndef CTC_CHANDEV
/**
 * Get the next free channel from the channel list
 *
 * @param type Type of channel we are interested in.
 * @param direction Direction we want to use this channel for.
 *
 * @return Pointer to a channel or NULL if no matching channel available.
 */
static channel *channel_get_next(channel_type_t type, int direction)
{
	channel *ch = channels;

	while (ch && (ch->type != type || (ch->flags & CHANNEL_FLAGS_INUSE)))
		ch = ch->next;
	if (ch) {
		ch->flags |= CHANNEL_FLAGS_INUSE;
		ch->flags &= ~CHANNEL_FLAGS_RWMASK;
		ch->flags |= (direction == WRITE)
			? CHANNEL_FLAGS_WRITE:CHANNEL_FLAGS_READ;
		fsm_newstate(ch->fsm, CH_STATE_STOPPED);
	}
	return ch;
}
#endif

/**
 * Return the channel type by name.
 *
 * @param name Name of network interface.
 *
 * @return Type class of channel to be used for that interface.
 */
static channel_type_t inline extract_channel_media(char *name)
{
	channel_type_t ret = channel_type_unknown;

	if (name != NULL) {
		if (strncmp(name, "ctc", 3) == 0)
			ret = channel_type_ctca;
		if (strncmp(name, "escon", 5) == 0)
			ret = channel_type_escon;
	}
	return ret;
}

/**
 * Find a channel in the list by its IRQ.
 *
 * @param irq IRQ to search for.
 *
 * @return Pointer to channel or NULL if no matching channel found.
 */
static channel *find_channel_by_irq(int irq)
{
	channel *ch = channels;
	while (ch && (ch->irq != irq))
		ch = ch->next;
	return ch;
}

/**
 * Main IRQ handler.
 *
 * @param irq     The IRQ to handle.
 * @param intparm IRQ params.
 * @param regs    CPU registers.
 */
static void ctc_irq_handler (int irq, void *intparm, struct pt_regs *regs)
{
	devstat_t  *devstat = (devstat_t *)intparm;
	channel    *ch = (channel *)devstat->intparm;
	net_device *dev;

	/**
	 * Check for unsolicited interrupts.
	 * If intparm is NULL, then loop over all our known
	 * channels and try matching the irq number.
	 */
	if (ch == NULL) {
		if ((ch = find_channel_by_irq(irq)) == NULL) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: Got unsolicited irq: %04x c-%02x d-%02x"
				       "f-%02x\n", devstat->devno, devstat->cstat,
				       devstat->dstat, devstat->flag);
			goto done;
		}
	}

	dev = (net_device *)(ch->netdev);
	if (dev == NULL) {
		if (loglevel & CTC_LOGLEVEL_CRIT)
			printk(KERN_CRIT
			       "ctc: ctc_irq_handler dev = NULL irq=%d, ch=0x%p\n",
			       irq, ch);
		goto done;
	}
	if ((intparm == NULL) && (loglevel & CTC_LOGLEVEL_DEBUG))
		printk(KERN_DEBUG "%s: Channel %04x found by IRQ %d\n",
		       dev->name, ch->devno, irq);

#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG
		       "%s: interrupt for device: %04x received c-%02x d-%02x "
		       "f-%02x\n", dev->name, devstat->devno, devstat->cstat,
		       devstat->dstat, devstat->flag);
#endif

	/* Check for good subchannel return code, otherwise error message */
	if (devstat->cstat) {
		fsm_event(ch->fsm, CH_EVENT_SC_UNKNOWN, ch);
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING
			       "%s: subchannel check for device: %04x - %02x %02x "
			       "%02x\n", dev->name, ch->devno, devstat->cstat,
			       devstat->dstat, devstat->flag);
		goto done;
	}

	/* Check the reason-code of a unit check */
	if (devstat->dstat & DEV_STAT_UNIT_CHECK) {
		ccw_unit_check(ch, devstat->ii.sense.data[0]);
		goto done;
	}
	if (devstat->dstat & DEV_STAT_BUSY) {
		if (devstat->dstat & DEV_STAT_ATTENTION)
			fsm_event(ch->fsm, CH_EVENT_ATTNBUSY, ch);
		else
			fsm_event(ch->fsm, CH_EVENT_BUSY, ch);
		goto done;
	}
	if (devstat->dstat & DEV_STAT_ATTENTION) {
		fsm_event(ch->fsm, CH_EVENT_ATTN, ch);
		goto done;
	}
	if (devstat->flag & DEVSTAT_FINAL_STATUS)
		fsm_event(ch->fsm, CH_EVENT_FINSTAT, ch);
	else
		fsm_event(ch->fsm, CH_EVENT_IRQ, ch);

 done:
}

/**
 * Actions for interface - statemachine.
 *****************************************************************************/

/**
 * Startup channels by sending CH_EVENT_START to each channel.
 *
 * @param fi    An instance of an interface statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from net_device * upon call.
 */
static void dev_action_start(fsm_instance *fi, int event, void *arg)
{
	net_device *dev = (net_device *)arg;
	ctc_priv   *privptr = dev->priv;
	int        direction;

	fsm_deltimer(&privptr->restart_timer);
	fsm_newstate(fi, DEV_STATE_STARTWAIT_RXTX);
	for (direction = READ; direction <= WRITE; direction++) {
		channel *ch = privptr->channel[direction];
		fsm_event(ch->fsm, CH_EVENT_START, ch);
	}
}

/**
 * Shutdown channels by sending CH_EVENT_STOP to each channel.
 *
 * @param fi    An instance of an interface statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from net_device * upon call.
 */
static void dev_action_stop(fsm_instance *fi, int event, void *arg)
{
	net_device *dev = (net_device *)arg;
	ctc_priv   *privptr = dev->priv;
	int        direction;

	fsm_newstate(fi, DEV_STATE_STOPWAIT_RXTX);
	for (direction = READ; direction <= WRITE; direction++) {
		channel *ch = privptr->channel[direction];
		fsm_event(ch->fsm, CH_EVENT_STOP, ch);
	}
}

static void dev_action_restart(fsm_instance *fi, int event, void *arg)
{
    net_device *dev = (net_device *)arg;
    ctc_priv   *privptr = dev->priv;

    if (loglevel & CTC_LOGLEVEL_DEBUG)
	    printk(KERN_DEBUG "%s: Restarting\n", dev->name);
    dev_action_stop(fi, event, arg);
    fsm_event(privptr->fsm, DEV_EVENT_STOP, dev);
    fsm_addtimer(&privptr->restart_timer, CTC_TIMEOUT_5SEC,
		 DEV_EVENT_START, dev);
}

/**
 * Called from channel statemachine
 * when a channel is up and running.
 *
 * @param fi    An instance of an interface statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from net_device * upon call.
 */
static void dev_action_chup(fsm_instance *fi, int event, void *arg)
{
	net_device *dev = (net_device *)arg;
	ctc_priv   *privptr = dev->priv;

	switch (fsm_getstate(fi)) {
		case DEV_STATE_STARTWAIT_RXTX:
			if (event == DEV_EVENT_RXUP)
				fsm_newstate(fi, DEV_STATE_STARTWAIT_TX);
			else
				fsm_newstate(fi, DEV_STATE_STARTWAIT_RX);
			break;
		case DEV_STATE_STARTWAIT_RX:
			if (event == DEV_EVENT_RXUP) {
				fsm_newstate(fi, DEV_STATE_RUNNING);
				if (loglevel & CTC_LOGLEVEL_INFO)
					printk(KERN_INFO
					       "%s: connected with remote side\n",
					       dev->name);
				if (privptr->protocol == CTC_PROTO_LINUX_TTY)
					ctc_tty_setcarrier(dev, 1);
				ctc_clear_busy(dev);
			}
			break;
		case DEV_STATE_STARTWAIT_TX:
			if (event == DEV_EVENT_TXUP) {
				fsm_newstate(fi, DEV_STATE_RUNNING);
				if (loglevel & CTC_LOGLEVEL_INFO)
					printk(KERN_INFO
					       "%s: connected with remote side\n",
					       dev->name);
				if (privptr->protocol == CTC_PROTO_LINUX_TTY)
					ctc_tty_setcarrier(dev, 1);
				ctc_clear_busy(dev);
			}
			break;
		case DEV_STATE_STOPWAIT_TX:
			if (event == DEV_EVENT_RXUP)
				fsm_newstate(fi, DEV_STATE_STOPWAIT_RXTX);
			break;
		case DEV_STATE_STOPWAIT_RX:
			if (event == DEV_EVENT_TXUP)
				fsm_newstate(fi, DEV_STATE_STOPWAIT_RXTX);
			break;
	}
}

/**
 * Called from channel statemachine
 * when a channel has been shutdown.
 *
 * @param fi    An instance of an interface statemachine.
 * @param event The event, just happened.
 * @param arg   Generic pointer, casted from net_device * upon call.
 */
static void dev_action_chdown(fsm_instance *fi, int event, void *arg)
{
	net_device *dev = (net_device *)arg;
	ctc_priv   *privptr = dev->priv;

	switch (fsm_getstate(fi)) {
		case DEV_STATE_RUNNING:
			if (privptr->protocol == CTC_PROTO_LINUX_TTY)
				ctc_tty_setcarrier(dev, 0);
			if (event == DEV_EVENT_TXDOWN)
				fsm_newstate(fi, DEV_STATE_STARTWAIT_TX);
			else
				fsm_newstate(fi, DEV_STATE_STARTWAIT_RX);
			break;
		case DEV_STATE_STARTWAIT_RX:
			if (event == DEV_EVENT_TXDOWN)
				fsm_newstate(fi, DEV_STATE_STARTWAIT_RXTX);
			break;
		case DEV_STATE_STARTWAIT_TX:
			if (event == DEV_EVENT_RXDOWN)
				fsm_newstate(fi, DEV_STATE_STARTWAIT_RXTX);
			break;
		case DEV_STATE_STOPWAIT_RXTX:
			if (event == DEV_EVENT_TXDOWN)
				fsm_newstate(fi, DEV_STATE_STOPWAIT_RX);
			else
				fsm_newstate(fi, DEV_STATE_STOPWAIT_TX);
			break;
		case DEV_STATE_STOPWAIT_RX:
			if (event == DEV_EVENT_RXDOWN)
				fsm_newstate(fi, DEV_STATE_STOPPED);
			break;
		case DEV_STATE_STOPWAIT_TX:
			if (event == DEV_EVENT_TXDOWN)
				fsm_newstate(fi, DEV_STATE_STOPPED);
			break;
	}
}

static const fsm_node dev_fsm[] = {
	{ DEV_STATE_STOPPED,        DEV_EVENT_START,   dev_action_start   },

	{ DEV_STATE_STOPWAIT_RXTX,  DEV_EVENT_START,   dev_action_start   },
	{ DEV_STATE_STOPWAIT_RXTX,  DEV_EVENT_RXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STOPWAIT_RXTX,  DEV_EVENT_TXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STOPWAIT_RXTX,  DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_STOPWAIT_RX,    DEV_EVENT_START,   dev_action_start   },
	{ DEV_STATE_STOPWAIT_RX,    DEV_EVENT_RXUP,    dev_action_chup    },
	{ DEV_STATE_STOPWAIT_RX,    DEV_EVENT_TXUP,    dev_action_chup    },
	{ DEV_STATE_STOPWAIT_RX,    DEV_EVENT_RXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STOPWAIT_RX,    DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_STOPWAIT_TX,    DEV_EVENT_START,   dev_action_start   },
	{ DEV_STATE_STOPWAIT_TX,    DEV_EVENT_RXUP,    dev_action_chup    },
	{ DEV_STATE_STOPWAIT_TX,    DEV_EVENT_TXUP,    dev_action_chup    },
	{ DEV_STATE_STOPWAIT_TX,    DEV_EVENT_TXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STOPWAIT_TX,    DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_STOP,    dev_action_stop    },
	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_RXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_TXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_RXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_TXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STARTWAIT_RXTX, DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_STARTWAIT_TX,   DEV_EVENT_STOP,    dev_action_stop    },
	{ DEV_STATE_STARTWAIT_TX,   DEV_EVENT_RXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_TX,   DEV_EVENT_TXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_TX,   DEV_EVENT_RXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STARTWAIT_TX,   DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_STARTWAIT_RX,   DEV_EVENT_STOP,    dev_action_stop    },
	{ DEV_STATE_STARTWAIT_RX,   DEV_EVENT_RXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_RX,   DEV_EVENT_TXUP,    dev_action_chup    },
	{ DEV_STATE_STARTWAIT_RX,   DEV_EVENT_TXDOWN,  dev_action_chdown  },
	{ DEV_STATE_STARTWAIT_RX,   DEV_EVENT_RESTART, dev_action_restart },

	{ DEV_STATE_RUNNING,        DEV_EVENT_STOP,    dev_action_stop    },
	{ DEV_STATE_RUNNING,        DEV_EVENT_RXDOWN,  dev_action_chdown  },
	{ DEV_STATE_RUNNING,        DEV_EVENT_TXDOWN,  dev_action_chdown  },
	{ DEV_STATE_RUNNING,        DEV_EVENT_TXUP,    fsm_action_nop     },
	{ DEV_STATE_RUNNING,        DEV_EVENT_RXUP,    fsm_action_nop     },
	{ DEV_STATE_RUNNING,        DEV_EVENT_RESTART, dev_action_restart },
};

static const int DEV_FSM_LEN = sizeof(dev_fsm) / sizeof(fsm_node);

/**
 * Transmit a packet.
 * This is a helper function for ctc_tx().
 *
 * @param ch Channel to be used for sending.
 * @param skb Pointer to struct sk_buff of packet to send.
 *            The linklevel header has already been set up
 *            by ctc_tx().
 *
 * @return 0 on success, -ERRNO on failure. (Never fails.)
 */
static int transmit_skb(channel *ch, struct sk_buff *skb) {
	unsigned long saveflags;
	ll_header header;
	int       rc = 0;

	if (fsm_getstate(ch->fsm) != CH_STATE_TXIDLE) {
		int l = skb->len + LL_HEADER_LENGTH;

		spin_lock_irqsave(&ch->collect_lock, saveflags);
		if (ch->collect_len + l > ch->max_bufsize - 2)
			rc = -EBUSY;
		else {
			atomic_inc(&skb->users);
			header.length = l;
			header.type = skb->protocol;
			header.unused = 0;
			memcpy(skb_push(skb, LL_HEADER_LENGTH), &header,
			       LL_HEADER_LENGTH);
			skb_queue_tail(&ch->collect_queue, skb);
			ch->collect_len += l;
		}
		spin_unlock_irqrestore(&ch->collect_lock, saveflags);
	} else {
		__u16 block_len;
		int ccw_idx;
		struct sk_buff *nskb;
		unsigned long hi;

		/**
		 * Protect skb against beeing free'd by upper
		 * layers.
		 */
		atomic_inc(&skb->users);
		ch->prof.txlen += skb->len;
		header.length = skb->len + LL_HEADER_LENGTH;
		header.type = skb->protocol;
		header.unused = 0;
		memcpy(skb_push(skb, LL_HEADER_LENGTH), &header,
		       LL_HEADER_LENGTH);
		block_len = skb->len + 2;
		*((__u16 *)skb_push(skb, 2)) = block_len;

		/**
		 * IDAL support in CTC is broken, so we have to
		 * care about skb's above 2G ourselves.
		 */
		hi = ((unsigned long)skb->tail + LL_HEADER_LENGTH) >> 31;
		if (hi) {
			nskb = alloc_skb(skb->len, GFP_ATOMIC | GFP_DMA);
			if (!nskb) {
				atomic_dec(&skb->users);
				skb_pull(skb, LL_HEADER_LENGTH + 2);
				return -ENOMEM;
			} else {
				memcpy(skb_put(nskb, skb->len),
				       skb->data, skb->len);
				atomic_inc(&nskb->users);
				atomic_dec(&skb->users);
				dev_kfree_skb_irq(skb);
				skb = nskb;
			}
		}

		ch->ccw[4].count = block_len;
		if (set_normalized_cda(&ch->ccw[4], virt_to_phys(skb->data))) {
			/**
			 * idal allocation failed, try via copying to
			 * trans_skb. trans_skb usually has a pre-allocated
			 * idal.
			 */
			if (ctc_checkalloc_buffer(ch, 1)) {
				/**
				 * Remove our header. It gets added
				 * again on retransmit.
				 */
				atomic_dec(&skb->users);
				skb_pull(skb, LL_HEADER_LENGTH + 2);
				return -EBUSY;
			}

			ch->trans_skb->tail = ch->trans_skb->data;
			ch->trans_skb->len = 0;
			ch->ccw[1].count = skb->len;
			memcpy(skb_put(ch->trans_skb, skb->len), skb->data,
			       skb->len);
			atomic_dec(&skb->users);
			dev_kfree_skb_irq(skb);
			ccw_idx = 0;
		} else {
			skb_queue_tail(&ch->io_queue, skb);
			ccw_idx = 3;
		}
		ch->retry = 0;
		fsm_newstate(ch->fsm, CH_STATE_TX);
		fsm_addtimer(&ch->timer, CTC_TIMEOUT_5SEC,
			     CH_EVENT_TIMER, ch);
		s390irq_spin_lock_irqsave(ch->irq, saveflags);
		ch->prof.send_stamp = xtime;
		rc = do_IO(ch->irq, &ch->ccw[ccw_idx], (intparm_t)ch, 0xff, 0);
		s390irq_spin_unlock_irqrestore(ch->irq, saveflags);
		if (ccw_idx == 3)
			ch->prof.doios_single++;
		if (rc != 0) {
			fsm_deltimer(&ch->timer);
			ccw_check_return_code(ch, rc);
			if (ccw_idx == 3)
				skb_dequeue_tail(&ch->io_queue);
			/**
			 * Remove our header. It gets added
			 * again on retransmit.
			 */
			skb_pull(skb, LL_HEADER_LENGTH + 2);
		} else {
			if (ccw_idx == 0) {
				net_device *dev = ch->netdev;
				ctc_priv   *privptr = dev->priv;
				privptr->stats.tx_packets++;
				privptr->stats.tx_bytes +=
					skb->len - LL_HEADER_LENGTH;
			}
		}
	}

	return rc;
}

/**
 * Interface API for upper network layers
 *****************************************************************************/

/**
 * Open an interface.
 * Called from generic network layer when ifconfig up is run.
 *
 * @param dev Pointer to interface struct.
 *
 * @return 0 on success, -ERRNO on failure. (Never fails.)
 */
static int ctc_open(net_device *dev) {
	MOD_INC_USE_COUNT;
	fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_START, dev);
	return 0;
}

/**
 * Close an interface.
 * Called from generic network layer when ifconfig down is run.
 *
 * @param dev Pointer to interface struct.
 *
 * @return 0 on success, -ERRNO on failure. (Never fails.)
 */
static int ctc_close(net_device *dev) {
	SET_DEVICE_START(dev, 0);
	fsm_event(((ctc_priv *)dev->priv)->fsm, DEV_EVENT_STOP, dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

/**
 * Start transmission of a packet.
 * Called from generic network device layer.
 *
 * @param skb Pointer to buffer containing the packet.
 * @param dev Pointer to interface struct.
 *
 * @return 0 if packet consumed, !0 if packet rejected.
 *         Note: If we return !0, then the packet is free'd by
 *               the generic network layer.
 */
static int ctc_tx(struct sk_buff *skb, net_device *dev)
{
	int       rc = 0;
	ctc_priv  *privptr = (ctc_priv *)dev->priv;

	/**
	 * Some sanity checks ...
	 */
	if (skb == NULL) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "%s: NULL sk_buff passed\n", dev->name);
		privptr->stats.tx_dropped++;
		return 0;
	}
	if (skb_headroom(skb) < (LL_HEADER_LENGTH + 2)) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING
			       "%s: Got sk_buff with head room < %ld bytes\n",
			       dev->name, LL_HEADER_LENGTH + 2);
		dev_kfree_skb(skb);
		privptr->stats.tx_dropped++;
		return 0;
	}

	/**
	 * If channels are not running, try to restart them
	 * notify anybody about a link failure and throw
	 * away packet. 
	 */
	if (fsm_getstate(privptr->fsm) != DEV_STATE_RUNNING) {
		fsm_event(privptr->fsm, DEV_EVENT_START, dev);
		if (privptr->protocol == CTC_PROTO_LINUX_TTY)
			return -EBUSY;
		dev_kfree_skb(skb);
		privptr->stats.tx_dropped++;
		privptr->stats.tx_errors++;
		privptr->stats.tx_carrier_errors++;
		return 0;
	}

	if (ctc_test_and_set_busy(dev))
		return -EBUSY;

	dev->trans_start = jiffies;
	if (transmit_skb(privptr->channel[WRITE], skb) != 0)
		rc = 1;
	ctc_clear_busy(dev);
	return rc;
}


/**
 * Sets MTU of an interface.
 *
 * @param dev     Pointer to interface struct.
 * @param new_mtu The new MTU to use for this interface.
 *
 * @return 0 on success, -EINVAL if MTU is out of valid range.
 *         (valid range is 576 .. 65527). If VM is on the
 *         remote side, maximum MTU is 32760, however this is
 *         <em>not</em> checked here.
 */
static int ctc_change_mtu(net_device *dev, int new_mtu) {
	ctc_priv  *privptr = (ctc_priv *)dev->priv;

	if ((new_mtu < 576) || (new_mtu > 65527) ||
	    (new_mtu > (privptr->channel[READ]->max_bufsize -
			LL_HEADER_LENGTH - 2)))
		return -EINVAL;
	dev->mtu = new_mtu;
	dev->hard_header_len = LL_HEADER_LENGTH + 2;
	return 0;
}


/**
 * Returns interface statistics of a device.
 *
 * @param dev Pointer to interface struct.
 *
 * @return Pointer to stats struct of this interface.
 */
static struct net_device_stats *ctc_stats(net_device *dev) {
	return &((ctc_priv *)dev->priv)->stats;
}

/**
 * procfs related structures and routines
 *****************************************************************************/

static net_device *find_netdev_by_ino(unsigned long ino)
{
	channel *ch = channels;
	net_device *dev = NULL;
	ctc_priv *privptr;

	while (ch) {
		if (ch->netdev != dev) {
			dev = ch->netdev;
			privptr = (ctc_priv *)dev->priv;

			if ((privptr->proc_ctrl_entry->low_ino == ino) ||
			    (privptr->proc_stat_entry->low_ino == ino) ||
			    (privptr->proc_loglevel_entry->low_ino == ino))
				return dev;
		}
		ch = ch->next;
	}
	return NULL;
}

#if LINUX_VERSION_CODE < 0x020363
/**
 * Lock the module, if someone changes into
 * our proc directory.
 */
static void ctc_fill_inode(struct inode *inode, int fill)
{
	if (fill) {
		MOD_INC_USE_COUNT;
	} else
		MOD_DEC_USE_COUNT;
}
#endif

#define CTRL_BUFSIZE 40

static int ctc_ctrl_open(struct inode *inode, struct file *file)
{
	file->private_data = kmalloc(CTRL_BUFSIZE, GFP_KERNEL);
	if (file->private_data == NULL)
		return -ENOMEM;
	MOD_INC_USE_COUNT;
	return 0;
}

static int ctc_ctrl_close(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	MOD_DEC_USE_COUNT;
	return 0;
}

static ssize_t ctc_ctrl_write(struct file *file, const char *buf, size_t count,
			   loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	net_device   *dev;
	ctc_priv     *privptr;
	char         *e;
	int          bs1;
	char         tmp[40];

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	if (off != &file->f_pos)
		return -ESPIPE;

	privptr = (ctc_priv *)dev->priv;

	if (count >= 39)
		return -EINVAL;

	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count+1] = '\0';
	bs1 = simple_strtoul(tmp, &e, 0);

	if ((bs1 > CTC_BUFSIZE_LIMIT) ||
	    (e && (!isspace(*e))))
		return -EINVAL;
	if ((dev->flags & IFF_RUNNING) &&
	    (bs1 < (dev->mtu + LL_HEADER_LENGTH + 2)))
		return -EINVAL;
	if (bs1 < (576 + LL_HEADER_LENGTH + 2))
		return -EINVAL;


	privptr->channel[READ]->max_bufsize =
		privptr->channel[WRITE]->max_bufsize = bs1;
	if (!(dev->flags & IFF_RUNNING))
		dev->mtu = bs1 - LL_HEADER_LENGTH - 2;
	privptr->channel[READ]->flags |= CHANNEL_FLAGS_BUFSIZE_CHANGED;
	privptr->channel[WRITE]->flags |= CHANNEL_FLAGS_BUFSIZE_CHANGED;

	return count;
}

static ssize_t ctc_ctrl_read(struct file *file, char *buf, size_t count,
			  loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	char *sbuf = (char *)file->private_data;
	net_device *dev;
	ctc_priv *privptr;
	ssize_t ret = 0;
	char *p = sbuf;
	int l;

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	if (off != &file->f_pos)
		return -ESPIPE;

	privptr = (ctc_priv *)dev->priv;

	if (file->f_pos == 0)
		sprintf(sbuf, "%d\n", privptr->channel[READ]->max_bufsize);

	l = strlen(sbuf);
	p = sbuf;
	if (file->f_pos < l) {
		p += file->f_pos;
		l = strlen(p);
		ret = (count > l) ? l : count;
		if (copy_to_user(buf, p, ret))
			return -EFAULT;
	}
	file->f_pos += ret;
	return ret;
}

#define LOGLEVEL_BUFSIZE 10

static int ctc_loglevel_open(struct inode *inode, struct file *file)
{
	file->private_data = kmalloc(LOGLEVEL_BUFSIZE, GFP_KERNEL);
	if (file->private_data == NULL)
		return -ENOMEM;
	MOD_INC_USE_COUNT;
	return 0;
}

static int ctc_loglevel_close(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	MOD_DEC_USE_COUNT;
	return 0;
}

static ssize_t ctc_loglevel_write(struct file *file, const char *buf, size_t count,
			   loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	net_device   *dev;
	ctc_priv     *privptr;
	char         *e;
	int          bs1;
	char         tmp[40];

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	if (off != &file->f_pos)
		return -ESPIPE;

	privptr = (ctc_priv *)dev->priv;

	if (count >= 20)
		return -EINVAL;

	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count+1] = '\0';
	bs1 = simple_strtoul(tmp, &e, 0);

	if ((bs1 > CTC_LOGLEVEL_MAX) ||
	    (e && (!isspace(*e))))
		return -EINVAL;

	loglevel = bs1;
	return count;
}

static ssize_t ctc_loglevel_read(struct file *file, char *buf, size_t count,
			  loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	char *sbuf = (char *)file->private_data;
	net_device *dev;
	ctc_priv *privptr;
	ssize_t ret = 0;
	char *p = sbuf;
	int l;

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	if (off != &file->f_pos)
		return -ESPIPE;

	privptr = (ctc_priv *)dev->priv;

	if (file->f_pos == 0)
		sprintf(sbuf, "0x%02x\n", loglevel);

	l = strlen(sbuf);
	p = sbuf;
	if (file->f_pos < l) {
		p += file->f_pos;
		l = strlen(p);
		ret = (count > l) ? l : count;
		if (copy_to_user(buf, p, ret))
			return -EFAULT;
	}
	file->f_pos += ret;
	return ret;
}

#define STATS_BUFSIZE 2048

static int ctc_stat_open(struct inode *inode, struct file *file)
{
	file->private_data = kmalloc(STATS_BUFSIZE, GFP_KERNEL);
	if (file->private_data == NULL)
		return -ENOMEM;
	MOD_INC_USE_COUNT;
	return 0;
}

static int ctc_stat_close(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	MOD_DEC_USE_COUNT;
	return 0;
}

static ssize_t ctc_stat_write(struct file *file, const char *buf, size_t count,
			      loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	net_device *dev;
	ctc_priv *privptr;

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	privptr = (ctc_priv *)dev->priv;
	privptr->channel[WRITE]->prof.maxmulti = 0;
	privptr->channel[WRITE]->prof.maxcqueue = 0;
	privptr->channel[WRITE]->prof.doios_single = 0;
	privptr->channel[WRITE]->prof.doios_multi = 0;
	privptr->channel[WRITE]->prof.txlen = 0;
	privptr->channel[WRITE]->prof.tx_time = 0;
	return count;
}

static ssize_t ctc_stat_read(struct file *file, char *buf, size_t count,
			      loff_t *off)
{
	unsigned int ino = ((struct inode *)file->f_dentry->d_inode)->i_ino;
	char *sbuf = (char *)file->private_data;
	net_device *dev;
	ctc_priv *privptr;
	ssize_t ret = 0;
	char *p = sbuf;
	int l;

	if (!(dev = find_netdev_by_ino(ino)))
		return -ENODEV;
	if (off != &file->f_pos)
		return -ESPIPE;

	privptr = (ctc_priv *)dev->priv;

	if (file->f_pos == 0) {
		p += sprintf(p, "Device FSM state: %s\n",
			     fsm_getstate_str(privptr->fsm));
		p += sprintf(p, "RX channel FSM state: %s\n",
			     fsm_getstate_str(privptr->channel[READ]->fsm));
		p += sprintf(p, "TX channel FSM state: %s\n",
			     fsm_getstate_str(privptr->channel[WRITE]->fsm));
		p += sprintf(p, "Max. TX buffer used: %ld\n",
			     privptr->channel[WRITE]->prof.maxmulti);
		p += sprintf(p, "Max. chained SKBs: %ld\n",
			     privptr->channel[WRITE]->prof.maxcqueue);
		p += sprintf(p, "TX single write ops: %ld\n",
			     privptr->channel[WRITE]->prof.doios_single);
		p += sprintf(p, "TX multi write ops: %ld\n",
			     privptr->channel[WRITE]->prof.doios_multi);
		p += sprintf(p, "Netto bytes written: %ld\n",
			     privptr->channel[WRITE]->prof.txlen);
		p += sprintf(p, "Max. TX IO-time: %ld\n",
			     privptr->channel[WRITE]->prof.tx_time);
	}
	l = strlen(sbuf);
	p = sbuf;
	if (file->f_pos < l) {
		p += file->f_pos;
		l = strlen(p);
		ret = (count > l) ? l : count;
		if (copy_to_user(buf, p, ret))
			return -EFAULT;
	}
	file->f_pos += ret;
	return ret;
}

static struct file_operations ctc_stat_fops = {
	read:    ctc_stat_read,
	write:   ctc_stat_write,
	open:    ctc_stat_open,
	release: ctc_stat_close,
};

static struct file_operations ctc_ctrl_fops = {
	read:    ctc_ctrl_read,
	write:   ctc_ctrl_write,
	open:    ctc_ctrl_open,
	release: ctc_ctrl_close,
};

static struct file_operations ctc_loglevel_fops = {
	read:    ctc_loglevel_read,
	write:   ctc_loglevel_write,
	open:    ctc_loglevel_open,
	release: ctc_loglevel_close,
};

static struct inode_operations ctc_stat_iops = {
#if LINUX_VERSION_CODE < 0x020363
	default_file_ops: &ctc_stat_fops
#endif
};
static struct inode_operations ctc_ctrl_iops = {
#if LINUX_VERSION_CODE < 0x020363
	default_file_ops: &ctc_ctrl_fops
#endif
};
static struct inode_operations ctc_loglevel_iops = {
#if LINUX_VERSION_CODE < 0x020363
	default_file_ops: &ctc_loglevel_fops
#endif
};

static struct proc_dir_entry stat_entry = {
	0,                           /* low_ino */
	10,                          /* namelen */
	"statistics",                /* name    */
	S_IFREG | S_IRUGO | S_IWUSR, /* mode    */
	1,                           /* nlink   */
	0,                           /* uid     */
	0,                           /* gid     */
	0,                           /* size    */
	&ctc_stat_iops               /* ops     */
};

static struct proc_dir_entry ctrl_entry = {
	0,                           /* low_ino */
	10,                          /* namelen */
	"buffersize",                /* name    */
	S_IFREG | S_IRUSR | S_IWUSR, /* mode    */
	1,                           /* nlink   */
	0,                           /* uid     */
	0,                           /* gid     */
	0,                           /* size    */
	&ctc_ctrl_iops               /* ops     */
};

static struct proc_dir_entry loglevel_entry = {
	0,                           /* low_ino */
	8,                           /* namelen */
	"loglevel",                  /* name    */
	S_IFREG | S_IRUSR | S_IWUSR, /* mode    */
	1,                           /* nlink   */
	0,                           /* uid     */
	0,                           /* gid     */
	0,                           /* size    */
	&ctc_loglevel_iops           /* ops     */
};

#if LINUX_VERSION_CODE < 0x020363
static struct proc_dir_entry ctc_dir = {
	0,                           /* low_ino  */
	3,                           /* namelen  */
	"ctc",                       /* name     */
	S_IFDIR | S_IRUGO | S_IXUGO, /* mode     */
	2,                           /* nlink    */
	0,                           /* uid      */
	0,                           /* gid      */
	0,                           /* size     */
	0,                           /* ops      */
	0,                           /* get_info */
	ctc_fill_inode               /* fill_ino (for locking) */
};

static struct proc_dir_entry ctc_template =
{
	0,                           /* low_ino  */
	0,                           /* namelen  */
	"",                          /* name     */
	S_IFDIR | S_IRUGO | S_IXUGO, /* mode     */
	2,                           /* nlink    */
	0,                           /* uid      */
	0,                           /* gid      */
	0,                           /* size     */
	0,                           /* ops      */
	0,                           /* get_info */
	ctc_fill_inode               /* fill_ino (for locking) */
};
#else
static struct proc_dir_entry *ctc_dir = NULL;
static struct proc_dir_entry *ctc_template = NULL;
#endif

/**
 * Create the driver's main directory /proc/net/ctc
 */
static void ctc_proc_create_main(void) {
	/**
	 * If not registered, register main proc dir-entry now
	 */
#if LINUX_VERSION_CODE > 0x020362
#ifdef CONFIG_PROC_FS
	if (!ctc_dir)
		ctc_dir = proc_mkdir("ctc", proc_net);
#endif
#else
	if (ctc_dir.low_ino == 0)
		proc_net_register(&ctc_dir);
#endif
}

#ifdef MODULE
/**
 * Destroy /proc/net/ctc
 */
static void ctc_proc_destroy_main(void) {
#if LINUX_VERSION_CODE > 0x020362
#ifdef CONFIG_PROC_FS
	remove_proc_entry("ctc", proc_net);
#endif
#else
	proc_net_unregister(ctc_dir.low_ino);
#endif
}
#endif /* MODULE */

/**
 * Create a device specific subdirectory in /proc/net/ctc/ with the
 * same name like the device. In that directory, create 2 entries
 * "statistics" and "buffersize".
 *
 * @param dev The device for which the subdirectory should be created.
 *
 */
static void ctc_proc_create_sub(net_device *dev) {
	ctc_priv *privptr = dev->priv;

#if LINUX_VERSION_CODE > 0x020362
	privptr->proc_dentry = proc_mkdir(dev->name, ctc_dir);
	privptr->proc_stat_entry =
		create_proc_entry("statistics",
				  S_IFREG | S_IRUSR | S_IWUSR,
				  privptr->proc_dentry);
	privptr->proc_stat_entry->proc_fops = &ctc_stat_fops;
	privptr->proc_stat_entry->proc_iops = &ctc_stat_iops;
	privptr->proc_ctrl_entry =
		create_proc_entry("buffersize",
				  S_IFREG | S_IRUSR | S_IWUSR,
				  privptr->proc_dentry);
	privptr->proc_ctrl_entry->proc_fops = &ctc_ctrl_fops;
	privptr->proc_ctrl_entry->proc_iops = &ctc_ctrl_iops;
	privptr->proc_loglevel_entry =
		create_proc_entry("loglevel",
				  S_IFREG | S_IRUSR | S_IWUSR,
				  privptr->proc_dentry);
	privptr->proc_loglevel_entry->proc_fops = &ctc_loglevel_fops;
	privptr->proc_loglevel_entry->proc_iops = &ctc_loglevel_iops;
#else
	privptr->proc_dentry->name = dev->name;
	privptr->proc_dentry->namelen = strlen(dev->name);
	proc_register(&ctc_dir, privptr->proc_dentry);
	proc_register(privptr->proc_dentry, privptr->proc_stat_entry);
	proc_register(privptr->proc_dentry, privptr->proc_ctrl_entry);
	proc_register(privptr->proc_dentry, privptr->proc_loglevel_entry);
#endif
	privptr->proc_registered = 1;
}


/**
 * Destroy a device specific subdirectory.
 *
 * @param privptr Pointer to device private data.
 */
static void ctc_proc_destroy_sub(ctc_priv *privptr) {
	if (!privptr->proc_registered)
		return;
#if LINUX_VERSION_CODE > 0x020362
	remove_proc_entry("loglevel", privptr->proc_dentry);
	remove_proc_entry("statistics", privptr->proc_dentry);
	remove_proc_entry("buffersize", privptr->proc_dentry);
	remove_proc_entry(privptr->proc_dentry->name, ctc_dir);
#else
	proc_unregister(privptr->proc_dentry,
			privptr->proc_loglevel_entry->low_ino);
	proc_unregister(privptr->proc_dentry,
			privptr->proc_stat_entry->low_ino);
	proc_unregister(privptr->proc_dentry,
			privptr->proc_ctrl_entry->low_ino);
	proc_unregister(&ctc_dir,
			privptr->proc_dentry->low_ino);
#endif
	privptr->proc_registered = 0;
}



#ifndef CTC_CHANDEV
/**
 * Setup related routines
 *****************************************************************************/

/**
 * Parse a portion of the setup string describing a single device or option
 * providing the following syntax:
 *
 * [Device/OptionName[:int1][:int2][:int3]]
 *
 *
 * @param setup    Pointer to a pointer to the remainder of the parameter
 *                 string to be parsed. On return, the content of this
 *                 pointer is updated to point to the first character after
 *                 the parsed portion (e.g. possible start of next portion)
 *                 NOTE: The string pointed to must be writeable, since a
 *                 \0 is written for termination of the device/option name.
 *
 * @param dev_name Pointer to a pointer to the name of the device whose
 *                 parameters are parsed. On return, this is set to the
 *                 name of the device/option.
 *
 * @param ints     Pointer to an array of integer parameters. On return,
 *                 element 0 is set to the number of parameters found.
 *
 * @param maxip    Maximum number of ints to parse.
 *                 (ints[] must have size maxip+1)
 *
 * @return     0 if string "setup" was empty, !=0 otherwise
 */
static int parse_opts(char **setup, char **dev_name, int *ints, int maxip) {
	char *cur = *setup;
	int i = 1;
	int rc = 0;
	int in_name = 1;
	int noauto = 0;

#ifdef DEBUG
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG
		       "ctc: parse_opts(): *setup='%s', maxip=%d\n", *setup, maxip);
#endif
	if (*setup) {
		*dev_name = *setup;

		if (strncmp(cur, "ctc", 3) && strncmp(cur, "escon", 5) &&
		    strncmp(cur, "noauto", 6)) {
			if ((*setup = strchr(cur, ':')))
				*(*setup)++ = '\0';
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: Invalid device name or option '%s'\n",
			       cur);
			return 1;
		}
		switch (*cur) {
			case 'c':
				cur += 3;
				break;
			case 'e':
				cur += 5;
				break;
			case 'n':
				cur += 6;
				*cur++ = '\0';
				noauto = 1;
		}
		if (!noauto) {
			while (cur &&
			       (*cur == '-' || isdigit(*cur)) &&
			       i <= maxip) {
				if (in_name) {
					cur++;
					if (*cur == ':') {
						*cur++ = '\0';
						in_name = 0;
					}
				} else {
					ints[i++] =
						simple_strtoul(cur, NULL, 0);
#ifdef DEBUG
					if (loglevel & CTC_LOGLEVEL_DEBUG)
						printk(KERN_DEBUG
						       "ctc: %s: ints[%d]=%d\n",
						       __FUNCTION__,
						       i-1, ints[i-1]);
#endif
					if ((cur = strchr(cur, ':')) != NULL)
						cur++;
				}
			}
		}
		ints[0] = i - 1;
		*setup = cur;
		if (cur && (*cur == ':'))
			(*setup)++;
		rc = 1;
	}
	return rc;
}

/**
 *
 * Allocate one param struct
 *
 * If the driver is loaded as a module this functions is called during
 *   module set up and we can allocate the struct by using kmalloc()
 *
 * If the driver is statically linked into the kernel this function is called
 * when kmalloc() is not yet available so we must allocate from a static array
 *
 */
#ifdef MODULE
#define alloc_param() ((param *)kmalloc(sizeof(param), GFP_KERNEL));
#else
static param parms_array[MAX_STATIC_DEVICES];
static param *next_param = parms_array;
#define alloc_param() \
        ((next_param<parms_array+MAX_STATIC_DEVICES)?next_param++:NULL)
#endif MODULE

/**
 * Returns commandline parameter using device name as key.
 *
 * @param name Name of interface to get parameters from.
 *
 * @return Pointer to corresponting param struct, NULL if not found.
 */
static param *find_param(char *name) {
	param *p = params;

	while (p && strcmp(p->name, name))
		p = p->next;
	return p;
}

/**
 * maximum number of integer parametes that may be specified
 * for one device in the setup string
 */
#define CTC_MAX_INTPARMS 3

/**
 * Parse configuration options for all interfaces.
 *
 * This function is called from two possible locations:
 *  - If built as module, this function is called from init_module().
 *  - If built in monolithic kernel, this function is called from within
 *    init/main.c.
 * Parsing is always done here.
 *
 * Valid parameters are:
 *
 *
 *   [NAME[:0xRRRR[:0xWWWW[:P]]]]
 *
 *     where P       is the channel protocol (always 0)
 *	      0xRRRR is the cu number for the read channel
 *	      0xWWWW is the cu number for the write channel
 *	      NAME   is either ctc0 ... ctcN for CTC/A
 *                      or     escon0 ... esconN for Escon.
 *                      or     noauto
 *                             which switches off auto-detection of channels.
 *
 * @param setup    The parameter string to parse. MUST be writeable!
 * @param ints     Pointer to an array of ints. Only for kernel 2.2,
 *                 builtin (not module) version. With kernel 2.2,
 *                 normally all integer-parameters, preceeding some
 *                 configuration-string are pre-parsed in init/main.c
 *                 and handed over here.
 *                 To simplify 2.2/2.4 compatibility, by definition,
 *                 our parameters always start with a string and ints
 *                 is always unset and ignored.
 */
#ifdef MODULE
   static void ctc_setup(char *setup)
#  define ctc_setup_return return
#else MODULE
#  if LINUX_VERSION_CODE < 0x020300
     __initfunc(void ctc_setup(char *setup, int *ints))
#    define ctc_setup_return return
#    define ints local_ints
#  else
     static int __init ctc_setup(char *setup)
#    define ctc_setup_return return(1)
#  endif
#endif MODULE
{
	int write_dev;
	int read_dev;
	int proto;
	param *par;
	char *dev_name;
	int ints[CTC_MAX_INTPARMS+1];

	while (parse_opts(&setup, &dev_name, ints, CTC_MAX_INTPARMS)) {
		write_dev = -1;
		read_dev = -1;
		proto = CTC_PROTO_S390;
#ifdef DEBUG
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG
			       "ctc: ctc_setup(): setup='%s' dev_name='%s',"
			       " ints[0]=%d)\n",
			       setup, dev_name, ints[0]);
#endif DEBUG
		if (dev_name == NULL) {
			/**
			 * happens if device name is not specified in
			 * parameter line (cf. init/main.c:get_options()
			 */
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: %s(): Device name not specified\n",
				       __FUNCTION__);
			ctc_setup_return;
		}

#ifdef DEBUG
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "name=´%s´ argc=%d\n", dev_name, ints[0]);
#endif

		if (strcmp(dev_name, "noauto") == 0) {
			if (loglevel & CTC_LOGLEVEL_INFO)
				printk(KERN_INFO "ctc: autoprobing disabled\n");
			ctc_no_auto = 1;
			continue;
		}

		if (find_param(dev_name) != NULL) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: Definition for device %s already set. "
				       "Ignoring second definition\n", dev_name);
			continue;
		}

		switch (ints[0]) {
			case 3: /* protocol type passed */
				proto = ints[3];
				if (proto > CTC_PROTO_MAX) {
					if (loglevel & CTC_LOGLEVEL_WARN)
						printk(KERN_WARNING
						       "%s: wrong protocol type "
						       "passed\n", dev_name);
					ctc_setup_return;
				}
			case 2: /* write channel passed */
				write_dev = ints[2];
			case 1: /* read channel passed */
				read_dev = ints[1];
				if (write_dev == -1)
					write_dev = read_dev + 1;
				break;
			default:
				if (loglevel & CTC_LOGLEVEL_WARN)
					printk(KERN_WARNING
					       "ctc: wrong number of parameter "
					       "passed (is: %d, expected: [1..3]\n",
					       ints[0]);
				ctc_setup_return;
		}
		par = alloc_param();
		if (!par) {
#ifdef MODULE
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: Couldn't allocate setup param block\n");
#else
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: Number of device definitions in "
				       " kernel commandline exceeds builtin limit "
				       " of %d devices.\n", MAX_STATIC_DEVICES);
#endif
			ctc_setup_return;
		}
		par->read_dev = read_dev;
		par->write_dev = write_dev;
		par->proto = proto;
		strncpy(par->name, dev_name, MAX_PARAM_NAME_LEN);
		par->next = params;
		params = par;
#ifdef DEBUG
		if (loglevel & CTC_LOGLEVEL_DEBUG)
			printk(KERN_DEBUG "%s: protocol=%x read=%04x write=%04x\n",
			       dev_name, proto, read_dev, write_dev);
#endif
	}
	ctc_setup_return;
}

#if LINUX_VERSION_CODE >= 0x020300
__setup("ctc=", ctc_setup);
#endif
#endif /* !CTC_CHANDEV */


static void
ctc_netdev_unregister(net_device *dev)
{
	ctc_priv *privptr;

	if (!dev)
		return;
	privptr = (ctc_priv *)dev->priv;
	if (privptr->protocol != CTC_PROTO_LINUX_TTY)
		unregister_netdev(dev);
	else
		ctc_tty_unregister_netdev(dev);
}

static int
ctc_netdev_register(net_device *dev)
{
	ctc_priv *privptr = (ctc_priv *)dev->priv;
	if (privptr->protocol != CTC_PROTO_LINUX_TTY)
		return register_netdev(dev);
	else
		return ctc_tty_register_netdev(dev);
}

static void
ctc_free_netdevice(net_device *dev, int free_dev)
{
	ctc_priv *privptr;
	if (!dev)
		return;
	privptr = dev->priv;
	if (privptr) {
		if (privptr->fsm)
			kfree_fsm(privptr->fsm);
		ctc_proc_destroy_sub(privptr);
		kfree(privptr);
	}
#ifdef MODULE
	if (free_dev)
		kfree(dev);
#endif
}

#ifdef CTC_CHANDEV
static int
ctc_shutdown(net_device *dev)
{
	ctc_priv *privptr;

	if (!dev)
		return 0;
	privptr = (ctc_priv *)dev->priv;
	channel_remove(privptr->channel[READ]);
	channel_remove(privptr->channel[WRITE]);
	ctc_free_netdevice(dev, 0);
	return 0;
}
#endif

/**
 * Initialize everything of the net device except the name and the
 * channel structs.
 */
static net_device *
ctc_init_netdevice(net_device *dev, int alloc_device)
{
	ctc_priv *privptr;
	int      priv_size;
	if (alloc_device) {
		dev = kmalloc(sizeof(net_device)
#if LINUX_VERSION_CODE < 0x020300
			      + 11 /* name + zero */
#endif
			      , GFP_KERNEL);
		if (!dev)
			return NULL;
		memset(dev, 0, sizeof(net_device));
	}
	priv_size = sizeof(ctc_priv) + sizeof(ctc_template) +
		sizeof(stat_entry) + sizeof(ctrl_entry);
	dev->priv = kmalloc(priv_size, GFP_KERNEL);
	if (dev->priv == NULL) {
		if (alloc_device)
			kfree(dev);
		return NULL;
	}
        memset(dev->priv, 0, priv_size);
        privptr = (ctc_priv *)dev->priv;
        privptr->proc_dentry = (struct proc_dir_entry *)
		(((char *)privptr) + sizeof(ctc_priv));
        privptr->proc_stat_entry = (struct proc_dir_entry *)
		(((char *)privptr) + sizeof(ctc_priv) +
		 sizeof(ctc_template));
        privptr->proc_ctrl_entry = (struct proc_dir_entry *)
		(((char *)privptr) + sizeof(ctc_priv) +
		 sizeof(ctc_template) + sizeof(stat_entry));
        privptr->proc_loglevel_entry = (struct proc_dir_entry *)
		(((char *)privptr) + sizeof(ctc_priv) +
		 sizeof(ctc_template) + sizeof(stat_entry) + sizeof(ctrl_entry));
	memcpy(privptr->proc_dentry, &ctc_template, sizeof(ctc_template));
	memcpy(privptr->proc_stat_entry, &stat_entry, sizeof(stat_entry));
	memcpy(privptr->proc_ctrl_entry, &ctrl_entry, sizeof(ctrl_entry));
	memcpy(privptr->proc_loglevel_entry, &loglevel_entry, sizeof(loglevel_entry));
	privptr->fsm = init_fsm("ctcdev", dev_state_names,
			dev_event_names, NR_DEV_STATES, NR_DEV_EVENTS,
			dev_fsm, DEV_FSM_LEN, GFP_KERNEL);
	if (privptr->fsm == NULL) {
		kfree(privptr);
		if (alloc_device)
			kfree(dev);
		return NULL;
	}
	fsm_newstate(privptr->fsm, DEV_STATE_STOPPED);
	fsm_settimer(privptr->fsm, &privptr->restart_timer);
	dev->mtu	         = CTC_BUFSIZE_DEFAULT - LL_HEADER_LENGTH - 2;
	dev->hard_start_xmit     = ctc_tx;
	dev->open	         = ctc_open;
	dev->stop	         = ctc_close;
	dev->get_stats	         = ctc_stats;
	dev->change_mtu          = ctc_change_mtu;
	dev->hard_header_len     = LL_HEADER_LENGTH + 2;
	dev->addr_len            = 0;
	dev->type                = ARPHRD_SLIP;
	dev->tx_queue_len        = 100;
	SET_DEVICE_START(dev, 1);
	dev_init_buffers(dev);
	dev->flags	         = IFF_POINTOPOINT | IFF_NOARP;
	return dev;
}

#ifdef CTC_CHANDEV
static void
ctc_chandev_msck_notify(void *dev, int msck_irq,
			chandev_msck_status prevstatus,
			chandev_msck_status newstatus)
{
	net_device *device = (net_device *)dev;
	ctc_priv *privptr;
	int direction;

	if (!dev)
		return;

	privptr = device->priv;
	if (prevstatus == chandev_status_revalidate)
		for (direction = READ; direction <= WRITE; direction++) {
			channel *ch = privptr->channel[direction];
			if(ch->irq == msck_irq) {
				s390_dev_info_t devinfo;

				if (get_dev_info_by_irq(ch->irq, &devinfo))
					ch->devno = devinfo.devno;
				else
					if (loglevel & CTC_LOGLEVEL_WARN)
						printk(KERN_WARNING
						       "ctc_chandev_msck_notify: "
						       "get_dev_info_by_irq failed for "
						       "irq %d\n", ch->irq);
			}
		}
	switch (newstatus) {
		case chandev_status_not_oper:
		case chandev_status_no_path:
		case chandev_status_gone:
			for (direction = READ; direction <= WRITE; direction++) {
				channel *ch = privptr->channel[direction];
				ch->flags |= CHANNEL_FLAGS_FAILED;
				fsm_event(ch->fsm, CH_EVENT_MC_FAIL, ch);
			}
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: %s channel deactivated\n", device->name);
			break;
		case chandev_status_all_chans_good:
			for (direction = READ; direction <= WRITE; direction++) {
				channel *ch = privptr->channel[direction];
				if (!(ch->flags & CHANNEL_FLAGS_FAILED))
					fsm_event(ch->fsm, CH_EVENT_MC_FAIL, ch);
			}
			for (direction = READ; direction <= WRITE; direction++) {
				channel *ch = privptr->channel[direction];
				ch->flags &= ~CHANNEL_FLAGS_FAILED;
				fsm_event(ch->fsm, CH_EVENT_MC_GOOD, ch);
			}
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "ctc: %s channel activated\n", device->name);
			break;
		default:
			break;
	}
}

/**
 *
 * Setup an interface.
 *
 * Like ctc_setup(), ctc_probe() can be called from two different locations:
 *  - If built as module, it is called from within init_module().
 *  - If built in monolithic kernel, it is called from within generic network
 *    layer during initialization for every corresponding device, declared in
 *    drivers/net/Space.c
 *
 * @param dev Pointer to net_device to be initialized.
 *
 * @returns 0 on success, !0 on failure.
 */
static int ctc_chandev_probe(chandev_probeinfo *info)
{
	int               devno[2];
	__u16             proto;
	int               rc;
	int               direction;
	channel_type_t    type;
	ctc_priv          *privptr;
	net_device        *dev;

	ctc_proc_create_main();


	switch (info->chan_type) {
		case chandev_type_ctc:
			type = channel_type_ctca;
			break;
		case chandev_type_escon:
			type = channel_type_escon;
			break;
		default:
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING "ctc_chandev_probe called with "
				       "unsupported channel type %d\n", info->chan_type);
			return -ENODEV;
	}
	devno[READ]  = info->read.devno;
	devno[WRITE] = info->write.devno;
	proto        = info->port_protocol_no;

	if (add_channel(info->read.irq, info->read.devno, type))
		return -ENOMEM;
	if (add_channel(info->write.irq, info->write.devno, type))
		return -ENOMEM;

	dev = ctc_init_netdevice(NULL, 1);

	
	if (!dev) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc_init_netdevice failed\n");
		return -ENODEV;
	}
	
	if (proto == CTC_PROTO_LINUX_TTY)
		chandev_build_device_name(info, dev->name, "ctctty", 1);
	else
		chandev_build_device_name(info, dev->name, "ctc", 1);

	privptr = (ctc_priv *)dev->priv;
	privptr->protocol = proto;
	for (direction = READ; direction <= WRITE; direction++) {
		privptr->channel[direction] =
			channel_get(type, devno[direction], direction);
		if (privptr->channel[direction] == NULL) {
			if (direction == WRITE) {
				FREE_IRQ(privptr->channel[READ]->irq,
					 privptr->channel[READ]->devstat);
				channel_free(privptr->channel[READ]);
			}
			ctc_free_netdevice(dev, 1);
			return -ENODEV;
		}
		privptr->channel[direction]->netdev = dev;
		privptr->channel[direction]->protocol = proto;
		privptr->channel[direction]->max_bufsize = CTC_BUFSIZE_DEFAULT;
		rc = REQUEST_IRQ(privptr->channel[direction]->irq,
				 (void *)ctc_irq_handler, SA_INTERRUPT,
				 dev->name,
				 privptr->channel[direction]->devstat);
		if (rc) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "%s: requested irq %d is busy rc=%02x\n",
				       dev->name, privptr->channel[direction]->irq,
				       rc);
			if (direction == WRITE) {
				FREE_IRQ(privptr->channel[READ]->irq,
					 privptr->channel[READ]->devstat);
				channel_free(privptr->channel[READ]);
			}
			channel_free(privptr->channel[direction]);
			ctc_free_netdevice(dev, 1);
			return -EBUSY;
		}
	}
	if (ctc_netdev_register(dev) != 0) {
		ctc_free_netdevice(dev, 1);
		return -ENODEV;
	}

	/**
	 * register subdir in /proc/net/ctc
	 */
	ctc_proc_create_sub(dev);
	strncpy(privptr->fsm->name, dev->name, sizeof(privptr->fsm->name));
	activated++;

	print_banner();

	if (loglevel & CTC_LOGLEVEL_INFO)
		printk(KERN_INFO
		       "%s: read: ch %04x (irq %04x), "
		       "write: ch %04x (irq %04x) proto: %d\n",
		       dev->name, privptr->channel[READ]->devno,
		       privptr->channel[READ]->irq, privptr->channel[WRITE]->devno,
		       privptr->channel[WRITE]->irq, proto);

	chandev_initdevice(info, dev, 0, dev->name,
			   (proto == CTC_PROTO_LINUX_TTY)
			   ? chandev_category_serial_device :
			   chandev_category_network_device,
			   (chandev_unregfunc)ctc_netdev_unregister);
	return 0;
}
#else /* ! CHANDEV */
/**
 *
 * Setup an interface.
 *
 * Like ctc_setup(), ctc_probe() can be called from two different locations:
 *  - If built as module, it is called from within init_module().
 *  - If built in monolithic kernel, it is called from within generic network
 *    layer during initialization for every corresponding device, declared in
 *    drivers/net/Space.c
 *
 * @param dev Pointer to net_device to be initialized.
 *
 * @returns 0 on success, !0 on failure.
 */
int ctc_probe(net_device *dev)
{
	int            devno[2];
	__u16          proto;
	int            rc;
	int            direction;
	channel_type_t type;
	ctc_priv       *privptr;
	param          *par;

	ctc_proc_create_main();

	/**
	 * Scan for available channels only the first time,
	 * ctc_probe gets control.
	 */
	if (channels == NULL)
		channel_scan();

	type = extract_channel_media(dev->name);
	if (type == channel_type_unknown)
		return -ENODEV;

	par = find_param(dev->name);
	if (par) {
		devno[READ] = par->read_dev;
		devno[WRITE] = par->write_dev;
		proto = par->proto;
	} else {
		if (ctc_no_auto)
			return -ENODEV;
		else {
			devno[READ] = -1;
			devno[WRITE] = -1;
			proto = CTC_PROTO_S390;
		}
	}

#ifndef MODULE
	if (ctc_init_netdevice(dev, 0) == NULL)
		return -ENODEV;
#endif
	privptr = (ctc_priv *)dev->priv;
	privptr->protocol = proto;

	for (direction = READ; direction <= WRITE; direction++) {
		if ((ctc_no_auto == 0) || (devno[direction] == -1))
			privptr->channel[direction] =
				channel_get_next(type, direction);
		else
			privptr->channel[direction] =
				channel_get(type, devno[direction], direction);
		if (privptr->channel[direction] == NULL) {
			if (direction == WRITE) {
				FREE_IRQ(privptr->channel[READ]->irq,
					 privptr->channel[READ]->devstat);
				channel_free(privptr->channel[READ]);
			}
			ctc_free_netdevice(dev, 1);
			return -ENODEV;
		}
		privptr->channel[direction]->netdev = dev;
		privptr->channel[direction]->protocol = proto;
		privptr->channel[direction]->max_bufsize = CTC_BUFSIZE_DEFAULT;
		rc = REQUEST_IRQ(privptr->channel[direction]->irq,
				 (void *)ctc_irq_handler, SA_INTERRUPT,
				 dev->name,
				 privptr->channel[direction]->devstat);
		if (rc) {
			if (loglevel & CTC_LOGLEVEL_WARN)
				printk(KERN_WARNING
				       "%s: requested irq %d is busy rc=%02x\n",
				       dev->name, privptr->channel[direction]->irq,
				       rc);
			if (direction == WRITE) {
				FREE_IRQ(privptr->channel[READ]->irq,
					 privptr->channel[READ]->devstat);
				channel_free(privptr->channel[READ]);
			}
			channel_free(privptr->channel[direction]);
			ctc_free_netdevice(dev, 1);
			return -EBUSY;
		}
	}

	/**
	 * register subdir in /proc/net/ctc
	 */
	ctc_proc_create_sub(dev);

	print_banner();

	if (loglevel & CTC_LOGLEVEL_INFO)
		printk(KERN_INFO
		       "%s: read: ch %04x (irq %04x), "
		       "write: ch %04x (irq %04x) proto: %d\n",
		       dev->name, privptr->channel[READ]->devno,
		       privptr->channel[READ]->irq, privptr->channel[WRITE]->devno,
		       privptr->channel[WRITE]->irq, proto);

	return 0;
}
#endif

/**
 * Module related routines
 *****************************************************************************/

#ifdef MODULE
/**
 * Prepare to be unloaded. Free IRQ's and release all resources.
 * This is called just before this module is unloaded. It is
 * <em>not</em> called, if the usage count is !0, so we don't need to check
 * for that.
 */
void cleanup_module(void) {

	ctc_tty_cleanup(0);
	/* we are called if all interfaces are down only, so no need
	 * to bother around with locking stuff
	 */
#ifndef CTC_CHANDEV
	while (channels) {
		if ((channels->flags & CHANNEL_FLAGS_INUSE) &&
		    (channels->netdev != NULL)) {
			net_device *dev = channels->netdev;
			ctc_priv *privptr = dev->priv;

			if (privptr) {
				privptr->channel[READ]->netdev = NULL;
				privptr->channel[WRITE]->netdev = NULL;
			}
			channels->netdev = NULL;
			ctc_netdev_unregister(dev);
			ctc_free_netdevice(dev, 1);
		}
		channel_remove(channels);
	}
	channels = NULL;
#endif
	ctc_tty_cleanup(1);
	ctc_proc_destroy_main();
#ifdef CTC_CHANDEV
	chandev_unregister(ctc_chandev_probe, 1);
#endif
	if (loglevel & CTC_LOGLEVEL_INFO)
		printk(KERN_INFO "CTC driver unloaded\n");
}

#define ctc_init init_module
#endif /* MODULE */

/**
 * Initialize module.
 * This is called just after the module is loaded.
 *
 * @return 0 on success, !0 on error.
 */
int ctc_init(void) {
#ifndef CTC_CHANDEV
	int   cnt[2];
	int   itype;
	int   activated;
	param *par;
#endif
	int   ret = 0;
	int   probed = 0;

	print_banner();

#if defined(DEBUG) && !defined(CTC_CHANDEV)
	if (loglevel & CTC_LOGLEVEL_DEBUG)
		printk(KERN_DEBUG
		       "ctc: init_module(): got string '%s'\n", ctc);
#endif

#ifndef CTC_CHANDEV
#ifdef MODULE
	ctc_setup(ctc);
#endif
	par = params;
#endif

	activated = 0;
	ctc_tty_init();
#ifdef CTC_CHANDEV
	chandev_register_and_probe(ctc_chandev_probe,
				   (chandev_shutdownfunc)ctc_shutdown,
				   ctc_chandev_msck_notify,
				   chandev_type_ctc|chandev_type_escon);
#else /* CTC_CHANDEV */
	for (itype = 0; itype < 2; itype++) {
		net_device *dev = NULL;
		char       *bname = (itype) ? "escon" : "ctc";

		cnt[itype] = 0;
		do {
			dev = ctc_init_netdevice(NULL, 1);
			if (!dev) {
				ret = -ENOMEM;
				break;
			}
#if LINUX_VERSION_CODE < 0x020300
			dev->name = (unsigned char *)dev + sizeof(net_device);
#endif
			if (par && par->name) {
				char *p;
				int  n;

				sprintf(dev->name, "%s", par->name);
				par = par->next;
				for (p = dev->name; p && *p; p++)
					if (isdigit(*p))
						break;
				if (p && *p) {
					int it =
						(strncmp(dev->name, "escon", 5))
						? 1 : 0;
					n = simple_strtoul(p, NULL, 0);
					if (n >= cnt[it])
						cnt[it] = n + 1;
				}
			} else {
				if (ctc_no_auto) {
					itype = 3;
					ctc_free_netdevice(dev, 1);
					dev = NULL;
					break;
				}
				sprintf(dev->name, "%s%d", bname,
					(cnt[itype])++);
			}
#ifdef DEBUG
			if (loglevel & CTC_LOGLEVEL_DEBUG)
				printk(KERN_DEBUG "ctc: %s(): probing for device %s\n",
				       __FUNCTION__, dev->name);
#endif			
			probed = 1;
			if (ctc_probe(dev) == 0) {
				ctc_priv *privptr = (ctc_priv *)dev->priv;
#ifdef DEBUG
				if (loglevel & CTC_LOGLEVEL_DEBUG) {
					printk(KERN_DEBUG
					       "ctc: %s(): probing succeeded\n",
					       __FUNCTION__);
					printk(KERN_DEBUG
					       "ctc: %s(): registering device %s\n",
					       __FUNCTION__, dev->name);
				}
#endif
				if (ctc_netdev_register(dev) != 0) {
					if (loglevel & CTC_LOGLEVEL_WARN)
						printk(KERN_WARNING
						       "ctc: Couldn't register %s\n",
						       dev->name);
					FREE_IRQ(
						privptr->channel[READ]->irq,
						privptr->channel[READ]->devstat);
					FREE_IRQ(
						privptr->channel[WRITE]->irq,
						privptr->channel[WRITE]->devstat);
					channel_free(privptr->channel[READ]);
					channel_free(privptr->channel[WRITE]);
					ctc_free_netdevice(dev, 1);
					dev = NULL;
				} else {
#ifdef DEBUG
					if (loglevel & CTC_LOGLEVEL_DEBUG)
						printk(KERN_DEBUG
						       "ctc: %s(): register succeed\n",
						       __FUNCTION__);
#endif			
					activated++;
				}
			} else {
#ifdef DEBUG
				if (loglevel & CTC_LOGLEVEL_DEBUG)
					printk(KERN_DEBUG
					       "ctc: %s(): probing failed\n",
					       __FUNCTION__);
#endif			
				dev = NULL;
			}
		} while (dev && (ret == 0));
	}
#endif /* CHANDEV */
#if !defined(CTC_CHANDEV) && defined(MODULE)
	if (!activated) {
		if (loglevel & CTC_LOGLEVEL_WARN)
			printk(KERN_WARNING "ctc: No devices registered\n");
		ret = -ENODEV;
	}
#endif
	if (ret) {
		ctc_tty_cleanup(0);
		ctc_tty_cleanup(1);
#if defined(CTC_CHANDEV) && defined(MODULE)
		chandev_unregister(ctc_chandev_probe, 0);
#endif
#ifdef MODULE
		if (probed)
			ctc_proc_destroy_main();
#endif
	}
	return ret;
}

#ifndef MODULE
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0))
__initcall(ctc_init);
#endif /* LINUX_VERSION_CODE */
#endif /* MODULE */

/* --- This is the END my friend --- */

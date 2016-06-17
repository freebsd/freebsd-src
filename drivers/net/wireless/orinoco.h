/* orinoco.h
 * 
 * Common definitions to all pieces of the various orinoco
 * drivers
 */

#ifndef _ORINOCO_H
#define _ORINOCO_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/version.h>
#include "hermes.h"

/* Workqueue / task queue backwards compatibility stuff */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,41)
#include <linux/workqueue.h>
#else
#include <linux/tqueue.h>
#define work_struct tq_struct
#define INIT_WORK INIT_TQUEUE
#define schedule_work schedule_task
#endif

/* To enable debug messages */
//#define ORINOCO_DEBUG		3

#if (! defined (WIRELESS_EXT)) || (WIRELESS_EXT < 10)
#error "orinoco driver requires Wireless extensions v10 or later."
#endif /* (! defined (WIRELESS_EXT)) || (WIRELESS_EXT < 10) */
#define WIRELESS_SPY		// enable iwspy support

#define ORINOCO_MAX_KEY_SIZE	14
#define ORINOCO_MAX_KEYS	4

struct orinoco_key {
	u16 len;	/* always stored as little-endian */
	char data[ORINOCO_MAX_KEY_SIZE];
} __attribute__ ((packed));

#define ORINOCO_INTEN	 	( HERMES_EV_RX | HERMES_EV_ALLOC | HERMES_EV_TX | \
				HERMES_EV_TXEXC | HERMES_EV_WTERR | HERMES_EV_INFO | \
				HERMES_EV_INFDROP )


struct orinoco_private {
	void *card;	/* Pointer to card dependant structure */
	int (*hard_reset)(struct orinoco_private *);

	/* Synchronisation stuff */
	spinlock_t lock;
	int hw_unavailable;
	struct work_struct reset_work;

	/* driver state */
	int open;
	u16 last_linkstatus;
	int connected;

	/* Net device stuff */
	struct net_device *ndev;
	struct net_device_stats stats;
	struct iw_statistics wstats;

	/* Hardware control variables */
	hermes_t hw;
	u16 txfid;


	/* Capabilities of the hardware/firmware */
	int firmware_type;
#define FIRMWARE_TYPE_AGERE 1
#define FIRMWARE_TYPE_INTERSIL 2
#define FIRMWARE_TYPE_SYMBOL 3
	int has_ibss, has_port3, has_ibss_any, ibss_port;
	int has_wep, has_big_wep;
	int has_mwo;
	int has_pm;
	int has_preamble;
	int has_sensitivity;
	int nicbuf_size;
	u16 channel_mask;
	int broken_disableport;

	/* Configuration paramaters */
	u32 iw_mode;
	int prefer_port3;
	u16 wep_on, wep_restrict, tx_key;
	struct orinoco_key keys[ORINOCO_MAX_KEYS];
	int bitratemode;
 	char nick[IW_ESSID_MAX_SIZE+1];
	char desired_essid[IW_ESSID_MAX_SIZE+1];
	u16 frag_thresh, mwo_robust;
	u16 channel;
	u16 ap_density, rts_thresh;
	u16 pm_on, pm_mcast, pm_period, pm_timeout;
	u16 preamble;
#ifdef WIRELESS_SPY
	int			spy_number;
	u_char			spy_address[IW_MAX_SPY][ETH_ALEN];
	struct iw_quality	spy_stat[IW_MAX_SPY];
#endif

	/* Configuration dependent variables */
	int port_type, createibss;
	int promiscuous, mc_count;
};

#ifdef ORINOCO_DEBUG
extern int orinoco_debug;
#define DEBUG(n, args...) do { if (orinoco_debug>(n)) printk(KERN_DEBUG args); } while(0)
#else
#define DEBUG(n, args...) do { } while (0)
#endif	/* ORINOCO_DEBUG */

#define TRACE_ENTER(devname) DEBUG(2, "%s: -> " __FUNCTION__ "()\n", devname);
#define TRACE_EXIT(devname)  DEBUG(2, "%s: <- " __FUNCTION__ "()\n", devname);

extern struct net_device *alloc_orinocodev(int sizeof_card,
					   int (*hard_reset)(struct orinoco_private *));
extern int __orinoco_up(struct net_device *dev);
extern int __orinoco_down(struct net_device *dev);
extern int orinoco_stop(struct net_device *dev);
extern int orinoco_reinit_firmware(struct net_device *dev);
extern void orinoco_interrupt(int irq, void * dev_id, struct pt_regs *regs);

/********************************************************************/
/* Locking and synchronization functions                            */
/********************************************************************/

/* These functions *must* be inline or they will break horribly on
 * SPARC, due to its weird semantics for save/restore flags. extern
 * inline should prevent the kernel from linking or module from
 * loading if they are not inlined. */
extern inline int orinoco_lock(struct orinoco_private *priv,
			       unsigned long *flags)
{
	spin_lock_irqsave(&priv->lock, *flags);
	if (priv->hw_unavailable) {
		printk(KERN_DEBUG "orinoco_lock() called with hw_unavailable (dev=%p)\n",
		       priv->ndev);
		spin_unlock_irqrestore(&priv->lock, *flags);
		return -EBUSY;
	}
	return 0;
}

extern inline void orinoco_unlock(struct orinoco_private *priv,
				  unsigned long *flags)
{
	spin_unlock_irqrestore(&priv->lock, *flags);
}

#endif /* _ORINOCO_H */

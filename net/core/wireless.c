/*
 * This file implement the Wireless Extensions APIs.
 *
 * Authors :	Jean Tourrilhes - HPL - <jt@hpl.hp.com>
 * Copyright (c) 1997-2003 Jean Tourrilhes, All Rights Reserved.
 *
 * (As all part of the Linux kernel, this file is GPL)
 */

/************************** DOCUMENTATION **************************/
/*
 * API definition :
 * --------------
 * See <linux/wireless.h> for details of the APIs and the rest.
 *
 * History :
 * -------
 *
 * v1 - 5.12.01 - Jean II
 *	o Created this file.
 *
 * v2 - 13.12.01 - Jean II
 *	o Move /proc/net/wireless stuff from net/core/dev.c to here
 *	o Make Wireless Extension IOCTLs go through here
 *	o Added iw_handler handling ;-)
 *	o Added standard ioctl description
 *	o Initial dumb commit strategy based on orinoco.c
 *
 * v3 - 19.12.01 - Jean II
 *	o Make sure we don't go out of standard_ioctl[] in ioctl_standard_call
 *	o Add event dispatcher function
 *	o Add event description
 *	o Propagate events as rtnetlink IFLA_WIRELESS option
 *	o Generate event on selected SET requests
 *
 * v4 - 18.04.02 - Jean II
 *	o Fix stupid off by one in iw_ioctl_description : IW_ESSID_MAX_SIZE + 1
 *
 * v5 - 21.06.02 - Jean II
 *	o Add IW_PRIV_TYPE_ADDR in priv_type_size (+cleanup)
 *	o Reshuffle IW_HEADER_TYPE_XXX to map IW_PRIV_TYPE_XXX changes
 *	o Add IWEVCUSTOM for driver specific event/scanning token
 *	o Turn on WE_STRICT_WRITE by default + kernel warning
 *	o Fix WE_STRICT_WRITE in ioctl_export_private() (32 => iw_num)
 *	o Fix off-by-one in test (extra_size <= IFNAMSIZ)
 *
 * v6 - 9.01.03 - Jean II
 *	o Add common spy support : iw_handler_set_spy(), wireless_spy_update()
 *	o Add enhanced spy support : iw_handler_set_thrspy() and event.
 *	o Add WIRELESS_EXT version display in /proc/net/wireless
 */

/***************************** INCLUDES *****************************/

#include <asm/uaccess.h>		/* copy_to_user() */
#include <linux/config.h>		/* Not needed ??? */
#include <linux/types.h>		/* off_t */
#include <linux/netdevice.h>		/* struct ifreq, dev_get_by_name() */
#include <linux/rtnetlink.h>		/* rtnetlink stuff */
#include <linux/if_arp.h>		/* ARPHRD_ETHER */

#include <linux/wireless.h>		/* Pretty obvious */
#include <net/iw_handler.h>		/* New driver API */

/**************************** CONSTANTS ****************************/

/* Enough lenience, let's make sure things are proper... */
#define WE_STRICT_WRITE		/* Check write buffer size */
/* I'll probably drop both the define and kernel message in the next version */

/* Debuging stuff */
#undef WE_IOCTL_DEBUG		/* Debug IOCTL API */
#undef WE_EVENT_DEBUG		/* Debug Event dispatcher */
#undef WE_SPY_DEBUG		/* Debug enhanced spy support */

/* Options */
#define WE_EVENT_NETLINK	/* Propagate events using rtnetlink */
#define WE_SET_EVENT		/* Generate an event on some set commands */

/************************* GLOBAL VARIABLES *************************/
/*
 * You should not use global variables, because of re-entrancy.
 * On our case, it's only const, so it's OK...
 */
/*
 * Meta-data about all the standard Wireless Extension request we
 * know about.
 */
static const struct iw_ioctl_description	standard_ioctl[] = {
	/* SIOCSIWCOMMIT */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCGIWNAME */
	{ IW_HEADER_TYPE_CHAR, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWNWID */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, IW_DESCR_FLAG_EVENT},
	/* SIOCGIWNWID */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWFREQ */
	{ IW_HEADER_TYPE_FREQ, 0, 0, 0, 0, IW_DESCR_FLAG_EVENT},
	/* SIOCGIWFREQ */
	{ IW_HEADER_TYPE_FREQ, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWMODE */
	{ IW_HEADER_TYPE_UINT, 0, 0, 0, 0, IW_DESCR_FLAG_EVENT},
	/* SIOCGIWMODE */
	{ IW_HEADER_TYPE_UINT, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWSENS */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWSENS */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWRANGE */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCGIWRANGE */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, sizeof(struct iw_range), IW_DESCR_FLAG_DUMP},
	/* SIOCSIWPRIV */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCGIWPRIV (handled directly by us) */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCSIWSTATS */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCGIWSTATS (handled directly by us) */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWSPY */
	{ IW_HEADER_TYPE_POINT, 0, sizeof(struct sockaddr), 0, IW_MAX_SPY, 0},
	/* SIOCGIWSPY */
	{ IW_HEADER_TYPE_POINT, 0, (sizeof(struct sockaddr) + sizeof(struct iw_quality)), 0, IW_MAX_SPY, 0},
	/* SIOCSIWTHRSPY */
	{ IW_HEADER_TYPE_POINT, 0, sizeof(struct iw_thrspy), 1, 1, 0},
	/* SIOCGIWTHRSPY */
	{ IW_HEADER_TYPE_POINT, 0, sizeof(struct iw_thrspy), 1, 1, 0},
	/* SIOCSIWAP */
	{ IW_HEADER_TYPE_ADDR, 0, 0, 0, 0, 0},
	/* SIOCGIWAP */
	{ IW_HEADER_TYPE_ADDR, 0, 0, 0, 0, IW_DESCR_FLAG_DUMP},
	/* -- hole -- */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCGIWAPLIST */
	{ IW_HEADER_TYPE_POINT, 0, (sizeof(struct sockaddr) + sizeof(struct iw_quality)), 0, IW_MAX_AP, 0},
	/* SIOCSIWSCAN */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWSCAN */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_SCAN_MAX_DATA, 0},
	/* SIOCSIWESSID */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ESSID_MAX_SIZE + 1, IW_DESCR_FLAG_EVENT},
	/* SIOCGIWESSID */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ESSID_MAX_SIZE + 1, IW_DESCR_FLAG_DUMP},
	/* SIOCSIWNICKN */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ESSID_MAX_SIZE + 1, 0},
	/* SIOCGIWNICKN */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ESSID_MAX_SIZE + 1, 0},
	/* -- hole -- */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* -- hole -- */
	{ IW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* SIOCSIWRATE */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWRATE */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWRTS */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWRTS */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWFRAG */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWFRAG */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWTXPOW */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWTXPOW */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWRETRY */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWRETRY */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCSIWENCODE */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ENCODING_TOKEN_MAX, IW_DESCR_FLAG_EVENT | IW_DESCR_FLAG_RESTRICT},
	/* SIOCGIWENCODE */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_ENCODING_TOKEN_MAX, IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT},
	/* SIOCSIWPOWER */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* SIOCGIWPOWER */
	{ IW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
};
static const int standard_ioctl_num = (sizeof(standard_ioctl) /
				       sizeof(struct iw_ioctl_description));

/*
 * Meta-data about all the additional standard Wireless Extension events
 * we know about.
 */
static const struct iw_ioctl_description	standard_event[] = {
	/* IWEVTXDROP */
	{ IW_HEADER_TYPE_ADDR, 0, 0, 0, 0, 0},
	/* IWEVQUAL */
	{ IW_HEADER_TYPE_QUAL, 0, 0, 0, 0, 0},
	/* IWEVCUSTOM */
	{ IW_HEADER_TYPE_POINT, 0, 1, 0, IW_CUSTOM_MAX, 0},
	/* IWEVREGISTERED */
	{ IW_HEADER_TYPE_ADDR, 0, 0, 0, 0, 0},
	/* IWEVEXPIRED */
	{ IW_HEADER_TYPE_ADDR, 0, 0, 0, 0, 0},
};
static const int standard_event_num = (sizeof(standard_event) /
				       sizeof(struct iw_ioctl_description));

/* Size (in bytes) of the various private data types */
static const char priv_type_size[] = {
	0,				/* IW_PRIV_TYPE_NONE */
	1,				/* IW_PRIV_TYPE_BYTE */
	1,				/* IW_PRIV_TYPE_CHAR */
	0,				/* Not defined */
	sizeof(__u32),			/* IW_PRIV_TYPE_INT */
	sizeof(struct iw_freq),		/* IW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),	/* IW_PRIV_TYPE_ADDR */
	0,				/* Not defined */
};

/* Size (in bytes) of various events */
static const int event_type_size[] = {
	IW_EV_LCP_LEN,			/* IW_HEADER_TYPE_NULL */
	0,
	IW_EV_CHAR_LEN,			/* IW_HEADER_TYPE_CHAR */
	0,
	IW_EV_UINT_LEN,			/* IW_HEADER_TYPE_UINT */
	IW_EV_FREQ_LEN,			/* IW_HEADER_TYPE_FREQ */
	IW_EV_ADDR_LEN,			/* IW_HEADER_TYPE_ADDR */
	0,
	IW_EV_POINT_LEN,		/* Without variable payload */
	IW_EV_PARAM_LEN,		/* IW_HEADER_TYPE_PARAM */
	IW_EV_QUAL_LEN,			/* IW_HEADER_TYPE_QUAL */
};

/************************ COMMON SUBROUTINES ************************/
/*
 * Stuff that may be used in various place or doesn't fit in one
 * of the section below.
 */

/* ---------------------------------------------------------------- */
/*
 * Return the driver handler associated with a specific Wireless Extension.
 * Called from various place, so make sure it remains efficient.
 */
static inline iw_handler get_handler(struct net_device *dev,
				     unsigned int cmd)
{
	/* Don't "optimise" the following variable, it will crash */
	unsigned int	index;		/* *MUST* be unsigned */

	/* Check if we have some wireless handlers defined */
	if(dev->wireless_handlers == NULL)
		return NULL;

	/* Try as a standard command */
	index = cmd - SIOCIWFIRST;
	if(index < dev->wireless_handlers->num_standard)
		return dev->wireless_handlers->standard[index];

	/* Try as a private command */
	index = cmd - SIOCIWFIRSTPRIV;
	if(index < dev->wireless_handlers->num_private)
		return dev->wireless_handlers->private[index];

	/* Not found */
	return NULL;
}

/* ---------------------------------------------------------------- */
/*
 * Get statistics out of the driver
 */
static inline struct iw_statistics *get_wireless_stats(struct net_device *dev)
{
	return (dev->get_wireless_stats ?
		dev->get_wireless_stats(dev) :
		(struct iw_statistics *) NULL);
	/* In the future, get_wireless_stats may move from 'struct net_device'
	 * to 'struct iw_handler_def', to de-bloat struct net_device.
	 * Definitely worse a thought... */
}

/* ---------------------------------------------------------------- */
/*
 * Call the commit handler in the driver
 * (if exist and if conditions are right)
 *
 * Note : our current commit strategy is currently pretty dumb,
 * but we will be able to improve on that...
 * The goal is to try to agreagate as many changes as possible
 * before doing the commit. Drivers that will define a commit handler
 * are usually those that need a reset after changing parameters, so
 * we want to minimise the number of reset.
 * A cool idea is to use a timer : at each "set" command, we re-set the
 * timer, when the timer eventually fires, we call the driver.
 * Hopefully, more on that later.
 *
 * Also, I'm waiting to see how many people will complain about the
 * netif_running(dev) test. I'm open on that one...
 * Hopefully, the driver will remember to do a commit in "open()" ;-)
 */
static inline int call_commit_handler(struct net_device *	dev)
{
	if((netif_running(dev)) &&
	   (dev->wireless_handlers->standard[0] != NULL)) {
		/* Call the commit handler on the driver */
		return dev->wireless_handlers->standard[0](dev, NULL,
							   NULL, NULL);
	} else
		return 0;		/* Command completed successfully */
}

/* ---------------------------------------------------------------- */
/*
 * Number of private arguments
 */
static inline int get_priv_size(__u16	args)
{
	int	num = args & IW_PRIV_SIZE_MASK;
	int	type = (args & IW_PRIV_TYPE_MASK) >> 12;

	return num * priv_type_size[type];
}


/******************** /proc/net/wireless SUPPORT ********************/
/*
 * The /proc/net/wireless file is a human readable user-space interface
 * exporting various wireless specific statistics from the wireless devices.
 * This is the most popular part of the Wireless Extensions ;-)
 *
 * This interface is a pure clone of /proc/net/dev (in net/core/dev.c).
 * The content of the file is basically the content of "struct iw_statistics".
 */

#ifdef CONFIG_PROC_FS

/* ---------------------------------------------------------------- */
/*
 * Print one entry (line) of /proc/net/wireless
 */
static inline int sprintf_wireless_stats(char *buffer, struct net_device *dev)
{
	/* Get stats from the driver */
	struct iw_statistics *stats;
	int size;

	stats = get_wireless_stats(dev);
	if (stats != (struct iw_statistics *) NULL) {
		size = sprintf(buffer,
			       "%6s: %04x  %3d%c  %3d%c  %3d%c  %6d %6d %6d %6d %6d   %6d\n",
			       dev->name,
			       stats->status,
			       stats->qual.qual,
			       stats->qual.updated & 1 ? '.' : ' ',
			       ((__u8) stats->qual.level),
			       stats->qual.updated & 2 ? '.' : ' ',
			       ((__u8) stats->qual.noise),
			       stats->qual.updated & 4 ? '.' : ' ',
			       stats->discard.nwid,
			       stats->discard.code,
			       stats->discard.fragment,
			       stats->discard.retries,
			       stats->discard.misc,
			       stats->miss.beacon);
		stats->qual.updated = 0;
	}
	else
		size = 0;

	return size;
}

/* ---------------------------------------------------------------- */
/*
 * Print info for /proc/net/wireless (print all entries)
 */
int dev_get_wireless_info(char * buffer, char **start, off_t offset,
			  int length)
{
	int		len = 0;
	off_t		begin = 0;
	off_t		pos = 0;
	int		size;
	
	struct net_device *	dev;

	size = sprintf(buffer,
		       "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
		       " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | %d\n",
		       WIRELESS_EXT);
	
	pos += size;
	len += size;

	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		size = sprintf_wireless_stats(buffer + len, dev);
		len += size;
		pos = begin + len;

		if (pos < offset) {
			len = 0;
			begin = pos;
		}
		if (pos > offset + length)
			break;
	}
	read_unlock(&dev_base_lock);

	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);		/* Start slop */
	if (len > length)
		len = length;			/* Ending slop */
	if (len < 0)
		len = 0;

	return len;
}
#endif	/* CONFIG_PROC_FS */

/************************** IOCTL SUPPORT **************************/
/*
 * The original user space API to configure all those Wireless Extensions
 * is through IOCTLs.
 * In there, we check if we need to call the new driver API (iw_handler)
 * or just call the driver ioctl handler.
 */

/* ---------------------------------------------------------------- */
/*
 *	Allow programatic access to /proc/net/wireless even if /proc
 *	doesn't exist... Also more efficient...
 */
static inline int dev_iwstats(struct net_device *dev, struct ifreq *ifr)
{
	/* Get stats from the driver */
	struct iw_statistics *stats;

	stats = get_wireless_stats(dev);
	if (stats != (struct iw_statistics *) NULL) {
		struct iwreq *	wrq = (struct iwreq *)ifr;

		/* Copy statistics to the user buffer */
		if(copy_to_user(wrq->u.data.pointer, stats,
				sizeof(struct iw_statistics)))
			return -EFAULT;

		/* Check if we need to clear the update flag */
		if(wrq->u.data.flags != 0)
			stats->qual.updated = 0;
		return 0;
	} else
		return -EOPNOTSUPP;
}

/* ---------------------------------------------------------------- */
/*
 * Export the driver private handler definition
 * They will be picked up by tools like iwpriv...
 */
static inline int ioctl_export_private(struct net_device *	dev,
				       struct ifreq *		ifr)
{
	struct iwreq *				iwr = (struct iwreq *) ifr;

	/* Check if the driver has something to export */
	if((dev->wireless_handlers->num_private_args == 0) ||
	   (dev->wireless_handlers->private_args == NULL))
		return -EOPNOTSUPP;

	/* Check NULL pointer */
	if(iwr->u.data.pointer == NULL)
		return -EFAULT;
#ifdef WE_STRICT_WRITE
	/* Check if there is enough buffer up there */
	if(iwr->u.data.length < dev->wireless_handlers->num_private_args) {
		printk(KERN_ERR "%s (WE) : Buffer for request SIOCGIWPRIV too small (%d<%d)\n", dev->name, iwr->u.data.length, dev->wireless_handlers->num_private_args);
		return -E2BIG;
	}
#endif	/* WE_STRICT_WRITE */

	/* Set the number of available ioctls. */
	iwr->u.data.length = dev->wireless_handlers->num_private_args;

	/* Copy structure to the user buffer. */
	if (copy_to_user(iwr->u.data.pointer,
			 dev->wireless_handlers->private_args,
			 sizeof(struct iw_priv_args) * iwr->u.data.length))
		return -EFAULT;

	return 0;
}

/* ---------------------------------------------------------------- */
/*
 * Wrapper to call a standard Wireless Extension handler.
 * We do various checks and also take care of moving data between
 * user space and kernel space.
 */
static inline int ioctl_standard_call(struct net_device *	dev,
				      struct ifreq *		ifr,
				      unsigned int		cmd,
				      iw_handler		handler)
{
	struct iwreq *				iwr = (struct iwreq *) ifr;
	const struct iw_ioctl_description *	descr;
	struct iw_request_info			info;
	int					ret = -EINVAL;
	int					user_size = 0;

	/* Get the description of the IOCTL */
	if((cmd - SIOCIWFIRST) >= standard_ioctl_num)
		return -EOPNOTSUPP;
	descr = &(standard_ioctl[cmd - SIOCIWFIRST]);

#ifdef WE_IOCTL_DEBUG
	printk(KERN_DEBUG "%s (WE) : Found standard handler for 0x%04X\n",
	       ifr->ifr_name, cmd);
	printk(KERN_DEBUG "%s (WE) : Header type : %d, Token type : %d, size : %d, token : %d\n", dev->name, descr->header_type, descr->token_type, descr->token_size, descr->max_tokens);
#endif	/* WE_IOCTL_DEBUG */

	/* Prepare the call */
	info.cmd = cmd;
	info.flags = 0;

	/* Check if we have a pointer to user space data or not */
	if(descr->header_type != IW_HEADER_TYPE_POINT) {

		/* No extra arguments. Trivial to handle */
		ret = handler(dev, &info, &(iwr->u), NULL);

#ifdef WE_SET_EVENT
		/* Generate an event to notify listeners of the change */
		if((descr->flags & IW_DESCR_FLAG_EVENT) &&
		   ((ret == 0) || (ret == -EIWCOMMIT)))
			wireless_send_event(dev, cmd, &(iwr->u), NULL);
#endif	/* WE_SET_EVENT */
	} else {
		char *	extra;
		int	err;

		/* Check what user space is giving us */
		if(IW_IS_SET(cmd)) {
			/* Check NULL pointer */
			if((iwr->u.data.pointer == NULL) &&
			   (iwr->u.data.length != 0))
				return -EFAULT;
			/* Check if number of token fits within bounds */
			if(iwr->u.data.length > descr->max_tokens)
				return -E2BIG;
			if(iwr->u.data.length < descr->min_tokens)
				return -EINVAL;
		} else {
			/* Check NULL pointer */
			if(iwr->u.data.pointer == NULL)
				return -EFAULT;
			/* Save user space buffer size for checking */
			user_size = iwr->u.data.length;
		}

#ifdef WE_IOCTL_DEBUG
		printk(KERN_DEBUG "%s (WE) : Malloc %d bytes\n",
		       dev->name, descr->max_tokens * descr->token_size);
#endif	/* WE_IOCTL_DEBUG */

		/* Always allocate for max space. Easier, and won't last
		 * long... */
		extra = kmalloc(descr->max_tokens * descr->token_size,
				GFP_KERNEL);
		if (extra == NULL) {
			return -ENOMEM;
		}

		/* If it is a SET, get all the extra data in here */
		if(IW_IS_SET(cmd) && (iwr->u.data.length != 0)) {
			err = copy_from_user(extra, iwr->u.data.pointer,
					     iwr->u.data.length *
					     descr->token_size);
			if (err) {
				kfree(extra);
				return -EFAULT;
			}
#ifdef WE_IOCTL_DEBUG
			printk(KERN_DEBUG "%s (WE) : Got %d bytes\n",
			       dev->name,
			       iwr->u.data.length * descr->token_size);
#endif	/* WE_IOCTL_DEBUG */
		}

		/* Call the handler */
		ret = handler(dev, &info, &(iwr->u), extra);

		/* If we have something to return to the user */
		if (!ret && IW_IS_GET(cmd)) {
#ifdef WE_STRICT_WRITE
			/* Check if there is enough buffer up there */
			if(user_size < iwr->u.data.length) {
				printk(KERN_ERR "%s (WE) : Buffer for request %04X too small (%d<%d)\n", dev->name, cmd, user_size, iwr->u.data.length);
				kfree(extra);
				return -E2BIG;
			}
#endif	/* WE_STRICT_WRITE */

			err = copy_to_user(iwr->u.data.pointer, extra,
					   iwr->u.data.length *
					   descr->token_size);
			if (err)
				ret =  -EFAULT;				   
#ifdef WE_IOCTL_DEBUG
			printk(KERN_DEBUG "%s (WE) : Wrote %d bytes\n",
			       dev->name,
			       iwr->u.data.length * descr->token_size);
#endif	/* WE_IOCTL_DEBUG */
		}

#ifdef WE_SET_EVENT
		/* Generate an event to notify listeners of the change */
		if((descr->flags & IW_DESCR_FLAG_EVENT) &&
		   ((ret == 0) || (ret == -EIWCOMMIT))) {
			if(descr->flags & IW_DESCR_FLAG_RESTRICT)
				/* If the event is restricted, don't
				 * export the payload */
				wireless_send_event(dev, cmd, &(iwr->u), NULL);
			else
				wireless_send_event(dev, cmd, &(iwr->u),
						    extra);
		}
#endif	/* WE_SET_EVENT */

		/* Cleanup - I told you it wasn't that long ;-) */
		kfree(extra);
	}

	/* Call commit handler if needed and defined */
	if(ret == -EIWCOMMIT)
		ret = call_commit_handler(dev);

	/* Here, we will generate the appropriate event if needed */

	return ret;
}

/* ---------------------------------------------------------------- */
/*
 * Wrapper to call a private Wireless Extension handler.
 * We do various checks and also take care of moving data between
 * user space and kernel space.
 * It's not as nice and slimline as the standard wrapper. The cause
 * is struct iw_priv_args, which was not really designed for the
 * job we are going here.
 *
 * IMPORTANT : This function prevent to set and get data on the same
 * IOCTL and enforce the SET/GET convention. Not doing it would be
 * far too hairy...
 * If you need to set and get data at the same time, please don't use
 * a iw_handler but process it in your ioctl handler (i.e. use the
 * old driver API).
 */
static inline int ioctl_private_call(struct net_device *	dev,
				     struct ifreq *		ifr,
				     unsigned int		cmd,
				     iw_handler		handler)
{
	struct iwreq *			iwr = (struct iwreq *) ifr;
	struct iw_priv_args *		descr = NULL;
	struct iw_request_info		info;
	int				extra_size = 0;
	int				i;
	int				ret = -EINVAL;

	/* Get the description of the IOCTL */
	for(i = 0; i < dev->wireless_handlers->num_private_args; i++)
		if(cmd == dev->wireless_handlers->private_args[i].cmd) {
			descr = &(dev->wireless_handlers->private_args[i]);
			break;
		}

#ifdef WE_IOCTL_DEBUG
	printk(KERN_DEBUG "%s (WE) : Found private handler for 0x%04X\n",
	       ifr->ifr_name, cmd);
	if(descr) {
		printk(KERN_DEBUG "%s (WE) : Name %s, set %X, get %X\n",
		       dev->name, descr->name,
		       descr->set_args, descr->get_args);
	}
#endif	/* WE_IOCTL_DEBUG */

	/* Compute the size of the set/get arguments */
	if(descr != NULL) {
		if(IW_IS_SET(cmd)) {
			int	offset = 0;	/* For sub-ioctls */
			/* Check for sub-ioctl handler */
			if(descr->name[0] == '\0')
				/* Reserve one int for sub-ioctl index */
				offset = sizeof(__u32);

			/* Size of set arguments */
			extra_size = get_priv_size(descr->set_args);

			/* Does it fits in iwr ? */
			if((descr->set_args & IW_PRIV_SIZE_FIXED) &&
			   ((extra_size + offset) <= IFNAMSIZ))
				extra_size = 0;
		} else {
			/* Size of set arguments */
			extra_size = get_priv_size(descr->get_args);

			/* Does it fits in iwr ? */
			if((descr->get_args & IW_PRIV_SIZE_FIXED) &&
			   (extra_size <= IFNAMSIZ))
				extra_size = 0;
		}
	}

	/* Prepare the call */
	info.cmd = cmd;
	info.flags = 0;

	/* Check if we have a pointer to user space data or not. */
	if(extra_size == 0) {
		/* No extra arguments. Trivial to handle */
		ret = handler(dev, &info, &(iwr->u), (char *) &(iwr->u));
	} else {
		char *	extra;
		int	err;

		/* Check what user space is giving us */
		if(IW_IS_SET(cmd)) {
			/* Check NULL pointer */
			if((iwr->u.data.pointer == NULL) &&
			   (iwr->u.data.length != 0))
				return -EFAULT;

			/* Does it fits within bounds ? */
			if(iwr->u.data.length > (descr->set_args &
						 IW_PRIV_SIZE_MASK))
				return -E2BIG;
		} else {
			/* Check NULL pointer */
			if(iwr->u.data.pointer == NULL)
				return -EFAULT;
		}

#ifdef WE_IOCTL_DEBUG
		printk(KERN_DEBUG "%s (WE) : Malloc %d bytes\n",
		       dev->name, extra_size);
#endif	/* WE_IOCTL_DEBUG */

		/* Always allocate for max space. Easier, and won't last
		 * long... */
		extra = kmalloc(extra_size, GFP_KERNEL);
		if (extra == NULL) {
			return -ENOMEM;
		}

		/* If it is a SET, get all the extra data in here */
		if(IW_IS_SET(cmd) && (iwr->u.data.length != 0)) {
			err = copy_from_user(extra, iwr->u.data.pointer,
					     extra_size);
			if (err) {
				kfree(extra);
				return -EFAULT;
			}
#ifdef WE_IOCTL_DEBUG
			printk(KERN_DEBUG "%s (WE) : Got %d elem\n",
			       dev->name, iwr->u.data.length);
#endif	/* WE_IOCTL_DEBUG */
		}

		/* Call the handler */
		ret = handler(dev, &info, &(iwr->u), extra);

		/* If we have something to return to the user */
		if (!ret && IW_IS_GET(cmd)) {
			err = copy_to_user(iwr->u.data.pointer, extra,
					   extra_size);
			if (err)
				ret =  -EFAULT;				   
#ifdef WE_IOCTL_DEBUG
			printk(KERN_DEBUG "%s (WE) : Wrote %d elem\n",
			       dev->name, iwr->u.data.length);
#endif	/* WE_IOCTL_DEBUG */
		}

		/* Cleanup - I told you it wasn't that long ;-) */
		kfree(extra);
	}


	/* Call commit handler if needed and defined */
	if(ret == -EIWCOMMIT)
		ret = call_commit_handler(dev);

	return ret;
}

/* ---------------------------------------------------------------- */
/*
 * Main IOCTl dispatcher. Called from the main networking code
 * (dev_ioctl() in net/core/dev.c).
 * Check the type of IOCTL and call the appropriate wrapper...
 */
int wireless_process_ioctl(struct ifreq *ifr, unsigned int cmd)
{
	struct net_device *dev;
	iw_handler	handler;

	/* Permissions are already checked in dev_ioctl() before calling us.
	 * The copy_to/from_user() of ifr is also dealt with in there */

	/* Make sure the device exist */
	if ((dev = __dev_get_by_name(ifr->ifr_name)) == NULL)
		return -ENODEV;

	/* A bunch of special cases, then the generic case...
	 * Note that 'cmd' is already filtered in dev_ioctl() with
	 * (cmd >= SIOCIWFIRST && cmd <= SIOCIWLAST) */
	switch(cmd) 
	{
		case SIOCGIWSTATS:
			/* Get Wireless Stats */
			return dev_iwstats(dev, ifr);

		case SIOCGIWPRIV:
			/* Check if we have some wireless handlers defined */
			if(dev->wireless_handlers != NULL) {
				/* We export to user space the definition of
				 * the private handler ourselves */
				return ioctl_export_private(dev, ifr);
			}
			// ## Fall-through for old API ##
		default:
			/* Generic IOCTL */
			/* Basic check */
			if (!netif_device_present(dev))
				return -ENODEV;
			/* New driver API : try to find the handler */
			handler = get_handler(dev, cmd);
			if(handler != NULL) {
				/* Standard and private are not the same */
				if(cmd < SIOCIWFIRSTPRIV)
					return ioctl_standard_call(dev,
								   ifr,
								   cmd,
								   handler);
				else
					return ioctl_private_call(dev,
								  ifr,
								  cmd,
								  handler);
			}
			/* Old driver API : call driver ioctl handler */
			if (dev->do_ioctl) {
				return dev->do_ioctl(dev, ifr, cmd);
			}
			return -EOPNOTSUPP;
	}
	/* Not reached */
	return -EINVAL;
}

/************************* EVENT PROCESSING *************************/
/*
 * Process events generated by the wireless layer or the driver.
 * Most often, the event will be propagated through rtnetlink
 */

#ifdef WE_EVENT_NETLINK
/* "rtnl" is defined in net/core/rtnetlink.c, but we need it here.
 * It is declared in <linux/rtnetlink.h> */

/* ---------------------------------------------------------------- */
/*
 * Fill a rtnetlink message with our event data.
 * Note that we propage only the specified event and don't dump the
 * current wireless config. Dumping the wireless config is far too
 * expensive (for each parameter, the driver need to query the hardware).
 */
static inline int rtnetlink_fill_iwinfo(struct sk_buff *	skb,
					struct net_device *	dev,
					int			type,
					char *			event,
					int			event_len)
{
	struct ifinfomsg *r;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;

	nlh = NLMSG_PUT(skb, 0, 0, type, sizeof(*r));
	r = NLMSG_DATA(nlh);
	r->ifi_family = AF_UNSPEC;
	r->ifi_type = dev->type;
	r->ifi_index = dev->ifindex;
	r->ifi_flags = dev->flags;
	r->ifi_change = 0;	/* Wireless changes don't affect those flags */

	/* Add the wireless events in the netlink packet */
	RTA_PUT(skb, IFLA_WIRELESS,
		event_len, event);

	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

/* ---------------------------------------------------------------- */
/*
 * Create and broadcast and send it on the standard rtnetlink socket
 * This is a pure clone rtmsg_ifinfo() in net/core/rtnetlink.c
 * Andrzej Krzysztofowicz mandated that I used a IFLA_XXX field
 * within a RTM_NEWLINK event.
 */
static inline void rtmsg_iwinfo(struct net_device *	dev,
				char *			event,
				int			event_len)
{
	struct sk_buff *skb;
	int size = NLMSG_GOODSIZE;

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		return;

	if (rtnetlink_fill_iwinfo(skb, dev, RTM_NEWLINK,
				  event, event_len) < 0) {
		kfree_skb(skb);
		return;
	}
	NETLINK_CB(skb).dst_groups = RTMGRP_LINK;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_LINK, GFP_ATOMIC);
}
#endif	/* WE_EVENT_NETLINK */

/* ---------------------------------------------------------------- */
/*
 * Main event dispatcher. Called from other parts and drivers.
 * Send the event on the apropriate channels.
 * May be called from interrupt context.
 */
void wireless_send_event(struct net_device *	dev,
			 unsigned int		cmd,
			 union iwreq_data *	wrqu,
			 char *			extra)
{
	const struct iw_ioctl_description *	descr = NULL;
	int extra_len = 0;
	struct iw_event  *event;		/* Mallocated whole event */
	int event_len;				/* Its size */
	int hdr_len;				/* Size of the event header */
	/* Don't "optimise" the following variable, it will crash */
	unsigned	cmd_index;		/* *MUST* be unsigned */

	/* Get the description of the IOCTL */
	if(cmd <= SIOCIWLAST) {
		cmd_index = cmd - SIOCIWFIRST;
		if(cmd_index < standard_ioctl_num)
			descr = &(standard_ioctl[cmd_index]);
	} else {
		cmd_index = cmd - IWEVFIRST;
		if(cmd_index < standard_event_num)
			descr = &(standard_event[cmd_index]);
	}
	/* Don't accept unknown events */
	if(descr == NULL) {
		/* Note : we don't return an error to the driver, because
		 * the driver would not know what to do about it. It can't
		 * return an error to the user, because the event is not
		 * initiated by a user request.
		 * The best the driver could do is to log an error message.
		 * We will do it ourselves instead...
		 */
	  	printk(KERN_ERR "%s (WE) : Invalid/Unknown Wireless Event (0x%04X)\n",
		       dev->name, cmd);
		return;
	}
#ifdef WE_EVENT_DEBUG
	printk(KERN_DEBUG "%s (WE) : Got event 0x%04X\n",
	       dev->name, cmd);
	printk(KERN_DEBUG "%s (WE) : Header type : %d, Token type : %d, size : %d, token : %d\n", dev->name, descr->header_type, descr->token_type, descr->token_size, descr->max_tokens);
#endif	/* WE_EVENT_DEBUG */

	/* Check extra parameters and set extra_len */
	if(descr->header_type == IW_HEADER_TYPE_POINT) {
		/* Check if number of token fits within bounds */
		if(wrqu->data.length > descr->max_tokens) {
		  	printk(KERN_ERR "%s (WE) : Wireless Event too big (%d)\n", dev->name, wrqu->data.length);
			return;
		}
		if(wrqu->data.length < descr->min_tokens) {
		  	printk(KERN_ERR "%s (WE) : Wireless Event too small (%d)\n", dev->name, wrqu->data.length);
			return;
		}
		/* Calculate extra_len - extra is NULL for restricted events */
		if(extra != NULL)
			extra_len = wrqu->data.length * descr->token_size;
#ifdef WE_EVENT_DEBUG
		printk(KERN_DEBUG "%s (WE) : Event 0x%04X, tokens %d, extra_len %d\n", dev->name, cmd, wrqu->data.length, extra_len);
#endif	/* WE_EVENT_DEBUG */
	}

	/* Total length of the event */
	hdr_len = event_type_size[descr->header_type];
	event_len = hdr_len + extra_len;

#ifdef WE_EVENT_DEBUG
	printk(KERN_DEBUG "%s (WE) : Event 0x%04X, hdr_len %d, event_len %d\n", dev->name, cmd, hdr_len, event_len);
#endif	/* WE_EVENT_DEBUG */

	/* Create temporary buffer to hold the event */
	event = kmalloc(event_len, GFP_ATOMIC);
	if(event == NULL)
		return;

	/* Fill event */
	event->len = event_len;
	event->cmd = cmd;
	memcpy(&event->u, wrqu, hdr_len - IW_EV_LCP_LEN);
	if(extra != NULL)
		memcpy(((char *) event) + hdr_len, extra, extra_len);

#ifdef WE_EVENT_NETLINK
	/* rtnetlink event channel */
	rtmsg_iwinfo(dev, (char *) event, event_len);
#endif	/* WE_EVENT_NETLINK */

	/* Cleanup */
	kfree(event);

	return;		/* Always success, I guess ;-) */
}

/********************** ENHANCED IWSPY SUPPORT **********************/
/*
 * In the old days, the driver was handling spy support all by itself.
 * Now, the driver can delegate this task to Wireless Extensions.
 * It needs to use those standard spy iw_handler in struct iw_handler_def,
 * push data to us via XXX and include struct iw_spy_data in its
 * private part.
 * One of the main advantage of centralising spy support here is that
 * it becomes much easier to improve and extend it without having to touch
 * the drivers. One example is the addition of the Spy-Threshold events.
 * Note : IW_WIRELESS_SPY is defined in iw_handler.h
 */

/*------------------------------------------------------------------*/
/*
 * Standard Wireless Handler : set Spy List
 */
int iw_handler_set_spy(struct net_device *	dev,
		       struct iw_request_info *	info,
		       union iwreq_data *	wrqu,
		       char *			extra)
{
#ifdef IW_WIRELESS_SPY
	struct iw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	struct sockaddr *	address = (struct sockaddr *) extra;

	/* Disable spy collection while we copy the addresses.
	 * As we don't disable interrupts, we need to do this to avoid races.
	 * As we are the only writer, this is good enough. */
	spydata->spy_number = 0;

	/* Are there are addresses to copy? */
	if(wrqu->data.length > 0) {
		int i;

		/* Copy addresses */
		for(i = 0; i < wrqu->data.length; i++)
			memcpy(spydata->spy_address[i], address[i].sa_data,
			       ETH_ALEN);
		/* Reset stats */
		memset(spydata->spy_stat, 0,
		       sizeof(struct iw_quality) * IW_MAX_SPY);

#ifdef WE_SPY_DEBUG
		printk(KERN_DEBUG "iw_handler_set_spy() :  offset %ld, spydata %p, num %d\n", dev->wireless_handlers->spy_offset, spydata, wrqu->data.length);
		for (i = 0; i < wrqu->data.length; i++)
			printk(KERN_DEBUG
			       "%02X:%02X:%02X:%02X:%02X:%02X \n",
			       spydata->spy_address[i][0],
			       spydata->spy_address[i][1],
			       spydata->spy_address[i][2],
			       spydata->spy_address[i][3],
			       spydata->spy_address[i][4],
			       spydata->spy_address[i][5]);
#endif	/* WE_SPY_DEBUG */
	}
	/* Enable addresses */
	spydata->spy_number = wrqu->data.length;

	return 0;
#else /* IW_WIRELESS_SPY */
	return -EOPNOTSUPP;
#endif /* IW_WIRELESS_SPY */
}

/*------------------------------------------------------------------*/
/*
 * Standard Wireless Handler : get Spy List
 */
int iw_handler_get_spy(struct net_device *	dev,
		       struct iw_request_info *	info,
		       union iwreq_data *	wrqu,
		       char *			extra)
{
#ifdef IW_WIRELESS_SPY
	struct iw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	struct sockaddr *	address = (struct sockaddr *) extra;
	int			i;

	wrqu->data.length = spydata->spy_number;

	/* Copy addresses. */
	for(i = 0; i < spydata->spy_number; i++) 	{
		memcpy(address[i].sa_data, spydata->spy_address[i], ETH_ALEN);
		address[i].sa_family = AF_UNIX;
	}
	/* Copy stats to the user buffer (just after). */
	if(spydata->spy_number > 0)
		memcpy(extra  + (sizeof(struct sockaddr) *spydata->spy_number),
		       spydata->spy_stat,
		       sizeof(struct iw_quality) * spydata->spy_number);
	/* Reset updated flags. */
	for(i = 0; i < spydata->spy_number; i++)
		spydata->spy_stat[i].updated = 0;
	return 0;
#else /* IW_WIRELESS_SPY */
	return -EOPNOTSUPP;
#endif /* IW_WIRELESS_SPY */
}

/*------------------------------------------------------------------*/
/*
 * Standard Wireless Handler : set spy threshold
 */
int iw_handler_set_thrspy(struct net_device *	dev,
			  struct iw_request_info *info,
			  union iwreq_data *	wrqu,
			  char *		extra)
{
#ifdef IW_WIRELESS_THRSPY
	struct iw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	struct iw_thrspy *	threshold = (struct iw_thrspy *) extra;

	/* Just do it */
	memcpy(&(spydata->spy_thr_low), &(threshold->low),
	       2 * sizeof(struct iw_quality));

	/* Clear flag */
	memset(spydata->spy_thr_under, '\0', sizeof(spydata->spy_thr_under));

#ifdef WE_SPY_DEBUG
	printk(KERN_DEBUG "iw_handler_set_thrspy() :  low %d ; high %d\n", spydata->spy_thr_low.level, spydata->spy_thr_high.level);
#endif	/* WE_SPY_DEBUG */

	return 0;
#else /* IW_WIRELESS_THRSPY */
	return -EOPNOTSUPP;
#endif /* IW_WIRELESS_THRSPY */
}

/*------------------------------------------------------------------*/
/*
 * Standard Wireless Handler : get spy threshold
 */
int iw_handler_get_thrspy(struct net_device *	dev,
			  struct iw_request_info *info,
			  union iwreq_data *	wrqu,
			  char *		extra)
{
#ifdef IW_WIRELESS_THRSPY
	struct iw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	struct iw_thrspy *	threshold = (struct iw_thrspy *) extra;

	/* Just do it */
	memcpy(&(threshold->low), &(spydata->spy_thr_low),
	       2 * sizeof(struct iw_quality));

	return 0;
#else /* IW_WIRELESS_THRSPY */
	return -EOPNOTSUPP;
#endif /* IW_WIRELESS_THRSPY */
}

#ifdef IW_WIRELESS_THRSPY
/*------------------------------------------------------------------*/
/*
 * Prepare and send a Spy Threshold event
 */
static void iw_send_thrspy_event(struct net_device *	dev,
				 struct iw_spy_data *	spydata,
				 unsigned char *	address,
				 struct iw_quality *	wstats)
{
	union iwreq_data	wrqu;
	struct iw_thrspy	threshold;

	/* Init */
	wrqu.data.length = 1;
	wrqu.data.flags = 0;
	/* Copy address */
	memcpy(threshold.addr.sa_data, address, ETH_ALEN);
	threshold.addr.sa_family = ARPHRD_ETHER;
	/* Copy stats */
	memcpy(&(threshold.qual), wstats, sizeof(struct iw_quality));
	/* Copy also thresholds */
	memcpy(&(threshold.low), &(spydata->spy_thr_low),
	       2 * sizeof(struct iw_quality));

#ifdef WE_SPY_DEBUG
	printk(KERN_DEBUG "iw_send_thrspy_event() : address %02X:%02X:%02X:%02X:%02X:%02X, level %d, up = %d\n",
	       threshold.addr.sa_data[0],
	       threshold.addr.sa_data[1],
	       threshold.addr.sa_data[2],
	       threshold.addr.sa_data[3],
	       threshold.addr.sa_data[4],
	       threshold.addr.sa_data[5], threshold.qual.level);
#endif	/* WE_SPY_DEBUG */

	/* Send event to user space */
	wireless_send_event(dev, SIOCGIWTHRSPY, &wrqu, (char *) &threshold);
}
#endif /* IW_WIRELESS_THRSPY */

/* ---------------------------------------------------------------- */
/*
 * Call for the driver to update the spy data.
 * For now, the spy data is a simple array. As the size of the array is
 * small, this is good enough. If we wanted to support larger number of
 * spy addresses, we should use something more efficient...
 */
void wireless_spy_update(struct net_device *	dev,
			 unsigned char *	address,
			 struct iw_quality *	wstats)
{
#ifdef IW_WIRELESS_SPY
	struct iw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	int			i;
	int			match = -1;

#ifdef WE_SPY_DEBUG
	printk(KERN_DEBUG "wireless_spy_update() :  offset %ld, spydata %p, address %02X:%02X:%02X:%02X:%02X:%02X\n", dev->wireless_handlers->spy_offset, spydata, address[0], address[1], address[2], address[3], address[4], address[5]);
#endif	/* WE_SPY_DEBUG */

	/* Update all records that match */
	for(i = 0; i < spydata->spy_number; i++)
		if(!memcmp(address, spydata->spy_address[i], ETH_ALEN)) {
			memcpy(&(spydata->spy_stat[i]), wstats,
			       sizeof(struct iw_quality));
			match = i;
		}
#ifdef IW_WIRELESS_THRSPY
	/* Generate an event if we cross the spy threshold.
	 * To avoid event storms, we have a simple hysteresis : we generate
	 * event only when we go under the low threshold or above the
	 * high threshold. */
	if(match >= 0) {
		if(spydata->spy_thr_under[match]) {
			if(wstats->level > spydata->spy_thr_high.level) {
				spydata->spy_thr_under[match] = 0;
				iw_send_thrspy_event(dev, spydata,
						     address, wstats);
			}
		} else {
			if(wstats->level < spydata->spy_thr_low.level) {
				spydata->spy_thr_under[match] = 1;
				iw_send_thrspy_event(dev, spydata,
						     address, wstats);
			}
		}
	}
#endif /* IW_WIRELESS_THRSPY */
#endif /* IW_WIRELESS_SPY */
}

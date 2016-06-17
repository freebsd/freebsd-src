/*
 *  pm.h - Power management interface
 *
 *  Copyright (C) 2000 Andrew Henroid
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_PM_H
#define _LINUX_PM_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <linux/list.h>

/*
 * Power management requests
 */
enum
{
	PM_SUSPEND, /* enter D1-D3 */
	PM_RESUME,  /* enter D0 */

	PM_SAVE_STATE,  /* save device's state */

	/* enable wake-on */
	PM_SET_WAKEUP,

	/* bus resource management */
	PM_GET_RESOURCES,
	PM_SET_RESOURCES,

	/* base station management */
	PM_EJECT,
	PM_LOCK,
};

typedef int pm_request_t;

/*
 * Device types
 */
enum
{
	PM_UNKNOWN_DEV = 0, /* generic */
	PM_SYS_DEV,	    /* system device (fan, KB controller, ...) */
	PM_PCI_DEV,	    /* PCI device */
	PM_USB_DEV,	    /* USB device */
	PM_SCSI_DEV,	    /* SCSI device */
	PM_ISA_DEV,	    /* ISA device */
	PM_MTD_DEV,	    /* Memory Technology Device */
};

typedef int pm_dev_t;

/*
 * System device hardware ID (PnP) values
 */
enum
{
	PM_SYS_UNKNOWN = 0x00000000, /* generic */
	PM_SYS_KBC =	 0x41d00303, /* keyboard controller */
	PM_SYS_COM =	 0x41d00500, /* serial port */
	PM_SYS_IRDA =	 0x41d00510, /* IRDA controller */
	PM_SYS_FDC =	 0x41d00700, /* floppy controller */
	PM_SYS_VGA =	 0x41d00900, /* VGA controller */
	PM_SYS_PCMCIA =	 0x41d00e00, /* PCMCIA controller */
};

/*
 * Device identifier
 */
#define PM_PCI_ID(dev) ((dev)->bus->number << 16 | (dev)->devfn)

/*
 * Request handler callback
 */
struct pm_dev;

typedef int (*pm_callback)(struct pm_dev *dev, pm_request_t rqst, void *data);

/*
 * Dynamic device information
 */
struct pm_dev
{
	pm_dev_t	 type;
	unsigned long	 id;
	pm_callback	 callback;
	void		*data;

	unsigned long	 flags;
	unsigned long	 state;
	unsigned long	 prev_state;

	struct list_head entry;
};

#ifdef CONFIG_PM

extern int pm_active;

#define PM_IS_ACTIVE() (pm_active != 0)

/*
 * Register a device with power management
 */
struct pm_dev *pm_register(pm_dev_t type,
			   unsigned long id,
			   pm_callback callback);

/*
 * Unregister a device with power management
 */
void pm_unregister(struct pm_dev *dev);

/*
 * Unregister all devices with matching callback
 */
void pm_unregister_all(pm_callback callback);

/*
 * Send a request to a single device
 */
int pm_send(struct pm_dev *dev, pm_request_t rqst, void *data);

/*
 * Send a request to all devices
 */
int pm_send_all(pm_request_t rqst, void *data);

/*
 * Find a device
 */
struct pm_dev *pm_find(pm_dev_t type, struct pm_dev *from);

static inline void pm_access(struct pm_dev *dev) {}
static inline void pm_dev_idle(struct pm_dev *dev) {}

#else /* CONFIG_PM */

#define PM_IS_ACTIVE() 0

static inline struct pm_dev *pm_register(pm_dev_t type,
					 unsigned long id,
					 pm_callback callback)
{
	return 0;
}

static inline void pm_unregister(struct pm_dev *dev) {}

static inline void pm_unregister_all(pm_callback callback) {}

static inline int pm_send(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	return 0;
}

static inline int pm_send_all(pm_request_t rqst, void *data)
{
	return 0;
}

static inline struct pm_dev *pm_find(pm_dev_t type, struct pm_dev *from)
{
	return 0;
}

static inline void pm_access(struct pm_dev *dev) {}
static inline void pm_dev_idle(struct pm_dev *dev) {}

#endif /* CONFIG_PM */

extern void (*pm_idle)(void);
extern void (*pm_power_off)(void);

#endif /* __KERNEL__ */

#endif /* _LINUX_PM_H */

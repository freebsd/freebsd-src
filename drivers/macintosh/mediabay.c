/*
 * Driver for the media bay on the PowerBook 3400 and 2400.
 *
 * Copyright (C) 1998 Paul Mackerras.
 *
 * Various evolutions by Benjamin Herrenschmidt & Henry Worth
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#define __KERNEL_SYSCALLS__

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/hdreg.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <asm/prom.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/mediabay.h>
#include <asm/sections.h>
#include <asm/ohare.h>
#include <asm/heathrow.h>
#include <asm/keylargo.h>
#include <linux/adb.h>
#include <linux/pmu.h>

#ifdef CONFIG_PMAC_PBOOK
static int mb_notify_sleep(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier mb_sleep_notifier = {
	mb_notify_sleep,
	SLEEP_LEVEL_MEDIABAY,
};
#endif

#undef MB_USE_INTERRUPTS
#define MB_DEBUG
#define MB_IGNORE_SIGNALS

#ifdef MB_DEBUG
#define MBDBG(fmt, arg...)	printk(KERN_INFO fmt , ## arg)
#else
#define MBDBG(fmt, arg...)	do { } while (0)
#endif

/* Type of media bay */
enum {
	mb_ohare,
	mb_heathrow,
	mb_keylargo
};

#define MB_FCR32(bay, r)	((bay)->base + ((r) >> 2))
#define MB_FCR8(bay, r)		(((volatile u8*)((bay)->base)) + (r))

#define MB_IN32(bay,r)		(in_le32(MB_FCR32(bay,r)))
#define MB_OUT32(bay,r,v)	(out_le32(MB_FCR32(bay,r), (v)))
#define MB_BIS(bay,r,v)		(MB_OUT32((bay), (r), MB_IN32((bay), r) | (v)))
#define MB_BIC(bay,r,v)		(MB_OUT32((bay), (r), MB_IN32((bay), r) & ~(v)))
#define MB_IN8(bay,r)		(in_8(MB_FCR8(bay,r)))
#define MB_OUT8(bay,r,v)	(out_8(MB_FCR8(bay,r), (v)))

struct media_bay_info;

struct mb_ops {
	char*	name;
	u8	(*content)(struct media_bay_info* bay);
	void	(*power)(struct media_bay_info* bay, int on_off);
	int	(*setup_bus)(struct media_bay_info* bay, u8 device_id);
	void	(*un_reset)(struct media_bay_info* bay);
	void	(*un_reset_ide)(struct media_bay_info* bay);
};

struct media_bay_info {
	volatile u32*			base;
	int				content_id;
	int				state;
	int				last_value;
	int				value_count;
	int				timer;
	struct device_node*		dev_node;
	int				mb_type;
	struct mb_ops*			ops;
	int				index;
	int				cached_gpio;
#ifdef CONFIG_BLK_DEV_IDE
	unsigned long			cd_base;
	int 				cd_index;
	int				cd_irq;
	int				cd_retry;
#endif
};

#define MAX_BAYS	2

static struct media_bay_info media_bays[MAX_BAYS];
int media_bay_count = 0;

#ifdef CONFIG_BLK_DEV_IDE
/* check the busy bit in the media-bay ide interface
   (assumes the media-bay contains an ide device) */
#define MB_IDE_READY(i)	((readb(media_bays[i].cd_base + 0x70) & 0x80) == 0)
#endif

/* Note: All delays are not in milliseconds and converted to HZ relative
 * values by the macro below
 */
#define MS_TO_HZ(ms)	((ms * HZ) / 1000)

/*
 * Consider the media-bay ID value stable if it is the same for
 * this number of milliseconds
 */
#define MB_STABLE_DELAY	100

/* Wait after powering up the media bay this delay in ms
 * timeout bumped for some powerbooks
 */
#define MB_POWER_DELAY	200

/*
 * Hold the media-bay reset signal true for this many ticks
 * after a device is inserted before releasing it.
 */
#define MB_RESET_DELAY	40

/*
 * Wait this long after the reset signal is released and before doing
 * further operations. After this delay, the IDE reset signal is released
 * too for an IDE device
 */
#define MB_SETUP_DELAY	100

/*
 * Wait this many ticks after an IDE device (e.g. CD-ROM) is inserted
 * (or until the device is ready) before waiting for busy bit to disappear
 */
#define MB_IDE_WAIT	1000

/*
 * Timeout waiting for busy bit of an IDE device to go down
 */
#define MB_IDE_TIMEOUT	5000

/*
 * Max retries of the full power up/down sequence for an IDE device
 */
#define MAX_CD_RETRIES	3

/*
 * States of a media bay
 */
enum {
	mb_empty = 0,		/* Idle */
	mb_powering_up,		/* power bit set, waiting MB_POWER_DELAY */
	mb_enabling_bay,	/* enable bits set, waiting MB_RESET_DELAY */
	mb_resetting,		/* reset bit unset, waiting MB_SETUP_DELAY */
	mb_ide_resetting,	/* IDE reset bit unser, waiting MB_IDE_WAIT */
	mb_ide_waiting,		/* Waiting for BUSY bit to go away until MB_IDE_TIMEOUT */
	mb_up,			/* Media bay full */
	mb_powering_down	/* Powering down (avoid too fast down/up) */
};

#define MB_POWER_SOUND		0x08
#define MB_POWER_FLOPPY		0x04
#define MB_POWER_ATA		0x02
#define MB_POWER_PCI		0x01
#define MB_POWER_OFF		0x00

/*
 * Functions for polling content of media bay
 */
 
static u8 __pmac
ohare_mb_content(struct media_bay_info *bay)
{
	return (MB_IN32(bay, OHARE_MBCR) >> 12) & 7;
}

static u8 __pmac
heathrow_mb_content(struct media_bay_info *bay)
{
	return (MB_IN32(bay, HEATHROW_MBCR) >> 12) & 7;
}

static u8 __pmac
keylargo_mb_content(struct media_bay_info *bay)
{
	int new_gpio;

	new_gpio = MB_IN8(bay, KL_GPIO_MEDIABAY_IRQ) & KEYLARGO_GPIO_INPUT_DATA;
	if (new_gpio) {
		bay->cached_gpio = new_gpio;
		return MB_NO;
	} else if (bay->cached_gpio != new_gpio) {
		MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_ENABLE);
		(void)MB_IN32(bay, KEYLARGO_MBCR);
		udelay(5);
		MB_BIC(bay, KEYLARGO_MBCR, 0x0000000F);
		(void)MB_IN32(bay, KEYLARGO_MBCR);
		udelay(5);
		bay->cached_gpio = new_gpio;
	}
	return (MB_IN32(bay, KEYLARGO_MBCR) >> 4) & 7;
}

/*
 * Functions for powering up/down the bay, puts the bay device
 * into reset state as well
 */

static void __pmac
ohare_mb_power(struct media_bay_info* bay, int on_off)
{
	if (on_off) {
		/* Power up device, assert it's reset line */
		MB_BIC(bay, OHARE_FCR, OH_BAY_RESET_N);
		MB_BIC(bay, OHARE_FCR, OH_BAY_POWER_N);
	} else {
		/* Disable all devices */
		MB_BIC(bay, OHARE_FCR, OH_BAY_DEV_MASK);
		MB_BIC(bay, OHARE_FCR, OH_FLOPPY_ENABLE);
		/* Cut power from bay, release reset line */
		MB_BIS(bay, OHARE_FCR, OH_BAY_POWER_N);
		MB_BIS(bay, OHARE_FCR, OH_BAY_RESET_N);
		MB_BIS(bay, OHARE_FCR, OH_IDE1_RESET_N);
	}
	MB_BIC(bay, OHARE_MBCR, 0x00000F00);
}

static void __pmac
heathrow_mb_power(struct media_bay_info* bay, int on_off)
{
	if (on_off) {
		/* Power up device, assert it's reset line */
		MB_BIC(bay, HEATHROW_FCR, HRW_BAY_RESET_N);
		MB_BIC(bay, HEATHROW_FCR, HRW_BAY_POWER_N);
	} else {
		/* Disable all devices */
		MB_BIC(bay, HEATHROW_FCR, HRW_BAY_DEV_MASK);
		MB_BIC(bay, HEATHROW_FCR, HRW_SWIM_ENABLE);
		/* Cut power from bay, release reset line */
		MB_BIS(bay, HEATHROW_FCR, HRW_BAY_POWER_N);
		MB_BIS(bay, HEATHROW_FCR, HRW_BAY_RESET_N);
		MB_BIS(bay, HEATHROW_FCR, HRW_IDE1_RESET_N);
	}
	MB_BIC(bay, HEATHROW_MBCR, 0x00000F00);
}

static void __pmac
keylargo_mb_power(struct media_bay_info* bay, int on_off)
{
	if (on_off) {
		/* Power up device, assert it's reset line */
            	MB_BIC(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_RESET);
            	MB_BIC(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_POWER);
	} else {
		/* Disable all devices */
		MB_BIC(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_MASK);
		MB_BIC(bay, KEYLARGO_FCR1, KL1_EIDE0_ENABLE);
		/* Cut power from bay, release reset line */
		MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_POWER);
		MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_RESET);
		MB_BIS(bay, KEYLARGO_FCR1, KL1_EIDE0_RESET_N);
	}
	MB_BIC(bay, KEYLARGO_MBCR, 0x0000000F);
}

/*
 * Functions for configuring the media bay for a given type of device,
 * enable the related busses
 */

static int __pmac
ohare_mb_setup_bus(struct media_bay_info* bay, u8 device_id)
{
	switch(device_id) {
		case MB_FD:
		case MB_FD1:
			MB_BIS(bay, OHARE_FCR, OH_BAY_FLOPPY_ENABLE);
			MB_BIS(bay, OHARE_FCR, OH_FLOPPY_ENABLE);
			return 0;
		case MB_CD:
			MB_BIC(bay, OHARE_FCR, OH_IDE1_RESET_N);
			MB_BIS(bay, OHARE_FCR, OH_BAY_IDE_ENABLE);
			return 0;
		case MB_PCI:
			MB_BIS(bay, OHARE_FCR, OH_BAY_PCI_ENABLE);
			return 0;
	}
	return -ENODEV;
}

static int __pmac
heathrow_mb_setup_bus(struct media_bay_info* bay, u8 device_id)
{
	switch(device_id) {
		case MB_FD:
		case MB_FD1:
			MB_BIS(bay, HEATHROW_FCR, HRW_BAY_FLOPPY_ENABLE);
			MB_BIS(bay, HEATHROW_FCR, HRW_SWIM_ENABLE);
			return 0;
		case MB_CD:
			MB_BIC(bay, HEATHROW_FCR, HRW_IDE1_RESET_N);
			MB_BIS(bay, HEATHROW_FCR, HRW_BAY_IDE_ENABLE);
			return 0;
		case MB_PCI:
			MB_BIS(bay, HEATHROW_FCR, HRW_BAY_PCI_ENABLE);
			return 0;
	}
	return -ENODEV;
}

static int __pmac
keylargo_mb_setup_bus(struct media_bay_info* bay, u8 device_id)
{
	switch(device_id) {
		case MB_CD:
			MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_IDE_ENABLE);
			MB_BIC(bay, KEYLARGO_FCR1, KL1_EIDE0_RESET_N);
			MB_BIS(bay, KEYLARGO_FCR1, KL1_EIDE0_ENABLE);
			return 0;
		case MB_PCI:
			MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_PCI_ENABLE);
			return 0;
		case MB_SOUND:
			MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_SOUND_ENABLE);
			return 0;
	}
	return -ENODEV;
}

/*
 * Functions for tweaking resets
 */

static void __pmac
ohare_mb_un_reset(struct media_bay_info* bay)
{
	MB_BIS(bay, OHARE_FCR, OH_BAY_RESET_N);
}

static void __pmac
heathrow_mb_un_reset(struct media_bay_info* bay)
{
	MB_BIS(bay, HEATHROW_FCR, HRW_BAY_RESET_N);
}

static void __pmac
keylargo_mb_un_reset(struct media_bay_info* bay)
{
	MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_DEV_RESET);
}

static void __pmac
ohare_mb_un_reset_ide(struct media_bay_info* bay)
{
	MB_BIS(bay, OHARE_FCR, OH_IDE1_RESET_N);
}

static void __pmac
heathrow_mb_un_reset_ide(struct media_bay_info* bay)
{
	MB_BIS(bay, HEATHROW_FCR, HRW_IDE1_RESET_N);
}

static void __pmac
keylargo_mb_un_reset_ide(struct media_bay_info* bay)
{
	MB_BIS(bay, KEYLARGO_FCR1, KL1_EIDE0_RESET_N);
}

static inline void __pmac
set_mb_power(struct media_bay_info* bay, int onoff)
{
	/* Power up up and assert the bay reset line */
	if (onoff) {
		bay->ops->power(bay, 1);
		bay->state = mb_powering_up;
		MBDBG("mediabay%d: powering up\n", bay->index);
	} else { 
		/* Make sure everything is powered down & disabled */
		bay->ops->power(bay, 0);
		bay->state = mb_powering_down;
		MBDBG("mediabay%d: powering down\n", bay->index);
	}
	bay->timer = MS_TO_HZ(MB_POWER_DELAY);
}

static void __pmac
poll_media_bay(struct media_bay_info* bay)
{
	int id = bay->ops->content(bay);

	if (id == bay->last_value) {
	    if (id != bay->content_id
	        && ++bay->value_count >= MS_TO_HZ(MB_STABLE_DELAY)) {
	        /* If the device type changes without going thru "MB_NO", we force
	           a pass by "MB_NO" to make sure things are properly reset */
	        if ((id != MB_NO) && (bay->content_id != MB_NO)) {
	            id = MB_NO;
		    MBDBG("mediabay%d: forcing MB_NO\n", bay->index);
		}
		MBDBG("mediabay%d: switching to %d\n", bay->index, id);
		set_mb_power(bay, id != MB_NO);
		bay->content_id = id;
		if (id == MB_NO) {
#ifdef CONFIG_BLK_DEV_IDE
		    bay->cd_retry = 0;
#endif
		    printk(KERN_INFO "media bay %d is empty\n", bay->index);
		}
 	    }
	} else {
		bay->last_value = id;
		bay->value_count = 0;
	}
}

int __pmac
check_media_bay(struct device_node *which_bay, int what)
{
#ifdef CONFIG_BLK_DEV_IDE
	int	i;

	for (i=0; i<media_bay_count; i++)
		if (which_bay == media_bays[i].dev_node)
		{
			if ((what == media_bays[i].content_id) && media_bays[i].state == mb_up)
				return 0;
			media_bays[i].cd_index = -1;
			return -EINVAL;
		}
#endif /* CONFIG_BLK_DEV_IDE */
	return -ENODEV;
}

int __pmac
check_media_bay_by_base(unsigned long base, int what)
{
#ifdef CONFIG_BLK_DEV_IDE
	int	i;

	for (i=0; i<media_bay_count; i++)
		if (base == media_bays[i].cd_base)
		{
			if ((what == media_bays[i].content_id) && media_bays[i].state == mb_up)
				return 0;
			media_bays[i].cd_index = -1;
			return -EINVAL;
		} 
#endif
	
	return -ENODEV;
}

int __pmac
media_bay_set_ide_infos(struct device_node* which_bay, unsigned long base,
	int irq, int index)
{
#ifdef CONFIG_BLK_DEV_IDE
	int	i;

	for (i=0; i<media_bay_count; i++)
		if (which_bay == media_bays[i].dev_node)
		{
			int timeout = 5000;
			
 			media_bays[i].cd_base	= base;
			media_bays[i].cd_irq	= irq;

			if ((MB_CD != media_bays[i].content_id) || media_bays[i].state != mb_up)
				return 0;

			printk(KERN_DEBUG "Registered ide %d for media bay %d\n", index, i);
			do {
				if (MB_IDE_READY(i)) {
					media_bays[i].cd_index	= index;
					return 0;
				}
				mdelay(1);
			} while(--timeout);
			printk(KERN_DEBUG "Timeount waiting IDE in bay %d\n", i);
			return -ENODEV;
		} 
#endif
	
	return -ENODEV;
}

static void __pmac
media_bay_step(int i)
{
	struct media_bay_info* bay = &media_bays[i];

	/* We don't poll when powering down */
	if (bay->state != mb_powering_down)
	    poll_media_bay(bay);

	/* If timer expired or polling IDE busy, run state machine */
	if ((bay->state != mb_ide_waiting) && (bay->timer != 0) && ((--bay->timer) != 0))
	    return;

	switch(bay->state) {
	case mb_powering_up:
	    	if (bay->ops->setup_bus(bay, bay->last_value) < 0) {
			MBDBG("mediabay%d: device not supported (kind:%d)\n", i, bay->content_id);
	    		set_mb_power(bay, 0);
	    		break;
	    	}
	    	bay->timer = MS_TO_HZ(MB_RESET_DELAY);
	    	bay->state = mb_enabling_bay;
		MBDBG("mediabay%d: enabling (kind:%d)\n", i, bay->content_id);
		break;
	case mb_enabling_bay:
		bay->ops->un_reset(bay);
	    	bay->timer = MS_TO_HZ(MB_SETUP_DELAY);
	    	bay->state = mb_resetting;
		MBDBG("mediabay%d: waiting reset (kind:%d)\n", i, bay->content_id);
	    	break;
	    
	case mb_resetting:
		if (bay->content_id != MB_CD) {
			MBDBG("mediabay%d: bay is up (kind:%d)\n", i, bay->content_id);
			bay->state = mb_up;
			break;
	    	}
#ifdef CONFIG_BLK_DEV_IDE
		MBDBG("mediabay%d: waiting IDE reset (kind:%d)\n", i, bay->content_id);
		bay->ops->un_reset_ide(bay);
	    	bay->timer = MS_TO_HZ(MB_IDE_WAIT);
	    	bay->state = mb_ide_resetting;
#else
		printk(KERN_DEBUG "media-bay %d is ide (not compiled in kernel)\n", i);
		set_mb_power(bay, 0);
#endif /* CONFIG_BLK_DEV_IDE */
	    	break;
	    
#ifdef CONFIG_BLK_DEV_IDE
	case mb_ide_resetting:
	    	bay->timer = MS_TO_HZ(MB_IDE_TIMEOUT);
	    	bay->state = mb_ide_waiting;
		MBDBG("mediabay%d: waiting IDE ready (kind:%d)\n", i, bay->content_id);
	    	break;
	    
	case mb_ide_waiting:
	    	if (bay->cd_base == 0) {
			bay->timer = 0;
			bay->state = mb_up;
			MBDBG("mediabay%d: up before IDE init\n", i);
			break;
	    	} else if (MB_IDE_READY(i)) {
			bay->timer = 0;
			bay->state = mb_up;
			if (bay->cd_index < 0) {
				pmu_suspend();
				bay->cd_index = ide_register(bay->cd_base, 0, bay->cd_irq);
				pmu_resume();
			}
			if (bay->cd_index == -1) {
				/* We eventually do a retry */
				bay->cd_retry++;
				printk("IDE register error\n");
				set_mb_power(bay, 0);
			} else {
				printk(KERN_DEBUG "media-bay %d is ide %d\n", i, bay->cd_index);
				MBDBG("mediabay %d IDE ready\n", i);
			}
			break;
	    	} else if (bay->timer > 0)
	    		bay->timer--;
	    	if (bay->timer == 0) {
			printk("\nIDE Timeout in bay %d !\n", i);
			MBDBG("mediabay%d: nIDE Timeout !\n", i);
			set_mb_power(bay, 0);
	    	}
		break;
#endif /* CONFIG_BLK_DEV_IDE */

	case mb_powering_down:
	    	bay->state = mb_empty;
#ifdef CONFIG_BLK_DEV_IDE
    	        if (bay->cd_index >= 0) {
			printk(KERN_DEBUG "Unregistering mb %d ide, index:%d\n", i,
			       bay->cd_index);
			ide_unregister(bay->cd_index);
			bay->cd_index = -1;
		}
	    	if (bay->cd_retry) {
			if (bay->cd_retry > MAX_CD_RETRIES) {
				/* Should add an error sound (sort of beep in dmasound) */
				printk("\nmedia-bay %d, IDE device badly inserted or unrecognised\n", i);
			} else {
				/* Force a new power down/up sequence */
				bay->content_id = MB_NO;
			}
	    	}
#endif /* CONFIG_BLK_DEV_IDE */    
		MBDBG("mediabay%d: end of power down\n", i);
	    	break;
	}
}

/*
 * This procedure runs as a kernel thread to poll the media bay
 * once each tick and register and unregister the IDE interface
 * with the IDE driver.  It needs to be a thread because
 * ide_register can't be called from interrupt context.
 */
static int __pmac
media_bay_task(void *x)
{
	int	i;

	strcpy(current->comm, "media-bay");
#ifdef MB_IGNORE_SIGNALS
	sigfillset(&current->blocked);
#endif

	for (;;) {
		for (i = 0; i < media_bay_count; ++i)
			media_bay_step(i);

		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
		if (signal_pending(current))
			return 0;
	}
}

#ifdef MB_USE_INTERRUPTS
static void __pmac
media_bay_intr(int irq, void *devid, struct pt_regs *regs)
{
}
#endif

#ifdef CONFIG_PMAC_PBOOK
/*
 * notify clients before sleep and reset bus afterwards
 */
int __pmac
mb_notify_sleep(struct pmu_sleep_notifier *self, int when)
{
	struct media_bay_info* bay;
	int i;
	
	switch (when) {
	case PBOOK_SLEEP_REQUEST:
	case PBOOK_SLEEP_REJECT:
		break;
		
	case PBOOK_SLEEP_NOW:
		for (i=0; i<media_bay_count; i++) {
			bay = &media_bays[i];
			set_mb_power(bay, 0);
			mdelay(10);
		}
		break;
	case PBOOK_WAKE:
		for (i=0; i<media_bay_count; i++) {
			bay = &media_bays[i];
			/* We re-enable the bay using it's previous content
			   only if it did not change. Note those bozo timings,
			   they seem to help the 3400 get it right.
			 */
			/* Force MB power to 0 */
			set_mb_power(bay, 0);
			mdelay(MB_POWER_DELAY);
			if (bay->ops->content(bay) != bay->content_id)
				continue;
			set_mb_power(bay, 1);
			bay->last_value = bay->content_id;
			bay->value_count = MS_TO_HZ(MB_STABLE_DELAY);
			bay->timer = MS_TO_HZ(MB_POWER_DELAY);
#ifdef CONFIG_BLK_DEV_IDE
			bay->cd_retry = 0;
#endif
			do {
				mdelay(1000/HZ);
				media_bay_step(i);
			} while((media_bays[i].state != mb_empty) &&
				(media_bays[i].state != mb_up));
		}
		break;
	}
	return PBOOK_SLEEP_OK;
}
#endif /* CONFIG_PMAC_PBOOK */


/* Definitions of "ops" structures.
 */
static struct mb_ops ohare_mb_ops __pmacdata = {
	name:		"Ohare",
	content:	ohare_mb_content,
	power:		ohare_mb_power,
	setup_bus:	ohare_mb_setup_bus,
	un_reset:	ohare_mb_un_reset,
	un_reset_ide:	ohare_mb_un_reset_ide,
};

static struct mb_ops heathrow_mb_ops __pmacdata = {
	name:		"Heathrow",
	content:	heathrow_mb_content,
	power:		heathrow_mb_power,
	setup_bus:	heathrow_mb_setup_bus,
	un_reset:	heathrow_mb_un_reset,
	un_reset_ide:	heathrow_mb_un_reset_ide,
};

static struct mb_ops keylargo_mb_ops __pmacdata = {
	name:		"KeyLargo",
	content:	keylargo_mb_content,
	power:		keylargo_mb_power,
	setup_bus:	keylargo_mb_setup_bus,
	un_reset:	keylargo_mb_un_reset,
	un_reset_ide:	keylargo_mb_un_reset_ide,
};

/*
 * It seems that the bit for the media-bay interrupt in the IRQ_LEVEL
 * register is always set when there is something in the media bay.
 * This causes problems for the interrupt code if we attach an interrupt
 * handler to the media-bay interrupt, because it tends to go into
 * an infinite loop calling the media bay interrupt handler.
 * Therefore we do it all by polling the media bay once each tick.
 */

void __pmac
media_bay_init(void)
{
	struct device_node *np;
	int		n,i;
	
	for (i=0; i<MAX_BAYS; i++) {
		memset((char *)&media_bays[i], 0, sizeof(struct media_bay_info));
		media_bays[i].content_id	= -1;
#ifdef CONFIG_BLK_DEV_IDE
		media_bays[i].cd_index		= -1;
#endif
	}
	
	np = find_devices("media-bay");
	n = 0;
	while(np && (n<MAX_BAYS)) {
		struct media_bay_info* bay = &media_bays[n];
		if (!np->parent || np->n_addrs == 0 || !request_OF_resource(np, 0, NULL)) {
			np = np->next;
			printk(KERN_ERR "mediabay: Can't request IO resource !\n");
			continue;
		}
		bay->mb_type = mb_ohare;

		if (device_is_compatible(np, "keylargo-media-bay")) {
			bay->mb_type = mb_keylargo;
			bay->ops = &keylargo_mb_ops;
		} else if (device_is_compatible(np, "heathrow-media-bay")) {
			bay->mb_type = mb_heathrow;
			bay->ops = &heathrow_mb_ops;
		} else if (device_is_compatible(np, "ohare-media-bay")) {
			bay->mb_type = mb_ohare;
			bay->ops = &ohare_mb_ops;
		} else {
			printk(KERN_ERR "media-bay: Unknown bay type !\n");
			np = np->next;
			continue;
		}
		bay->base = (volatile u32*)ioremap(np->parent->addrs[0].address, 0x1000);

		/* Enable probe logic on keylargo */
		if (bay->mb_type == mb_keylargo)
			MB_BIS(bay, KEYLARGO_MBCR, KL_MBCR_MB0_ENABLE);
#ifdef MB_USE_INTERRUPTS
		if (np->n_intrs == 0) {
			printk(KERN_ERR "media-bay %d has no irq\n",n);
			np = np->next;
			continue;
		}

		if (request_irq(np->intrs[0].line, media_bay_intr, 0, "Media bay", (void *)n)) {
			printk(KERN_ERR "Couldn't get IRQ %d for media bay %d\n",
				np->intrs[0].line, n);
			np = np->next;
			continue;
		}
#endif	
		media_bay_count++;
	
		printk(KERN_INFO "mediabay%d: Registered %s media-bay\n", n, bay->ops->name);
		bay->dev_node = np;
		bay->index = n;

		/* Force an immediate detect */
		set_mb_power(bay, 0);
		mdelay(MB_POWER_DELAY);
		bay->content_id = MB_NO;
		bay->last_value = bay->ops->content(bay);
		bay->value_count = MS_TO_HZ(MB_STABLE_DELAY);
		bay->state = mb_empty;
		do {
			mdelay(1000/HZ);
			media_bay_step(n);
		} while((bay->state != mb_empty) &&
			(bay->state != mb_up));

		n++;
		np=np->next;
	}

	if (media_bay_count)
	{
#ifdef CONFIG_PMAC_PBOOK
		pmu_register_sleep_notifier(&mb_sleep_notifier);
#endif /* CONFIG_PMAC_PBOOK */

		if (kernel_thread(media_bay_task, NULL,
			      CLONE_FS | CLONE_FILES | CLONE_SIGHAND) < 0)
			printk(KERN_ERR "media-bay: Cannot create polling thread !\n");
	}
}


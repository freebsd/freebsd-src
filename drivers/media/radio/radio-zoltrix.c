/* zoltrix radio plus driver for Linux radio support
 * (c) 1998 C. van Schaik <carl@leg.uct.ac.za>
 *
 * BUGS  
 *  Due to the inconsistancy in reading from the signal flags
 *  it is difficult to get an accurate tuned signal.
 *
 *  It seems that the card is not linear to 0 volume. It cuts off
 *  at a low volume, and it is not possible (at least I have not found)
 *  to get fine volume control over the low volume range.
 *
 *  Some code derived from code by Romolo Manfredini
 *				   romolo@bicnet.it
 *
 * 1999-05-06 - (C. van Schaik)
 *	      - Make signal strength and stereo scans
 *	        kinder to cpu while in delay
 * 1999-01-05 - (C. van Schaik)
 *	      - Changed tuning to 1/160Mhz accuracy
 *	      - Added stereo support
 *		(card defaults to stereo)
 *		(can explicitly force mono on the card)
 *		(can detect if station is in stereo)
 *	      - Added unmute function
 *	      - Reworked ioctl functions
 * 2002-07-15 - Fix Stereo typo
 */

#include <linux/module.h>	/* Modules                        */
#include <linux/init.h>		/* Initdata                       */
#include <linux/ioport.h>	/* check_region, request_region   */
#include <linux/delay.h>	/* udelay                 */
#include <asm/io.h>		/* outb, outb_p                   */
#include <asm/uaccess.h>	/* copy to/from user              */
#include <linux/videodev.h>	/* kernel radio structs           */
#include <linux/config.h>	/* CONFIG_RADIO_ZOLTRIX_PORT      */

#ifndef CONFIG_RADIO_ZOLTRIX_PORT
#define CONFIG_RADIO_ZOLTRIX_PORT -1
#endif

static int io = CONFIG_RADIO_ZOLTRIX_PORT;
static int radio_nr = -1;
static int users = 0;

struct zol_device {
	int port;
	int curvol;
	unsigned long curfreq;
	int muted;
	unsigned int stereo;
	struct semaphore lock;
};


/* local things */

static void sleep_delay(void)
{
	/* Sleep nicely for +/- 10 mS */
	schedule();
}

static int zol_setvol(struct zol_device *dev, int vol)
{
	dev->curvol = vol;
	if (dev->muted)
		return 0;

	down(&dev->lock);
	if (vol == 0) {
		outb(0, io);
		outb(0, io);
		inb(io + 3);    /* Zoltrix needs to be read to confirm */
		up(&dev->lock);
		return 0;
	}

	outb(dev->curvol-1, io);
	sleep_delay();
	inb(io + 2);
	up(&dev->lock);
	return 0;
}

static void zol_mute(struct zol_device *dev)
{
	dev->muted = 1;
	down(&dev->lock);
	outb(0, io);
	outb(0, io);
	inb(io + 3);            /* Zoltrix needs to be read to confirm */
	up(&dev->lock);
}

static void zol_unmute(struct zol_device *dev)
{
	dev->muted = 0;
	zol_setvol(dev, dev->curvol);
}

static int zol_setfreq(struct zol_device *dev, unsigned long freq)
{
	/* tunes the radio to the desired frequency */
	unsigned long long bitmask, f, m;
	unsigned int stereo = dev->stereo;
	int i;

	if (freq == 0)
		return 1;
	m = (freq / 160 - 8800) * 2;
	f = (unsigned long long) m + 0x4d1c;

	bitmask = 0xc480402c10080000ull;
	i = 45;

	down(&dev->lock);
	
	outb(0, io);
	outb(0, io);
	inb(io + 3);            /* Zoltrix needs to be read to confirm */

	outb(0x40, io);
	outb(0xc0, io);

	bitmask = (bitmask ^ ((f & 0xff) << 47) ^ ((f & 0xff00) << 30) ^ ( stereo << 31));
	while (i--) {
		if ((bitmask & 0x8000000000000000ull) != 0) {
			outb(0x80, io);
			udelay(50);
			outb(0x00, io);
			udelay(50);
			outb(0x80, io);
			udelay(50);
		} else {
			outb(0xc0, io);
			udelay(50);
			outb(0x40, io);
			udelay(50);
			outb(0xc0, io);
			udelay(50);
		}
		bitmask *= 2;
	}
	/* termination sequence */
	outb(0x80, io);
	outb(0xc0, io);
	outb(0x40, io);
	udelay(1000);
	inb(io+2);

        udelay(1000);
        
	if (dev->muted)
	{
		outb(0, io);
		outb(0, io);
		inb(io + 3);
		udelay(1000);
	}
	
	up(&dev->lock);
	
	if(!dev->muted)
	{
	        zol_setvol(dev, dev->curvol);
	}
	return 0;
}

/* Get signal strength */

int zol_getsigstr(struct zol_device *dev)
{
	int a, b;

	down(&dev->lock);
	outb(0x00, io);         /* This stuff I found to do nothing */
	outb(dev->curvol, io);
	sleep_delay();
	sleep_delay();

	a = inb(io);
	sleep_delay();
	b = inb(io);

	up(&dev->lock);
	
	if (a != b)
		return (0);

        if ((a == 0xcf) || (a == 0xdf)  /* I found this out by playing */
		|| (a == 0xef))       /* with a binary scanner on the card io */
		return (1);
 	return (0);
}

int zol_is_stereo (struct zol_device *dev)
{
	int x1, x2;

	down(&dev->lock);
	
	outb(0x00, io);
	outb(dev->curvol, io);
	sleep_delay();
	sleep_delay();

	x1 = inb(io);
	sleep_delay();
	x2 = inb(io);

	up(&dev->lock);
	
	if ((x1 == x2) && (x1 == 0xcf))
		return 1;
	return 0;
}

static int zol_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct zol_device *zol = dev->priv;

	switch (cmd) {
	case VIDIOCGCAP:
		{
			struct video_capability v;
			v.type = VID_TYPE_TUNER;
			v.channels = 1 + zol->stereo;
			v.audios = 1;
			/* No we don't do pictures */
			v.maxwidth = 0;
			v.maxheight = 0;
			v.minwidth = 0;
			v.minheight = 0;
			strcpy(v.name, "Zoltrix Radio");
			if (copy_to_user(arg, &v, sizeof(v)))
				return -EFAULT;
			return 0;
		}
	case VIDIOCGTUNER:
		{
			struct video_tuner v;
			if (copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if (v.tuner)	
				return -EINVAL;
			strcpy(v.name, "FM");
			v.rangelow = (int) (88.0 * 16000);
			v.rangehigh = (int) (108.0 * 16000);
			v.flags = zol_is_stereo(zol)
					? VIDEO_TUNER_STEREO_ON : 0;
			v.flags |= VIDEO_TUNER_LOW;
			v.mode = VIDEO_MODE_AUTO;
			v.signal = 0xFFFF * zol_getsigstr(zol);
			if (copy_to_user(arg, &v, sizeof(v)))
				return -EFAULT;
			return 0;
		}
	case VIDIOCSTUNER:
		{
			struct video_tuner v;
			if (copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if (v.tuner != 0)
				return -EINVAL;
			/* Only 1 tuner so no setting needed ! */
			return 0;
		}
	case VIDIOCGFREQ:
		if (copy_to_user(arg, &zol->curfreq, sizeof(zol->curfreq)))
			return -EFAULT;
		return 0;
	case VIDIOCSFREQ:
		if (copy_from_user(&zol->curfreq, arg, sizeof(zol->curfreq)))
			return -EFAULT;
		zol_setfreq(zol, zol->curfreq);
		return 0;
	case VIDIOCGAUDIO:
		{
			struct video_audio v;
			memset(&v, 0, sizeof(v));
			v.flags |= VIDEO_AUDIO_MUTABLE | VIDEO_AUDIO_VOLUME;
			v.mode |= zol_is_stereo(zol)
				? VIDEO_SOUND_STEREO : VIDEO_SOUND_MONO;
			v.volume = zol->curvol * 4096;
			v.step = 4096;
			strcpy(v.name, "Zoltrix Radio");
			if (copy_to_user(arg, &v, sizeof(v)))
				return -EFAULT;
			return 0;
		}
	case VIDIOCSAUDIO:
		{
			struct video_audio v;
			if (copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if (v.audio)
				return -EINVAL;

			if (v.flags & VIDEO_AUDIO_MUTE)
				zol_mute(zol);
			else
			{
				zol_unmute(zol);
				zol_setvol(zol, v.volume / 4096);
			}

			if (v.mode & VIDEO_SOUND_STEREO)
			{
				zol->stereo = 1;
				zol_setfreq(zol, zol->curfreq);
			}
			if (v.mode & VIDEO_SOUND_MONO)
			{
				zol->stereo = 0;
				zol_setfreq(zol, zol->curfreq);
			}

			return 0;
		}
	default:
		return -ENOIOCTLCMD;
	}
}

static int zol_open(struct video_device *dev, int flags)
{
	if (users)
		return -EBUSY;
	users++;
	return 0;
}

static void zol_close(struct video_device *dev)
{
	users--;
}

static struct zol_device zoltrix_unit;

static struct video_device zoltrix_radio =
{
	owner:		THIS_MODULE,
	name:		"Zoltrix Radio Plus",
	type:		VID_TYPE_TUNER,
	hardware:	VID_HARDWARE_ZOLTRIX,
	open:		zol_open,
	close:		zol_close,
	ioctl:		zol_ioctl,
};

static int __init zoltrix_init(void)
{
	if (io == -1) {
		printk(KERN_ERR "You must set an I/O address with io=0x???\n");
		return -EINVAL;
	}
	if ((io != 0x20c) && (io != 0x30c)) {
		printk(KERN_ERR "zoltrix: invalid port, try 0x20c or 0x30c\n");
		return -ENXIO;
	}

	zoltrix_radio.priv = &zoltrix_unit;
	if (!request_region(io, 2, "zoltrix")) {
		printk(KERN_ERR "zoltrix: port 0x%x already in use\n", io);
		return -EBUSY;
	}

	if (video_register_device(&zoltrix_radio, VFL_TYPE_RADIO, radio_nr) == -1)
	{
		release_region(io, 2);
		return -EINVAL;
	}
	printk(KERN_INFO "Zoltrix Radio Plus card driver.\n");

	init_MUTEX(&zoltrix_unit.lock);
	
	/* mute card - prevents noisy bootups */

	/* this ensures that the volume is all the way down  */

	outb(0, io);
	outb(0, io);
	sleep_delay();
	sleep_delay();
	inb(io + 3);

	zoltrix_unit.curvol = 0;
	zoltrix_unit.stereo = 1;

	return 0;
}

MODULE_AUTHOR("C.van Schaik");
MODULE_DESCRIPTION("A driver for the Zoltrix Radio Plus.");
MODULE_LICENSE("GPL");

MODULE_PARM(io, "i");
MODULE_PARM_DESC(io, "I/O address of the Zoltrix Radio Plus (0x20c or 0x30c)");
MODULE_PARM(radio_nr, "i");

EXPORT_NO_SYMBOLS;

static void __exit zoltrix_cleanup_module(void)
{
	video_unregister_device(&zoltrix_radio);
	release_region(io, 2);
}

module_init(zoltrix_init);
module_exit(zoltrix_cleanup_module);


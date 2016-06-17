/* SF16FMR2 radio driver for Linux radio support
 * heavily based on fmi driver...
 * (c) 2000-2002 Ziglio Frediano, freddy77@angelfire.com
 *
 * Notes on the hardware
 *
 *  Frequency control is done digitally -- ie out(port,encodefreq(95.8));
 *  No volume control - only mute/unmute - you have to use line volume
 *  
 *  For read stereo/mono you must wait 0.1 sec after set frequency and 
 *  card unmuted so I set frequency on unmute
 *  Signal handling seem to work only on autoscanning (not implemented)
 */

#include <linux/module.h>	/* Modules 			*/
#include <linux/init.h>		/* Initdata			*/
#include <linux/ioport.h>	/* check_region, request_region	*/
#include <linux/delay.h>	/* udelay			*/
#include <asm/io.h>		/* outb, outb_p			*/
#include <asm/uaccess.h>	/* copy to/from user		*/
#include <linux/videodev.h>	/* kernel radio structs		*/
#include <asm/semaphore.h>

static struct semaphore lock;

#undef DEBUG
//#define DEBUG 1

#ifdef DEBUG
# define  debug_print(s) printk s
#else
# define  debug_print(s)
#endif

/* this should be static vars for module size */
struct fmr2_device
{
	int port;
	int curvol; /* 0-65535, if not volume 0 or 65535 */
	int mute;
	int stereo; /* card is producing stereo audio */
	unsigned long curfreq; /* freq in kHz */
	int card_type; 
	__u32 flags;
};

static int io = 0x384; 
static int radio_nr = -1;
static int users = 0;

/* hw precision is 12.5 kHz 
 * It is only usefull to give freq in intervall of 200 (=0.0125Mhz),
 * other bits will be truncated
 */
#define RSF16_ENCODE(x)	((x)/200+856)
#define RSF16_MINFREQ 87*16000
#define RSF16_MAXFREQ 108*16000

/* from radio-aimslab */
static void sleep_delay(unsigned long n)
{
	unsigned d=n/(1000000U/HZ);
	if (!d)
		udelay(n);
	else
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(d);
	}
}

static inline void wait(int n,int port)
{
	for (;n;--n) inb(port);
}

static void outbits(int bits, unsigned int data, int nWait, int port)
{
	int bit;
	for(;--bits>=0;) {
		bit = (data>>bits) & 1;
		outb(bit,port);
		wait(nWait,port);
		outb(bit|2,port);
		wait(nWait,port);
		outb(bit,port);
		wait(nWait,port);
	}
}

static inline void fmr2_mute(int port)
{
	outb(0x00, port);
	wait(4,port);
}

static inline void fmr2_unmute(int port)
{
	outb(0x04, port);
	wait(4,port);
}

static inline int fmr2_stereo_mode(int port)
{
	int n = inb(port);
	outb(6,port);
	inb(port);
	n = ((n>>3)&1)^1;
	debug_print((KERN_DEBUG "stereo: %d\n", n));
	return n;
}

static int fmr2_product_info(struct fmr2_device *dev)
{
	int n = inb(dev->port);
	n &= 0xC1;
	if (n == 0)
	{
		/* this should support volume set */
		dev->card_type = 12;
		return 0;
	}
	/* not volume (mine is 11) */
	dev->card_type = (n==128)?11:0;
	return n;
}

static inline int fmr2_getsigstr(struct fmr2_device *dev)
{
	/* !!! work only if scanning freq */
	int port = dev->port, res = 0xffff;
	outb(5,port);
	wait(4,port);
	if (!(inb(port)&1)) res = 0;
	debug_print((KERN_DEBUG "signal: %d\n", res));
	return res;
}

/* set frequency and unmute card */
static int fmr2_setfreq(struct fmr2_device *dev)
{
	int port = dev->port;
	unsigned long freq = dev->curfreq;

	fmr2_mute(port);

	/* 0x42 for mono output
	 * 0x102 forward scanning
	 * 0x182 scansione avanti 
	 */
	outbits(9,0x2,3,port);
	outbits(16,RSF16_ENCODE(freq),2,port);

	fmr2_unmute(port);

	/* wait 0.11 sec */
	sleep_delay(110000LU);

	/* NOTE if mute this stop radio
	   you must set freq on unmute */
	dev->stereo = fmr2_stereo_mode(port);
	return 0;
}

/* !!! not tested, in my card this does't work !!! */
static int fmr2_setvolume(struct fmr2_device *dev)
{
	int i,a,n, port = dev->port;

	if (dev->card_type != 11) return 1;

	switch( (dev->curvol+(1<<11)) >> 12 )
	{
	case 0: case 1: n = 0x21; break;
	case 2: n = 0x84; break;
	case 3: n = 0x90; break;
	case 4: n = 0x104; break;
	case 5: n = 0x110; break;
	case 6: n = 0x204; break;
	case 7: n = 0x210; break;
	case 8: n = 0x402; break;
	case 9: n = 0x404; break;
	default:
	case 10: n = 0x408; break;
	case 11: n = 0x410; break;
	case 12: n = 0x801; break;
	case 13: n = 0x802; break;
	case 14: n = 0x804; break;
	case 15: n = 0x808; break;
	case 16: n = 0x810; break;
	}
	for(i=12;--i>=0;)
	{
		a = ((n >> i) & 1) << 6; /* if (a=0) a= 0; else a= 0x40; */
		outb(a|4, port);
		wait(4,port);
		outb(a|0x24, port);
		wait(4,port);
		outb(a|4, port);
		wait(4,port);
	}
	for(i=6;--i>=0;)
	{
		a = ((0x18 >> i) & 1) << 6;
		outb(a|4, port);
		wait(4,port);
		outb(a|0x24, port);
		wait(4,port);
		outb(a|4, port);
		wait(4,port);
	}
	wait(4,port);
	outb(0x14, port);

	return 0;
}

static int fmr2_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct fmr2_device *fmr2=dev->priv;
	debug_print((KERN_DEBUG "freq %ld flags %d vol %d mute %d "
		"stereo %d type %d\n",
		fmr2->curfreq, fmr2->flags, fmr2->curvol, fmr2->mute, 
		fmr2->stereo, fmr2->card_type));
	
	switch(cmd)
	{
		case VIDIOCGCAP:
		{
			struct video_capability v;
			strcpy(v.name, "SF16-FMR2 radio");
			v.type=VID_TYPE_TUNER;
			v.channels=1;
			v.audios=1;
			/* No we don't do pictures */
			v.maxwidth=0;
			v.maxheight=0;
			v.minwidth=0;
			v.minheight=0;
			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCGTUNER:
		{
			struct video_tuner v;
			int mult;

			if(copy_from_user(&v, arg,sizeof(v))!=0)
				return -EFAULT;
			if(v.tuner)	/* Only 1 tuner */
				return -EINVAL;
			strcpy(v.name, "FM");
			mult = (fmr2->flags & VIDEO_TUNER_LOW) ? 1 : 1000;
			v.rangelow = RSF16_MINFREQ/mult;
			v.rangehigh = RSF16_MAXFREQ/mult;
			v.flags = fmr2->flags | VIDEO_AUDIO_MUTABLE;
			if (fmr2->mute)
				v.flags |= VIDEO_AUDIO_MUTE;
			v.mode=VIDEO_MODE_AUTO;
			down(&lock);
			v.signal = fmr2_getsigstr(fmr2);
			up(&lock);
			if(copy_to_user(arg,&v, sizeof(v)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSTUNER:
		{
			struct video_tuner v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if(v.tuner!=0)
				return -EINVAL;
			fmr2->flags = v.flags & VIDEO_TUNER_LOW;
			return 0;
		}
		case VIDIOCGFREQ:
		{
			unsigned long tmp = fmr2->curfreq;
			if (!(fmr2->flags & VIDEO_TUNER_LOW))
				tmp /= 1000;
			if(copy_to_user(arg, &tmp, sizeof(tmp)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSFREQ:
		{
			unsigned long tmp;
			if(copy_from_user(&tmp, arg, sizeof(tmp)))
				return -EFAULT;
			if (!(fmr2->flags & VIDEO_TUNER_LOW))
				tmp *= 1000;
			if ( tmp<RSF16_MINFREQ || tmp>RSF16_MAXFREQ )
				return -EINVAL;
			/* rounding in steps of 200 to match th freq
			 * that will be used 
			 */
			fmr2->curfreq = (tmp/200)*200; 

			/* set card freq (if not muted) */
			if (fmr2->curvol && !fmr2->mute)
			{
				down(&lock);
				fmr2_setfreq(fmr2);
				up(&lock);
			}
			return 0;
		}
		case VIDIOCGAUDIO:
		{
			struct video_audio v;
			v.audio=0;
			v.volume=0;
			v.bass=0;
			v.treble=0;
			/* !!! do not return VIDEO_AUDIO_MUTE */
			v.flags = VIDEO_AUDIO_MUTABLE;
			strcpy(v.name, "Radio");
			/* get current stereo mode */
			v.mode = fmr2->stereo ? VIDEO_SOUND_STEREO: VIDEO_SOUND_MONO;
			v.balance = 0;
			v.step=0; /* No volume, just (un)mute */
			/* volume supported ? */
			if (fmr2->card_type == 11)
			{
				v.flags |= VIDEO_AUDIO_VOLUME;
				v.step = 1 << 12;
				v.volume = fmr2->curvol;
			}
			debug_print((KERN_DEBUG "Get flags %d vol %d\n", v.flags, v.volume));
			if(copy_to_user(arg,&v, sizeof(v)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSAUDIO:
		{
			struct video_audio v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if(v.audio)
				return -EINVAL;
			debug_print((KERN_DEBUG "Set flags %d vol %d\n", v.flags, v.volume));
			/* set volume */
			if (v.flags & VIDEO_AUDIO_VOLUME)
				fmr2->curvol = v.volume; /* !!! set with precision */
			if (fmr2->card_type != 11) fmr2->curvol = 65535;
			fmr2->mute = 0;
			if (v.flags&VIDEO_AUDIO_MUTE)
				fmr2->mute = 1;
#ifdef DEBUG
			if (fmr2->curvol && !fmr2->mute)
				printk(KERN_DEBUG "unmute\n");
			else
				printk(KERN_DEBUG "mute\n");
#endif
			down(&lock);
			if (fmr2->curvol && !fmr2->mute)
			{
				fmr2_setvolume(fmr2);
				fmr2_setfreq(fmr2);
			}
			else fmr2_mute(fmr2->port);
			up(&lock);
			return 0;
		}
		case VIDIOCGUNIT:
		{
			struct video_unit v;
			v.video=VIDEO_NO_UNIT;
			v.vbi=VIDEO_NO_UNIT;
			v.radio=dev->minor;
			v.audio=0; /* How do we find out this??? */
			v.teletext=VIDEO_NO_UNIT;
			if(copy_to_user(arg, &v, sizeof(v)))
				return -EFAULT;
			return 0;			
		}
		default:
			return -ENOIOCTLCMD;
	}
}

static int fmr2_open(struct video_device *dev, int flags)
{
	if(users)
		return -EBUSY;
	users++;
	return 0;
}

static void fmr2_close(struct video_device *dev)
{
	users--;
}

static struct fmr2_device fmr2_unit;

static struct video_device fmr2_radio=
{
	owner:		THIS_MODULE,
	name:		"SF16FMR2 radio",
	type:		VID_TYPE_TUNER,
	hardware:	VID_HARDWARE_SF16FMR2,
	open:		fmr2_open,
	close:		fmr2_close,
	ioctl:		fmr2_ioctl,
};

static int __init fmr2_init(void)
{
	if (check_region(io, 2)) 
	{
		printk(KERN_ERR "fmr2: port 0x%x already in use\n", io);
		return -EBUSY;
	}

	fmr2_unit.port = io;
	fmr2_unit.curvol = 0;
	fmr2_unit.mute = 0;
	fmr2_unit.curfreq = 0;
	fmr2_unit.stereo = 1;
	fmr2_unit.flags = VIDEO_TUNER_LOW;
	fmr2_unit.card_type = 0;
	fmr2_radio.priv = &fmr2_unit;

	init_MUTEX(&lock);

	if(video_register_device(&fmr2_radio, VFL_TYPE_RADIO, radio_nr)==-1)
		return -EINVAL;

	request_region(io, 2, "fmr2");
	printk(KERN_INFO "SF16FMR2 radio card driver at 0x%x.\n", io);
	debug_print((KERN_DEBUG "Mute %d Low %d\n",VIDEO_AUDIO_MUTE,VIDEO_TUNER_LOW));
	/* mute card - prevents noisy bootups */
	down(&lock);
	fmr2_mute(io);
	fmr2_product_info(&fmr2_unit);
	up(&lock);
	debug_print((KERN_DEBUG "card_type %d\n", fmr2_unit.card_type));
	return 0;
}

MODULE_AUTHOR("Ziglio Frediano, freddy77@angelfire.com");
MODULE_DESCRIPTION("A driver for the SF16FMR2 radio.");
MODULE_LICENSE("GPL");

MODULE_PARM(io, "i");
MODULE_PARM_DESC(io, "I/O address of the SF16FMR2 card (should be 0x384, if do not work try 0x284)");
MODULE_PARM(radio_nr, "i");

EXPORT_NO_SYMBOLS;

static void __exit fmr2_cleanup_module(void)
{
	video_unregister_device(&fmr2_radio);
	release_region(io,2);
}

module_init(fmr2_init);
module_exit(fmr2_cleanup_module);

#ifndef MODULE

static int __init fmr2_setup_io(char *str)
{
	get_option(&str, &io);
	return 1;
}

__setup("sf16fmr2=", fmr2_setup_io);

#endif

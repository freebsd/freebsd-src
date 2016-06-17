/* Maestro PCI sound card radio driver for Linux support
 * (c) 2000 A. Tlalka, atlka@pg.gda.pl
 * Notes on the hardware
 *
 *  + Frequency control is done digitally 
 *  + No volume control - only mute/unmute - you have to use Aux line volume
 *  control on Maestro card to set the volume
 *  + Radio status (tuned/not_tuned and stereo/mono) is valid some time after
 *  frequency setting (>100ms) and only when the radio is unmuted.
 *  version 0.02
 *  + io port is automatically detected - only the first radio is used
 *  version 0.03
 *  + thread access locking additions
 *  version 0.04
 * + code improvements
 * + VIDEO_TUNER_LOW is permanent
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <linux/pci.h>
#include <linux/videodev.h>

#define DRIVER_VERSION	"0.04"

#define PCI_VENDOR_ESS                  0x125D
#define PCI_DEVICE_ID_ESS_ESS1968       0x1968          /* Maestro 2    */
#define PCI_DEVICE_ID_ESS_ESS1978       0x1978          /* Maestro 2E   */

#define GPIO_DATA       0x60   /* port offset from ESS_IO_BASE */

#define IO_MASK		4      /* mask      register offset from GPIO_DATA
				bits 1=unmask write to given bit */
#define IO_DIR		8      /* direction register offset from GPIO_DATA
				bits 0/1=read/write direction */

#define GPIO6           0x0040 /* mask bits for GPIO lines */
#define GPIO7           0x0080
#define GPIO8           0x0100
#define GPIO9           0x0200

#define STR_DATA        GPIO6  /* radio TEA5757 pins and GPIO bits */
#define STR_CLK         GPIO7
#define STR_WREN        GPIO8
#define STR_MOST        GPIO9

#define FREQ_LO		 50*16000
#define FREQ_HI		150*16000

#define FREQ_IF         171200 /* 10.7*16000   */
#define FREQ_STEP       200    /* 12.5*16      */

#define FREQ2BITS(x)	((((unsigned int)(x)+FREQ_IF+(FREQ_STEP<<1))\
			/(FREQ_STEP<<2))<<2) /* (x==fmhz*16*1000) -> bits */

#define BITS2FREQ(x)	((x) * FREQ_STEP - FREQ_IF)

static int radio_nr = -1;
MODULE_PARM(radio_nr, "i");


static int radio_open(struct video_device *, int);
static int radio_ioctl(struct video_device *, unsigned int, void *);
static void radio_close(struct video_device *);

static struct video_device maestro_radio=
{
	owner:		THIS_MODULE,
	name:		"Maestro radio",
	type:		VID_TYPE_TUNER,
	hardware:	VID_HARDWARE_SF16MI,
	open:		radio_open,
	close:		radio_close,
	ioctl:		radio_ioctl,
};

static struct radio_device
{
	__u16	io,	/* base of Maestro card radio io (GPIO_DATA)*/
		muted,	/* VIDEO_AUDIO_MUTE */
		stereo,	/* VIDEO_TUNER_STEREO_ON */	
		tuned;	/* signal strength (0 or 0xffff) */
	struct  semaphore lock;
} radio_unit = {0, 0, 0, 0, };

static int users = 0;

static void sleep_125ms(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(HZ >> 3);
}

static void udelay2(void)
{
	udelay(2);
}

static void udelay4(void)
{
	udelay(4);
}

static void udelay16(void)
{
	udelay(16);
}

static __u32 radio_bits_get(struct radio_device *dev)
{
	register __u16 io=dev->io, l, rdata;
	register __u32 data=0;
	__u16 omask;
	omask = inw(io + IO_MASK);
	outw(~(STR_CLK | STR_WREN), io + IO_MASK);
	outw(0, io);
	udelay16();
	for (l=24;l--;) {
		outw(STR_CLK, io);		/* HI state */
		udelay2();
		if(!l) 
			dev->tuned = inw(io) & STR_MOST ? 0 : 0xffff;
		outw(0, io);			/* LO state */
		udelay2();
		data <<= 1;			/* shift data */
		rdata = inw(io);
		if(!l)
			dev->stereo =  rdata & STR_MOST ? 
			0 : VIDEO_TUNER_STEREO_ON;
		else
			if(rdata & STR_DATA)
				data++;
		udelay2();
	}
	if(dev->muted)
		outw(STR_WREN, io);
	udelay4();
	outw(omask, io + IO_MASK);
	return data & 0x3ffe;
}

static void radio_bits_set(struct radio_device *dev, __u32 data)
{
	register __u16 io=dev->io, l, bits;
	__u16 omask, odir;
	omask = inw(io + IO_MASK);
	odir  = (inw(io + IO_DIR) & ~STR_DATA) | (STR_CLK | STR_WREN);
	outw(odir | STR_DATA, io + IO_DIR);
	outw(~(STR_DATA | STR_CLK | STR_WREN), io + IO_MASK);
	udelay16();
	for (l=25;l;l--) {
		bits = ((data >> 18) & STR_DATA) | STR_WREN ;
		data <<= 1;			/* shift data */
		outw(bits, io);			/* start strobe */
		udelay2();
		outw(bits | STR_CLK, io);	/* HI level */
		udelay2();   
		outw(bits, io);			/* LO level */
		udelay4();
	}
	if(!dev->muted)
		outw(0, io);
	udelay4();
	outw(omask, io + IO_MASK);
	outw(odir, io + IO_DIR);
	sleep_125ms();
}

inline static int radio_function(struct video_device *dev, 
				 unsigned int cmd, void *arg)
{
	struct radio_device *card=dev->priv;
	switch(cmd) {
		case VIDIOCGCAP: {
			struct video_capability v;
			strcpy(v.name, "Maestro radio");
			v.type=VID_TYPE_TUNER;
			v.channels=v.audios=1;
			v.maxwidth=v.maxheight=v.minwidth=v.minheight=0;
			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCGTUNER: {
			struct video_tuner v;
			if(copy_from_user(&v, arg,sizeof(v))!=0)
				return -EFAULT;
			if(v.tuner)
				return -EINVAL;
			(void)radio_bits_get(card);
			v.flags = VIDEO_TUNER_LOW | card->stereo;
			v.signal = card->tuned;
			strcpy(v.name, "FM");
			v.rangelow = FREQ_LO;
			v.rangehigh = FREQ_HI;
			v.mode = VIDEO_MODE_AUTO;
			if(copy_to_user(arg,&v, sizeof(v)))
				return -EFAULT;
		        return 0;
		}
		case VIDIOCSTUNER: {
			struct video_tuner v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if(v.tuner!=0)
				return -EINVAL;
			return 0;
		}
		case VIDIOCGFREQ: {
			unsigned long tmp=BITS2FREQ(radio_bits_get(card));
			if(copy_to_user(arg, &tmp, sizeof(tmp)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSFREQ: {
			unsigned long tmp;
			if(copy_from_user(&tmp, arg, sizeof(tmp)))
				return -EFAULT;
			if ( tmp<FREQ_LO || tmp>FREQ_HI )
				return -EINVAL;
			radio_bits_set(card, FREQ2BITS(tmp));
			return 0;
		}
		case VIDIOCGAUDIO: {	
			struct video_audio v;
			strcpy(v.name, "Radio");
			v.audio=v.volume=v.bass=v.treble=v.balance=v.step=0;
			v.flags=VIDEO_AUDIO_MUTABLE | card->muted;
			v.mode=VIDEO_SOUND_STEREO;
			if(copy_to_user(arg,&v, sizeof(v)))
				return -EFAULT;
			return 0;		
		}
		case VIDIOCSAUDIO: {
			struct video_audio v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;
			if(v.audio)
				return -EINVAL;
			{
				register __u16 io=card->io;
				register __u16 omask = inw(io + IO_MASK);
				outw(~STR_WREN, io + IO_MASK);
				outw((card->muted = v.flags & VIDEO_AUDIO_MUTE)
				     ? STR_WREN : 0, io);
				udelay4();
				outw(omask, io + IO_MASK);
				sleep_125ms();
				return 0;
			}
		}
		case VIDIOCGUNIT: {
			struct video_unit v;
			v.video=VIDEO_NO_UNIT;
			v.vbi=VIDEO_NO_UNIT;
			v.radio=dev->minor;
			v.audio=0;
			v.teletext=VIDEO_NO_UNIT;
			if(copy_to_user(arg, &v, sizeof(v)))
				return -EFAULT;
			return 0;		
		}
		default: return -ENOIOCTLCMD;
	}
}

static int radio_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct radio_device *card=dev->priv;
	int ret;
	down(&card->lock);
	ret = radio_function(dev, cmd, arg);
	up(&card->lock);
	return ret;
}

static int radio_open(struct video_device *dev, int flags)
{
	if(users)
		return -EBUSY;
	users++;
	return 0;
}

static void radio_close(struct video_device *dev)
{
	users--;
}


inline static __u16 radio_install(struct pci_dev *pcidev);

MODULE_AUTHOR("Adam Tlalka, atlka@pg.gda.pl");
MODULE_DESCRIPTION("Radio driver for the Maestro PCI sound card radio.");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

void __exit maestro_radio_exit(void)
{
	video_unregister_device(&maestro_radio);
}

int __init maestro_radio_init(void)
{
	register __u16 found=0;
	struct pci_dev *pcidev = NULL;
	if(!pci_present())
		return -ENODEV;
	while(!found && (pcidev = pci_find_device(PCI_VENDOR_ESS, 
						  PCI_DEVICE_ID_ESS_ESS1968,
						  pcidev)))
		found |= radio_install(pcidev);
	while(!found && (pcidev = pci_find_device(PCI_VENDOR_ESS,
						  PCI_DEVICE_ID_ESS_ESS1978, 
						  pcidev)))
		found |= radio_install(pcidev);
	if(!found) {
		printk(KERN_INFO "radio-maestro: no devices found.\n");
		return -ENODEV;
	}
	return 0;
}

module_init(maestro_radio_init);
module_exit(maestro_radio_exit);

inline static __u16 radio_power_on(struct radio_device *dev)
{
	register __u16 io=dev->io;
	register __u32 ofreq;
	__u16 omask, odir;
	omask = inw(io + IO_MASK);
	odir  = (inw(io + IO_DIR) & ~STR_DATA) | (STR_CLK | STR_WREN);
	outw(odir & ~STR_WREN, io + IO_DIR);
	dev->muted = inw(io) & STR_WREN ? 0 : VIDEO_AUDIO_MUTE;
	outw(odir, io + IO_DIR);
	outw(~(STR_WREN | STR_CLK), io + IO_MASK);
	outw(dev->muted ? 0 : STR_WREN, io);
	udelay16();
	outw(omask, io + IO_MASK);
	ofreq = radio_bits_get(dev);
	if((ofreq<FREQ2BITS(FREQ_LO)) || (ofreq>FREQ2BITS(FREQ_HI)))
		ofreq = FREQ2BITS(FREQ_LO);
	radio_bits_set(dev, ofreq);
	return (ofreq == radio_bits_get(dev));
}

inline static __u16 radio_install(struct pci_dev *pcidev)
{
	if(((pcidev->class >> 8) & 0xffff) != PCI_CLASS_MULTIMEDIA_AUDIO)
		return 0;
	
	radio_unit.io = pcidev->resource[0].start + GPIO_DATA;
	maestro_radio.priv = &radio_unit;
	init_MUTEX(&radio_unit.lock);
	
	if(radio_power_on(&radio_unit)) {
		if(video_register_device(&maestro_radio, VFL_TYPE_RADIO, radio_nr)==-1) {
			printk("radio-maestro: can't register device!");
			return 0;
		}
		printk(KERN_INFO "radio-maestro: version "
		       DRIVER_VERSION 
		       " time " 
		       __TIME__ "  "
		       __DATE__
		       "\n");
		printk(KERN_INFO "radio-maestro: radio chip initialized\n");
		return 1;
	} else
		return 0;   
}


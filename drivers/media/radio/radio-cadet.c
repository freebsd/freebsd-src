/* radio-cadet.c - A video4linux driver for the ADS Cadet AM/FM Radio Card 
 *
 * by Fred Gleason <fredg@wava.com>
 * Version 0.3.3
 *
 * (Loosely) based on code for the Aztech radio card by
 *
 * Russell Kroll    (rkroll@exploits.org)
 * Quay Ly
 * Donald Song
 * Jason Lewis      (jlewis@twilight.vtc.vsc.edu) 
 * Scott McGrath    (smcgrath@twilight.vtc.vsc.edu)
 * William McGrath  (wmcgrath@twilight.vtc.vsc.edu)
 *
 * History:
 * 2000-04-29	Russell Kroll <rkroll@exploits.org>
 *		Added ISAPnP detection for Linux 2.3/2.4
 *
 * 2001-01-10	Russell Kroll <rkroll@exploits.org>
 *		Removed dead CONFIG_RADIO_CADET_PORT code
 *		PnP detection on load is now default (no args necessary)
 *
 * 2003-01-31	Alan Cox <alan@redhat.com>
 *		Cleaned up locking, delay code, general odds and ends
 */

#include <linux/module.h>	/* Modules 			*/
#include <linux/init.h>		/* Initdata			*/
#include <linux/ioport.h>	/* check_region, request_region	*/
#include <linux/delay.h>	/* udelay			*/
#include <asm/io.h>		/* outb, outb_p			*/
#include <asm/uaccess.h>	/* copy to/from user		*/
#include <linux/videodev.h>	/* kernel radio structs		*/
#include <linux/param.h>
#include <linux/isapnp.h>

#define RDS_BUFFER 256

static int io=-1;		/* default to isapnp activation */
static int radio_nr = -1;
static int users=0;
static int curtuner=0;
static int tunestat=0;
static int sigstrength=0;
static wait_queue_head_t readq;
struct timer_list tunertimer,rdstimer,readtimer;
static __u8 rdsin=0,rdsout=0,rdsstat=0;
static unsigned char rdsbuf[RDS_BUFFER];
static spinlock_t cadet_io_lock;

static int cadet_probe(void);
static struct pci_dev *dev;
static int isapnp_cadet_probe(void);

/*
 * Signal Strength Threshold Values
 * The V4L API spec does not define any particular unit for the signal 
 * strength value.  These values are in microvolts of RF at the tuner's input.
 */
static __u16 sigtable[2][4]={{5,10,30,150},{28,40,63,1000}};

static int cadet_getrds(void)
{
        int rdsstat=0;

	spin_lock(&cadet_io_lock);
        outb(3,io);                 /* Select Decoder Control/Status */
	outb(inb(io+1)&0x7f,io+1);  /* Reset RDS detection */
	spin_unlock(&cadet_io_lock);
	
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ/10);

	spin_lock(&cadet_io_lock);	
        outb(3,io);                 /* Select Decoder Control/Status */
	if((inb(io+1)&0x80)!=0) {
	        rdsstat|=VIDEO_TUNER_RDS_ON;
	}
	if((inb(io+1)&0x10)!=0) {
	        rdsstat|=VIDEO_TUNER_MBS_ON;
	}
	spin_unlock(&cadet_io_lock);
	return rdsstat;
	
}

static int cadet_getstereo(void)
{
	int ret = 0;
        if(curtuner != 0)           /* Only FM has stereo capability! */
	        return 0;
	spin_lock(&cadet_io_lock);
        outb(7,io);          /* Select tuner control */
	if( (inb(io+1) & 0x40) == 0)
        	ret = 1;
        spin_unlock(&cadet_io_lock);
        return ret;
}

static unsigned cadet_gettune(void)
{
        int curvol,i;
	unsigned fifo=0;

        /*
         * Prepare for read
         */

	spin_lock(&cadet_io_lock);
	
        outb(7,io);       /* Select tuner control */
        curvol=inb(io+1); /* Save current volume/mute setting */
        outb(0x00,io+1);  /* Ensure WRITE-ENABLE is LOW */
	tunestat=0xffff;

        /*
         * Read the shift register
         */
        for(i=0;i<25;i++) {
                fifo=(fifo<<1)|((inb(io+1)>>7)&0x01);
                if(i<24) {
                        outb(0x01,io+1);
			tunestat&=inb(io+1);
                        outb(0x00,io+1);
                }
        }

        /*
         * Restore volume/mute setting
         */
        outb(curvol,io+1);

	spin_unlock(&cadet_io_lock);
	
	return fifo;
}



static unsigned cadet_getfreq(void)
{
        int i;
        unsigned freq=0,test,fifo=0;

	/*
	 * Read current tuning
	 */
	fifo=cadet_gettune();

        /*
         * Convert to actual frequency
         */
	if(curtuner==0) {    /* FM */
	        test=12500;
                for(i=0;i<14;i++) {
                        if((fifo&0x01)!=0) {
                                freq+=test;
                        }
                        test=test<<1;
                        fifo=fifo>>1;
                }
                freq-=10700000;           /* IF frequency is 10.7 MHz */
                freq=(freq*16)/1000000;   /* Make it 1/16 MHz */
	}
	if(curtuner==1) {    /* AM */
	        freq=((fifo&0x7fff)-2010)*16;
	}

        return freq;
}



static void cadet_settune(unsigned fifo)
{
        int i;
	unsigned test;  

	spin_lock(&cadet_io_lock);
	
	outb(7,io);                /* Select tuner control */
	/*
	 * Write the shift register
	 */
	test=0;
	test=(fifo>>23)&0x02;      /* Align data for SDO */
	test|=0x1c;                /* SDM=1, SWE=1, SEN=1, SCK=0 */
	outb(7,io);                /* Select tuner control */
	outb(test,io+1);           /* Initialize for write */
	for(i=0;i<25;i++) {
   	        test|=0x01;              /* Toggle SCK High */
		outb(test,io+1);
		test&=0xfe;              /* Toggle SCK Low */
		outb(test,io+1);
		fifo=fifo<<1;            /* Prepare the next bit */
		test=0x1c|((fifo>>23)&0x02);
		outb(test,io+1);
	}
	spin_unlock(&cadet_io_lock);
}



static void cadet_setfreq(unsigned freq)
{
        unsigned fifo;
        int i,j,test;
        int curvol;

        /* 
         * Formulate a fifo command
         */
	fifo=0;
	if(curtuner==0) {    /* FM */
        	test=102400;
                freq=(freq*1000)/16;       /* Make it kHz */
                freq+=10700;               /* IF is 10700 kHz */
                for(i=0;i<14;i++) {
                        fifo=fifo<<1;
                        if(freq>=test) {
                                fifo|=0x01;
                                freq-=test;
                        }
                        test=test>>1;
                }
	}
	if(curtuner==1) {    /* AM */
                fifo=(freq/16)+2010;            /* Make it kHz */
		fifo|=0x100000;            /* Select AM Band */
	}

        /*
         * Save current volume/mute setting
         */

	spin_lock(&cadet_io_lock);
	outb(7,io);                /* Select tuner control */
        curvol=inb(io+1); 
        spin_unlock(&cadet_io_lock);

	/*
	 * Tune the card
	 */
	for(j=3;j>-1;j--) {
	        cadet_settune(fifo|(j<<16));
	        
	        spin_lock(&cadet_io_lock);
		outb(7,io);         /* Select tuner control */
		outb(curvol,io+1);
		spin_unlock(&cadet_io_lock);
		
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/10);

		cadet_gettune();
		if((tunestat & 0x40) == 0) {   /* Tuned */
		        sigstrength=sigtable[curtuner][j];
			return;
		}
	}
	sigstrength=0;
}


static int cadet_getvol(void)
{
	int ret = 0;
	
	spin_lock(&cadet_io_lock);
	
        outb(7,io);                /* Select tuner control */
        if((inb(io + 1) & 0x20) != 0)
        	ret = 0xffff;
        
        spin_unlock(&cadet_io_lock);
        return ret;
}


static void cadet_setvol(int vol)
{
	spin_lock(&cadet_io_lock);
        outb(7,io);                /* Select tuner control */
        if(vol>0)
                outb(0x20,io+1);
        else
                outb(0x00,io+1);

	spin_unlock(&cadet_io_lock);
}  


static void cadet_handler(unsigned long data)
{
	/*
	 * Service the RDS fifo
	 */

	if(spin_trylock(&cadet_io_lock))
	{
	        outb(0x3,io);       /* Select RDS Decoder Control */
		if((inb(io+1)&0x20)!=0) {
		        printk(KERN_CRIT "cadet: RDS fifo overflow\n");
		}
		outb(0x80,io);      /* Select RDS fifo */
		while((inb(io)&0x80)!=0) {
		        rdsbuf[rdsin]=inb(io+1);
			if(rdsin==rdsout) {
			        printk(KERN_WARNING "cadet: RDS buffer overflow\n");
			}
			else rdsin++;
		}
		spin_unlock(&cadet_io_lock);
	}

	/*
	 * Service pending read
	 */
	if( rdsin!=rdsout)
	        wake_up_interruptible(&readq);

	/* 
	 * Clean up and exit
	 */
	init_timer(&readtimer);
	readtimer.function=cadet_handler;
	readtimer.data=(unsigned long)0;
	readtimer.expires=jiffies+(HZ/20);
	add_timer(&readtimer);
}



static long cadet_read(struct video_device *v,char *buf,unsigned long count,
		       int nonblock)
{
        int i=0;
	unsigned char readbuf[RDS_BUFFER];

        if(rdsstat==0) {
		spin_lock(&cadet_io_lock);
	        rdsstat=1;
		outb(0x80,io);        /* Select RDS fifo */
		spin_unlock(&cadet_io_lock);
		init_timer(&readtimer);
		readtimer.function=cadet_handler;
		readtimer.data=(unsigned long)0;
		readtimer.expires=jiffies+(HZ/20);
		add_timer(&readtimer);
	}
	if(rdsin==rdsout) {
  	        if(nonblock)
		        return -EWOULDBLOCK;
	        interruptible_sleep_on(&readq);
	}		
	while( i < count && rdsin != rdsout) {
	        readbuf[i++]=rdsbuf[rdsout++];
	}
	if(copy_to_user(buf,readbuf,i))
	        return -EFAULT;
	return i;
}



static int cadet_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
        unsigned freq;
	switch(cmd)
	{
		case VIDIOCGCAP:
		{
			struct video_capability v;
			memset(&v, 0, sizeof(v));
			v.type=VID_TYPE_TUNER;
			v.channels=2;
			v.audios=1;
			strcpy(v.name, "ADS Cadet");
			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCGTUNER:
		{
			struct video_tuner v;
			if(copy_from_user(&v, arg,sizeof(v))!=0)
				return -EFAULT;
			
			switch(v.tuner) {
		        case 0:
			        strcpy(v.name,"FM");
			        v.rangelow=1400;     /* 87.5 MHz */
			        v.rangehigh=1728;    /* 108.0 MHz */
			        v.flags=0;
			        v.mode|=VIDEO_MODE_AUTO;
			        if(cadet_getstereo()==1)
				        v.flags|=VIDEO_TUNER_STEREO_ON;
			        
				v.flags|=cadet_getrds();
			        break;
		        case 1:
			        strcpy(v.name,"AM");
			        v.rangelow=8320;      /* 520 kHz */
			        v.rangehigh=26400;    /* 1650 kHz */
			        v.flags = VIDEO_TUNER_LOW;
			        v.mode = VIDEO_MODE_AUTO;
			        break;
			default:
				return -EINVAL;
			}
		        v.signal=sigstrength;
		        if(copy_to_user(arg,&v, sizeof(v))) {
			        return -EFAULT;
		        }
			return 0;
		}
		case VIDIOCSTUNER:
		{
			struct video_tuner v;
			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;

			if(v.tuner < 0 || v.tuner > 1)
				return -EINVAL;

			curtuner=v.tuner;	
			return 0;
		}
		case VIDIOCGFREQ:
		        freq=cadet_getfreq();
			if(copy_to_user(arg, &freq, sizeof(freq)))
				return -EFAULT;
			return 0;
		case VIDIOCSFREQ:
			if(copy_from_user(&freq, arg,sizeof(freq)))
				return -EFAULT;
			if(curtuner==0 && (freq<1400 || freq>1728))
			        return -EINVAL;
			if(curtuner==1 && (freq<8320 || freq>26400))
			        return -EINVAL;

			cadet_setfreq(freq);
			return 0;

		case VIDIOCGAUDIO:
		{	
			struct video_audio v;
			memset(&v,0, sizeof(v));
			v.flags=VIDEO_AUDIO_MUTABLE|VIDEO_AUDIO_VOLUME;
			if(cadet_getstereo()==0)
			        v.mode=VIDEO_SOUND_MONO;
			else
				v.mode=VIDEO_SOUND_STEREO;

			v.volume=cadet_getvol();
			v.step=0xffff;
			strcpy(v.name, "Radio");
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
			cadet_setvol(v.volume);
			if(v.flags&VIDEO_AUDIO_MUTE) 
				cadet_setvol(0);
			else
				cadet_setvol(0xffff);
			return 0;
		}
		default:
			return -ENOIOCTLCMD;
	}
}


static int cadet_open(struct video_device *dev, int flags)
{
	if(users)
		return -EBUSY;
	users++;
	init_waitqueue_head(&readq);
	return 0;
}

static void cadet_close(struct video_device *dev)
{
	del_timer_sync(&readtimer);
	rdsstat=0;
	users--;
}


static struct video_device cadet_radio=
{
	owner:		THIS_MODULE,
	name:		"Cadet radio",
	type:		VID_TYPE_TUNER,
	hardware:	VID_HARDWARE_CADET,
	open:		cadet_open,
	close:		cadet_close,
	read:		cadet_read,
	ioctl:		cadet_ioctl,
};

static int isapnp_cadet_probe(void)
{
	dev = isapnp_find_dev (NULL, ISAPNP_VENDOR('M','S','M'),
	                       ISAPNP_FUNCTION(0x0c24), NULL);

	if (!dev)
		return -ENODEV;
	if (dev->prepare(dev)<0)
		return -EAGAIN;
	if (!(dev->resource[0].flags & IORESOURCE_IO))
		return -ENODEV;
	if (dev->activate(dev)<0) {
		printk(KERN_ERR "radio-cadet: isapnp configure failed (out of resources?)\n");
		return -ENOMEM;
	}

	io = dev->resource[0].start;

	printk(KERN_INFO "radio-cadet: ISAPnP reports card at %#x\n", io);

	return io;
}

static int cadet_probe(void)
{
        static int iovals[8]={0x330,0x332,0x334,0x336,0x338,0x33a,0x33c,0x33e};
	int i;

	for(i=0;i<8;i++) {
	        io=iovals[i];
	        if(request_region(io,2, "cadet-probe") != NULL) {
		        cadet_setfreq(1410);
			if(cadet_getfreq()==1410) {
				release_region(io, 2);
			        return io;
			}
			release_region(io, 2);
		}
	}
	return -1;
}

	/* 
	 * io should only be set if the user has used something like
	 * isapnp (the userspace program) to initialize this card for us
	 */

static int __init cadet_init(void)
{

	spin_lock_init(&cadet_io_lock);
	
	/*
	 *	If a probe was requested then probe ISAPnP first (safest)
	 */
	if (io < 0)
		io = isapnp_cadet_probe();
	/*
	 *	If that fails then probe unsafely if probe is requested
	 */
	if(io < 0)
		io = cadet_probe ();

	/*
	 *	Else we bail out
	 */
	 
        if(io < 0) {
#ifdef MODULE        
		printk(KERN_ERR "You must set an I/O address with io=0x???\n");
#endif
	        return -EINVAL;
	}
	if (!request_region(io,2,"cadet"))
		return -EBUSY;
	if(video_register_device(&cadet_radio,VFL_TYPE_RADIO,radio_nr)==-1) {
		release_region(io,2);
		return -EINVAL;
	}
	printk(KERN_INFO "ADS Cadet Radio Card at 0x%x\n",io);
	return 0;
}



MODULE_AUTHOR("Fred Gleason, Russell Kroll, Quay Lu, Donald Song, Jason Lewis, Scott McGrath, William McGrath");
MODULE_DESCRIPTION("A driver for the ADS Cadet AM/FM/RDS radio card.");
MODULE_LICENSE("GPL");

MODULE_PARM(io, "i");
MODULE_PARM_DESC(io, "I/O address of Cadet card (0x330,0x332,0x334,0x336,0x338,0x33a,0x33c,0x33e)");
MODULE_PARM(radio_nr, "i");

static struct isapnp_device_id id_table[] __devinitdata = {
	{ 	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('M','S','M'), ISAPNP_FUNCTION(0x0c24), 0 },
	{0}
};

MODULE_DEVICE_TABLE(isapnp, id_table);

EXPORT_NO_SYMBOLS;

static void __exit cadet_cleanup_module(void)
{
	video_unregister_device(&cadet_radio);
	release_region(io,2);

	if (dev)
		dev->deactivate(dev);
}

module_init(cadet_init);
module_exit(cadet_cleanup_module);


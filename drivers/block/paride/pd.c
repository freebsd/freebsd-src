/* 
        pd.c    (c) 1997-8  Grant R. Guenther <grant@torque.net>
                            Under the terms of the GNU General Public License.

        This is the high-level driver for parallel port IDE hard
        drives based on chips supported by the paride module.

	By default, the driver will autoprobe for a single parallel
	port IDE drive, but if their individual parameters are
        specified, the driver can handle up to 4 drives.

        The behaviour of the pd driver can be altered by setting
        some parameters from the insmod command line.  The following
        parameters are adjustable:
 
	    drive0  	These four arguments can be arrays of	    
	    drive1	1-8 integers as follows:
	    drive2
	    drive3	<prt>,<pro>,<uni>,<mod>,<geo>,<sby>,<dly>,<slv>

			Where,

		<prt>	is the base of the parallel port address for
			the corresponding drive.  (required)

		<pro>   is the protocol number for the adapter that
			supports this drive.  These numbers are
                        logged by 'paride' when the protocol modules
			are initialised.  (0 if not given)

		<uni>   for those adapters that support chained
			devices, this is the unit selector for the
		        chain of devices on the given port.  It should
			be zero for devices that don't support chaining.
			(0 if not given)

		<mod>   this can be -1 to choose the best mode, or one
		        of the mode numbers supported by the adapter.
			(-1 if not given)

		<geo>   this defaults to 0 to indicate that the driver
			should use the CHS geometry provided by the drive
			itself.  If set to 1, the driver will provide
			a logical geometry with 64 heads and 32 sectors
			per track, to be consistent with most SCSI
		        drivers.  (0 if not given)

		<sby>   set this to zero to disable the power saving
			standby mode, if needed.  (1 if not given)

		<dly>   some parallel ports require the driver to 
			go more slowly.  -1 sets a default value that
			should work with the chosen protocol.  Otherwise,
			set this to a small integer, the larger it is
			the slower the port i/o.  In some cases, setting
			this to zero will speed up the device. (default -1)

		<slv>   IDE disks can be jumpered to master or slave.
                        Set this to 0 to choose the master drive, 1 to
                        choose the slave, -1 (the default) to choose the
                        first drive found.
			

            major       You may use this parameter to overide the
                        default major number (45) that this driver
                        will use.  Be sure to change the device
                        name as well.

            name        This parameter is a character string that
                        contains the name the kernel will use for this
                        device (in /proc output, for instance).
			(default "pd")

	    cluster	The driver will attempt to aggregate requests
			for adjacent blocks into larger multi-block
			clusters.  The maximum cluster size (in 512
			byte sectors) is set with this parameter.
			(default 64)

	    verbose	This parameter controls the amount of logging
			that the driver will do.  Set it to 0 for 
			normal operation, 1 to see autoprobe progress
			messages, or 2 to see additional debugging
			output.  (default 0)

            nice        This parameter controls the driver's use of
                        idle CPU time, at the expense of some speed.

        If this driver is built into the kernel, you can use kernel
        the following command line parameters, with the same values
        as the corresponding module parameters listed above:

            pd.drive0
            pd.drive1
            pd.drive2
            pd.drive3
            pd.cluster
            pd.nice

        In addition, you can use the parameter pd.disable to disable
        the driver entirely.
 
*/

/* Changes:

	1.01	GRG 1997.01.24	Restored pd_reset()
				Added eject ioctl
	1.02    GRG 1998.05.06  SMP spinlock changes, 
				Added slave support
	1.03    GRG 1998.06.16  Eliminate an Ugh.
	1.04	GRG 1998.08.15  Extra debugging, use HZ in loop timing
	1.05    GRG 1998.09.24  Added jumbo support

*/

#define PD_VERSION      "1.05"
#define PD_MAJOR	45
#define PD_NAME		"pd"
#define PD_UNITS	4

/* Here are things one can override from the insmod command.
   Most are autoprobed by paride unless set here.  Verbose is off
   by default.

*/

static int	verbose = 0;
static int	major = PD_MAJOR;
static char	*name = PD_NAME;
static int	cluster = 64;	
static int      nice = 0;
static int      disable = 0;

static int drive0[8] = {0,0,0,-1,0,1,-1,-1};
static int drive1[8] = {0,0,0,-1,0,1,-1,-1};
static int drive2[8] = {0,0,0,-1,0,1,-1,-1};
static int drive3[8] = {0,0,0,-1,0,1,-1,-1};

static int (*drives[4])[8] = {&drive0,&drive1,&drive2,&drive3};
static int pd_drive_count;

#define D_PRT	0
#define D_PRO   1
#define D_UNI	2
#define D_MOD	3
#define D_GEO	4
#define D_SBY	5
#define D_DLY	6
#define D_SLV   7

#define	DU		(*drives[unit])

/* end of parameters */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>	/* for the eject ioctl */
#include <linux/spinlock.h>

#include <asm/uaccess.h>

#ifndef MODULE

#include "setup.h"

static STT pd_stt[7] = {{"drive0",8,drive0},
		        {"drive1",8,drive1},
		        {"drive2",8,drive2},
		        {"drive3",8,drive3},
			{"disable",1,&disable},
		        {"cluster",1,&cluster},
		        {"nice",1,&nice}};

void pd_setup( char *str, int *ints)

{
	generic_setup(pd_stt,7,str);
}

#endif

MODULE_PARM(verbose,"i");
MODULE_PARM(major,"i");
MODULE_PARM(name,"s");
MODULE_PARM(cluster,"i");
MODULE_PARM(nice,"i");
MODULE_PARM(drive0,"1-8i");
MODULE_PARM(drive1,"1-8i");
MODULE_PARM(drive2,"1-8i");
MODULE_PARM(drive3,"1-8i");

#include "paride.h"

#define PD_BITS    4

/* set up defines for blk.h,  why don't all drivers do it this way ? */

#define MAJOR_NR   major
#define DEVICE_NAME "PD"
#define DEVICE_REQUEST do_pd_request
#define DEVICE_NR(device) (MINOR(device)>>PD_BITS)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#include <linux/blk.h>
#include <linux/blkpg.h>

#include "pseudo.h"

#define PD_PARTNS  	(1<<PD_BITS)
#define PD_DEVS		PD_PARTNS*PD_UNITS

/* numbers for "SCSI" geometry */

#define PD_LOG_HEADS    64
#define PD_LOG_SECTS    32

#define PD_ID_OFF       54
#define PD_ID_LEN       14

#define PD_MAX_RETRIES  5
#define PD_TMO          800             /* interrupt timeout in jiffies */
#define PD_SPIN_DEL     50              /* spin delay in micro-seconds  */

#define PD_SPIN         (1000000*PD_TMO)/(HZ*PD_SPIN_DEL)  

#define STAT_ERR        0x00001
#define STAT_INDEX      0x00002
#define STAT_ECC        0x00004
#define STAT_DRQ        0x00008
#define STAT_SEEK       0x00010
#define STAT_WRERR      0x00020
#define STAT_READY      0x00040
#define STAT_BUSY       0x00080

#define ERR_AMNF        0x00100
#define ERR_TK0NF       0x00200
#define ERR_ABRT        0x00400
#define ERR_MCR         0x00800
#define ERR_IDNF        0x01000
#define ERR_MC          0x02000
#define ERR_UNC         0x04000
#define ERR_TMO         0x10000

#define IDE_READ        	0x20
#define IDE_WRITE       	0x30
#define IDE_READ_VRFY		0x40
#define IDE_INIT_DEV_PARMS	0x91
#define IDE_STANDBY     	0x96
#define IDE_ACKCHANGE   	0xdb
#define IDE_DOORLOCK    	0xde
#define IDE_DOORUNLOCK  	0xdf
#define IDE_IDENTIFY    	0xec
#define IDE_EJECT		0xed

int pd_init(void);
void pd_setup(char * str, int * ints);
#ifdef MODULE
void cleanup_module( void );
#endif
static int pd_open(struct inode *inode, struct file *file);
static void do_pd_request(request_queue_t * q);
static int pd_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg);
static int pd_release (struct inode *inode, struct file *file);
static int pd_revalidate(kdev_t dev);
static int pd_detect(void);
static void do_pd_read(void);
static void do_pd_read_start(void);
static void do_pd_write(void);
static void do_pd_write_start(void);
static void do_pd_read_drq( void );
static void do_pd_write_done( void );

static int pd_identify (int unit);
static void pd_media_check(int unit);
static void pd_doorlock(int unit, int func);
static int pd_check_media(kdev_t dev);
static void pd_eject( int unit);

static struct hd_struct pd_hd[PD_DEVS];
static int pd_sizes[PD_DEVS];
static int pd_blocksizes[PD_DEVS];
static int pd_maxsectors[PD_DEVS];

#define PD_NAMELEN	8

struct pd_unit {
	struct pi_adapter pia;		/* interface to paride layer */
	struct pi_adapter *pi;
	int access;               	/* count of active opens ... */
	int capacity;             	/* Size of this volume in sectors */
	int heads;                	/* physical geometry */
	int sectors;
	int cylinders;
        int can_lba;
	int drive;			/* master=0 slave=1 */
	int changed;			/* Have we seen a disk change ? */
	int removable;			/* removable media device  ?  */
	int standby;
	int alt_geom;
	int present;
	char name[PD_NAMELEN];		/* pda, pdb, etc ... */
};

struct pd_unit pd[PD_UNITS];

/*  'unit' must be defined in all functions - either as a local or a param */

#define PD pd[unit]
#define PI PD.pi

static int pd_valid = 1;		/* serialise partition checks */
static char pd_scratch[512];            /* scratch block buffer */

/* the variables below are used mainly in the I/O request engine, which
   processes only one request at a time.
*/

static int pd_retries = 0;              /* i/o error retry count */
static int pd_busy = 0;                 /* request being processed ? */
static int pd_block;                    /* address of next requested block */
static int pd_count;                    /* number of blocks still to do */
static int pd_run;			/* sectors in current cluster */
static int pd_cmd;			/* current command READ/WRITE */
static int pd_unit;			/* unit of current request */
static int pd_dev;			/* minor of current request */
static int pd_poffs;			/* partition offset of current minor */
static char * pd_buf;                   /* buffer for request in progress */

static DECLARE_WAIT_QUEUE_HEAD(pd_wait_open);

static char *pd_errs[17] = { "ERR","INDEX","ECC","DRQ","SEEK","WRERR",
                             "READY","BUSY","AMNF","TK0NF","ABRT","MCR",
                             "IDNF","MC","UNC","???","TMO"};

/* kernel glue structures */

extern struct block_device_operations pd_fops;

static struct gendisk pd_gendisk = {
	major:		PD_MAJOR,
	major_name:	PD_NAME,
	minor_shift:	PD_BITS,
	max_p:		PD_PARTNS,
	part:		pd_hd,
	sizes:		pd_sizes,
	fops:		&pd_fops,
};

static struct block_device_operations pd_fops = {
	owner:			THIS_MODULE,
        open:			pd_open,
        release:		pd_release,
        ioctl:			pd_ioctl,
        check_media_change:	pd_check_media,
        revalidate:		pd_revalidate
};

void pd_init_units( void )

{	int	unit, j;

	pd_drive_count = 0;
	for (unit=0;unit<PD_UNITS;unit++) {
		PD.pi = & PD.pia;
		PD.access = 0;
		PD.changed = 1;
		PD.capacity = 0;
		PD.drive = DU[D_SLV];
		PD.present = 0;
		j = 0;
		while ((j < PD_NAMELEN-2) && (PD.name[j]=name[j])) j++;
		PD.name[j++] = 'a' + unit;
		PD.name[j] = 0;
		PD.alt_geom = DU[D_GEO];
		PD.standby = DU[D_SBY];
		if (DU[D_PRT]) pd_drive_count++;
	}
}

int pd_init (void)

{       int i;
	request_queue_t * q; 

	if (disable) return -1;
        if (devfs_register_blkdev(MAJOR_NR,name,&pd_fops)) {
                printk("%s: unable to get major number %d\n",
                        name,major);
                return -1;
        }
	q = BLK_DEFAULT_QUEUE(MAJOR_NR);
	blk_init_queue(q, DEVICE_REQUEST);
        read_ahead[MAJOR_NR] = 8;       /* 8 sector (4kB) read ahead */
        
	pd_gendisk.major = major;
	pd_gendisk.major_name = name;
	add_gendisk(&pd_gendisk);

	for(i=0;i<PD_DEVS;i++) pd_blocksizes[i] = 1024;
	blksize_size[MAJOR_NR] = pd_blocksizes;

	for(i=0;i<PD_DEVS;i++) pd_maxsectors[i] = cluster;
	max_sectors[MAJOR_NR] = pd_maxsectors;

	printk("%s: %s version %s, major %d, cluster %d, nice %d\n",
		name,name,PD_VERSION,major,cluster,nice);
	pd_init_units();
	pd_valid = 0;
	pd_gendisk.nr_real = pd_detect();
	pd_valid = 1;

#ifdef MODULE
        if (!pd_gendisk.nr_real) {
		cleanup_module();
		return -1;
	}
#endif
        return 0;
}

static int pd_open (struct inode *inode, struct file *file)

{       int unit = DEVICE_NR(inode->i_rdev);

        if ((unit >= PD_UNITS) || (!PD.present)) return -ENODEV;

	wait_event (pd_wait_open, pd_valid);

        PD.access++;

        if (PD.removable) {
		pd_media_check(unit);
		pd_doorlock(unit,IDE_DOORLOCK);
	}
        return 0;
}

static int pd_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg)

{       struct hd_geometry *geo = (struct hd_geometry *) arg;
        int dev, err, unit;

        if ((!inode) || (!inode->i_rdev)) return -EINVAL;
        dev = MINOR(inode->i_rdev);
	unit = DEVICE_NR(inode->i_rdev);
        if (dev >= PD_DEVS) return -EINVAL;
	if (!PD.present) return -ENODEV;

        switch (cmd) {
	    case CDROMEJECT:
		if (PD.access == 1) pd_eject(unit);
		return 0;
            case HDIO_GETGEO:
                if (!geo) return -EINVAL;
                err = verify_area(VERIFY_WRITE,geo,sizeof(*geo));
                if (err) return err;

		if (PD.alt_geom) {
                    put_user(PD.capacity/(PD_LOG_HEADS*PD_LOG_SECTS), 
		    		(short *) &geo->cylinders);
                    put_user(PD_LOG_HEADS, (char *) &geo->heads);
                    put_user(PD_LOG_SECTS, (char *) &geo->sectors);
		} else {
                    put_user(PD.cylinders, (short *) &geo->cylinders);
                    put_user(PD.heads, (char *) &geo->heads);
                    put_user(PD.sectors, (char *) &geo->sectors);
		}
                put_user(pd_hd[dev].start_sect,(long *)&geo->start);
                return 0;
            case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
                return pd_revalidate(inode->i_rdev);
	    case BLKGETSIZE:
	    case BLKGETSIZE64:
	    case BLKROSET:
	    case BLKROGET:
	    case BLKRASET:
	    case BLKRAGET:
	    case BLKFLSBUF:
	    case BLKPG:
		return blk_ioctl(inode->i_rdev, cmd, arg);
            default:
                return -EINVAL;
        }
}

static int pd_release (struct inode *inode, struct file *file)

{       kdev_t devp;
	int	unit;

        devp = inode->i_rdev;
	unit = DEVICE_NR(devp);

	if ((unit >= PD_UNITS) || (PD.access <= 0)) 
		return -EINVAL;

	PD.access--;

        if (!PD.access && PD.removable)
		pd_doorlock(unit,IDE_DOORUNLOCK);

	return 0;
}

static int pd_check_media( kdev_t dev)

{       int r, unit;

	unit = DEVICE_NR(dev);
	if ((unit >= PD_UNITS) || (!PD.present)) return -ENODEV;
        if (!PD.removable) return 0;
	pd_media_check(unit);
	r = PD.changed;
	PD.changed = 0;
	return r;
}

static int pd_revalidate(kdev_t dev)

{       int p, unit, minor;
        long flags;

        unit = DEVICE_NR(dev);
        if ((unit >= PD_UNITS) || (!PD.present)) return -ENODEV;

        save_flags(flags);
        cli(); 
        if (PD.access > 1) {
                restore_flags(flags);
                return -EBUSY;
        }
        pd_valid = 0;
        restore_flags(flags);   

        for (p=(PD_PARTNS-1);p>=0;p--) {
		minor = p + unit*PD_PARTNS;
                invalidate_device(MKDEV(MAJOR_NR, minor), 1);
                pd_hd[minor].start_sect = 0;
                pd_hd[minor].nr_sects = 0;
        }

	if (pd_identify(unit))
		grok_partitions(&pd_gendisk,unit,1<<PD_BITS,PD.capacity);

        pd_valid = 1;
        wake_up(&pd_wait_open);

        return 0;
}

#ifdef MODULE

/* Glue for modules ... */

void    cleanup_module(void);

int     init_module(void)

{

#ifdef PARIDE_JUMBO
       { extern paride_init();
         paride_init();
       } 
#endif
        return pd_init();
}

void    cleanup_module(void)
{
	int unit;

        devfs_unregister_blkdev(MAJOR_NR,name);
	del_gendisk(&pd_gendisk);

	for (unit=0;unit<PD_UNITS;unit++) 
	   if (PD.present) pi_release(PI);

	max_sectors[MAJOR_NR] = NULL;
}

#endif

#define	WR(c,r,v)	pi_write_regr(PI,c,r,v)
#define	RR(c,r)		(pi_read_regr(PI,c,r))

#define DRIVE		(0xa0+0x10*PD.drive)

/*  ide command interface */

static void pd_print_error( int unit, char * msg, int status )

{       int     i;

	printk("%s: %s: status = 0x%x =",PD.name,msg,status);
        for(i=0;i<18;i++) if (status & (1<<i)) printk(" %s",pd_errs[i]);
	printk("\n");
}

static void pd_reset( int unit )    /* called only for MASTER drive */

{       pi_connect(PI);
	WR(1,6,4);
        udelay(50);
        WR(1,6,0);
	pi_disconnect(PI);
	udelay(250);
}

#define DBMSG(msg)	((verbose>1)?(msg):NULL)

static int pd_wait_for( int unit, int w, char * msg )    /* polled wait */

{       int     k, r, e;

        k=0;
        while(k < PD_SPIN) { 
            r = RR(1,6);
            k++;
            if (((r & w) == w) && !(r & STAT_BUSY)) break;
            udelay(PD_SPIN_DEL);
        }
        e = (RR(0,1)<<8) + RR(0,7);
        if (k >= PD_SPIN)  e |= ERR_TMO;
        if ((e & (STAT_ERR|ERR_TMO)) && (msg != NULL)) 
		pd_print_error(unit,msg,e);
        return e;
}

static void pd_send_command( int unit, int n, int s, int h, 
			     int c0, int c1, int func )

{
        WR(0,6,DRIVE+h);
        WR(0,1,0);                /* the IDE task file */
        WR(0,2,n);
        WR(0,3,s);
        WR(0,4,c0);
        WR(0,5,c1);
        WR(0,7,func);

        udelay(1);
}

static void pd_ide_command( int unit, int func, int block, int count )

/* Don't use this call if the capacity is zero. */

{
       int c1, c0, h, s;
       if (PD.can_lba) {
               s = block & 255;
               c0 = (block >>= 8) & 255;
               c1 = (block >>= 8) & 255;
               h = ((block >>= 8) & 15) + 0x40;
       } else {
               s  = ( block % PD.sectors) + 1;
               h  = ( block /= PD.sectors) % PD.heads;
               c0 = ( block /= PD.heads) % 256;
               c1 = (block >>= 8);
       }
       pd_send_command(unit,count,s,h,c0,c1,func);
}

/* According to the ATA standard, the default CHS geometry should be
   available following a reset.  Some Western Digital drives come up
   in a mode where only LBA addresses are accepted until the device
   parameters are initialised.
*/

static void pd_init_dev_parms( int unit )
 
{       pi_connect(PI);
        pd_wait_for(unit,0,DBMSG("before init_dev_parms"));
        pd_send_command(unit,PD.sectors,0,PD.heads-1,0,0,IDE_INIT_DEV_PARMS);
        udelay(300);
        pd_wait_for(unit,0,"Initialise device parameters");
        pi_disconnect(PI);
}

static void pd_doorlock( int unit, int func )

{       pi_connect(PI);
        if (pd_wait_for(unit,STAT_READY,"Lock") & STAT_ERR) {
                pi_disconnect(PI);
                return;
        }
        pd_send_command(unit,1,0,0,0,0,func);
        pd_wait_for(unit,STAT_READY,"Lock done");
        pi_disconnect(PI);
}

static void pd_eject( int unit )

{	pi_connect(PI);
        pd_wait_for(unit,0,DBMSG("before unlock on eject"));
        pd_send_command(unit,1,0,0,0,0,IDE_DOORUNLOCK);
        pd_wait_for(unit,0,DBMSG("after unlock on eject"));
        pd_wait_for(unit,0,DBMSG("before eject"));
        pd_send_command(unit,0,0,0,0,0,IDE_EJECT);
        pd_wait_for(unit,0,DBMSG("after eject"));
        pi_disconnect(PI);
}

static void pd_media_check( int unit )

{       int 	r;

        pi_connect(PI);
        r = pd_wait_for(unit,STAT_READY,DBMSG("before media_check"));
        if (!(r & STAT_ERR)) {
                pd_send_command(unit,1,1,0,0,0,IDE_READ_VRFY);  
                r = pd_wait_for(unit,STAT_READY,DBMSG("RDY after READ_VRFY"));
        } else PD.changed = 1;   /* say changed if other error */
        if (r & ERR_MC) {
                PD.changed = 1;
                pd_send_command(unit,1,0,0,0,0,IDE_ACKCHANGE);
                pd_wait_for(unit,STAT_READY,DBMSG("RDY after ACKCHANGE"));
		pd_send_command(unit,1,1,0,0,0,IDE_READ_VRFY);
                r = pd_wait_for(unit,STAT_READY,DBMSG("RDY after VRFY"));
        }
        pi_disconnect(PI);

}

static void pd_standby_off( int unit )

{       pi_connect(PI);
        pd_wait_for(unit,0,DBMSG("before STANDBY"));
        pd_send_command(unit,0,0,0,0,0,IDE_STANDBY);
        pd_wait_for(unit,0,DBMSG("after STANDBY"));
        pi_disconnect(PI);
}

#define  word_val(n) ((pd_scratch[2*n]&0xff)+256*(pd_scratch[2*n+1]&0xff))

static int pd_identify( int unit )

{       int	j;
	char id[PD_ID_LEN+1];

/* WARNING:  here there may be dragons.  reset() applies to both drives,
   but we call it only on probing the MASTER. This should allow most
   common configurations to work, but be warned that a reset can clear
   settings on the SLAVE drive.
*/ 

	if (PD.drive == 0) pd_reset(unit);

        pi_connect(PI);
	WR(0,6,DRIVE);
        pd_wait_for(unit,0,DBMSG("before IDENT"));  
        pd_send_command(unit,1,0,0,0,0,IDE_IDENTIFY);

        if (pd_wait_for(unit,STAT_DRQ,DBMSG("IDENT DRQ")) & STAT_ERR) {
                pi_disconnect(PI);
                return 0;
        }
        pi_read_block(PI,pd_scratch,512);
        pi_disconnect(PI);
	PD.can_lba = pd_scratch[99] & 2;
	PD.sectors = le16_to_cpu(*(u16*)(pd_scratch+12));
	PD.heads = le16_to_cpu(*(u16*)(pd_scratch+6));
	PD.cylinders  = le16_to_cpu(*(u16*)(pd_scratch+2));
	if (PD.can_lba)
	  PD.capacity = le32_to_cpu(*(u32*)(pd_scratch + 120));
	else
	  PD.capacity = PD.sectors*PD.heads*PD.cylinders;

        for(j=0;j<PD_ID_LEN;j++) id[j^1] = pd_scratch[j+PD_ID_OFF];
        j = PD_ID_LEN-1;
        while ((j >= 0) && (id[j] <= 0x20)) j--;
        j++; id[j] = 0;

        PD.removable = (word_val(0) & 0x80);
 
        printk("%s: %s, %s, %d blocks [%dM], (%d/%d/%d), %s media\n",
                    PD.name,id,
		    PD.drive?"slave":"master",
		    PD.capacity,PD.capacity/2048,
                    PD.cylinders,PD.heads,PD.sectors,
                    PD.removable?"removable":"fixed");

        if (PD.capacity) pd_init_dev_parms(unit);
        if (!PD.standby) pd_standby_off(unit);
	
        return 1;
}

static int pd_probe_drive( int unit )
{
	if (PD.drive == -1) {
		for (PD.drive=0;PD.drive<=1;PD.drive++)
			if (pd_identify(unit))
				return 1;
		return 0;
	}
	return pd_identify(unit);
}

static int pd_detect( void )

{       int	k, unit;

	k = 0;
	if (pd_drive_count == 0) {  /* nothing spec'd - so autoprobe for 1 */
	    unit = 0;
	    if (pi_init(PI,1,-1,-1,-1,-1,-1,pd_scratch,
	             PI_PD,verbose,PD.name)) {
		if (pd_probe_drive(unit)) {
			PD.present = 1;
			k = 1;
		} else pi_release(PI);
	    }

   	} else for (unit=0;unit<PD_UNITS;unit++) if (DU[D_PRT])
	    if (pi_init(PI,0,DU[D_PRT],DU[D_MOD],DU[D_UNI],
			DU[D_PRO],DU[D_DLY],pd_scratch,
			PI_PD,verbose,PD.name)) {
                if (pd_probe_drive(unit)) {
                        PD.present = 1;
                        k = unit+1;
                } else pi_release(PI);
            }
	for (unit=0;unit<PD_UNITS;unit++)
		register_disk(&pd_gendisk,MKDEV(MAJOR_NR,unit<<PD_BITS),
				PD_PARTNS,&pd_fops,
				PD.present?PD.capacity:0);

/* We lie about the number of drives found, as the generic partition
   scanner assumes that the drives are numbered sequentially from 0.
   This can result in some bogus error messages if non-sequential
   drive numbers are used.
*/
	if (k)
		return k; 
        printk("%s: no valid drive found\n",name);
        return 0;
}

/* The i/o request engine */

static int pd_ready( void )

{ 	int unit = pd_unit;

	return (!(RR(1,6) & STAT_BUSY)) ;
}

static void do_pd_request (request_queue_t * q)

{       struct buffer_head * bh;
	int	unit;

        if (pd_busy) return;
repeat:
        if (QUEUE_EMPTY || (CURRENT->rq_status == RQ_INACTIVE)) return;
        INIT_REQUEST;

        pd_dev = MINOR(CURRENT->rq_dev);
	pd_unit = unit = DEVICE_NR(CURRENT->rq_dev);
        pd_block = CURRENT->sector;
        pd_run = CURRENT->nr_sectors;
        pd_count = CURRENT->current_nr_sectors;

	bh = CURRENT->bh;

        if ((pd_dev >= PD_DEVS) || 
	    ((pd_block+pd_count) > pd_hd[pd_dev].nr_sects)) {
                end_request(0);
                goto repeat;
        }

	pd_cmd = CURRENT->cmd;
	pd_poffs = pd_hd[pd_dev].start_sect;
        pd_block += pd_poffs;
        pd_buf = CURRENT->buffer;
        pd_retries = 0;

	pd_busy = 1;
        if (pd_cmd == READ) pi_do_claimed(PI,do_pd_read);
        else if (pd_cmd == WRITE) pi_do_claimed(PI,do_pd_write);
        else {  pd_busy = 0;
		end_request(0);
                goto repeat;
        }
}

static void pd_next_buf( int unit )

{	unsigned long	saved_flags;

	spin_lock_irqsave(&io_request_lock,saved_flags);
	end_request(1);
	if (!pd_run) {  spin_unlock_irqrestore(&io_request_lock,saved_flags);
			return; 
	}
	
/* paranoia */

	if (QUEUE_EMPTY ||
	    (CURRENT->cmd != pd_cmd) ||
	    (MINOR(CURRENT->rq_dev) != pd_dev) ||
	    (CURRENT->rq_status == RQ_INACTIVE) ||
	    (CURRENT->sector+pd_poffs != pd_block)) 
		printk("%s: OUCH: request list changed unexpectedly\n",
			PD.name);

	pd_count = CURRENT->current_nr_sectors;
	pd_buf = CURRENT->buffer;
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

static void do_pd_read( void )

{	ps_set_intr(do_pd_read_start,0,0,nice);
}

static void do_pd_read_start( void )
 
{       int	unit = pd_unit;
	unsigned long    saved_flags;

	pd_busy = 1;

        pi_connect(PI);
        if (pd_wait_for(unit,STAT_READY,"do_pd_read") & STAT_ERR) {
                pi_disconnect(PI);
                if (pd_retries < PD_MAX_RETRIES) {
                        pd_retries++;
                        pi_do_claimed(PI,do_pd_read_start);
			return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pd_busy = 0;
		do_pd_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }
        pd_ide_command(unit,IDE_READ,pd_block,pd_run);
        ps_set_intr(do_pd_read_drq,pd_ready,PD_TMO,nice);
}

static void do_pd_read_drq( void )

{       int	unit = pd_unit;
	unsigned long    saved_flags;

	while (1) {
            if (pd_wait_for(unit,STAT_DRQ,"do_pd_read_drq") & STAT_ERR) {
                pi_disconnect(PI);
                if (pd_retries < PD_MAX_RETRIES) {
                        pd_retries++;
                        pi_do_claimed(PI,do_pd_read_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pd_busy = 0;
		do_pd_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
            }
            pi_read_block(PI,pd_buf,512);
            pd_count--; pd_run--;
            pd_buf += 512;
	    pd_block++;
	    if (!pd_run) break;
	    if (!pd_count) pd_next_buf(unit);
        }
        pi_disconnect(PI);
	spin_lock_irqsave(&io_request_lock,saved_flags);
        end_request(1);
        pd_busy = 0;
	do_pd_request(NULL);
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

static void do_pd_write( void )

{	 ps_set_intr(do_pd_write_start,0,0,nice);
}

static void do_pd_write_start( void )

{       int 	unit = pd_unit;
	unsigned long    saved_flags;

	pd_busy = 1;

        pi_connect(PI);
        if (pd_wait_for(unit,STAT_READY,"do_pd_write") & STAT_ERR) {
                pi_disconnect(PI);
                if (pd_retries < PD_MAX_RETRIES) {
                        pd_retries++;
			pi_do_claimed(PI,do_pd_write_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pd_busy = 0;
		do_pd_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }
        pd_ide_command(unit,IDE_WRITE,pd_block,pd_run);
	while (1) {
            if (pd_wait_for(unit,STAT_DRQ,"do_pd_write_drq") & STAT_ERR) {
                pi_disconnect(PI);
                if (pd_retries < PD_MAX_RETRIES) {
                        pd_retries++;
                        pi_do_claimed(PI,do_pd_write_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pd_busy = 0;
		do_pd_request(NULL);
                spin_unlock_irqrestore(&io_request_lock,saved_flags);
		return;
            }
            pi_write_block(PI,pd_buf,512);
	    pd_count--; pd_run--;
	    pd_buf += 512;
	    pd_block++;
	    if (!pd_run) break;
	    if (!pd_count) pd_next_buf(unit);
	}
        ps_set_intr(do_pd_write_done,pd_ready,PD_TMO,nice);
}

static void do_pd_write_done( void )

{       int	unit = pd_unit;
	unsigned long    saved_flags;

        if (pd_wait_for(unit,STAT_READY,"do_pd_write_done") & STAT_ERR) {
                pi_disconnect(PI);
                if (pd_retries < PD_MAX_RETRIES) {
                        pd_retries++;
                        pi_do_claimed(PI,do_pd_write_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pd_busy = 0;
		do_pd_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }
        pi_disconnect(PI);
	spin_lock_irqsave(&io_request_lock,saved_flags);
        end_request(1);
        pd_busy = 0;
	do_pd_request(NULL);
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

/* end of pd.c */

MODULE_LICENSE("GPL");

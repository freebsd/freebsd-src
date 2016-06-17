/* 
        pf.c    (c) 1997-8  Grant R. Guenther <grant@torque.net>
                            Under the terms of the GNU General Public License.

        This is the high-level driver for parallel port ATAPI disk
        drives based on chips supported by the paride module.

        By default, the driver will autoprobe for a single parallel
        port ATAPI disk drive, but if their individual parameters are
        specified, the driver can handle up to 4 drives.

        The behaviour of the pf driver can be altered by setting
        some parameters from the insmod command line.  The following
        parameters are adjustable:

            drive0      These four arguments can be arrays of       
            drive1      1-7 integers as follows:
            drive2
            drive3      <prt>,<pro>,<uni>,<mod>,<slv>,<lun>,<dly>

                        Where,

                <prt>   is the base of the parallel port address for
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

                <slv>   ATAPI CDroms can be jumpered to master or slave.
                        Set this to 0 to choose the master drive, 1 to
                        choose the slave, -1 (the default) to choose the
                        first drive found.

		<lun>   Some ATAPI devices support multiple LUNs.
                        One example is the ATAPI PD/CD drive from
                        Matshita/Panasonic.  This device has a 
                        CD drive on LUN 0 and a PD drive on LUN 1.
                        By default, the driver will search for the
                        first LUN with a supported device.  Set 
                        this parameter to force it to use a specific
                        LUN.  (default -1)

                <dly>   some parallel ports require the driver to 
                        go more slowly.  -1 sets a default value that
                        should work with the chosen protocol.  Otherwise,
                        set this to a small integer, the larger it is
                        the slower the port i/o.  In some cases, setting
                        this to zero will speed up the device. (default -1)

	    major	You may use this parameter to overide the
			default major number (47) that this driver
			will use.  Be sure to change the device
			name as well.

	    name	This parameter is a character string that
			contains the name the kernel will use for this
			device (in /proc output, for instance).
			(default "pf").

            cluster     The driver will attempt to aggregate requests
                        for adjacent blocks into larger multi-block
                        clusters.  The maximum cluster size (in 512
                        byte sectors) is set with this parameter.
                        (default 64)

            verbose     This parameter controls the amount of logging
                        that the driver will do.  Set it to 0 for
                        normal operation, 1 to see autoprobe progress
                        messages, or 2 to see additional debugging
                        output.  (default 0)
 
	    nice        This parameter controls the driver's use of
			idle CPU time, at the expense of some speed.

        If this driver is built into the kernel, you can use the
        following command line parameters, with the same values
        as the corresponding module parameters listed above:

            pf.drive0
            pf.drive1
            pf.drive2
            pf.drive3
	    pf.cluster
            pf.nice

        In addition, you can use the parameter pf.disable to disable
        the driver entirely.

*/

/* Changes:

	1.01	GRG 1998.05.03  Changes for SMP.  Eliminate sti().
				Fix for drives that don't clear STAT_ERR
			        until after next CDB delivered.
				Small change in pf_completion to round
				up transfer size.
	1.02    GRG 1998.06.16  Eliminated an Ugh
	1.03    GRG 1998.08.16  Use HZ in loop timings, extra debugging
	1.04    GRG 1998.09.24  Added jumbo support

*/

#define PF_VERSION      "1.04"
#define PF_MAJOR	47
#define PF_NAME		"pf"
#define PF_UNITS	4

/* Here are things one can override from the insmod command.
   Most are autoprobed by paride unless set here.  Verbose is off
   by default.

*/

static int	verbose = 0;
static int	major = PF_MAJOR;
static char	*name = PF_NAME;
static int      cluster = 64;
static int      nice = 0;
static int      disable = 0;

static int drive0[7] = {0,0,0,-1,-1,-1,-1};
static int drive1[7] = {0,0,0,-1,-1,-1,-1};
static int drive2[7] = {0,0,0,-1,-1,-1,-1};
static int drive3[7] = {0,0,0,-1,-1,-1,-1};

static int (*drives[4])[7] = {&drive0,&drive1,&drive2,&drive3};
static int pf_drive_count;

#define D_PRT   0
#define D_PRO   1
#define D_UNI   2
#define D_MOD   3
#define D_SLV   4
#define D_LUN   5
#define D_DLY   6

#define DU              (*drives[unit])

/* end of parameters */


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>

#ifndef MODULE

#include "setup.h"

static STT pf_stt[7] = {{"drive0",7,drive0},
                        {"drive1",7,drive1},
                        {"drive2",7,drive2},
                        {"drive3",7,drive3},
			{"disable",1,&disable},
                        {"cluster",1,&cluster},
                        {"nice",1,&nice}};

void pf_setup( char *str, int *ints)

{       generic_setup(pf_stt,7,str);
}

#endif

MODULE_PARM(verbose,"i");
MODULE_PARM(major,"i");
MODULE_PARM(name,"s");
MODULE_PARM(cluster,"i");
MODULE_PARM(nice,"i");
MODULE_PARM(drive0,"1-7i");
MODULE_PARM(drive1,"1-7i");
MODULE_PARM(drive2,"1-7i");
MODULE_PARM(drive3,"1-7i");

#include "paride.h"

/* set up defines for blk.h,  why don't all drivers do it this way ? */

#define MAJOR_NR   major
#define DEVICE_NAME "PF"
#define DEVICE_REQUEST do_pf_request
#define DEVICE_NR(device) MINOR(device)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#include <linux/blk.h>
#include <linux/blkpg.h>

#include "pseudo.h"

/* constants for faking geometry numbers */

#define PF_FD_MAX	8192		/* use FD geometry under this size */
#define PF_FD_HDS	2
#define PF_FD_SPT	18
#define PF_HD_HDS	64
#define PF_HD_SPT	32

#define PF_MAX_RETRIES  5
#define PF_TMO          800             /* interrupt timeout in jiffies */
#define PF_SPIN_DEL     50              /* spin delay in micro-seconds  */

#define PF_SPIN         (1000000*PF_TMO)/(HZ*PF_SPIN_DEL)

#define STAT_ERR        0x00001
#define STAT_INDEX      0x00002
#define STAT_ECC        0x00004
#define STAT_DRQ        0x00008
#define STAT_SEEK       0x00010
#define STAT_WRERR      0x00020
#define STAT_READY      0x00040
#define STAT_BUSY       0x00080

#define ATAPI_REQ_SENSE		0x03
#define ATAPI_LOCK		0x1e
#define ATAPI_DOOR		0x1b
#define ATAPI_MODE_SENSE	0x5a
#define ATAPI_CAPACITY		0x25
#define ATAPI_IDENTIFY		0x12
#define ATAPI_READ_10		0x28
#define ATAPI_WRITE_10		0x2a

int pf_init(void);
#ifdef MODULE
void cleanup_module( void );
#endif
static int pf_open(struct inode *inode, struct file *file);
static void do_pf_request(request_queue_t * q);
static int pf_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg);

static int pf_release (struct inode *inode, struct file *file);

static int pf_detect(void);
static void do_pf_read(void);
static void do_pf_read_start(void);
static void do_pf_write(void);
static void do_pf_write_start(void);
static void do_pf_read_drq( void );
static void do_pf_write_done( void );

static int pf_identify (int unit);
static void pf_lock(int unit, int func);
static void pf_eject(int unit);
static int pf_check_media(kdev_t dev);

static int pf_blocksizes[PF_UNITS];

#define PF_NM           0
#define PF_RO           1
#define PF_RW           2

#define PF_NAMELEN      8

struct pf_unit {
	struct pi_adapter pia;    /* interface to paride layer */
	struct pi_adapter *pi;
	int removable;		  /* removable media device  ?  */
	int media_status;	  /* media present ?  WP ? */
	int drive;		  /* drive */
	int lun;
	int access;               /* count of active opens ... */
	int capacity;             /* Size of this volume in sectors */
	int present;		  /* device present ? */
	char name[PF_NAMELEN];	  /* pf0, pf1, ... */
	};

struct pf_unit pf[PF_UNITS];

/*  'unit' must be defined in all functions - either as a local or a param */

#define PF pf[unit]
#define PI PF.pi

static char pf_scratch[512];            /* scratch block buffer */

/* the variables below are used mainly in the I/O request engine, which
   processes only one request at a time.
*/

static int pf_retries = 0;              /* i/o error retry count */
static int pf_busy = 0;                 /* request being processed ? */
static int pf_block;                    /* address of next requested block */
static int pf_count;                    /* number of blocks still to do */
static int pf_run;			/* sectors in current cluster */
static int pf_cmd;			/* current command READ/WRITE */
static int pf_unit;			/* unit of current request */
static int pf_mask;			/* stopper for pseudo-int */
static char * pf_buf;                   /* buffer for request in progress */

/* kernel glue structures */

static struct block_device_operations pf_fops = {
	owner:			THIS_MODULE,
	open:			pf_open,
	release:		pf_release,
	ioctl:			pf_ioctl,
	check_media_change:	pf_check_media,
};

void pf_init_units( void )

{       int     unit, j;

        pf_drive_count = 0;
        for (unit=0;unit<PF_UNITS;unit++) {
                PF.pi = & PF.pia;
                PF.access = 0;
                PF.media_status = PF_NM;
                PF.capacity = 0;
                PF.present = 0;
		PF.drive = DU[D_SLV];
		PF.lun = DU[D_LUN];
                j = 0;
                while ((j < PF_NAMELEN-2) && (PF.name[j]=name[j])) j++;
                PF.name[j++] = '0' + unit;
                PF.name[j] = 0;
                if (DU[D_PRT]) pf_drive_count++;
        }
} 

static inline int pf_new_segment(request_queue_t *q, struct request *req, int max_segments)
{
	if (max_segments > cluster)
		max_segments = cluster;

	if (req->nr_segments < max_segments) {
		req->nr_segments++;
		return 1;
	}
	return 0;
}

static int pf_back_merge_fn(request_queue_t *q, struct request *req, 
			    struct buffer_head *bh, int max_segments)
{
	if (req->bhtail->b_data + req->bhtail->b_size == bh->b_data)
		return 1;
	return pf_new_segment(q, req, max_segments);
}

static int pf_front_merge_fn(request_queue_t *q, struct request *req, 
			     struct buffer_head *bh, int max_segments)
{
	if (bh->b_data + bh->b_size == req->bh->b_data)
		return 1;
	return pf_new_segment(q, req, max_segments);
}

static int pf_merge_requests_fn(request_queue_t *q, struct request *req,
				struct request *next, int max_segments)
{
	int total_segments = req->nr_segments + next->nr_segments;
	int same_segment;

	if (max_segments > cluster)
		max_segments = cluster;

	same_segment = 0;
	if (req->bhtail->b_data + req->bhtail->b_size == next->bh->b_data) {
		total_segments--;
		same_segment = 1;
	}
    
	if (total_segments > max_segments)
		return 0;

	req->nr_segments = total_segments;
	return 1;
}

int pf_init (void)      /* preliminary initialisation */

{       int i;
	request_queue_t * q; 

	if (disable) return -1;

	pf_init_units();

	if (pf_detect()) return -1;
	pf_busy = 0;

        if (register_blkdev(MAJOR_NR,name,&pf_fops)) {
                printk("pf_init: unable to get major number %d\n",
                        major);
                return -1;
        }
	q = BLK_DEFAULT_QUEUE(MAJOR_NR);
	blk_init_queue(q, DEVICE_REQUEST);
	q->back_merge_fn = pf_back_merge_fn;
	q->front_merge_fn = pf_front_merge_fn;
	q->merge_requests_fn = pf_merge_requests_fn;
        read_ahead[MAJOR_NR] = 8;       /* 8 sector (4kB) read ahead */
        
	for (i=0;i<PF_UNITS;i++) pf_blocksizes[i] = 1024;
	blksize_size[MAJOR_NR] = pf_blocksizes;
	for (i=0;i<PF_UNITS;i++)
		register_disk(NULL, MKDEV(MAJOR_NR, i), 1, &pf_fops, 0);

        return 0;
}

static int pf_open (struct inode *inode, struct file *file)

{       int	unit = DEVICE_NR(inode->i_rdev);

        if ((unit >= PF_UNITS) || (!PF.present)) return -ENODEV;

	pf_identify(unit);

	if (PF.media_status == PF_NM)
		return -ENODEV;

	if ((PF.media_status == PF_RO) && (file ->f_mode & 2))
		return -EROFS;

        PF.access++;
        if (PF.removable) pf_lock(unit,1);

        return 0;
}

static int pf_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg)

{       int err, unit;
	struct hd_geometry *geo = (struct hd_geometry *) arg;

        if ((!inode) || (!inode->i_rdev)) return -EINVAL;
        unit = DEVICE_NR(inode->i_rdev);
        if (unit >= PF_UNITS) return -EINVAL;
        if (!PF.present) return -ENODEV;

        switch (cmd) {
	    case CDROMEJECT: 
		if (PF.access == 1) {
			pf_eject(unit);
			return 0;
			}
	    case HDIO_GETGEO:
                if (!geo) return -EINVAL;
                err = verify_area(VERIFY_WRITE,geo,sizeof(*geo));
                if (err) return err;
                if (PF.capacity < PF_FD_MAX) {
                    put_user(PF.capacity/(PF_FD_HDS*PF_FD_SPT),
                                (short *) &geo->cylinders);
                    put_user(PF_FD_HDS, (char *) &geo->heads);
                    put_user(PF_FD_SPT, (char *) &geo->sectors);
                } else {
                    put_user(PF.capacity/(PF_HD_HDS*PF_HD_SPT), 
				(short *) &geo->cylinders);
                    put_user(PF_HD_HDS, (char *) &geo->heads);
                    put_user(PF_HD_SPT, (char *) &geo->sectors);
                }
                put_user(0,(long *)&geo->start);
                return 0;
            case BLKGETSIZE:
                return put_user(PF.capacity,(long *) arg);
            case BLKGETSIZE64:
                return put_user((u64)PF.capacity << 9,(u64 *)arg);
	    case BLKROSET:
	    case BLKROGET:
	    case BLKRASET:
	    case BLKRAGET:
	    case BLKFLSBUF:
		return blk_ioctl(inode->i_rdev, cmd, arg);
            default:
                return -EINVAL;
        }
}


static int pf_release (struct inode *inode, struct file *file)

{       kdev_t devp;
	int	unit;

        devp = inode->i_rdev;
        unit = DEVICE_NR(devp);

        if ((unit >= PF_UNITS) || (PF.access <= 0)) 
                return -EINVAL;

	PF.access--;

	if (!PF.access && PF.removable)
		pf_lock(unit,0);

	return 0;

}

static int pf_check_media( kdev_t dev)

{       return 1;
}

#ifdef MODULE

/* Glue for modules ... */

void    cleanup_module(void);

int     init_module(void)

{       int     err;

#ifdef PARIDE_JUMBO
       { extern paride_init();
         paride_init();
       } 
#endif

        err = pf_init();

        return err;
}

void    cleanup_module(void)

{       int unit;

        unregister_blkdev(MAJOR_NR,name);

	for (unit=0;unit<PF_UNITS;unit++)
	  if (PF.present) pi_release(PI);
}

#endif

#define	WR(c,r,v)	pi_write_regr(PI,c,r,v)
#define	RR(c,r)		(pi_read_regr(PI,c,r))

#define LUN             (0x20*PF.lun)
#define DRIVE           (0xa0+0x10*PF.drive)

static int pf_wait( int unit, int go, int stop, char * fun, char * msg )

{       int j, r, e, s, p;

        j = 0;
        while ((((r=RR(1,6))&go)||(stop&&(!(r&stop))))&&(j++<PF_SPIN))
                udelay(PF_SPIN_DEL);

        if ((r&(STAT_ERR&stop))||(j>=PF_SPIN)) {
           s = RR(0,7);
           e = RR(0,1);
           p = RR(0,2);
           if (j >= PF_SPIN) e |= 0x100;
           if (fun) printk("%s: %s %s: alt=0x%x stat=0x%x err=0x%x"
                           " loop=%d phase=%d\n",
                            PF.name,fun,msg,r,s,e,j,p);
           return (e<<8)+s;
        }
        return 0;
}

static int pf_command( int unit, char * cmd, int dlen, char * fun )

{       pi_connect(PI);

        WR(0,6,DRIVE);

        if (pf_wait(unit,STAT_BUSY|STAT_DRQ,0,fun,"before command")) {
                pi_disconnect(PI);
                return -1;
        }

        WR(0,4,dlen % 256);
        WR(0,5,dlen / 256);
        WR(0,7,0xa0);          /* ATAPI packet command */

        if (pf_wait(unit,STAT_BUSY,STAT_DRQ,fun,"command DRQ")) {
                pi_disconnect(PI);
                return -1;
        }

        if (RR(0,2) != 1) {
           printk("%s: %s: command phase error\n",PF.name,fun);
           pi_disconnect(PI);
           return -1;
        }

        pi_write_block(PI,cmd,12);

        return 0;
}

static int pf_completion( int unit, char * buf, char * fun )

{       int r, s, n;

        r = pf_wait(unit,STAT_BUSY,STAT_DRQ|STAT_READY|STAT_ERR,
			fun,"completion");

        if ((RR(0,2)&2) && (RR(0,7)&STAT_DRQ)) { 
                n = (((RR(0,4)+256*RR(0,5))+3)&0xfffc);
                pi_read_block(PI,buf,n);
        }

        s = pf_wait(unit,STAT_BUSY,STAT_READY|STAT_ERR,fun,"data done");

        pi_disconnect(PI); 

        return (r?r:s);
}

static void pf_req_sense( int unit, int quiet )

{       char    rs_cmd[12] = { ATAPI_REQ_SENSE,LUN,0,0,16,0,0,0,0,0,0,0 };
        char    buf[16];
        int     r;

        r = pf_command(unit,rs_cmd,16,"Request sense");
        mdelay(1);
        if (!r) pf_completion(unit,buf,"Request sense");

        if ((!r)&&(!quiet)) 
                printk("%s: Sense key: %x, ASC: %x, ASQ: %x\n",
                       PF.name,buf[2]&0xf,buf[12],buf[13]);
}

static int pf_atapi( int unit, char * cmd, int dlen, char * buf, char * fun )

{       int r;

        r = pf_command(unit,cmd,dlen,fun);
        mdelay(1);
        if (!r) r = pf_completion(unit,buf,fun);
        if (r) pf_req_sense(unit,!fun);
        
        return r;
}

#define DBMSG(msg)      ((verbose>1)?(msg):NULL)

static void pf_lock(int unit, int func)

{	char	lo_cmd[12] = { ATAPI_LOCK,LUN,0,0,func,0,0,0,0,0,0,0 };

        pf_atapi(unit,lo_cmd,0,pf_scratch,func?"unlock":"lock");
}


static void pf_eject( int unit )

{	char	ej_cmd[12] = { ATAPI_DOOR,LUN,0,0,2,0,0,0,0,0,0,0 };

	pf_lock(unit,0);
	pf_atapi(unit,ej_cmd,0,pf_scratch,"eject");
}

#define PF_RESET_TMO   30              /* in tenths of a second */

static void pf_sleep( int cs )

{       current->state = TASK_INTERRUPTIBLE;
        schedule_timeout(cs);
}


static int pf_reset( int unit )

/* the ATAPI standard actually specifies the contents of all 7 registers
   after a reset, but the specification is ambiguous concerning the last
   two bytes, and different drives interpret the standard differently.
*/

{	int	i, k, flg;
	int	expect[5] = {1,1,1,0x14,0xeb};

	pi_connect(PI);
	WR(0,6,DRIVE);
	WR(0,7,8);

	pf_sleep(20*HZ/1000);

        k = 0;
        while ((k++ < PF_RESET_TMO) && (RR(1,6)&STAT_BUSY))
                pf_sleep(HZ/10);

	flg = 1;
	for(i=0;i<5;i++) flg &= (RR(0,i+1) == expect[i]);

	if (verbose) {
		printk("%s: Reset (%d) signature = ",PF.name,k);
		for (i=0;i<5;i++) printk("%3x",RR(0,i+1));
		if (!flg) printk(" (incorrect)");
		printk("\n");
	}
	
	pi_disconnect(PI);
	return flg-1;	
}

static void pf_mode_sense( int unit )

{       char    ms_cmd[12] = { ATAPI_MODE_SENSE,LUN,0,0,0,0,0,0,8,0,0,0};
	char	buf[8];

        pf_atapi(unit,ms_cmd,8,buf,DBMSG("mode sense"));
	PF.media_status = PF_RW;
	if (buf[3] & 0x80) PF.media_status = PF_RO;
}

static void xs( char *buf, char *targ, int offs, int len )

{	int	j,k,l;

	j=0; l=0;
	for (k=0;k<len;k++) 
	   if((buf[k+offs]!=0x20)||(buf[k+offs]!=l))
		l=targ[j++]=buf[k+offs];
	if (l==0x20) j--;
	targ[j]=0;
}

static int xl( char *buf, int offs )

{	int	v,k;

	v=0; 
	for(k=0;k<4;k++) v=v*256+(buf[k+offs]&0xff);
	return v;
}

static void pf_get_capacity( int unit )

{	char    rc_cmd[12] = { ATAPI_CAPACITY,LUN,0,0,0,0,0,0,0,0,0,0};
	char	buf[8];
        int 	bs;

	if (pf_atapi(unit,rc_cmd,8,buf,DBMSG("get capacity"))) {
		PF.media_status = PF_NM;
		return;
	}
	PF.capacity = xl(buf,0) + 1;  
	bs = xl(buf,4);
	if (bs != 512) {
		PF.capacity = 0;
		if (verbose) printk("%s: Drive %d, LUN %d,"
		       		    " unsupported block size %d\n",
			            PF.name,PF.drive,PF.lun,bs);
		}
}

static int pf_identify( int unit )

{	int 	dt, s;
	char	*ms[2] = {"master","slave"};
	char	mf[10], id[18];
	char    id_cmd[12] = { ATAPI_IDENTIFY,LUN,0,0,36,0,0,0,0,0,0,0};
	char	buf[36];

        s = pf_atapi(unit,id_cmd,36,buf,"identify");
	if (s) return -1;

	dt = buf[0] & 0x1f;
	if ((dt != 0) && (dt != 7)) {
	  	if (verbose) 
		   printk("%s: Drive %d, LUN %d, unsupported type %d\n",
				PF.name,PF.drive,PF.lun,dt);
	  	return -1;
       	}

	xs(buf,mf,8,8);
	xs(buf,id,16,16);

	PF.removable = (buf[1] & 0x80);

	pf_mode_sense(unit);
	pf_mode_sense(unit);
	pf_mode_sense(unit);

	pf_get_capacity(unit);

        printk("%s: %s %s, %s LUN %d, type %d",
		PF.name,mf,id,ms[PF.drive],PF.lun,dt);
        if (PF.removable) printk(", removable");
        if (PF.media_status == PF_NM) 
                printk(", no media\n");
        else {  if (PF.media_status == PF_RO) printk(", RO");
                printk(", %d blocks\n",PF.capacity);
        }

	return 0;
}

static int pf_probe( int unit )

/*	returns  0, with id set if drive is detected
	        -1, if drive detection failed
*/

{	if (PF.drive == -1) {
	   for (PF.drive=0;PF.drive<=1;PF.drive++)
		if (!pf_reset(unit)) {
		   if (PF.lun != -1) return pf_identify(unit);
		   else for (PF.lun=0;PF.lun<8;PF.lun++) 
                           if (!pf_identify(unit)) return 0;
		}
	} else {
	   if (pf_reset(unit)) return -1;
	   if (PF.lun != -1) return pf_identify(unit);
	   for (PF.lun=0;PF.lun<8;PF.lun++) 
	      if (!pf_identify(unit)) return 0;
	}
        return -1; 
}

static int pf_detect( void )

{	int	k, unit;

	printk("%s: %s version %s, major %d, cluster %d, nice %d\n",
		name,name,PF_VERSION,major,cluster,nice);

	k = 0;
	if (pf_drive_count == 0) {
	    unit = 0;
	    if (pi_init(PI,1,-1,-1,-1,-1,-1,pf_scratch,
                        PI_PF,verbose,PF.name)) {
	        if (!pf_probe(unit)) {
			PF.present = 1;
			k++;
	        } else pi_release(PI);
	    }

	} else for (unit=0;unit<PF_UNITS;unit++) if (DU[D_PRT])
	    if (pi_init(PI,0,DU[D_PRT],DU[D_MOD],DU[D_UNI],
			DU[D_PRO],DU[D_DLY],pf_scratch,PI_PF,verbose,
			PF.name)) { 
                if (!pf_probe(unit)) {
                        PF.present = 1;
                        k++;
                } else pi_release(PI);
            }

	if (k) return 0;

	printk("%s: No ATAPI disk detected\n",name);
	return -1;
}

/* The i/o request engine */

static int pf_start( int unit, int cmd, int b, int c )

{	int	i;
	char	io_cmd[12] = {cmd,LUN,0,0,0,0,0,0,0,0,0,0};

	for(i=0;i<4;i++) { 
	   io_cmd[5-i] = b & 0xff;
	   b = b >> 8;
	}
	
	io_cmd[8] = c & 0xff;
	io_cmd[7] = (c >> 8) & 0xff;

	i = pf_command(unit,io_cmd,c*512,"start i/o");

        mdelay(1);

	return i;	
}

static int pf_ready( void )

{	int	unit = pf_unit;

	return (((RR(1,6)&(STAT_BUSY|pf_mask)) == pf_mask));
}

static void do_pf_request (request_queue_t * q)

{       struct buffer_head * bh;
	int unit;

        if (pf_busy) return;
repeat:
        if (QUEUE_EMPTY || (CURRENT->rq_status == RQ_INACTIVE)) return;
        INIT_REQUEST;

        pf_unit = unit = DEVICE_NR(CURRENT->rq_dev);
        pf_block = CURRENT->sector;
        pf_run = CURRENT->nr_sectors;
        pf_count = CURRENT->current_nr_sectors;

	bh = CURRENT->bh;

        if ((pf_unit >= PF_UNITS) || (pf_block+pf_count > PF.capacity)) {
                end_request(0);
                goto repeat;
        }

	pf_cmd = CURRENT->cmd;
        pf_buf = CURRENT->buffer;
        pf_retries = 0;

	pf_busy = 1;
        if (pf_cmd == READ) pi_do_claimed(PI,do_pf_read);
        else if (pf_cmd == WRITE) pi_do_claimed(PI,do_pf_write);
        else {  pf_busy = 0;
		end_request(0);
                goto repeat;
        }
}

static void pf_next_buf( int unit )

{	unsigned long	saved_flags;

	spin_lock_irqsave(&io_request_lock,saved_flags);
	end_request(1);
	if (!pf_run) { spin_unlock_irqrestore(&io_request_lock,saved_flags);
		       return; 
	}
	
/* paranoia */

	if (QUEUE_EMPTY ||
	    (CURRENT->cmd != pf_cmd) ||
	    (DEVICE_NR(CURRENT->rq_dev) != pf_unit) ||
	    (CURRENT->rq_status == RQ_INACTIVE) ||
	    (CURRENT->sector != pf_block)) 
		printk("%s: OUCH: request list changed unexpectedly\n",
			PF.name);

	pf_count = CURRENT->current_nr_sectors;
	pf_buf = CURRENT->buffer;
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

static void do_pf_read( void )

/* detach from the calling context - in case the spinlock is held */

{	ps_set_intr(do_pf_read_start,0,0,nice);
}

static void do_pf_read_start( void )

{       int	unit = pf_unit;
	unsigned long	saved_flags;

	pf_busy = 1;

	if (pf_start(unit,ATAPI_READ_10,pf_block,pf_run)) {
                pi_disconnect(PI);
                if (pf_retries < PF_MAX_RETRIES) {
                        pf_retries++;
                        pi_do_claimed(PI,do_pf_read_start);
			return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pf_busy = 0;
		do_pf_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }
	pf_mask = STAT_DRQ;
        ps_set_intr(do_pf_read_drq,pf_ready,PF_TMO,nice);
}

static void do_pf_read_drq( void )

{       int	unit = pf_unit;
	unsigned long	saved_flags;
	
	while (1) {
            if (pf_wait(unit,STAT_BUSY,STAT_DRQ|STAT_ERR,
			"read block","completion") & STAT_ERR) {
                pi_disconnect(PI);
                if (pf_retries < PF_MAX_RETRIES) {
			pf_req_sense(unit,0);
                        pf_retries++;
                        pi_do_claimed(PI,do_pf_read_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pf_busy = 0;
		do_pf_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
            }
            pi_read_block(PI,pf_buf,512);
            pf_count--; pf_run--;
            pf_buf += 512;
	    pf_block++;
	    if (!pf_run) break;
	    if (!pf_count) pf_next_buf(unit);
        }
        pi_disconnect(PI);
	spin_lock_irqsave(&io_request_lock,saved_flags); 
        end_request(1);
        pf_busy = 0;
	do_pf_request(NULL);
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

static void do_pf_write( void )

{	ps_set_intr(do_pf_write_start,0,0,nice);
}

static void do_pf_write_start( void )

{       int	unit = pf_unit;
	unsigned long	saved_flags;

	pf_busy = 1;

	if (pf_start(unit,ATAPI_WRITE_10,pf_block,pf_run)) {
                pi_disconnect(PI);
                if (pf_retries < PF_MAX_RETRIES) {
                        pf_retries++;
                        pi_do_claimed(PI,do_pf_write_start);
			return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pf_busy = 0;
		do_pf_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }

	while (1) {
            if (pf_wait(unit,STAT_BUSY,STAT_DRQ|STAT_ERR,
			"write block","data wait") & STAT_ERR) {
                pi_disconnect(PI);
                if (pf_retries < PF_MAX_RETRIES) {
                        pf_retries++;
                        pi_do_claimed(PI,do_pf_write_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pf_busy = 0;
		do_pf_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
            }
            pi_write_block(PI,pf_buf,512);
	    pf_count--; pf_run--;
	    pf_buf += 512;
	    pf_block++;
	    if (!pf_run) break;
	    if (!pf_count) pf_next_buf(unit);
	}
	pf_mask = 0;
        ps_set_intr(do_pf_write_done,pf_ready,PF_TMO,nice);
}

static void do_pf_write_done( void )

{       int	unit = pf_unit;
	unsigned long	saved_flags;

        if (pf_wait(unit,STAT_BUSY,0,"write block","done") & STAT_ERR) {
                pi_disconnect(PI);
                if (pf_retries < PF_MAX_RETRIES) {
                        pf_retries++;
			pi_do_claimed(PI,do_pf_write_start);
                        return;
                }
		spin_lock_irqsave(&io_request_lock,saved_flags);
                end_request(0);
                pf_busy = 0;
		do_pf_request(NULL);
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
                return;
        }
        pi_disconnect(PI);
	spin_lock_irqsave(&io_request_lock,saved_flags);
        end_request(1);
        pf_busy = 0;
	do_pf_request(NULL);
	spin_unlock_irqrestore(&io_request_lock,saved_flags);
}

/* end of pf.c */

MODULE_LICENSE("GPL");

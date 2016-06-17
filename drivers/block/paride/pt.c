/* 
        pt.c    (c) 1998  Grant R. Guenther <grant@torque.net>
                          Under the terms of the GNU General Public License.

        This is the high-level driver for parallel port ATAPI tape
        drives based on chips supported by the paride module.

	The driver implements both rewinding and non-rewinding
	devices, filemarks, and the rewind ioctl.  It allocates
	a small internal "bounce buffer" for each open device, but
        otherwise expects buffering and blocking to be done at the
        user level.  As with most block-structured tapes, short
	writes are padded to full tape blocks, so reading back a file
        may return more data than was actually written.

        By default, the driver will autoprobe for a single parallel
        port ATAPI tape drive, but if their individual parameters are
        specified, the driver can handle up to 4 drives.

	The rewinding devices are named /dev/pt0, /dev/pt1, ...
	while the non-rewinding devices are /dev/npt0, /dev/npt1, etc.

        The behaviour of the pt driver can be altered by setting
        some parameters from the insmod command line.  The following
        parameters are adjustable:

            drive0      These four arguments can be arrays of       
            drive1      1-6 integers as follows:
            drive2
            drive3      <prt>,<pro>,<uni>,<mod>,<slv>,<dly>

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

                <slv>   ATAPI devices can be jumpered to master or slave.
                        Set this to 0 to choose the master drive, 1 to
                        choose the slave, -1 (the default) to choose the
                        first drive found.

                <dly>   some parallel ports require the driver to 
                        go more slowly.  -1 sets a default value that
                        should work with the chosen protocol.  Otherwise,
                        set this to a small integer, the larger it is
                        the slower the port i/o.  In some cases, setting
                        this to zero will speed up the device. (default -1)

	    major	You may use this parameter to overide the
			default major number (96) that this driver
			will use.  Be sure to change the device
			name as well.

	    name	This parameter is a character string that
			contains the name the kernel will use for this
			device (in /proc output, for instance).
			(default "pt").

            verbose     This parameter controls the amount of logging
                        that the driver will do.  Set it to 0 for
                        normal operation, 1 to see autoprobe progress
                        messages, or 2 to see additional debugging
                        output.  (default 0)
 
        If this driver is built into the kernel, you can use 
        the following command line parameters, with the same values
        as the corresponding module parameters listed above:

            pt.drive0
            pt.drive1
            pt.drive2
            pt.drive3

        In addition, you can use the parameter pt.disable to disable
        the driver entirely.

*/

/*   Changes:

	1.01	GRG 1998.05.06	Round up transfer size, fix ready_wait,
			        loosed interpretation of ATAPI standard
				for clearing error status.
				Eliminate sti();
	1.02    GRG 1998.06.16  Eliminate an Ugh.
	1.03    GRG 1998.08.15  Adjusted PT_TMO, use HZ in loop timing,
				extra debugging
	1.04    GRG 1998.09.24  Repair minor coding error, added jumbo support
	
*/

#define PT_VERSION      "1.04"
#define PT_MAJOR	96
#define PT_NAME		"pt"
#define PT_UNITS	4

/* Here are things one can override from the insmod command.
   Most are autoprobed by paride unless set here.  Verbose is on
   by default.

*/

static int	verbose = 0;
static int	major = PT_MAJOR;
static char	*name = PT_NAME;
static int      disable = 0;

static int drive0[6] = {0,0,0,-1,-1,-1};
static int drive1[6] = {0,0,0,-1,-1,-1};
static int drive2[6] = {0,0,0,-1,-1,-1};
static int drive3[6] = {0,0,0,-1,-1,-1};

static int (*drives[4])[6] = {&drive0,&drive1,&drive2,&drive3};
static int pt_drive_count;

#define D_PRT   0
#define D_PRO   1
#define D_UNI   2
#define D_MOD   3
#define D_SLV   4
#define D_DLY   5

#define DU              (*drives[unit])

/* end of parameters */


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mtio.h>
#include <linux/wait.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

#ifndef MODULE

#include "setup.h"

static STT pt_stt[5] = {{"drive0",6,drive0},
                        {"drive1",6,drive1},
                        {"drive2",6,drive2},
                        {"drive3",6,drive3},
			{"disable",1,&disable}};

void pt_setup( char *str, int *ints)

{       generic_setup(pt_stt,5,str);
}

#endif

MODULE_PARM(verbose,"i");
MODULE_PARM(major,"i");
MODULE_PARM(name,"s");
MODULE_PARM(drive0,"1-6i");
MODULE_PARM(drive1,"1-6i");
MODULE_PARM(drive2,"1-6i");
MODULE_PARM(drive3,"1-6i");

#include "paride.h"

#define PT_MAX_RETRIES  5
#define PT_TMO          3000            /* interrupt timeout in jiffies */
#define PT_SPIN_DEL     50              /* spin delay in micro-seconds  */
#define PT_RESET_TMO    30		/* 30 seconds */
#define PT_READY_TMO	60		/* 60 seconds */
#define PT_REWIND_TMO	1200		/* 20 minutes */

#define PT_SPIN         ((1000000/(HZ*PT_SPIN_DEL))*PT_TMO)  

#define STAT_ERR        0x00001
#define STAT_INDEX      0x00002
#define STAT_ECC        0x00004
#define STAT_DRQ        0x00008
#define STAT_SEEK       0x00010
#define STAT_WRERR      0x00020
#define STAT_READY      0x00040
#define STAT_BUSY       0x00080
#define STAT_SENSE	0x1f000

#define ATAPI_TEST_READY	0x00
#define ATAPI_REWIND		0x01
#define ATAPI_REQ_SENSE		0x03
#define ATAPI_READ_6		0x08
#define ATAPI_WRITE_6		0x0a
#define ATAPI_WFM		0x10
#define ATAPI_IDENTIFY		0x12
#define ATAPI_MODE_SENSE	0x1a
#define ATAPI_LOG_SENSE		0x4d

int pt_init(void);
#ifdef MODULE
void cleanup_module( void );
#endif

static int pt_open(struct inode *inode, struct file *file);
static int pt_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg);
static int pt_release (struct inode *inode, struct file *file);
static ssize_t pt_read(struct file * filp, char * buf, 
                       size_t count, loff_t *ppos);
static ssize_t pt_write(struct file * filp, const char * buf, 
                        size_t count, loff_t *ppos);
static int pt_detect(void);

static int pt_identify (int unit);

/* bits in PT.flags */

#define PT_MEDIA	1
#define PT_WRITE_OK	2
#define PT_REWIND	4
#define PT_WRITING      8
#define PT_READING     16
#define PT_EOF	       32

#define PT_NAMELEN      8
#define PT_BUFSIZE  16384

struct pt_unit {
	struct pi_adapter pia;    /* interface to paride layer */
	struct pi_adapter *pi;
	int flags;        	  /* various state flags */
	int last_sense;		  /* result of last request sense */
	int drive;		  /* drive */
	int access;               /* count of active opens ... */
	int bs;			  /* block size */
	int capacity;             /* Size of tape in KB */
	int present;		  /* device present ? */
	char *bufptr;
	char name[PT_NAMELEN];	  /* pf0, pf1, ... */
	};

struct pt_unit pt[PT_UNITS];

/*  'unit' must be defined in all functions - either as a local or a param */

#define PT pt[unit]
#define PI PT.pi

static char pt_scratch[512];            /* scratch block buffer */

/* kernel glue structures */

static struct file_operations pt_fops = {
	owner:		THIS_MODULE,
	read:		pt_read,
	write:		pt_write,
	ioctl:		pt_ioctl,
	open:		pt_open,
	release:	pt_release,
};

void pt_init_units( void )

{       int     unit, j;

        pt_drive_count = 0;
        for (unit=0;unit<PT_UNITS;unit++) {
                PT.pi = & PT.pia;
                PT.access = 0;
                PT.flags = 0;
		PT.last_sense = 0;
                PT.present = 0;
		PT.bufptr = NULL;
		PT.drive = DU[D_SLV];
                j = 0;
                while ((j < PT_NAMELEN-2) && (PT.name[j]=name[j])) j++;
                PT.name[j++] = '0' + unit;
                PT.name[j] = 0;
                if (DU[D_PRT]) pt_drive_count++;
        }
} 

static devfs_handle_t devfs_handle;

int pt_init (void)      /* preliminary initialisation */

{       int unit;

	if (disable) return -1;

	pt_init_units();

	if (pt_detect()) return -1;

	if (devfs_register_chrdev(major,name,&pt_fops)) {
                printk("pt_init: unable to get major number %d\n",
                        major);
	        for (unit=0;unit<PT_UNITS;unit++)
        	  if (PT.present) pi_release(PI);
                return -1;
        }

	devfs_handle = devfs_mk_dir (NULL, "pt", NULL);
	devfs_register_series (devfs_handle, "%u", 4, DEVFS_FL_DEFAULT,
			       major, 0, S_IFCHR | S_IRUSR | S_IWUSR,
			       &pt_fops, NULL);
	devfs_register_series (devfs_handle, "%un", 4, DEVFS_FL_DEFAULT,
			       major, 128, S_IFCHR | S_IRUSR | S_IWUSR,
			       &pt_fops, NULL);
        return 0;
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

        err = pt_init();

        return err;
}

void    cleanup_module(void)

{	int unit;

	devfs_unregister (devfs_handle);
	devfs_unregister_chrdev(major,name);

	for (unit=0;unit<PT_UNITS;unit++)
	  if (PT.present) pi_release(PI);
}

#endif

#define	WR(c,r,v)	pi_write_regr(PI,c,r,v)
#define	RR(c,r)		(pi_read_regr(PI,c,r))

#define DRIVE           (0xa0+0x10*PT.drive)

static int pt_wait( int unit, int go, int stop, char * fun, char * msg )

{       int j, r, e, s, p;

        j = 0;
        while ((((r=RR(1,6))&go)||(stop&&(!(r&stop))))&&(j++<PT_SPIN))
                udelay(PT_SPIN_DEL);

        if ((r&(STAT_ERR&stop))||(j>=PT_SPIN)) {
           s = RR(0,7);
           e = RR(0,1);
           p = RR(0,2);
           if (j >= PT_SPIN) e |= 0x100;
           if (fun) printk("%s: %s %s: alt=0x%x stat=0x%x err=0x%x"
                           " loop=%d phase=%d\n",
                            PT.name,fun,msg,r,s,e,j,p);
           return (e<<8)+s;
        }
        return 0;
}

static int pt_command( int unit, char * cmd, int dlen, char * fun )

{       pi_connect(PI);

        WR(0,6,DRIVE);

        if (pt_wait(unit,STAT_BUSY|STAT_DRQ,0,fun,"before command")) {
                pi_disconnect(PI);
                return -1;
        }

        WR(0,4,dlen % 256);
        WR(0,5,dlen / 256);
        WR(0,7,0xa0);          /* ATAPI packet command */

        if (pt_wait(unit,STAT_BUSY,STAT_DRQ,fun,"command DRQ")) {
                pi_disconnect(PI);
                return -1;
        }

        if (RR(0,2) != 1) {
           printk("%s: %s: command phase error\n",PT.name,fun);
           pi_disconnect(PI);
           return -1;
        }

        pi_write_block(PI,cmd,12);

        return 0;
}

static int pt_completion( int unit, char * buf, char * fun )

{       int r, s, n, p;

        r = pt_wait(unit,STAT_BUSY,STAT_DRQ|STAT_READY|STAT_ERR,
			fun,"completion");

        if (RR(0,7)&STAT_DRQ) { 
           n = (((RR(0,4)+256*RR(0,5))+3)&0xfffc);
	   p = RR(0,2)&3;
	   if (p == 0) pi_write_block(PI,buf,n);
	   if (p == 2) pi_read_block(PI,buf,n);
        }

        s = pt_wait(unit,STAT_BUSY,STAT_READY|STAT_ERR,fun,"data done");

        pi_disconnect(PI); 

        return (r?r:s);
}

static void pt_req_sense( int unit, int quiet )

{       char    rs_cmd[12] = { ATAPI_REQ_SENSE,0,0,0,16,0,0,0,0,0,0,0 };
        char    buf[16];
        int     r;

        r = pt_command(unit,rs_cmd,16,"Request sense");
        mdelay(1);
        if (!r) pt_completion(unit,buf,"Request sense");

	PT.last_sense = -1;
        if (!r) {
	    if (!quiet) printk("%s: Sense key: %x, ASC: %x, ASQ: %x\n",
                                    PT.name,buf[2]&0xf,buf[12],buf[13]);
	    PT.last_sense = (buf[2]&0xf) | ((buf[12]&0xff)<<8)
					 | ((buf[13]&0xff)<<16) ;
	} 
}

static int pt_atapi( int unit, char * cmd, int dlen, char * buf, char * fun )

{       int r;

        r = pt_command(unit,cmd,dlen,fun);
        mdelay(1);
        if (!r) r = pt_completion(unit,buf,fun);
        if (r) pt_req_sense(unit,!fun);
        
        return r;
}

static void pt_sleep( int cs )

{       current->state = TASK_INTERRUPTIBLE;
        schedule_timeout(cs);
}

static int pt_poll_dsc( int unit, int pause, int tmo, char *msg )

{	int	k, e, s;

	k = 0; e = 0; s = 0;
	while (k < tmo) {
		pt_sleep(pause);
		k++;
		pi_connect(PI);
		WR(0,6,DRIVE);
		s = RR(0,7);
		e = RR(0,1);
		pi_disconnect(PI);
		if (s & (STAT_ERR|STAT_SEEK)) break;
	}
	if ((k >= tmo) || (s & STAT_ERR)) {
	   if (k >= tmo) printk("%s: %s DSC timeout\n",PT.name,msg);
	     else printk("%s: %s stat=0x%x err=0x%x\n",PT.name,msg,s,e);
	   pt_req_sense(unit,0);
	   return 0;
	}
	return 1;
}

static void pt_media_access_cmd( int unit, int tmo, char *cmd, char *fun)

{	if (pt_command(unit,cmd,0,fun)) {
		pt_req_sense(unit,0);
		return;
	}
	pi_disconnect(PI);
	pt_poll_dsc(unit,HZ,tmo,fun);
}

static void pt_rewind( int unit )

{	char	rw_cmd[12] = {ATAPI_REWIND,0,0,0,0,0,0,0,0,0,0,0};

	pt_media_access_cmd(unit,PT_REWIND_TMO,rw_cmd,"rewind");
}

static void pt_write_fm( int unit )

{	char	wm_cmd[12] = {ATAPI_WFM,0,0,0,1,0,0,0,0,0,0,0};

        pt_media_access_cmd(unit,PT_TMO,wm_cmd,"write filemark");
}

#define DBMSG(msg)      ((verbose>1)?(msg):NULL)

static int pt_reset( int unit )

{	int	i, k, flg;
	int	expect[5] = {1,1,1,0x14,0xeb};

	pi_connect(PI);
	WR(0,6,DRIVE);
	WR(0,7,8);

	pt_sleep(20*HZ/1000);

        k = 0;
        while ((k++ < PT_RESET_TMO) && (RR(1,6)&STAT_BUSY))
                pt_sleep(HZ/10);

	flg = 1;
	for(i=0;i<5;i++) flg &= (RR(0,i+1) == expect[i]);

	if (verbose) {
		printk("%s: Reset (%d) signature = ",PT.name,k);
		for (i=0;i<5;i++) printk("%3x",RR(0,i+1));
		if (!flg) printk(" (incorrect)");
		printk("\n");
	}
	
	pi_disconnect(PI);
	return flg-1;	
}

static int pt_ready_wait( int unit, int tmo )

{	char	tr_cmd[12] = {ATAPI_TEST_READY,0,0,0,0,0,0,0,0,0,0,0};
	int	k, p;

	k = 0;
	while (k < tmo) {
	  PT.last_sense = 0;
	  pt_atapi(unit,tr_cmd,0,NULL,DBMSG("test unit ready"));
	  p = PT.last_sense;
	  if (!p) return 0;
	  if (!(((p & 0xffff) == 0x0402)||((p & 0xff) == 6))) return p;
	  k++;
          pt_sleep(HZ);
	}
	return 0x000020;	/* timeout */
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

static int xn( char *buf, int offs, int size )

{	int	v,k;

	v=0; 
	for(k=0;k<size;k++) v=v*256+(buf[k+offs]&0xff);
	return v;
}

static int pt_identify( int unit )

{	int 	dt, s;
	char	*ms[2] = {"master","slave"};
	char	mf[10], id[18];
	char    id_cmd[12] = { ATAPI_IDENTIFY,0,0,0,36,0,0,0,0,0,0,0};
        char    ms_cmd[12] = { ATAPI_MODE_SENSE,0,0x2a,0,36,0,0,0,0,0,0,0};
	char    ls_cmd[12] = { ATAPI_LOG_SENSE,0,0x71,0,0,0,0,0,36,0,0,0};
	char	buf[36];

        s = pt_atapi(unit,id_cmd,36,buf,"identify");
	if (s) return -1;

	dt = buf[0] & 0x1f;
	if (dt != 1) {
	  	if (verbose) 
		   printk("%s: Drive %d, unsupported type %d\n",
				PT.name,PT.drive,dt);
	  	return -1;
       	}

	xs(buf,mf,8,8);
	xs(buf,id,16,16);

	PT.flags = 0;
	PT.capacity = 0;
	PT.bs = 0;

	if (!pt_ready_wait(unit,PT_READY_TMO)) PT.flags |= PT_MEDIA;

        if (!pt_atapi(unit,ms_cmd,36,buf,"mode sense")) {
          if (!(buf[2] & 0x80)) PT.flags |= PT_WRITE_OK;
	  PT.bs = xn(buf,10,2);
	}

        if (!pt_atapi(unit,ls_cmd,36,buf,"log sense")) 
		PT.capacity = xn(buf,24,4);

        printk("%s: %s %s, %s",
		PT.name,mf,id,ms[PT.drive]);
        if (!(PT.flags & PT_MEDIA)) 
                printk(", no media\n");
        else {  if (!(PT.flags & PT_WRITE_OK)) printk(", RO");
                printk(", blocksize %d, %d MB\n",
		       PT.bs,PT.capacity/1024);
        }

	return 0;
}

static int pt_probe( int unit )

/*	returns  0, with id set if drive is detected
	        -1, if drive detection failed
*/

{	if (PT.drive == -1) {
	   for (PT.drive=0;PT.drive<=1;PT.drive++)
		if (!pt_reset(unit)) return pt_identify(unit);
	} else {
	   if (!pt_reset(unit)) return pt_identify(unit);
	}
        return -1; 
}

static int pt_detect( void )

{	int	k, unit;

	printk("%s: %s version %s, major %d\n",
		name,name,PT_VERSION,major);

	k = 0;
	if (pt_drive_count == 0) {
	    unit = 0;
	    if (pi_init(PI,1,-1,-1,-1,-1,-1,pt_scratch,
                        PI_PT,verbose,PT.name)) {
	        if (!pt_probe(unit)) {
			PT.present = 1;
			k++;
	        } else pi_release(PI);
	    }

	} else for (unit=0;unit<PT_UNITS;unit++) if (DU[D_PRT])
	    if (pi_init(PI,0,DU[D_PRT],DU[D_MOD],DU[D_UNI],
			DU[D_PRO],DU[D_DLY],pt_scratch,PI_PT,verbose,
			PT.name)) { 
                if (!pt_probe(unit)) {
                        PT.present = 1;
                        k++;
                } else pi_release(PI);
            }

	if (k) return 0;

	printk("%s: No ATAPI tape drive detected\n",name);
	return -1;
}

#define DEVICE_NR(dev)	(MINOR(dev) % 128)

static int pt_open (struct inode *inode, struct file *file)

{       int	unit = DEVICE_NR(inode->i_rdev);

        if ((unit >= PT_UNITS) || (!PT.present)) return -ENODEV;

        PT.access++;

	if (PT.access > 1) {
		PT.access--;
		return -EBUSY;
	}

	pt_identify(unit);

	if (!PT.flags & PT_MEDIA) {
		PT.access--;
		return -ENODEV;
		}

	if ((!PT.flags & PT_WRITE_OK) && (file ->f_mode & 2)) {
		PT.access--;
		return -EROFS;
		}

	if (!(MINOR(inode->i_rdev) & 128))
		PT.flags |= PT_REWIND;

	PT.bufptr = kmalloc(PT_BUFSIZE,GFP_KERNEL);
	if (PT.bufptr == NULL) {
		PT.access--;
		printk("%s: buffer allocation failed\n",PT.name);
		return -ENOMEM;
	}

        return 0;
}

static int pt_ioctl(struct inode *inode,struct file *file,
                    unsigned int cmd, unsigned long arg)
{
	int unit;
	struct mtop mtop;

        if (!inode || !inode->i_rdev)
		return -EINVAL;
        unit = DEVICE_NR(inode->i_rdev);
        if (unit >= PT_UNITS)
		return -EINVAL;
        if (!PT.present)
		return -ENODEV;

        switch (cmd) {
	    case MTIOCTOP:	
		if (copy_from_user((char *)&mtop, (char *)arg, 
			           sizeof(struct mtop))) return -EFAULT;

		switch (mtop.mt_op) {

		    case MTREW: 
			pt_rewind(unit);
			return 0;

		    case MTWEOF:
			pt_write_fm(unit);
			return 0;

		    default:	
			printk("%s: Unimplemented mt_op %d\n",PT.name,
					mtop.mt_op);
			return -EINVAL;
		}

            default:
		printk("%s: Unimplemented ioctl 0x%x\n",PT.name,cmd);
                return -EINVAL;

        }
}


static int pt_release (struct inode *inode, struct file *file)
{
        int	unit = DEVICE_NR(inode->i_rdev);

        if ((unit >= PT_UNITS) || (PT.access <= 0)) 
                return -EINVAL;

	lock_kernel();
	if (PT.flags & PT_WRITING) pt_write_fm(unit);

	if (PT.flags & PT_REWIND) pt_rewind(unit);	

	PT.access--;

	kfree(PT.bufptr);
	PT.bufptr = NULL;
	unlock_kernel();

	return 0;

}

static ssize_t pt_read(struct file * filp, char * buf, 
                       size_t count, loff_t *ppos)
{
  	struct 	inode *ino = filp->f_dentry->d_inode;
	int	unit = DEVICE_NR(ino->i_rdev);
	char	rd_cmd[12] = {ATAPI_READ_6,1,0,0,0,0,0,0,0,0,0,0};
	int	k, n, r, p, s, t, b;

	if (!(PT.flags & (PT_READING|PT_WRITING))) {
	    PT.flags |= PT_READING;
	    if (pt_atapi(unit,rd_cmd,0,NULL,"start read-ahead"))
			return -EIO;
	} else if (PT.flags & PT_WRITING) return -EIO;

	if (PT.flags & PT_EOF) return 0;

	t = 0;

	while (count > 0) {

	    if (!pt_poll_dsc(unit,HZ/100,PT_TMO,"read")) return -EIO;

	    n = count;
	    if (n > 32768) n = 32768;   /* max per command */
	    b = (n-1+PT.bs)/PT.bs;
	    n = b*PT.bs;		/* rounded up to even block */

	    rd_cmd[4] = b;

	    r = pt_command(unit,rd_cmd,n,"read");

	    mdelay(1);

	    if (r) {
	        pt_req_sense(unit,0);
	        return -EIO;
	    }

	    while (1) {

	        r = pt_wait(unit,STAT_BUSY,STAT_DRQ|STAT_ERR|STAT_READY,
                                           DBMSG("read DRQ"),"");

	        if (r & STAT_SENSE) {
	            pi_disconnect(PI);
		    pt_req_sense(unit,0);
		    return -EIO;
	        }

	        if (r) PT.flags |= PT_EOF; 

	        s = RR(0,7);

	        if (!(s & STAT_DRQ)) break;

	    	n = (RR(0,4)+256*RR(0,5));
	    	p = (RR(0,2)&3);
	    	if (p != 2) {
		    pi_disconnect(PI);
		    printk("%s: Phase error on read: %d\n",PT.name,p);
		    return -EIO;
	    	}

	        while (n > 0) {
		    k = n;
		    if (k > PT_BUFSIZE) k = PT_BUFSIZE; 
		    pi_read_block(PI,PT.bufptr,k);
		    n -= k;
		    b = k;
		    if (b > count) b = count;
		    if (copy_to_user(buf + t, PT.bufptr, b)) {
	    		pi_disconnect(PI);
			return -EFAULT;
		    }
		    t += b;
		    count -= b;
	        }

	    }
	    pi_disconnect(PI);
	    if (PT.flags & PT_EOF) break;
	}

	return t;

}

static ssize_t pt_write(struct file * filp, const char * buf, 
                        size_t count, loff_t *ppos)
{
        struct inode *ino = filp->f_dentry->d_inode;
        int unit = DEVICE_NR(ino->i_rdev);
        char    wr_cmd[12] = {ATAPI_WRITE_6,1,0,0,0,0,0,0,0,0,0,0};
        int     k, n, r, p, s, t, b;

	if (!(PT.flags & PT_WRITE_OK)) return -EROFS;

        if (!(PT.flags & (PT_READING|PT_WRITING))) {
            PT.flags |= PT_WRITING;
            if (pt_atapi(unit,wr_cmd,0,NULL,"start buffer-available mode"))
                        return -EIO;
        } else if (PT.flags&PT_READING) return -EIO;

	if (PT.flags & PT_EOF) return -ENOSPC;

	t = 0;

	while (count > 0) {

	    if (!pt_poll_dsc(unit,HZ/100,PT_TMO,"write")) return -EIO;

            n = count;
            if (n > 32768) n = 32768;	/* max per command */
            b = (n-1+PT.bs)/PT.bs;
            n = b*PT.bs;                /* rounded up to even block */

            wr_cmd[4] = b;

            r = pt_command(unit,wr_cmd,n,"write");

            mdelay(1);

            if (r) {			/* error delivering command only */
                pt_req_sense(unit,0);
                return -EIO;
            }

	    while (1) {

                r = pt_wait(unit,STAT_BUSY,STAT_DRQ|STAT_ERR|STAT_READY,
			                        DBMSG("write DRQ"),NULL);

                if (r & STAT_SENSE) {
                    pi_disconnect(PI);
                    pt_req_sense(unit,0);
                    return -EIO;
                }

                if (r) PT.flags |= PT_EOF;

	        s = RR(0,7);

	        if (!(s & STAT_DRQ)) break;

                n = (RR(0,4)+256*RR(0,5));
                p = (RR(0,2)&3);
                if (p != 0) {
                    pi_disconnect(PI);
                    printk("%s: Phase error on write: %d \n",PT.name,p);
                    return -EIO;
                }

                while (n > 0) {
		    k = n;
		    if (k > PT_BUFSIZE) k = PT_BUFSIZE;
		    b = k;
		    if (b > count) b = count;
		    if (copy_from_user(PT.bufptr, buf + t, b)) {
			pi_disconnect(PI);
			return -EFAULT;
		    }
                    pi_write_block(PI,PT.bufptr,k);
		    t += b;
		    count -= b;
		    n -= k;
                }

	    }
	    pi_disconnect(PI);
	    if (PT.flags & PT_EOF) break;
	}

	return t;
}

/* end of pt.c */

MODULE_LICENSE("GPL");

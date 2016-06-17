/* 
	pg.c    (c) 1998  Grant R. Guenther <grant@torque.net>
			  Under the terms of the GNU General Public License.

	The pg driver provides a simple character device interface for
	sending ATAPI commands to a device.  With the exception of the
	ATAPI reset operation, all operations are performed by a pair
	of read and write operations to the appropriate /dev/pgN device.
	A write operation delivers a command and any outbound data in
	a single buffer.  Normally, the write will succeed unless the
	device is offline or malfunctioning, or there is already another
	command pending.  If the write succeeds, it should be followed
	immediately by a read operation, to obtain any returned data and
	status information.  A read will fail if there is no operation
	in progress.

	As a special case, the device can be reset with a write operation,
	and in this case, no following read is expected, or permitted.

	There are no ioctl() operations.  Any single operation
	may transfer at most PG_MAX_DATA bytes.  Note that the driver must
	copy the data through an internal buffer.  In keeping with all
	current ATAPI devices, command packets are assumed to be exactly
	12 bytes in length.

	To permit future changes to this interface, the headers in the
	read and write buffers contain a single character "magic" flag.
	Currently this flag must be the character "P".

	By default, the driver will autoprobe for a single parallel
	port ATAPI device, but if their individual parameters are
	specified, the driver can handle up to 4 devices.

	To use this device, you must have the following device 
	special files defined:

		/dev/pg0 c 97 0
		/dev/pg1 c 97 1
		/dev/pg2 c 97 2
		/dev/pg3 c 97 3

	(You'll need to change the 97 to something else if you use
	the 'major' parameter to install the driver on a different
	major number.)

	The behaviour of the pg driver can be altered by setting
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
			default major number (97) that this driver
			will use.  Be sure to change the device
			name as well.

	    name	This parameter is a character string that
			contains the name the kernel will use for this
			device (in /proc output, for instance).
			(default "pg").

	    verbose     This parameter controls the amount of logging
			that is done by the driver.  Set it to 0 for 
			quiet operation, to 1 to enable progress
			messages while the driver probes for devices,
			or to 2 for full debug logging.  (default 0)

	If this driver is built into the kernel, you can use 
	the following command line parameters, with the same values
	as the corresponding module parameters listed above:

	    pg.drive0
	    pg.drive1
	    pg.drive2
	    pg.drive3

	In addition, you can use the parameter pg.disable to disable
	the driver entirely.

*/

/* Changes:

	1.01	GRG 1998.06.16	Bug fixes
	1.02    GRG 1998.09.24  Added jumbo support

*/

#define PG_VERSION      "1.02"
#define PG_MAJOR	97
#define PG_NAME		"pg"
#define PG_UNITS	4

#ifndef PI_PG
#define PI_PG	4
#endif

/* Here are things one can override from the insmod command.
   Most are autoprobed by paride unless set here.  Verbose is 0
   by default.

*/

static int	verbose = 0;
static int	major = PG_MAJOR;
static char	*name = PG_NAME;
static int      disable = 0;

static int drive0[6] = {0,0,0,-1,-1,-1};
static int drive1[6] = {0,0,0,-1,-1,-1};
static int drive2[6] = {0,0,0,-1,-1,-1};
static int drive3[6] = {0,0,0,-1,-1,-1};

static int (*drives[4])[6] = {&drive0,&drive1,&drive2,&drive3};
static int pg_drive_count;

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
#include <linux/pg.h>
#include <linux/wait.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

#ifndef MODULE

#include "setup.h"

static STT pg_stt[5] = {{"drive0",6,drive0},
			{"drive1",6,drive1},
			{"drive2",6,drive2},
			{"drive3",6,drive3},
			{"disable",1,&disable}};

void pg_setup( char *str, int *ints)

{       generic_setup(pg_stt,5,str);
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

#define PG_SPIN_DEL     50              /* spin delay in micro-seconds  */
#define PG_SPIN         200
#define PG_TMO		HZ
#define PG_RESET_TMO	10*HZ

#define STAT_ERR        0x01
#define STAT_INDEX      0x02
#define STAT_ECC        0x04
#define STAT_DRQ        0x08
#define STAT_SEEK       0x10
#define STAT_WRERR      0x20
#define STAT_READY      0x40
#define STAT_BUSY       0x80

#define ATAPI_IDENTIFY		0x12

int pg_init(void);
#ifdef MODULE
void cleanup_module( void );
#endif

static int pg_open(struct inode *inode, struct file *file);
static int pg_release (struct inode *inode, struct file *file);
static ssize_t pg_read(struct file * filp, char * buf, 
		       size_t count, loff_t *ppos);
static ssize_t pg_write(struct file * filp, const char * buf, 
			size_t count, loff_t *ppos);
static int pg_detect(void);

static int pg_identify (int unit, int log);

#define PG_NAMELEN      8

struct pg_unit {
	struct pi_adapter pia;    /* interface to paride layer */
	struct pi_adapter *pi;
	int busy;        	  /* write done, read expected */
	int start;		  /* jiffies at command start */
	int dlen;		  /* transfer size requested */
	int timeout;		  /* timeout requested */
	int status;		  /* last sense key */
	int drive;		  /* drive */
	int access;               /* count of active opens ... */
	int present;		  /* device present ? */
	char *bufptr;
	char name[PG_NAMELEN];	  /* pg0, pg1, ... */
	};

struct pg_unit pg[PG_UNITS];

/*  'unit' must be defined in all functions - either as a local or a param */

#define PG pg[unit]
#define PI PG.pi

static char pg_scratch[512];            /* scratch block buffer */

/* kernel glue structures */

static struct file_operations pg_fops = {
	owner:		THIS_MODULE,
	read:		pg_read,
	write:		pg_write,
	open:		pg_open,
	release:	pg_release,
};

void pg_init_units( void )

{       int     unit, j;

	pg_drive_count = 0;
	for (unit=0;unit<PG_UNITS;unit++) {
		PG.pi = & PG.pia;
		PG.access = 0;
		PG.busy = 0;
		PG.present = 0;
		PG.bufptr = NULL;
		PG.drive = DU[D_SLV];
		j = 0;
		while ((j < PG_NAMELEN-2) && (PG.name[j]=name[j])) j++;
		PG.name[j++] = '0' + unit;
		PG.name[j] = 0;
		if (DU[D_PRT]) pg_drive_count++;
	}
} 

static devfs_handle_t devfs_handle;

int pg_init (void)      /* preliminary initialisation */

{       int unit;

	if (disable) return -1;

	pg_init_units();

	if (pg_detect()) return -1;

	if (devfs_register_chrdev(major,name,&pg_fops)) {
		printk("pg_init: unable to get major number %d\n",
			major);
		for (unit=0;unit<PG_UNITS;unit++)
		  if (PG.present) pi_release(PI);
		return -1;
	}
	devfs_handle = devfs_mk_dir (NULL, "pg", NULL);
	devfs_register_series (devfs_handle, "%u", 4, DEVFS_FL_DEFAULT,
			       major, 0, S_IFCHR | S_IRUSR | S_IWUSR,
			       &pg_fops, NULL);
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

	err = pg_init();

	return err;
}

void    cleanup_module(void)

{       int unit;

	devfs_unregister (devfs_handle);
	devfs_unregister_chrdev(major,name);

	for (unit=0;unit<PG_UNITS;unit++)
	  if (PG.present) pi_release(PI);
}

#endif

#define	WR(c,r,v)	pi_write_regr(PI,c,r,v)
#define	RR(c,r)		(pi_read_regr(PI,c,r))

#define DRIVE           (0xa0+0x10*PG.drive)

static void pg_sleep( int cs )

{       current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(cs);
}

static int pg_wait( int unit, int go, int stop, int tmo, char * msg )

{       int j, r, e, s, p;

	PG.status = 0;

	j = 0;
	while ((((r=RR(1,6))&go)||(stop&&(!(r&stop))))&&(time_before(jiffies,tmo))) {
		if (j++ < PG_SPIN) udelay(PG_SPIN_DEL);
		else pg_sleep(1);
	}

	if ((r&(STAT_ERR&stop))||time_after_eq(jiffies, tmo)) {
	   s = RR(0,7);
	   e = RR(0,1);
	   p = RR(0,2);
	   if (verbose > 1)
	     printk("%s: %s: stat=0x%x err=0x%x phase=%d%s\n",
		   PG.name,msg,s,e,p,time_after_eq(jiffies, tmo)?" timeout":"");


	   if (time_after_eq(jiffies, tmo)) e |= 0x100;
	   PG.status = (e >> 4) & 0xff;
	   return -1;
	}
	return 0;
}

static int pg_command( int unit, char * cmd, int dlen, int tmo )

{       int k;

	pi_connect(PI);

	WR(0,6,DRIVE);

	if (pg_wait(unit,STAT_BUSY|STAT_DRQ,0,tmo,"before command")) {
		pi_disconnect(PI);
		return -1;
	}

	WR(0,4,dlen % 256);
	WR(0,5,dlen / 256);
	WR(0,7,0xa0);          /* ATAPI packet command */

	if (pg_wait(unit,STAT_BUSY,STAT_DRQ,tmo,"command DRQ")) {
		pi_disconnect(PI);
		return -1;
	}

	if (RR(0,2) != 1) {
	   printk("%s: command phase error\n",PG.name);
	   pi_disconnect(PI);
	   return -1;
	}

	pi_write_block(PI,cmd,12);

	if (verbose > 1) {
		printk("%s: Command sent, dlen=%d packet= ", PG.name,dlen);
		for (k=0;k<12;k++) printk("%02x ",cmd[k]&0xff);
		printk("\n");
	}
	return 0;
}

static int pg_completion( int unit, char * buf, int tmo)

{       int r, d, n, p;

	r = pg_wait(unit,STAT_BUSY,STAT_DRQ|STAT_READY|STAT_ERR,
			tmo,"completion");

	PG.dlen = 0;

	while (RR(0,7)&STAT_DRQ) {
	   d = (RR(0,4)+256*RR(0,5));
	   n = ((d+3)&0xfffc);
	   p = RR(0,2)&3;
	   if (p == 0) pi_write_block(PI,buf,n);
	   if (p == 2) pi_read_block(PI,buf,n);
	   if (verbose > 1) printk("%s: %s %d bytes\n",PG.name,
				    p?"Read":"Write",n);
	   PG.dlen += (1-p)*d;
	   buf += d;
	   r = pg_wait(unit,STAT_BUSY,STAT_DRQ|STAT_READY|STAT_ERR,
			tmo,"completion");
	}

	pi_disconnect(PI); 

	return r;
}

static int pg_reset( int unit )

{	int	i, k, flg;
	int	expect[5] = {1,1,1,0x14,0xeb};

	pi_connect(PI);
	WR(0,6,DRIVE);
	WR(0,7,8);

	pg_sleep(20*HZ/1000);

	k = 0;
	while ((k++ < PG_RESET_TMO) && (RR(1,6)&STAT_BUSY))
		pg_sleep(1);

	flg = 1;
	for(i=0;i<5;i++) flg &= (RR(0,i+1) == expect[i]);

	if (verbose) {
		printk("%s: Reset (%d) signature = ",PG.name,k);
		for (i=0;i<5;i++) printk("%3x",RR(0,i+1));
		if (!flg) printk(" (incorrect)");
		printk("\n");
	}
	
	pi_disconnect(PI);
	return flg-1;	
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

static int pg_identify( int unit, int log )

{	int 	s;
	char	*ms[2] = {"master","slave"};
	char	mf[10], id[18];
	char    id_cmd[12] = { ATAPI_IDENTIFY,0,0,0,36,0,0,0,0,0,0,0};
	char	buf[36];

	s = pg_command(unit,id_cmd,36,jiffies+PG_TMO);
	if (s) return -1;
	s = pg_completion(unit,buf,jiffies+PG_TMO);
	if (s) return -1;

	if (log) {
		xs(buf,mf,8,8);
		xs(buf,id,16,16);
		printk("%s: %s %s, %s\n",PG.name,mf,id,ms[PG.drive]);
	}

	return 0;
}

static int pg_probe( int unit )

/*	returns  0, with id set if drive is detected
		-1, if drive detection failed
*/

{	if (PG.drive == -1) {
	   for (PG.drive=0;PG.drive<=1;PG.drive++)
		if (!pg_reset(unit)) return pg_identify(unit,1);
	} else {
	   if (!pg_reset(unit)) return pg_identify(unit,1);
	}
	return -1; 
}

static int pg_detect( void )

{	int	k, unit;

	printk("%s: %s version %s, major %d\n",
		name,name,PG_VERSION,major);

	k = 0;
	if (pg_drive_count == 0) {
	    unit = 0;
	    if (pi_init(PI,1,-1,-1,-1,-1,-1,pg_scratch,
			PI_PG,verbose,PG.name)) {
		if (!pg_probe(unit)) {
			PG.present = 1;
			k++;
		} else pi_release(PI);
	    }

	} else for (unit=0;unit<PG_UNITS;unit++) if (DU[D_PRT])
	    if (pi_init(PI,0,DU[D_PRT],DU[D_MOD],DU[D_UNI],
			DU[D_PRO],DU[D_DLY],pg_scratch,PI_PG,verbose,
			PG.name)) { 
		if (!pg_probe(unit)) {
			PG.present = 1;
			k++;
		} else pi_release(PI);
	    }

	if (k) return 0;

	printk("%s: No ATAPI device detected\n",name);
	return -1;
}

#define DEVICE_NR(dev)	(MINOR(dev) % 128)

static int pg_open (struct inode *inode, struct file *file)

{       int	unit = DEVICE_NR(inode->i_rdev);

	if ((unit >= PG_UNITS) || (!PG.present)) return -ENODEV;

	PG.access++;

	if (PG.access > 1) {
		PG.access--;
		return -EBUSY;
	}

	if (PG.busy) {
		pg_reset(unit);
		PG.busy = 0;
	}

	pg_identify(unit,(verbose>1));


	PG.bufptr = kmalloc(PG_MAX_DATA,GFP_KERNEL);
	if (PG.bufptr == NULL) {
		PG.access--;
		printk("%s: buffer allocation failed\n",PG.name);
		return -ENOMEM;
	}

	return 0;
}

static int pg_release (struct inode *inode, struct file *file)
{
	int	unit = DEVICE_NR(inode->i_rdev);

	if ((unit >= PG_UNITS) || (PG.access <= 0)) 
		return -EINVAL;

	lock_kernel();
	PG.access--;

	kfree(PG.bufptr);
	PG.bufptr = NULL;
	unlock_kernel();

	return 0;

}

static ssize_t pg_write(struct file * filp, const char * buf, 
			size_t count, loff_t *ppos)

{       struct inode            *ino = filp->f_dentry->d_inode;
	int                     unit = DEVICE_NR(ino->i_rdev);
	struct pg_write_hdr     hdr;
	int                     hs = sizeof(hdr);

	if (PG.busy) return -EBUSY;
	if (count < hs) return -EINVAL;
	
	if (copy_from_user((char *)&hdr, buf, hs))
		return -EFAULT;

	if (hdr.magic != PG_MAGIC) return -EINVAL;
	if (hdr.dlen > PG_MAX_DATA) return -EINVAL;
	if ((count - hs) > PG_MAX_DATA) return -EINVAL;

	if (hdr.func == PG_RESET) {
		if (count != hs) return -EINVAL;
		if (pg_reset(unit)) return -EIO;
		return count;
	}

	if (hdr.func != PG_COMMAND) return -EINVAL;

	PG.start = jiffies;
	PG.timeout = hdr.timeout*HZ + HZ/2 + jiffies;

	if (pg_command(unit,hdr.packet,hdr.dlen,jiffies+PG_TMO)) {
		if (PG.status & 0x10) return -ETIME;
		return -EIO;
	}

	PG.busy = 1;

	if (copy_from_user(PG.bufptr, buf + hs, count - hs))
		return -EFAULT;
	return count;
}

static ssize_t pg_read(struct file * filp, char * buf, 
		       size_t count, loff_t *ppos)

{  	struct inode 		*ino = filp->f_dentry->d_inode;
	int			unit = DEVICE_NR(ino->i_rdev);
	struct pg_read_hdr 	hdr;
	int			hs = sizeof(hdr);
	int			copy;

	if (!PG.busy) return -EINVAL;
	if (count < hs) return -EINVAL;

	PG.busy = 0;

	if (pg_completion(unit,PG.bufptr,PG.timeout))
	  if (PG.status & 0x10) return -ETIME;

	hdr.magic = PG_MAGIC;
	hdr.dlen = PG.dlen;
	copy = 0;

	if (hdr.dlen < 0) {
		hdr.dlen = -1 * hdr.dlen;
		copy = hdr.dlen;
		if (copy > (count - hs)) copy = count - hs;
	}

	hdr.duration = (jiffies - PG.start + HZ/2) / HZ;
	hdr.scsi = PG.status & 0x0f;

	if (copy_to_user(buf, (char *)&hdr, hs))
		return -EFAULT;
	if (copy > 0)
		if (copy_to_user(buf+hs,PG.bufptr,copy))
			return -EFAULT;
	return copy+hs;
}

/* end of pg.c */

MODULE_LICENSE("GPL");

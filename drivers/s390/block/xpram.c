
/*
 * Xpram.c -- the S/390 expanded memory RAM-disk
 *           
 * significant parts of this code are based on
 * the sbull device driver presented in
 * A. Rubini: Linux Device Drivers
 *
 * Author of XPRAM specific coding: Reinhard Buendgen
 *                                  buendgen@de.ibm.com
 *
 * External interfaces:
 *   Interfaces to linux kernel
 *        xpram_setup: read kernel parameters   (see init/main.c)
 *        xpram_init:  initialize device driver (see drivers/block/ll_rw_blk.c)
 *   Module interfaces
 *        init_module
 *        cleanup_module
 *   Device specific file operations
 *        xpram_iotcl
 *        xpram_open
 *        xpram_release
 *
 * "ad-hoc" partitioning:         
 *    the expanded memory can be partitioned among several devices 
 *    (with different minors). The partitioning set up can be
 *    set by kernel or module parameters (int devs & int sizes[])
 *
 *    module parameters: devs= and sizes=
 *    kernel parameters: xpram_parts=
 *      note: I did not succeed in parsing numbers 
 *            for module parameters of type string "s" ?!?
 *
 * Other kenel files/modules affected(gerp for "xpram" or "XPRAM":
 *    drivers/s390/Config.in
 *    drivers/s390/block/Makefile
 *    include/linux/blk.h
 *    include/linux/major.h
 *    init/main.c
 *    drivers/block//ll_rw_blk.c
 *
 *
 * Potential future improvements:
 *   request clustering: first coding started not yet tested or integrated
 *                       I doubt that it really pays off 
 *   generic hard disk support to replace ad-hoc partitioning
 *
 * Tested with 2.2.14 (under VM)
 */

#ifdef MODULE
#  ifndef __KERNEL__
#    define __KERNEL__
#  endif
#  define __NO_VERSION__ /* don't define kernel_version in module.h */
#endif /* MODULE */

#include <linux/module.h>
#include <linux/version.h>

#ifdef MODULE
char kernel_version [] = UTS_RELEASE; 
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
#  define XPRAM_VERSION 24
#else
#  define XPRAM_VERSION 22
#endif 

#if (XPRAM_VERSION == 24)
#  include <linux/config.h>
#  include <linux/init.h>
#endif /* V24 */
#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#if (XPRAM_VERSION == 24)
#  include <linux/devfs_fs_kernel.h>
#endif /* V24 */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/timer.h>
#include <linux/types.h>  /* size_t */
#include <linux/ctype.h>  /* isdigit, isxdigit */
#include <linux/fcntl.h>  /* O_ACCMODE */
#include <linux/hdreg.h>  /* HDIO_GETGEO */

#include <asm/system.h>   /* cli(), *_flags */
#include <asm/uaccess.h>  /* put_user */

#if (XPRAM_VERSION == 24)
#define MAJOR_NR xpram_major /* force definitions on in blk.h */
int xpram_major;   /* must be declared before including blk.h */
devfs_handle_t xpram_devfs_handle;

#define DEVICE_NR(device) MINOR(device)   /* xpram has no partition bits */
#define DEVICE_NAME "xpram"               /* name for messaging */
#define DEVICE_INTR xpram_intrptr         /* pointer to the bottom half */
#define DEVICE_NO_RANDOM                  /* no entropy to contribute */
#define DEVICE_OFF(d)                     /* do-nothing */

#include <linux/blk.h>

#include "xpram.h"        /* local definitions */

__setup("xpram_parts=", xpram_setup);
#endif /* V24 */

/*
   define the debug levels:
   - 0 No debugging output to console or syslog
   - 1 Log internal errors to syslog, ignore check conditions 
   - 2 Log internal errors and check conditions to syslog
   - 3 Log internal errors to console, log check conditions to syslog
   - 4 Log internal errors and check conditions to console
   - 5 panic on internal errors, log check conditions to console
   - 6 panic on both, internal errors and check conditions
 */
#define XPRAM_DEBUG 4

#define PRINTK_HEADER XPRAM_NAME

#if XPRAM_DEBUG > 0
#define PRINT_DEBUG(x...) printk ( KERN_DEBUG PRINTK_HEADER "debug:" x )
#define PRINT_INFO(x...) printk ( KERN_INFO PRINTK_HEADER "info:" x )
#define PRINT_WARN(x...) printk ( KERN_WARNING PRINTK_HEADER "warning:" x )
#define PRINT_ERR(x...) printk ( KERN_ERR PRINTK_HEADER "error:" x )
#define PRINT_FATAL(x...) panic ( PRINTK_HEADER "panic:"x )
#else
#define PRINT_DEBUG(x...) printk ( KERN_DEBUG PRINTK_HEADER "debug:"  x )
#define PRINT_INFO(x...) printk ( KERN_DEBUG PRINTK_HEADER "info:" x )
#define PRINT_WARN(x...) printk ( KERN_DEBUG PRINTK_HEADER "warning:" x )
#define PRINT_ERR(x...) printk ( KERN_DEBUG PRINTK_HEADER "error:" x )
#define PRINT_FATAL(x...) printk ( KERN_DEBUG PRINTK_HEADER "panic:" x )
#endif	

#if (XPRAM_VERSION == 22)
#define MAJOR_NR xpram_major /* force definitions on in blk.h */
int xpram_major;   /* must be declared before including blk.h */

#define DEVICE_NR(device) MINOR(device)   /* xpram has no partition bits */
#define DEVICE_NAME "xpram"               /* name for messaging */
#define DEVICE_INTR xpram_intrptr         /* pointer to the bottom half */
#define DEVICE_NO_RANDOM                  /* no entropy to contribute */


#define DEVICE_OFF(d) /* do-nothing */

#define DEVICE_REQUEST *xpram_dummy_device_request  /* dummy function variable 
						     * to prevent warnings 
						     */#include <linux/blk.h>

#include "xpram.h"        /* local definitions */
#endif /* V22 */

/*
 * Non-prefixed symbols are static. They are meant to be assigned at
 * load time. Prefixed symbols are not static, so they can be used in
 * debugging. They are hidden anyways by register_symtab() unless
 * XPRAM_DEBUG is defined.
 */

static int major    = XPRAM_MAJOR;
static int devs     = XPRAM_DEVS;
static int rahead   = XPRAM_RAHEAD;
static int sizes[XPRAM_MAX_DEVS] = { 0, };
static int blksize  = XPRAM_BLKSIZE;
static int hardsect = XPRAM_HARDSECT;

int xpram_devs, xpram_rahead;
int xpram_blksize, xpram_hardsect;
int xpram_mem_avail = 0;
unsigned int xpram_sizes[XPRAM_MAX_DEVS];


MODULE_PARM(devs,"i");
MODULE_PARM(sizes,"1-" __MODULE_STRING(XPRAM_MAX_DEVS) "i"); 

MODULE_PARM_DESC(devs, "number of devices (\"partitions\"), " \
		 "the default is " __MODULE_STRING(XPRAM_DEVS) "\n");
MODULE_PARM_DESC(sizes, "list of device (partition) sizes " \
		 "the defaults are 0s \n" \
		 "All devices with size 0 equally partition the "
		 "remaining space on the expanded strorage not "
		 "claimed by explicit sizes\n");
MODULE_LICENSE("GPL");


/* The following items are obtained through kmalloc() in init_module() */

Xpram_Dev *xpram_devices = NULL;
int *xpram_blksizes = NULL;
int *xpram_hardsects = NULL;
int *xpram_offsets = NULL;   /* partition offsets */

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* 
 *              compute nearest multiple of 4 , argument must be non-negative
 *              the macros used depends on XPRAM_KB_IN_PG = 4 
 */

#define NEXT4(x) ((x & 0x3) ? (x+4-(x &0x3)) : (x))   /* increment if needed */
#define LAST4(x) ((x & 0x3) ? (x-4+(x & 0x3)) : (x))  /* decrement if needed */

#if 0               /* this is probably not faster than the previous code */
#define NEXT4(x)   ((((x-1)>>2)>>2)+4)             /* increment if needed */
#define LAST4(x)   (((x+3)>>2)<<2)                 /* decrement if needed */
#endif

/* integer formats */
#define XPRAM_INVALF -1    /* invalid     */
#define XPRAM_HEXF    0    /* hexadecimal */
#define XPRAM_DECF    1    /* decimal     */

/* 
 *    parsing operations (needed for kernel parameter parsing)
 */

/* -------------------------------------------------------------------------
 * sets the string pointer after the next comma 
 *
 * argument:    strptr pointer to string
 * side effect: strptr points to endof string or to position of the next 
 *              comma 
 * ------------------------------------------------------------------------*/
static void
xpram_scan_to_next_comma (char **strptr)
{
	while ( ((**strptr) != ',') && (**strptr) )
		(*strptr)++;
}

/* -------------------------------------------------------------------------
 * interpret character as hex-digit
 *
 * argument: c charcter
 * result: c interpreted as hex-digit
 * note: can be used to read digits for any base <= 16
 * ------------------------------------------------------------------------*/
static int
xpram_get_hexdigit (char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	if ((c >= 'a') && (c <= 'f'))
		return c + 10 - 'a';
	if ((c >= 'A') && (c <= 'F'))
		return c + 10 - 'A';
	return -1;
}

/*--------------------------------------------------------------------------
 * Check format of unsigned integer
 *
 * Argument: strptr pointer to string 
 * result:   -1 if strptr does not start with a digit 
 *                (does not start an integer)
 *           0  if strptr starts a positive hex-integer with "0x" 
 *           1  if strptr start a positive decimal integer
 *
 * side effect: if strptr start a positive hex-integer then strptr is
 *              set to the character after the "0x"
 *-------------------------------------------------------------------------*/
static int
xpram_int_format(char **strptr)
{
	if ( !isdigit(**strptr) )
		return XPRAM_INVALF;
	if ( (**strptr == '0') 
	     && ( (*((*strptr)+1) == 'x') || (*((*strptr) +1) == 'X') ) 
	     && isxdigit(*((*strptr)+2)) ) {
		*strptr=(*strptr)+2;
		return XPRAM_HEXF;
	} else return XPRAM_DECF;
}

/*--------------------------------------------------------------------------
 * Read non-negative decimal integer
 *
 * Argument: strptr pointer to string starting with a non-negative integer
 *           in decimal format
 * result:   the value of theinitial integer pointed to by strptr
 *
 * side effect: strptr is set to the first character following the integer
 *-------------------------------------------------------------------------*/

static int
xpram_read_decint (char ** strptr)
{
	int res=0;
	while ( isdigit(**strptr) ) {
		res = (res*10) + xpram_get_hexdigit(**strptr);
		(*strptr)++;
	}
	return res;
}

/*--------------------------------------------------------------------------
 * Read non-negative hex-integer
 *
 * Argument: strptr pointer to string starting with a non-negative integer
 *           in hexformat (without "0x" prefix)
 * result:   the value of the initial integer pointed to by strptr
 *
 * side effect: strptr is set to the first character following the integer
 *-------------------------------------------------------------------------*/

static int
xpram_read_hexint (char ** strptr)
{
	int res=0;
	while ( isxdigit(**strptr) ) {
		res = (res<<4) + xpram_get_hexdigit(**strptr);
		(*strptr)++;
	}
	return res;
}
/*--------------------------------------------------------------------------
 * Read non-negative integer
 *
 * Argument: strptr pointer to string starting with a non-negative integer
             (either in decimal- or in hex-format
 * result:   the value of the initial integer pointed to by strptr
 *           in case of a parsing error the result is -EINVAL
 *
 * side effect: strptr is set to the first character following the integer
 *-------------------------------------------------------------------------*/

static int
xpram_read_int (char ** strptr)
{
	switch (  xpram_int_format(strptr) ) {
	case XPRAM_INVALF: return -EINVAL;
	case XPRAM_HEXF:   return xpram_read_hexint(strptr);
	case XPRAM_DECF:   return xpram_read_decint(strptr);
	default: return -EINVAL;
	}
}

/*--------------------------------------------------------------------------
 * Read size
 *
 * Argument: strptr pointer to string starting with a non-negative integer
 *           followed optionally by a size modifier:
 *             k or K for kilo (default),
 *             m or M for mega
 *             g or G for giga
 * result:   the value of the initial integer pointed to by strptr
 *           multiplied by the modifier value devided by 1024
 *           in case of a parsing error the result is -EINVAL
 *
 * side effect: strptr is set to the first character following the size
 *-------------------------------------------------------------------------*/

static int
xpram_read_size (char ** strptr)
{
	int res;
  
	res=xpram_read_int(strptr);
	if ( res < 0 )return res;
	switch ( **strptr ) {
	case 'g':
	case 'G': res=res*1024;
	case 'm':
	case 'M': res=res*1024;
	case 'k' :
	case 'K' : (* strptr)++;
	}
  
	return res;
}


/*--------------------------------------------------------------------------
 * Read tail of comma separated size list  ",i1,i2,...,in"
 *
 * Arguments:strptr pointer to string. It is assumed that the string has
 *                  the format (","<size>)*
 *           maxl integer describing the maximal number of elements in the
                  list pointed to by strptr, max must be > 0.
 *           ilist array of dimension >= maxl of integers to be modified
 *
 * result:   -EINVAL if the list is longer than maxl
 *           0 otherwise
 *
 * side effects: for j=1,...,n ilist[ij] is set to the value of ij if it is
 *               a valid non-negative integer and to -EINVAL otherwise
 *               if no comma is found where it is expected an entry in
 *               ilist is set to -EINVAL
 *-------------------------------------------------------------------------*/
static int
xpram_read_size_list_tail (char ** strptr, int maxl, int * ilist)
{ 
	int i=0;
	char *str = *strptr;
	int res=0;

	while ( (*str == ',') && (i < maxl) ) {
		str++;      
		ilist[i] = xpram_read_size(&str);
		if ( ilist[i] == -EINVAL ) {
			xpram_scan_to_next_comma(&str);
			res = -EINVAL;
		}
		i++;
	}
	return res;
#if 0  /* be lenient about trailing stuff */
	if ( *str != 0 && *str != ' ' ) {
		ilist[MAX(i-1,0)] = -EINVAL;
		return -EINVAL;
	} else return 0;
#endif
}


/*
 *   expanded memory operations
 */


/*--------------------------------------------------------------------*/
/* Copy expanded memory page (4kB) into main memory                   */
/* Arguments                                                          */
/*           page_addr:    address of target page                     */
/*           xpage_index:  index of expandeded memory page            */
/* Return value                                                       */
/*           0:            if operation succeeds                      */
/*           non-0:       otherwise                                   */
/*--------------------------------------------------------------------*/
long xpram_page_in (unsigned long page_addr, unsigned long xpage_index)
{
	int cc=0;
	unsigned long real_page_addr = __pa(page_addr);
#ifndef CONFIG_ARCH_S390X
	__asm__ __volatile__ (
		"   lr  1,%1         \n"   /* r1 = real_page_addr            */
		"   lr  2,%2         \n"   /* r2 = xpage_index               */
		"   .long 0xb22e0012 \n"   /* pgin r1,r2                     */
		/* copy page from expanded memory */
		"0: ipm  %0          \n"   /* save status (cc & program mask */
		"   srl  %0,28       \n"   /* cc into least significant bits */
                "1:                  \n"   /* we are done                    */
                ".section .fixup,\"ax\"\n" /* start of fix up section        */
                "2: lhi    %0,2      \n"   /* return unused condition code 2 */
                "   bras 1,3f        \n"   /* safe label 1: in r1 and goto 3 */
                "   .long 1b         \n"   /* literal containing label 1     */
                "3: l    1,0(1)      \n"   /* load label 1 address into r1   */
                "   br   1           \n"   /* goto label 1 (across sections) */
                ".previous           \n"   /* back in text section           */
                ".section __ex_table,\"a\"\n" /* start __extable             */
                "   .align 4         \n"
                "   .long 0b,2b      \n"   /* failure point 0, fixup code 2  */
                ".previous           \n"
		: "=d" (cc) : "d" (real_page_addr), "d" (xpage_index) : "cc", "1", "2"
		);
#else /* CONFIG_ARCH_S390X */
	__asm__ __volatile__ (
		"   lgr  1,%1        \n"   /* r1 = real_page_addr            */
		"   lgr  2,%2        \n"   /* r2 = xpage_index               */
		"   .long 0xb22e0012 \n"   /* pgin r1,r2                     */
		/* copy page from expanded memory */
		"0: ipm  %0          \n"   /* save status (cc & program mask */
		"   srl  %0,28       \n"   /* cc into least significant bits */
                "1:                  \n"   /* we are done                    */
                ".section .fixup,\"ax\"\n" /* start of fix up section        */
                "2: lghi %0,2        \n"   /* return unused condition code 2 */
                "   jg   1b          \n"   /* goto label 1 above             */
                ".previous           \n"   /* back in text section           */
                ".section __ex_table,\"a\"\n" /* start __extable             */
                "   .align 8         \n"
                "   .quad 0b,2b      \n"   /* failure point 0, fixup code 2  */
                ".previous           \n"
		: "=d" (cc) : "d" (real_page_addr), "d" (xpage_index) : "cc", "1", "2"
		);
#endif /* CONFIG_ARCH_S390X */
	switch (cc) {
	case 0: return 0;
	case 1: return -EIO;
        case 2: return -ENXIO;
	case 3: return -ENXIO;
	default: return -EIO;  /* should not happen */
	};
}

/*--------------------------------------------------------------------*/
/* Copy a 4kB page of main memory to an expanded memory page          */
/* Arguments                                                          */
/*           page_addr:    address of source page                     */
/*           xpage_index:  index of expandeded memory page            */
/* Return value                                                       */
/*           0:            if operation succeeds                      */
/*           non-0:        otherwise                                  */
/*--------------------------------------------------------------------*/
long xpram_page_out (unsigned long page_addr, unsigned long xpage_index)
{
	int cc=0;
	unsigned long real_page_addr = __pa(page_addr);
#ifndef CONFIG_ARCH_S390X
	__asm__ __volatile__ (
		"  lr  1,%1        \n"   /* r1 = mem_page                  */
		"  lr  2,%2        \n"   /* r2 = rpi                       */
		" .long 0xb22f0012 \n"   /* pgout r1,r2                    */
                                /* copy page from expanded memory */
		"0: ipm  %0        \n"   /* save status (cc & program mask */
                " srl  %0,28       \n"   /* cc into least significant bits */
                "1:                  \n"   /* we are done                    */
                ".section .fixup,\"ax\"\n" /* start of fix up section        */
                "2: lhi   %0,2       \n"   /* return unused condition code 2 */
                "   bras 1,3f        \n"   /* safe label 1: in r1 and goto 3 */
                "   .long 1b         \n"   /* literal containing label 1     */
                "3: l    1,0(1)      \n"   /* load label 1 address into r1   */
                "   br   1           \n"   /* goto label 1 (across sections) */
                ".previous           \n"   /* back in text section           */
                ".section __ex_table,\"a\"\n" /* start __extable             */
                "   .align 4         \n"
                "   .long 0b,2b      \n"   /* failure point 0, fixup code 2  */
                ".previous           \n"
		: "=d" (cc) : "d" (real_page_addr), "d" (xpage_index) : "cc", "1", "2"
		);
#else /* CONFIG_ARCH_S390X */
	__asm__ __volatile__ (
		"  lgr  1,%1       \n"   /* r1 = mem_page                  */
		"  lgr  2,%2       \n"   /* r2 = rpi                       */
		" .long 0xb22f0012 \n"   /* pgout r1,r2                    */
                                         /* copy page from expanded memory */
		"0: ipm  %0        \n"   /* save status (cc & program mask */
                "  srl  %0,28      \n"   /* cc into least significant bits */
                "1:                \n"   /* we are done                    */
                ".section .fixup,\"ax\"\n" /* start of fix up section      */
                "2: lghi %0,2      \n"   /* return unused condition code 2 */
                "   jg   1b        \n"   /* goto label 1 above             */
                ".previous         \n"   /* back in text section           */
                ".section __ex_table,\"a\"\n" /* start __extable           */
                "   .align 8       \n"
                "   .quad 0b,2b    \n"   /* failure point 0, fixup code 2  */
                ".previous         \n"
		: "=d" (cc) : "d" (real_page_addr), "d" (xpage_index) : "cc", "1", "2"
		);
#endif  /* CONFIG_ARCH_S390X */
	switch (cc) {
	case 0: return 0;
	case 1: return -EIO;
        case 2: { PRINT_ERR("expanded storage lost!\n"); return -ENXIO; }
	case 3: return -ENXIO;
	default: return -EIO;  /* should not happen */
        }
}

/*--------------------------------------------------------------------*/
/* Measure expanded memory                                            */
/* Return value                                                       */
/*           size of expanded memory in kB (must be a multipe of 4)   */
/*--------------------------------------------------------------------*/
int xpram_size(void)
{
	int cc=0;  
        unsigned long base=0;
	unsigned long po, pi, rpi;   /* page index order, page index */

	unsigned long mem_page = __get_free_page(GFP_KERNEL);

	/* for po=0,1,2,... try to move in page number base+(2^po)-1 */
	pi=1;   
	for (po=0; po <= 32; po++) { /* pi = 2^po */
		cc=xpram_page_in(mem_page,base+pi-1);
		if ( cc ) break;
		pi <<= 1;  
	}
	if ( cc && (po < 31 ) ) {
                pi >>=1;
		base += pi;
		pi >>=1;
		for ( ; pi > 0; pi >>= 1) {
			rpi = pi - 1;
			cc=xpram_page_in(mem_page,base+rpi);
			if ( !cc ) base += pi;
		}
	}
	
	free_page (mem_page);

	if ( cc && (po < 31) ) 
		return (XPRAM_KB_IN_PG * base);
	else          /* return maximal value possible */
		return INT_MAX;
}

/*
 * Open and close
 */

int xpram_open (struct inode *inode, struct file *filp)
{
	Xpram_Dev *dev; /* device information */
	int num = MINOR(inode->i_rdev);


	if (num >= xpram_devs) return -ENODEV;
	dev = xpram_devices + num;

	PRINT_DEBUG("calling xpram_open for device %d\n",num);
        PRINT_DEBUG("  size %dkB, name %s, usage: %d\n", 
                     dev->size,dev->device_name, atomic_read(&(dev->usage)));

	atomic_inc(&(dev->usage));
	return 0;          /* success */
}

int xpram_release (struct inode *inode, struct file *filp)
{
	Xpram_Dev *dev = xpram_devices + MINOR(inode->i_rdev);

	PRINT_DEBUG("calling xpram_release for device %d (size %dkB, usage: %d)\n",MINOR(inode->i_rdev) ,dev->size,atomic_read(&(dev->usage)));

	/*
	 * If the device is closed for the last time, start a timer
	 * to release RAM in half a minute. The function and argument
	 * for the timer have been setup in init_module()
	 */
	if (!atomic_dec_return(&(dev->usage))) {
		/* but flush it right now */
		/* Everything is already flushed by caller -- AV */
	}
	return(0);
}


/*
 * The ioctl() implementation
 */

int xpram_ioctl (struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	int err, size;
	struct hd_geometry *geo = (struct hd_geometry *)arg;

	PRINT_DEBUG("ioctl 0x%x 0x%lx\n", cmd, arg);
	switch(cmd) {

	case BLKGETSIZE:  /* 0x1260 */
		/* Return the device size, expressed in sectors */
		return put_user( 1024* xpram_sizes[MINOR(inode->i_rdev)]
                           / XPRAM_SOFTSECT,
			   (unsigned long *) arg);

	case BLKGETSIZE64:
		return put_user( (u64)(1024* xpram_sizes[MINOR(inode->i_rdev)]
                           / XPRAM_SOFTSECT) << 9,
			   (u64 *) arg);

	case BLKFLSBUF: /* flush, 0x1261 */
		fsync_dev(inode->i_rdev);
		if ( capable(CAP_SYS_ADMIN) )invalidate_buffers(inode->i_rdev);
		return 0;

	case BLKRAGET: /* return the readahead value, 0x1263 */
		if (!arg)  return -EINVAL;
		err = 0; /* verify_area_20(VERIFY_WRITE, (long *) arg, sizeof(long));
		          * if (err) return err;
                          */
		put_user(read_ahead[MAJOR(inode->i_rdev)], (long *)arg);

		return 0;

	case BLKRASET: /* set the readahead value, 0x1262 */
		if (!capable(CAP_SYS_ADMIN)) return -EACCES;
		if (arg > 0xff) return -EINVAL; /* limit it */
		read_ahead[MAJOR(inode->i_rdev)] = arg;
                atomic_eieio();
		return 0;

	case BLKRRPART: /* re-read partition table: can't do it, 0x1259 */
		return -EINVAL;


#if (XPRAM_VERSION == 22)
		RO_IOCTLS(inode->i_rdev, arg); /* the default RO operations 
                                                * BLKROSET
						* BLKROGET
                                                */
#endif /* V22 */

	case HDIO_GETGEO:
		/*
		 * get geometry: we have to fake one...  trim the size to a
		 * multiple of 64 (32k): tell we have 16 sectors, 4 heads,
		 * whatever cylinders. Tell also that data starts at sector. 4.
		 */
		size = xpram_mem_avail * 1024 / XPRAM_SOFTSECT;
		/* size = xpram_mem_avail * 1024 / xpram_hardsect; */
		size &= ~0x3f; /* multiple of 64 */
		if (geo==NULL) return -EINVAL;
                /* 
                 * err=verify_area_20(VERIFY_WRITE, geo, sizeof(*geo));
		 * if (err) return err;
                 */

		put_user(size >> 6, &geo->cylinders);
		put_user(        4, &geo->heads);
		put_user(       16, &geo->sectors);
		put_user(        4, &geo->start);

		return 0;
	}

	return -EINVAL; /* unknown command */
}

/*
 * The file operations
 */

#if (XPRAM_VERSION == 22)
struct file_operations xpram_fops = {
	NULL,          /* lseek: default */
	block_read,
	block_write,
	NULL,          /* xpram_readdir */
	NULL,          /* xpram_select */
	xpram_ioctl,
	NULL,          /* xpram_mmap */
	xpram_open,
	NULL,          /* flush */
	xpram_release,
	block_fsync,
	NULL,          /* xpram_fasync */
        NULL,
        NULL
};
#endif /* V22 */

#if (XPRAM_VERSION == 24)
struct block_device_operations xpram_devops =
{
	owner:   THIS_MODULE,
	ioctl:   xpram_ioctl,
	open:    xpram_open,
	release: xpram_release,
};
#endif /* V24 */

/*
 * Block-driver specific functions
 */

void xpram_request(request_queue_t * queue)
{
	Xpram_Dev *device;
	/*     u8 *ptr;          */
	/*    int size;          */

	unsigned long page_no;         /* expanded memory page number */
	unsigned long sects_to_copy;   /* number of sectors to be copied */
        char * buffer;                 /* local pointer into buffer cache */
	int dev_no;                    /* device number of request */
	int fault;                     /* faulty access to expanded memory */
#if ( XPRAM_VERSION == 24 )	
        struct request * current_req;      /* working request */
#else 
#       define current_req CURRENT
#endif /* V24 */

	while(1) {
		INIT_REQUEST;

		fault=0;
#if ( XPRAM_VERSION == 24 )
		current_req = blkdev_entry_next_request (&queue->queue_head);
#endif /* V24 */
		dev_no = DEVICE_NR(current_req->rq_dev); 
		/* Check if the minor number is in range */
		if ( dev_no > xpram_devs ) {
			static int count = 0;
			if (count++ < 5) /* print the message at most five times */
				PRINT_WARN(" request for unknown device\n");
			end_request(0);
			continue;
		}

		/* pointer to device structure, from the global array */
		device = xpram_devices + dev_no;   
		sects_to_copy = current_req->current_nr_sectors;
                /* does request exceed size of device ? */
		if ( XPRAM_SEC2KB(sects_to_copy) > xpram_sizes[dev_no] ) {
			PRINT_WARN(" request past end of device\n");
			end_request(0);
			continue;
		}

                /* Does request start at page boundery? -- paranoia */
#if 0
		PRINT_DEBUG(" req %lx, sect %lx, to copy %lx, buf addr %lx\n", (unsigned long) current_req, current_req->sector, sects_to_copy, (unsigned long) current_req->buffer);
#endif
                buffer = current_req->buffer;
#if XPRAM_SEC_IN_PG != 1
                /* Does request start at an expanded storage page boundery? */
                if ( current_req->sector &  (XPRAM_SEC_IN_PG - 1) ) {
			PRINT_WARN(" request does not start at an expanded storage page boundery\n");
			PRINT_WARN(" referenced sector: %ld\n",current_req->sector);
			end_request(0);
			continue;
		}
		/* Does request refere to partial expanded storage pages? */
                if ( sects_to_copy & (XPRAM_SEC_IN_PG - 1) ) {
			PRINT_WARN(" request referes to a partial expanded storage page\n");
			end_request(0);
			continue;
		}
#endif /*  XPRAM_SEC_IN_PG != 1 */
		/* Is request buffer aligned with kernel pages? */
		if ( ((unsigned long)buffer) & (XPRAM_PGSIZE-1) ) {
			PRINT_WARN(" request buffer is not aligned with kernel pages\n");
			end_request(0);
			continue;
		}

                /* which page of expanded storage is affected first? */
		page_no = (xpram_offsets[dev_no] >> XPRAM_KB_IN_PG_ORDER)
			+ (current_req->sector >> XPRAM_SEC_IN_PG_ORDER); 

#if 0 
		PRINT_DEBUG("request: %d ( dev %d, copy %d sectors, at page %d ) \n", current_req->cmd,dev_no,sects_to_copy,page_no);
#endif

		switch(current_req->cmd) {
		case READ:
			do {
				if ( (fault=xpram_page_in((unsigned long)buffer,page_no)) ) {
					PRINT_WARN("xpram(dev %d): page in failed for page %ld.\n",dev_no,page_no);
					break;
				}
				sects_to_copy -= XPRAM_SEC_IN_PG;
                                buffer += XPRAM_PGSIZE;
				page_no++;
			} while ( sects_to_copy > 0 );
			break;
		case WRITE:
			do {
				if ( (fault=xpram_page_out((unsigned long)buffer,page_no)) 
					) {
					PRINT_WARN("xpram(dev %d): page out failed for page %ld.\n",dev_no,page_no);
					break;
				}
				sects_to_copy -= XPRAM_SEC_IN_PG;
				buffer += XPRAM_PGSIZE;
				page_no++;
			} while ( sects_to_copy > 0 );
			break;
		default:
			/* can't happen */
			end_request(0);
			continue;
		}
		if ( fault ) end_request(0);
		else end_request(1); /* success */
	}
}

/*
 *    Kernel interfaces
 */

/*
 * Parses the kernel parameters given in the kernel parameter line.
 * The expected format is 
 *           <number_of_partitions>[","<partition_size>]*
 * where 
 *           devices is a positive integer that initializes xpram_devs
 *           each size is a non-negative integer possibly followed by a
 *           magnitude (k,K,m,M,g,G), the list of sizes initialises 
 *           xpram_sizes
 *
 * Arguments
 *           str: substring of kernel parameter line that contains xprams
 *                kernel parameters. 
 *           ints: not used -- not in Version > 2.3 any more
 *
 * Result    0 on success, -EINVAl else -- only for Version > 2.3
 *
 * Side effects
 *           the global variabls devs is set to the value of 
 *           <number_of_partitions> and sizes[i] is set to the i-th
 *           partition size (if provided). A parsing error of a value
 *           results in this value being set to -EINVAL.
 */
#if (XPRAM_VERSION == 22)
void xpram_setup (char *str, int *ints)
#else 
int xpram_setup (char *str)
#endif /* V22 */
{
	devs = xpram_read_int(&str);
	if ( devs != -EINVAL ) 
	  if ( xpram_read_size_list_tail(&str,devs,sizes) < 0 ) {
			PRINT_ERR("error while reading xpram parameters.\n");
#if (XPRAM_VERSION == 24)
			return -EINVAL;
#endif /* V24 */
			  }
#if (XPRAM_VERSION == 24)
	  else return 0;
	else return -EINVAL;
#elif (XPRAM_VERSION == 22)
	return; 
#endif /* V24/V22 */
}

/*
 * initialize xpram device driver
 *
 * Result: 0 ok
 *         negative number: negative error code
 */

int xpram_init(void)
{
	int result, i;
	int mem_usable;       /* net size of expanded memory */
	int mem_needed=0;     /* size of expanded memory needed to fullfill
			       * requirements of non-zero parameters in sizes
			       */

	int mem_auto_no=0;    /* number of (implicit) zero parameters in sizes */
	int mem_auto;         /* automatically determined device size          */
#if (XPRAM_VERSION == 24)
	int minor_length;     /* store the length of a minor (w/o '\0') */
        int minor_thresh;     /* threshhold for minor lenght            */

        request_queue_t *q;   /* request queue */
#endif /* V24 */

				/*
				 * Copy the (static) cfg variables to public prefixed ones to allow
				 * snoozing with a debugger.
				 */

	xpram_rahead   = rahead;
	xpram_blksize  = blksize;
	xpram_hardsect = hardsect;

	PRINT_INFO("initializing: %s\n","");
				/* check arguments */
	xpram_major    = major;
	if ( (devs <= 0) || (devs > XPRAM_MAX_DEVS) ) {
		PRINT_ERR("invalid number %d of devices\n",devs);
                PRINT_ERR("Giving up xpram\n");
		return -EINVAL;
	}
	xpram_devs     = devs;
	for (i=0; i < xpram_devs; i++) {
		if ( sizes[i] < 0 ) {
			PRINT_ERR("Invalid partition size %d kB\n",xpram_sizes[i]);
                        PRINT_ERR("Giving up xpram\n");
			return -EINVAL;
		} else {
		  xpram_sizes[i] = NEXT4(sizes[i]);  /* page align */
			if ( sizes[i] ) mem_needed += xpram_sizes[i];
			else mem_auto_no++;
		}
	}

	PRINT_DEBUG("  major %d \n", xpram_major);
	PRINT_INFO("  number of devices (partitions): %d \n", xpram_devs);
	for (i=0; i < xpram_devs; i++) {
		if ( sizes[i] )
			PRINT_INFO("  size of partition %d: %d kB\n", i, xpram_sizes[i]);
		else
			PRINT_INFO("  size of partition %d to be set automatically\n",i);
	}
	PRINT_DEBUG("  memory needed (for sized partitions): %d kB\n", mem_needed);
	PRINT_DEBUG("  partitions to be sized automatically: %d\n", mem_auto_no);

#if 0
				/* Hardsect can't be changed :( */
                                /* I try it any way. Yet I must distinguish
                                 * between hardsects (to be changed to 4096)
                                 * and soft sectors, hard-coded for buffer 
                                 * sizes within the requests
                                 */
	if (hardsect != 512) {
		PRINT_ERR("Can't change hardsect size\n");
		hardsect = xpram_hardsect = 512;
	}
#endif
        PRINT_INFO("  hardsector size: %dB \n",xpram_hardsect);

	/*
	 * Register your major, and accept a dynamic number
	 */
#if (XPRAM_VERSION == 22)
	result = register_blkdev(xpram_major, "xpram", &xpram_fops);
#elif (XPRAM_VERSION == 24)
	result = devfs_register_blkdev(xpram_major, "xpram", &xpram_devops);
#endif /* V22/V24 */
	if (result < 0) {
		PRINT_ERR("Can't get major %d\n",xpram_major);
                PRINT_ERR("Giving up xpram\n");
		return result;
	}
#if (XPRAM_VERSION == 24)
	xpram_devfs_handle = devfs_mk_dir (NULL, "slram", NULL);
	devfs_register_series (xpram_devfs_handle, "%u", XPRAM_MAX_DEVS,
			       DEVFS_FL_DEFAULT, XPRAM_MAJOR, 0,
			       S_IFBLK | S_IRUSR | S_IWUSR,
			       &xpram_devops, NULL);
#endif /* V22/V24 */
	if (xpram_major == 0) xpram_major = result; /* dynamic */
	major = xpram_major; /* Use `major' later on to save typing */

	result = -ENOMEM; /* for the possible errors */

	/* 
	 * measure expanded memory
	 */

	xpram_mem_avail = xpram_size();
	if (!xpram_mem_avail) {
		PRINT_ERR("No or not enough expanded memory available\n");
                PRINT_ERR("Giving up xpram\n");
		result = -ENODEV;
		goto fail_malloc;
	}
	PRINT_INFO("  %d kB expanded memory found.\n",xpram_mem_avail );

	/*
	 * Assign the other needed values: request, rahead, size, blksize,
	 * hardsect. All the minor devices feature the same value.
	 * Note that `xpram' defines all of them to allow testing non-default
	 * values. A real device could well avoid setting values in global
	 * arrays if it uses the default values.
	 */

#if (XPRAM_VERSION == 22)
	blk_dev[major].request_fn = xpram_request;
#elif (XPRAM_VERSION == 24)
	q = BLK_DEFAULT_QUEUE (major);
	blk_init_queue (q, xpram_request);
	blk_queue_headactive (BLK_DEFAULT_QUEUE (major), 0);
#endif /* V22/V24 */
	read_ahead[major] = xpram_rahead;

	/* we want to have XPRAM_UNUSED blocks security buffer between devices */
	mem_usable=xpram_mem_avail-(XPRAM_UNUSED*(xpram_devs-1));
	if ( mem_needed > mem_usable ) {
		PRINT_ERR("Not enough expanded memory available\n");
                PRINT_ERR("Giving up xpram\n");
		goto fail_malloc;
	}

	/*
	 * partitioning:
	 * xpram_sizes[i] != 0; partition i has size xpram_sizes[i] kB
	 * else:             ; all partitions i with xpram_sizesxpram_size[i] 
	 *                     partition equally the remaining space
	 */

	if ( mem_auto_no ) {
		mem_auto=LAST4((mem_usable-mem_needed)/mem_auto_no);
		PRINT_INFO("  automatically determined partition size: %d kB\n", mem_auto);
		for (i=0; i < xpram_devs; i++) 
			if (xpram_sizes[i] == 0) xpram_sizes[i] = mem_auto;
	}
	blk_size[major]=xpram_sizes;

	xpram_offsets = kmalloc(xpram_devs * sizeof(int), GFP_KERNEL);
	if (!xpram_offsets) {
		PRINT_ERR("Not enough memory for xpram_offsets\n");
                PRINT_ERR("Giving up xpram\n");
		goto fail_malloc;
	}
	xpram_offsets[0] = 0;
	for (i=1; i < xpram_devs; i++) 
		xpram_offsets[i] = xpram_offsets[i-1] + xpram_sizes[i-1] + XPRAM_UNUSED;

#if 0
	for (i=0; i < xpram_devs; i++)
		PRINT_DEBUG(" device(%d) offset = %d kB, size = %d kB\n",i, xpram_offsets[i], xpram_sizes[i]);
#endif

	xpram_blksizes = kmalloc(xpram_devs * sizeof(int), GFP_KERNEL);
	if (!xpram_blksizes) {
		PRINT_ERR("Not enough memory for xpram_blksizes\n");
                PRINT_ERR("Giving up xpram\n");
		goto fail_malloc_blksizes;
	}
	for (i=0; i < xpram_devs; i++) /* all the same blocksize */
		xpram_blksizes[i] = xpram_blksize;
	blksize_size[major]=xpram_blksizes;

	xpram_hardsects = kmalloc(xpram_devs * sizeof(int), GFP_KERNEL);
	if (!xpram_hardsects) {
		PRINT_ERR("Not enough memory for xpram_hardsects\n");
                PRINT_ERR("Giving up xpram\n");
		goto fail_malloc_hardsects;
	}
	for (i=0; i < xpram_devs; i++) /* all the same hardsect */
		xpram_hardsects[i] = xpram_hardsect;
	hardsect_size[major]=xpram_hardsects;
   
	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */

	xpram_devices = kmalloc(xpram_devs * sizeof (Xpram_Dev), GFP_KERNEL);
	if (!xpram_devices) {
		PRINT_ERR("Not enough memory for xpram_devices\n");
                PRINT_ERR("Giving up xpram\n");
		goto fail_malloc_devices;
	}
	memset(xpram_devices, 0, xpram_devs * sizeof (Xpram_Dev));
#if (XPRAM_VERSION == 24)
        minor_length = 1;
        minor_thresh = 10;
#endif /* V24 */
	for (i=0; i < xpram_devs; i++) {
		/* data and usage remain zeroed */
		xpram_devices[i].size = xpram_sizes[i];  /* size in kB not in bytes */
		atomic_set(&(xpram_devices[i].usage),0);
#if (XPRAM_VERSION == 24)
                if (i == minor_thresh) {
		  minor_length++;
		  minor_thresh *= 10;
		}
                xpram_devices[i].device_name = 
                  kmalloc(1 + strlen(XPRAM_DEVICE_NAME_PREFIX) + minor_length,GFP_KERNEL);
		if ( xpram_devices[i].device_name == NULL ) {
		  PRINT_ERR("Not enough memory for xpram_devices[%d].device_name\n",i);
                  PRINT_ERR("Giving up xpram\n");
		  goto fail_devfs_register;
		}
                sprintf(xpram_devices[i].device_name,XPRAM_DEVICE_NAME_PREFIX "%d",i);

	PRINT_DEBUG("initializing xpram_open for device %d\n",i);
        PRINT_DEBUG("  size %dkB, name %s, usage: %d\n", 
                     xpram_devices[i].size,xpram_devices[i].device_name, atomic_read(&(xpram_devices[i].usage)));

#if 0  /* WHY? */
                xpram_devices[i].devfs_entry =
		  devfs_register(NULL /* devfs root dir */,
                                 xpram_devices[i].device_name, 0,
                                 0 /* flags */,
				 XPRAM_MAJOR,i,
                                 0755 /* access mode */,
				 0 /* uid */, 0 /* gid */,
                                 &xpram_devops,
				 (void *) &(xpram_devices[i])
				 );
		if ( xpram_devices[i].devfs_entry == NULL ) {
		  PRINT_ERR("devfs system registry failed\n");
		  PRINT_ERR("Giving up xpram\n");
		  goto fail_devfs_register;
		}
#endif  /* WHY? */
#endif /* V24 */
				 
	}

	return 0; /* succeed */

	/* clean up memory in case of failures */
#if (XPRAM_VERSION == 24)
 fail_devfs_register:
        for (i=0; i < xpram_devs; i++) {
	  if ( xpram_devices[i].device_name )
	    kfree(xpram_devices[i].device_name);
	}
	kfree(xpram_devices);
#endif /* V24 */
 fail_malloc_blksizes:
	kfree (xpram_offsets);
 fail_malloc_hardsects:
	kfree (xpram_blksizes);
	blksize_size[major] = NULL;
 fail_malloc_devices:
	kfree(xpram_hardsects);
	hardsect_size[major] = NULL;
 fail_malloc:
	read_ahead[major] = 0;
#if (XPRAM_VERSION == 22)
	blk_dev[major].request_fn = NULL;
#endif /* V22 */
	/* ???	unregister_chrdev(major, "xpram"); */
	unregister_blkdev(major, "xpram");
	return result;
}

/*
 * Finally, the module stuff
 */

int init_module(void)
{
	int rc = 0;

	PRINT_INFO ("trying to load module\n");
	rc = xpram_init ();
	if (rc == 0) {
		PRINT_INFO ("Module loaded successfully\n");
	} else {
		PRINT_WARN ("Module load returned rc=%d\n", rc);
	}
	return rc;
}

void cleanup_module(void)
{
	int i;

				/* first of all, flush it all and reset all the data structures */


	for (i=0; i<xpram_devs; i++)
		fsync_dev(MKDEV(xpram_major, i)); /* flush the devices */

#if (XPRAM_VERSION == 22)
	blk_dev[major].request_fn = NULL;
#endif /* V22 */
	read_ahead[major] = 0;
	blk_size[major] = NULL;
	kfree(blksize_size[major]);
	blksize_size[major] = NULL;
	kfree(hardsect_size[major]);
	hardsect_size[major] = NULL;
	kfree(xpram_offsets);

				/* finally, the usual cleanup */
#if (XPRAM_VERSION == 22)
	unregister_blkdev(major, "xpram");
#elif (XPRAM_VERSION == 24)
	devfs_unregister(xpram_devfs_handle);
	if (devfs_unregister_blkdev(MAJOR_NR, "xpram"))
		printk(KERN_WARNING "xpram: cannot unregister blkdev\n");
#endif /* V22/V24 */
	kfree(xpram_devices);
}

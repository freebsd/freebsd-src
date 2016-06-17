/*
 *  drivers/s390/s390io.c
 *   S/390 common I/O routines
 *   $Revision: 1.258 $
 *
 *  S390 version
 *    Copyright (C) 1999, 2000 IBM Deutschland Entwicklung GmbH,
 *                             IBM Corporation
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 *               Cornelia Huck (cohuck@de.ibm.com) 
 *    ChangeLog: 01/07/2001 Blacklist cleanup (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *               01/04/2001 Holger Smolinski (smolinsk@de.ibm.com)
 *                          Fixed lost interrupts and do_adapter_IO
 *               xx/xx/xxxx nnn          multiple changes not reflected
 *               03/12/2001 Ingo Adlung  blacklist= - changed to cio_ignore=  
 *               03/14/2001 Ingo Adlung  disable interrupts before start_IO
 *                                        in Path Group processing 
 *                                       decrease retry2 on busy while 
 *                                        disabling sync_isc; reset isc_cnt
 *                                        on io error during sync_isc enablement
 *               05/09/2001 Cornelia Huck added exploitation of debug feature
 *               05/16/2001 Cornelia Huck added /proc/deviceinfo/<devno>/
 *               05/22/2001 Cornelia Huck added /proc/cio_ignore
 *                                        un-ignore blacklisted devices by piping 
 *                                        to /proc/cio_ignore
 *               xx/xx/xxxx some bugfixes & cleanups
 *               08/02/2001 Cornelia Huck not already known devices can be blacklisted
 *                                        by piping to /proc/cio_ignore
 *               09/xx/2001 couple more fixes
 *               10/15/2001 Cornelia Huck xsch - internal only for now
 *               10/29/2001 Cornelia Huck Blacklisting reworked again
 *               10/29/2001 Cornelia Huck improved utilization of debug feature
 *               10/29/2001 Cornelia Huck more work on cancel_IO - use the flag
 *                                        DOIO_CANCEL_ON_TIMEOUT in do_IO to get
 *                                        io cancelled
 *               11/15/2001 Cornelia Huck proper behaviour with procfs off
 *               12/10/2001 Cornelia Huck added private_data + functions to 
 *                                        ioinfo_t
 *               11-12/2001 Cornelia Huck various cleanups
 *               01/09/2002 Cornelia Huck PGID fixes
 *                                        process css machine checks 
 *               01/10/2002 Cornelia Huck added /proc/chpids
 *               04/10/2002 Cornelia Huck fixed reaction on css machine checks
 *               04/23/2002 Cornelia Huck fixed console isc (un)setting
 *               06/06/2002 Cornelia Huck added detection of locked devices
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/ctype.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/smp.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/processor.h>
#include <asm/lowcore.h>
#include <asm/idals.h>
#include <asm/uaccess.h>
#include <asm/cpcmd.h>

#include <asm/s390io.h>
#include <asm/s390dyn.h>
#include <asm/s390mach.h>
#include <asm/debug.h>
#include <asm/queue.h>

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define SANITY_CHECK(irq) do { \
if (irq > highest_subchannel || irq < 0) \
		return (-ENODEV); \
	if (ioinfo[irq] == INVALID_STORAGE_AREA) \
		return (-ENODEV); \
        if (ioinfo[irq]->st) \
                return -ENODEV; \
	} while(0)

#define CIO_TRACE_EVENT(imp, txt) do { \
	if (cio_debug_initialized) \
		debug_text_event(cio_debug_trace_id, \
				 imp, \
				 txt); \
        }while (0)

#define CIO_MSG_EVENT(imp, args...) do { \
        if (cio_debug_initialized) \
                debug_sprintf_event(cio_debug_msg_id, \
                                    imp, \
                                    ##args); \
        } while (0)

#define CIO_CRW_EVENT(imp, args...) do { \
        if (cio_debug_initialized) \
                debug_sprintf_event(cio_debug_crw_id, \
                                    imp, \
                                    ##args); \
        } while (0)

#define CIO_HEX_EVENT(imp, args...) do { \
	if (cio_debug_initialized) \
                debug_event(cio_debug_trace_id, imp, ##args); \
        } while (0)

#undef  CONFIG_DEBUG_IO
#define CONFIG_DEBUG_CRW
#define CONFIG_DEBUG_CHSC

unsigned int highest_subchannel;
ioinfo_t *ioinfo_head = NULL;
ioinfo_t *ioinfo_tail = NULL;
ioinfo_t *ioinfo[__MAX_SUBCHANNELS] = {
	[0 ... (__MAX_SUBCHANNELS - 1)] = INVALID_STORAGE_AREA
};

#ifdef CONFIG_CHSC
__u64 chpids[4] = {0,0,0,0};
__u64 chpids_logical[4] = {-1,-1,-1,-1};
__u64 chpids_known[4] = {0,0,0,0};
#endif /* CONFIG_CHSC */

static atomic_t sync_isc = ATOMIC_INIT (-1);
static int sync_isc_cnt = 0;	/* synchronous irq processing lock */

static spinlock_t adapter_lock = SPIN_LOCK_UNLOCKED;	/* adapter interrupt lock */
static int cons_dev = -1;	/* identify console device */
static int init_IRQ_complete = 0;
static int cio_show_msg = 0;
static schib_t *p_init_schib = NULL;
static irb_t *p_init_irb = NULL;
static __u64 irq_IPL_TOD;
static adapter_int_handler_t adapter_handler = NULL;
static pgid_t * global_pgid;

/* for use of debug feature */
debug_info_t *cio_debug_msg_id = NULL;
debug_info_t *cio_debug_trace_id = NULL;
debug_info_t *cio_debug_crw_id = NULL;
int cio_debug_initialized = 0;

#ifdef CONFIG_CHSC
int cio_chsc_desc_avail = 0;
int cio_chsc_err_msg = 0;
#endif

static void init_IRQ_handler (int irq, void *dev_id, struct pt_regs *regs);
static void s390_process_subchannels (void);
static void s390_device_recognition_all (void);
static void s390_device_recognition_irq (int irq);
#ifdef CONFIG_PROC_FS
static void s390_redo_validation (void);
#endif
static int s390_validate_subchannel (int irq, int enable);
static int s390_SenseID (int irq, senseid_t * sid, __u8 lpm);
static int s390_SetPGID (int irq, __u8 lpm);
static int s390_SensePGID (int irq, __u8 lpm, pgid_t * pgid);
static int s390_process_IRQ (unsigned int irq);
static int enable_subchannel (unsigned int irq);
static int disable_subchannel (unsigned int irq);
int cancel_IO (int irq);
int s390_start_IO (int irq, ccw1_t * cpa, unsigned long user_intparm,
		   __u8 lpm, unsigned long flag);

#ifdef CONFIG_PROC_FS
static int chan_proc_init (void);
#endif

static inline void do_adapter_IO (__u32 intparm);

static void s390_schedule_path_verification(unsigned long irq);
int s390_DevicePathVerification (int irq, __u8 domask);
int s390_register_adapter_interrupt (adapter_int_handler_t handler);
int s390_unregister_adapter_interrupt (adapter_int_handler_t handler);

extern int do_none (unsigned int irq, int cpu, struct pt_regs *regs);
extern int enable_none (unsigned int irq);
extern int disable_none (unsigned int irq);

asmlinkage void do_IRQ (struct pt_regs regs);

#ifdef CONFIG_CHSC
static chsc_area_t *chsc_area_ssd = NULL;
static chsc_area_t *chsc_area_sei = NULL;
static spinlock_t chsc_lock_ssd = SPIN_LOCK_UNLOCKED;
static spinlock_t chsc_lock_sei = SPIN_LOCK_UNLOCKED;
static int chsc_get_sch_descriptions( void );
int s390_vary_chpid( __u8 chpid, int on );
#endif

#ifdef CONFIG_PROC_FS
#define MAX_CIO_PROCFS_ENTRIES 0x300
/* magic number; we want to have some room to spare */

int cio_procfs_device_create (int devno);
int cio_procfs_device_remove (int devno);
int cio_procfs_device_purge (void);
#endif

int cio_notoper_msg = 1;

#ifdef CONFIG_PROC_FS
int cio_proc_devinfo = 0;	/* switch off the /proc/deviceinfo/ stuff by default
				   until problems are dealt with */
#endif

unsigned long s390_irq_count[NR_CPUS];	/* trace how many irqs have occured per cpu... */
int cio_count_irqs = 1;		/* toggle use here... */

int cio_sid_with_pgid = 0;     /* if we need a PGID for SenseID, switch this on */

/* 
 * "Blacklisting" of certain devices:
 * Device numbers given in the commandline as cio_ignore=... won't be known to Linux
 * These can be single devices or ranges of devices
 * 
 * 10/23/01 reworked to get rid of lists
 */

static u32 bl_dev[2048];

static spinlock_t blacklist_lock = SPIN_LOCK_UNLOCKED;
static int highest_ignored = 0;
static int nr_ignored = 0;

/* 
 * Function: blacklist_range_add
 * Blacklist the devices from-to
 */

static inline void
blacklist_range_add (int from, int to, int locked)
{

	unsigned long flags;
	int i;

	if ((to && (from > to))
	    || (to<0) || (to > 0xffff)
	    || (from<0) || (from > 0xffff))
		return;

	if (!locked)
		spin_lock_irqsave (&blacklist_lock, flags);

	if (!to)
		to = from;
	for (i = from; i <= to; i++) {
		if (!test_and_set_bit (i, &bl_dev))
			nr_ignored++;
	}

	if (to >= highest_ignored)
		highest_ignored = to;

	if (!locked)
		spin_unlock_irqrestore (&blacklist_lock, flags);
}

/* 
 * Function: blacklist_range_remove
 * Removes a range from the blacklist chain
 */

static inline void
blacklist_range_remove (int from, int to)
{
	long flags;
	int i;

	if ((to && (from > to))
	    || (to<0) || (to > 0xffff)
	    || (from<0) || (from > 0xffff))
		return;

	spin_lock_irqsave (&blacklist_lock, flags);

	for (i = from; i <= to; i++) {
		if (test_and_clear_bit (i, &bl_dev))
			nr_ignored--;
	}

	if (to == highest_ignored)
		for (highest_ignored = from; (highest_ignored > 0)
		     && (!test_bit (highest_ignored, &bl_dev));
		     highest_ignored--) ;

	spin_unlock_irqrestore (&blacklist_lock, flags);
}

/* Parsing the commandline for blacklist parameters */

/* 
 * Variable to hold the blacklisted devices given by the parameter line
 * cio_ignore=...
 */
char *blacklist[256] = { NULL, };

/*
 * Get the cio_ignore=... items from the parameter line
 */

static void
blacklist_split_parm_string (char *str)
{
	char *tmp = str;
	int count = 0;
	do {
		char *end;
		int len;
		end = strchr (tmp, ',');
		if (end == NULL) {
			len = strlen (tmp) + 1;
		} else {
			len = (long) end - (long) tmp + 1;
			*end = '\0';
			end++;
		}
		blacklist[count] = alloc_bootmem (len * sizeof (char));
		if (blacklist == NULL) {
			printk (KERN_WARNING
				"can't store cio_ignore= parameter no %d\n",
				count + 1);
			break;
		}
		memset (blacklist[count], 0, len * sizeof (char));
		memcpy (blacklist[count], tmp, len * sizeof (char));
		count++;
		tmp = end;
	} while (tmp != NULL && *tmp != '\0');
}

/*
 * The blacklist parameters as one concatenated string
 */

static char blacklist_parm_string[1024] __initdata = { 0, };

/* 
 * function: blacklist_strtoul
 * Strip leading '0x' and interpret the values as Hex
 */
static inline int
blacklist_strtoul (char *str, char **stra)
{
	if (*str == '0') {
		str++;		/* strip leading zero */
		if (*str == 'x')
			str++;	/* strip leading x */
	}
	return simple_strtoul (str, stra, 16);	/* interpret anything as hex */
}

/*
 * Function: blacklist_parse
 * Parse the parameters given to cio_ignore=... 
 * Add the blacklisted devices to the blacklist chain
 */

static inline void
blacklist_parse (char **str)
{
	char *temp;
	int from, to;

	while (*str) {
		temp = *str;
		from = 0;
		to = 0;

		from = blacklist_strtoul (temp, &temp);
		if (*temp == '-') {
			temp++;
			to = blacklist_strtoul (temp, &temp);
		}
		blacklist_range_add (from, to, 0);
#ifdef CONFIG_DEBUG_IO
		printk (KERN_INFO "Blacklisted range from %X to %X\n", from,
			to);
#endif
		str++;
	}
}

/*
 * Initialisation of blacklist 
 */

void __init
blacklist_init (void)
{
#ifdef CONFIG_DEBUG_IO
	printk (KERN_DEBUG "Reading blacklist...\n");
#endif
	CIO_MSG_EVENT(6, "Reading blacklist\n");

	blacklist_split_parm_string (blacklist_parm_string);
	blacklist_parse (blacklist);
}

/*
 * Get all the blacklist parameters from parameter line
 */

void __init
blacklist_setup (char *str, int *ints)
{
	int len = strlen (blacklist_parm_string);
	if (len != 0) {
		strcat (blacklist_parm_string, ",");
	}
	strcat (blacklist_parm_string, str);
}

int __init
blacklist_call_setup (char *str)
{
	int dummy;
#ifdef CONFIG_DEBUG_IO
	printk (KERN_DEBUG "Reading blacklist parameters...\n");
#endif
	CIO_MSG_EVENT(6, "Reading blacklist parameters\n");

	blacklist_setup (str, &dummy);

	/* Blacklist ranges must be ready when device recognition starts */
	blacklist_init ();

	return 1;
}

__setup ("cio_ignore=", blacklist_call_setup);

/* Checking if devices are blacklisted */

/*
 * Function: is_blacklisted
 * Returns 1 if the given devicenumber can be found in the blacklist, otherwise 0.
 */

static inline int
is_blacklisted (int devno)
{
	long flags;
	int retval = 0;

	spin_lock_irqsave (&blacklist_lock, flags);

	if (test_bit (devno, &bl_dev))
		retval = 1;

	spin_unlock_irqrestore (&blacklist_lock, flags);
	return retval;
}

/*
 * Function: blacklist_free_all_ranges
 * set all blacklisted devices free...
 */

void
blacklist_free_all_ranges (void)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave (&blacklist_lock, flags);

	for (i = 0; i <= highest_ignored; i++)
		clear_bit (i, &bl_dev);
	highest_ignored = 0;
	nr_ignored = 0;

	spin_unlock_irqrestore (&blacklist_lock, flags);
}

#ifdef CONFIG_PROC_FS
/*
 * Function: blacklist_parse_proc_parameters
 * parse the stuff which is piped to /proc/cio_ignore
 */
void
blacklist_parse_proc_parameters (char *buf)
{
	int i;
	int from = 0;
	int to = 0;
	long flags;
	int err = 0;

	if (strstr (buf, "free ")) {
		for (i = 0; i < 5; i++) {
			buf++;
		}
		if (strstr (buf, "all")) {
			blacklist_free_all_ranges ();
			s390_redo_validation ();
		} else {
			while (*buf != 0 && *buf != '\n') {
				if (!isxdigit(*buf)) {
					printk(KERN_WARNING "%s: error parsing "
					       "\"%s\"\n", __FUNCTION__, buf);
					return;
				}
				
				from = blacklist_strtoul (buf, &buf);
				to = (*buf == '-') ?
					blacklist_strtoul (buf+1, &buf) : from;
				
				blacklist_range_remove (from, to);
			
				if (*buf == ',')
					buf++;
			}
			s390_redo_validation();
		}
	} else if (strstr (buf, "add ")) {
		for (i = 0; i < 4; i++) {
			buf++;
		}
		while (*buf != 0 && *buf != '\n') {
			if (!isxdigit(*buf)) {
				printk(KERN_WARNING "%s: error parsing "
				       "\"%s\"\n", __FUNCTION__, buf);
				return;
			}
			
			from = blacklist_strtoul (buf, &buf);
			to = (*buf == '-') ?
				blacklist_strtoul (buf+1, &buf) : from;
			
			spin_lock_irqsave (&blacklist_lock, flags);
			
			/*
			 * Don't allow for already known devices to be
			 * blacklisted
			 * The criterion is a bit dumb, devices which once were
			 * there but are already gone are also caught...
			 */
			
			err = 0;
			for (i = 0; i <= highest_subchannel; i++) {
				if (ioinfo[i] != INVALID_STORAGE_AREA) {
					if (!ioinfo[i]->st) 
						if ((ioinfo[i]->schib.pmcw.dev >= from)
						    && (ioinfo[i]->schib.pmcw.dev <=
							to)) {
							printk (KERN_WARNING
								"cio_ignore: Won't blacklist "
								"already known devices, "
								"skipping range %x to %x\n",
								from, to);
							err = 1;
							break;
						}
				}
			}
			
			if (!err)
				blacklist_range_add (from, to, 1);
			
			spin_unlock_irqrestore (&blacklist_lock, flags);
			if (*buf == ',')
				buf++;
		}

	} else {
		printk (KERN_WARNING
			"cio_ignore: Parse error; "
			"try using 'free all|<devno-range>,<devno-range>,...'\n");
		printk (KERN_WARNING
			"or 'add <devno-range>,<devno-range>,...'\n");
	}
}
#endif
/* End of blacklist handling */

void s390_displayhex (char *str, void *ptr, s32 cnt);

void
s390_displayhex (char *str, void *ptr, s32 cnt)
{
	s32 cnt1, cnt2, maxcnt2;
	u32 *currptr = (__u32 *) ptr;

	printk ("\n%s\n", str);

	for (cnt1 = 0; cnt1 < cnt; cnt1 += 16) {
		printk ("%08lX ", (unsigned long) currptr);
		maxcnt2 = cnt - cnt1;
		if (maxcnt2 > 16)
			maxcnt2 = 16;
		for (cnt2 = 0; cnt2 < maxcnt2; cnt2 += 4)
			printk ("%08X ", *currptr++);
		printk ("\n");
	}
}

static int __init
cio_setup (char *parm)
{
	if (!strcmp (parm, "yes")) {
		cio_show_msg = 1;
	} else if (!strcmp (parm, "no")) {
		cio_show_msg = 0;
	} else {
		printk (KERN_ERR "cio_setup : invalid cio_msg parameter '%s'",
			parm);

	}

	return 1;
}

__setup ("cio_msg=", cio_setup);

static int __init
cio_notoper_setup (char *parm)
{
	if (!strcmp (parm, "yes")) {
		cio_notoper_msg = 1;
	} else if (!strcmp (parm, "no")) {
		cio_notoper_msg = 0;
	} else {
		printk (KERN_ERR
			"cio_notoper_setup: "
			"invalid cio_notoper_msg parameter '%s'", parm);
	}

	return 1;
}

__setup ("cio_notoper_msg=", cio_notoper_setup);

#ifdef CONFIG_PROC_FS
static int __init
cio_proc_devinfo_setup (char *parm)
{
	if (!strcmp (parm, "yes")) {
		cio_proc_devinfo = 1;
	} else if (!strcmp (parm, "no")) {
		cio_proc_devinfo = 0;
	} else {
		printk (KERN_ERR
			"cio_proc_devinfo_setup: invalid parameter '%s'\n",
			parm);
	}

	return 1;
}

__setup ("cio_proc_devinfo=", cio_proc_devinfo_setup);
#endif

static int __init
cio_pgid_setup (char *parm)
{
	if (!strcmp (parm, "yes")) {
		cio_sid_with_pgid = 1;
	} else if (!strcmp (parm, "no")) {
		cio_sid_with_pgid = 0;
	} else {
		printk (KERN_ERR 
			"cio_pgid_setup : invalid cio_msg parameter '%s'",
			parm);

	}

	return 1;
}

__setup ("cio_sid_with_pgid=", cio_pgid_setup);

/*
 * register for adapter interrupts
 *
 * With HiperSockets the zSeries architecture provides for
 *  means of adapter interrups, pseudo I/O interrupts that are
 *  not tied to an I/O subchannel, but to an adapter. However,
 *  it doesn't disclose the info how to enable/disable them, but
 *  to recognize them only. Perhaps we should consider them
 *  being shared interrupts, and thus build a linked list
 *  of adapter handlers ... to be evaluated ...
 */
int
s390_register_adapter_interrupt (adapter_int_handler_t handler)
{
	int ret = 0;
	char dbf_txt[15];

	CIO_TRACE_EVENT (4, "rgaint");

	spin_lock (&adapter_lock);

	if (handler == NULL)
		ret = -EINVAL;
	else if (adapter_handler)
		ret = -EBUSY;
	else
		adapter_handler = handler;

	spin_unlock (&adapter_lock);

	sprintf (dbf_txt, "ret:%d", ret);
	CIO_TRACE_EVENT (4, dbf_txt);

	return (ret);
}

int
s390_unregister_adapter_interrupt (adapter_int_handler_t handler)
{
	int ret = 0;
	char dbf_txt[15];

	CIO_TRACE_EVENT (4, "urgaint");

	spin_lock (&adapter_lock);

	if (handler == NULL)
		ret = -EINVAL;
	else if (handler != adapter_handler)
		ret = -EINVAL;
	else
		adapter_handler = NULL;

	spin_unlock (&adapter_lock);

	sprintf (dbf_txt, "ret:%d", ret);
	CIO_TRACE_EVENT (4, dbf_txt);

	return (ret);
}

static inline void
do_adapter_IO (__u32 intparm)
{
	CIO_TRACE_EVENT (4, "doaio");

	spin_lock (&adapter_lock);

	if (adapter_handler)
		(*adapter_handler) (intparm);

	spin_unlock (&adapter_lock);

	return;
}

void s390_free_irq (unsigned int irq, void *dev_id);

/*
 * Note : internal use of irqflags SA_PROBE for NOT path grouping 
 *
 */
int
s390_request_irq_special (int irq,
			  io_handler_func_t io_handler,
			  not_oper_handler_func_t not_oper_handler,
			  unsigned long irqflags,
			  const char *devname, void *dev_id)
{
	int retval = 0;
	unsigned long flags;
	char dbf_txt[15];
	int retry;

	if (irq >= __MAX_SUBCHANNELS)
		return -EINVAL;

	if (!io_handler || !dev_id)
		return -EINVAL;

	if (ioinfo[irq] == INVALID_STORAGE_AREA)
		return -ENODEV;
	
	if (ioinfo[irq]->st)
		return -ENODEV;

	sprintf (dbf_txt, "reqsp%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * The following block of code has to be executed atomically
	 */
	s390irq_spin_lock_irqsave (irq, flags);

	if (ioinfo[irq]->ui.flags.unfriendly &&
	    !(irqflags & SA_FORCE)) {
		retval = -EUSERS;

	} else if (!ioinfo[irq]->ui.flags.ready) {
		retry = 5;

		ioinfo[irq]->irq_desc.handler = io_handler;
		ioinfo[irq]->irq_desc.name = devname;
		ioinfo[irq]->irq_desc.dev_id = dev_id;
		ioinfo[irq]->ui.flags.ready = 1;

		do {
			retval = enable_subchannel (irq);
			if (retval) {
				ioinfo[irq]->ui.flags.ready = 0;
				break;
			}

			stsch (irq, &ioinfo[irq]->schib);
			if (ioinfo[irq]->schib.pmcw.ena)
				retry = 0;
			else
				retry--;

		} while (retry);
	} else {
		/*
		 *  interrupt already owned, and shared interrupts
		 *   aren't supported on S/390.
		 */
		retval = -EBUSY;

	}

	s390irq_spin_unlock_irqrestore (irq, flags);

	if (retval == 0) {
		if (irqflags & SA_DOPATHGROUP) {
			ioinfo[irq]->ui.flags.pgid_supp = 1;
			ioinfo[irq]->ui.flags.notacccap = 1;
		}
		if ((irqflags & SA_DOPATHGROUP) &&
		    (!ioinfo[irq]->ui.flags.pgid ||
		     irqflags & SA_PROBE)) {
			pgid_t pgid;
			int i, mask;
			/* 
			 * Do an initial SensePGID to find out if device
			 * is locked by someone else.
			 */
			memcpy(&pgid, global_pgid, sizeof(pgid_t));
			
			retval = -EAGAIN;
			for (i=0; i<8 && retval==-EAGAIN; i++) {

				mask = (0x80 >> i) & ioinfo[irq]->opm;
				
				if (!mask)
					continue;
				
				retval = s390_SensePGID(irq, mask, &pgid);
				
				if (retval == -EOPNOTSUPP) 
					/* Doesn't prevent us from proceeding */
					retval = 0;
			}
			
		}
		if (!(irqflags & SA_PROBE) &&
		    (irqflags & SA_DOPATHGROUP) &&
		    (!ioinfo[irq]->ui.flags.unfriendly)) 
			s390_DevicePathVerification (irq, 0);

		if (ioinfo[irq]->ui.flags.unfriendly &&
		    !(irqflags & SA_FORCE)) {
			/* 
			 * We found out during path verification that the
			 * device is locked by someone else and we have to
			 * let the device driver know.
			 */
			retval = -EUSERS;
			free_irq(irq, dev_id);
		} else {
			ioinfo[irq]->ui.flags.newreq = 1;
			ioinfo[irq]->nopfunc = not_oper_handler;
		}
	}

	if (cio_debug_initialized)
		debug_int_event (cio_debug_trace_id, 4, retval);

	return retval;
}

int
s390_request_irq (unsigned int irq,
		  void (*handler) (int, void *, struct pt_regs *),
		  unsigned long irqflags, const char *devname, void *dev_id)
{
	int ret;

	ret = s390_request_irq_special (irq,
					(io_handler_func_t) handler,
					NULL, irqflags, devname, dev_id);

	if (ret == 0) {
		ioinfo[irq]->ui.flags.newreq = 0;

	}
	return (ret);
}

void
s390_free_irq (unsigned int irq, void *dev_id)
{
	unsigned long flags;
	int ret;

	char dbf_txt[15];

	if (irq >= __MAX_SUBCHANNELS || ioinfo[irq] == INVALID_STORAGE_AREA)
		return;

	if (ioinfo[irq]->st)
		return;

	sprintf (dbf_txt, "free%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	s390irq_spin_lock_irqsave (irq, flags);

#ifdef  CONFIG_KERNEL_DEBUG
	if (irq != cons_dev)
		printk (KERN_DEBUG "Trying to free IRQ%d\n", irq);
#endif
	CIO_MSG_EVENT(2, "Trying to free IRQ %d\n", irq);

	/*
	 * disable the device and reset all IRQ info if
	 *  the IRQ is actually owned by the handler ...
	 */
	if (ioinfo[irq]->ui.flags.ready) {
		if (dev_id == ioinfo[irq]->irq_desc.dev_id) {
			/* start deregister */
			ioinfo[irq]->ui.flags.unready = 1;

			ret = disable_subchannel (irq);

			if (ret == -EBUSY) {

				/*
				 * kill it !
				 * We try to terminate the I/O by halt_IO first,
				 * then clear_IO.
				 * Because the device may be gone (machine 
				 * check handling), we can't use sync I/O.
				 */

				halt_IO (irq, 0xC8C1D3E3, 0);
				s390irq_spin_unlock_irqrestore (irq, flags);
				udelay (200000);	/* 200 ms */
				s390irq_spin_lock_irqsave (irq, flags);

				ret = disable_subchannel (irq);

				if (ret == -EBUSY) {

					clear_IO (irq, 0x40C3D3D9, 0);
					s390irq_spin_unlock_irqrestore (irq,
									flags);
					udelay (1000000);	/* 1000 ms */
					s390irq_spin_lock_irqsave (irq, flags);

					/* give it a very last try ... */
					disable_subchannel (irq);

					if (ioinfo[irq]->ui.flags.busy) {
						printk (KERN_CRIT
							"free_irq(%04X) "
							"- device %04X busy, retry "
							"count exceeded\n", irq,
							ioinfo[irq]->devstat.
							devno);
						CIO_MSG_EVENT( 0,
							       "free_irq(%04X) - "
							       "device %04X busy, "
							       "retry count exceeded\n",
							       irq,
							       ioinfo[irq]->
							       devstat.devno);
						
					}
				}
			}

			ioinfo[irq]->ui.flags.ready = 0;
			ioinfo[irq]->ui.flags.unready = 0;	/* deregister ended */

			ioinfo[irq]->nopfunc = NULL;

			s390irq_spin_unlock_irqrestore (irq, flags);
		} else {
			s390irq_spin_unlock_irqrestore (irq, flags);

			printk (KERN_ERR "free_irq(%04X) : error, "
				"dev_id does not match !\n", irq);
			CIO_MSG_EVENT( 0,
				       "free_irq(%04X) : error, "
				       "dev_id does not match !\n",
				       irq);

		}
	} else {
		s390irq_spin_unlock_irqrestore (irq, flags);

		printk (KERN_ERR "free_irq(%04X) : error, "
			"no action block ... !\n", irq);
		CIO_MSG_EVENT(0,
			      "free_irq(%04X) : error, "
			      "no action block ... !\n", irq);

	}
}

/*
 * Enable IRQ by modifying the subchannel
 */
static int
enable_subchannel (unsigned int irq)
{
	int ret = 0;
	int ccode;
	int retry = 5;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	sprintf (dbf_txt, "ensch%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	/*
	 * If a previous disable request is pending we reset it. However, this
	 *  status implies that the device may (still) be not-operational.
	 */
	if (ioinfo[irq]->ui.flags.d_disable) {
		ioinfo[irq]->ui.flags.d_disable = 0;
		ret = 0;
	} else {
		ccode = stsch (irq, &(ioinfo[irq]->schib));

		if (ccode) {
			ret = -ENODEV;
		} else {
			ioinfo[irq]->schib.pmcw.ena = 1;

			if (irq == cons_dev) {
				ioinfo[irq]->schib.pmcw.isc = 7;
			} else {
				ioinfo[irq]->schib.pmcw.isc = 3;

			}

			do {
				ccode = msch (irq, &(ioinfo[irq]->schib));

				switch (ccode) {
				case 0:	/* ok */
					ret = 0;
					retry = 0;
					break;

				case 1:	/* status pending */

					ioinfo[irq]->ui.flags.s_pend = 1;
					s390_process_IRQ (irq);
					ioinfo[irq]->ui.flags.s_pend = 0;

					ret = -EIO;
					/* 
					 * might be overwritten on re-driving 
					 * the msch()       
					 */
					retry--;
					break;

				case 2:	/* busy */
					udelay (100);	/* allow for recovery */
					ret = -EBUSY;
					retry--;
					break;

				case 3:	/* not oper */
					ioinfo[irq]->ui.flags.oper = 0;
					retry = 0;
					ret = -ENODEV;
					break;
				}

			} while (retry);

		}
	}

	sprintf (dbf_txt, "ret:%d", ret);
	CIO_TRACE_EVENT (2, dbf_txt);

	return (ret);
}

/*
 * Disable IRQ by modifying the subchannel
 */
static int
disable_subchannel (unsigned int irq)
{
	int cc;			/* condition code */
	int ret = 0;		/* function return value */
	int retry = 5;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	sprintf (dbf_txt, "dissch%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	if (ioinfo[irq]->ui.flags.busy) {
		/*
		 * the disable function must not be called while there are
		 *  requests pending for completion !
		 */
		ret = -EBUSY;
	} else {

		/*
		 * If device isn't operational we have to perform delayed
		 *  disabling when the next interrupt occurs - unless the
		 *  irq is re-requested prior to the interrupt to occur.
		 */
		cc = stsch (irq, &(ioinfo[irq]->schib));

		if (cc == 3) {
			ioinfo[irq]->ui.flags.oper = 0;
			ioinfo[irq]->ui.flags.d_disable = 1;

			ret = 0;
		} else {	/* cc == 0 */

			ioinfo[irq]->schib.pmcw.ena = 0;

			do {
				cc = msch (irq, &(ioinfo[irq]->schib));

				switch (cc) {
				case 0:	/* ok */
					retry = 0;
					ret = 0;
					break;

				case 1:	/* status pending */
					ioinfo[irq]->ui.flags.s_pend = 1;
					s390_process_IRQ (irq);
					ioinfo[irq]->ui.flags.s_pend = 0;

					ret = -EIO;
					/* 
					 * might be overwritten on re-driving 
					 * the msch() call       
					 */
					retry--;
					break;

				case 2:	/* busy; this should not happen! */
					printk (KERN_CRIT
						"disable_subchannel(%04X) "
						"- unexpected busy condition for "
						"device %04X received !\n", irq,
						ioinfo[irq]->devstat.devno);
					CIO_MSG_EVENT(0,
						      "disable_subchannel(%04X) "
						      "- unexpected busy condition "
						      "for device %04X received !\n",
						      irq,
						      ioinfo[irq]->devstat.
						      devno);
					retry = 0;
					ret = -EBUSY;
					break;

				case 3:	/* not oper */
					/*
					 * should hardly occur ?!
					 */
					ioinfo[irq]->ui.flags.oper = 0;
					ioinfo[irq]->ui.flags.d_disable = 1;
					retry = 0;

					ret = 0;
					/* 
					 * if the device has gone, we don't need 
					 * to disable it anymore !          
					 */
					break;

				}

			} while (retry);

		}
	}

	sprintf (dbf_txt, "ret:%d", ret);
	CIO_TRACE_EVENT (2, dbf_txt);

	return (ret);
}

void
s390_init_IRQ (void)
{
	unsigned long flags;	/* PSW flags */
	long cr6 __attribute__ ((aligned (8)));
	cpuid_t cpuid;

	asm volatile ("STCK %0":"=m" (irq_IPL_TOD));

	p_init_schib = alloc_bootmem_low (sizeof (schib_t));
	p_init_irb = alloc_bootmem_low (sizeof (irb_t));

	/*
	 * As we don't know about the calling environment
	 *  we assure running disabled. Before leaving the
	 *  function we resestablish the old environment.
	 *
	 * Note : as we don't need a system wide lock, therefore
	 *        we shouldn't use cli(), but __cli() as this
	 *        affects the current CPU only.
	 */
	__save_flags (flags);
	__cli ();

	/*
	 * disable all interrupts
	 */
	cr6 = 0;
	__ctl_load (cr6, 6, 6);

	s390_process_subchannels ();

	if (cio_count_irqs) {
		int i;
		for (i = 0; i < NR_CPUS; i++)
			s390_irq_count[i] = 0;
	}

	
	/*
	 * Let's build our path group ID here.
	 */
	
	global_pgid = (pgid_t *)alloc_bootmem(sizeof(pgid_t));

	cpuid = *(cpuid_t*) __LC_CPUID;
	
	if (MACHINE_NEW_STIDP)
		global_pgid->cpu_addr = 0x8000;
	else {
#ifdef CONFIG_SMP
		global_pgid->cpu_addr = hard_smp_processor_id();
#else
		global_pgid->cpu_addr = 0;
#endif
	}
	global_pgid->cpu_id = cpuid.ident;
	global_pgid->cpu_model = ((cpuid_t *) __LC_CPUID)->machine;
	global_pgid->tod_high = *(__u32 *) & irq_IPL_TOD;


	/*
	 * enable default I/O-interrupt sublass 3
	 */
	cr6 = 0x10000000;
	__ctl_load (cr6, 6, 6);

	s390_device_recognition_all ();

	init_IRQ_complete = 1;

	__restore_flags (flags);

	return;
}

/*
 * dummy handler, used during init_IRQ() processing for compatibility only
 */
void
init_IRQ_handler (int irq, void *dev_id, struct pt_regs *regs)
{
	/* this is a dummy handler only ... */
}

int
s390_start_IO (int irq,		/* IRQ */
	       ccw1_t * cpa,	/* logical channel prog addr */
	       unsigned long user_intparm,	/* interruption parameter */
	       __u8 lpm,	/* logical path mask */
	       unsigned long flag)
{				/* flags */
	int ccode;
	int ret = 0;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	/*
	 * The flag usage is mutal exclusive ...
	 */
	if ((flag & DOIO_EARLY_NOTIFICATION)
	    && (flag & DOIO_REPORT_ALL)) {
		return (-EINVAL);

	}

	sprintf (dbf_txt, "stIO%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * setup ORB
	 */
	ioinfo[irq]->orb.intparm = (__u32) (long) &ioinfo[irq]->u_intparm;
	ioinfo[irq]->orb.fmt = 1;

	ioinfo[irq]->orb.pfch = !(flag & DOIO_DENY_PREFETCH);
	ioinfo[irq]->orb.spnd = (flag & DOIO_ALLOW_SUSPEND ? TRUE : FALSE);
	ioinfo[irq]->orb.ssic = ((flag & DOIO_ALLOW_SUSPEND)
				 && (flag & DOIO_SUPPRESS_INTER));

	if (flag & DOIO_VALID_LPM) {
		ioinfo[irq]->orb.lpm = lpm;
	} else {
		ioinfo[irq]->orb.lpm = ioinfo[irq]->opm;

	}

#ifdef CONFIG_ARCH_S390X
	/* 
	 * for 64 bit we always support 64 bit IDAWs with 4k page size only
	 */
	ioinfo[irq]->orb.c64 = 1;
	ioinfo[irq]->orb.i2k = 0;
#endif

	ioinfo[irq]->orb.cpa = (__u32) virt_to_phys (cpa);

	/*
	 * If sync processing was requested we lock the sync ISC, modify the
	 *  device to present interrupts for this ISC only and switch the
	 *  CPU to handle this ISC + the console ISC exclusively.
	 */
	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		ret = enable_cpu_sync_isc (irq);

		if (ret) {
			return (ret);
		}

	}

	if (flag & DOIO_DONT_CALL_INTHDLR) {
		ioinfo[irq]->ui.flags.repnone = 1;

	}

	/*
	 * Issue "Start subchannel" and process condition code
	 */
	if (flag & DOIO_USE_DIAG98) {
		ioinfo[irq]->orb.key = get_storage_key() >> 4;
		ioinfo[irq]->orb.cpa =
			(__u32) pfix_get_addr((void *)ioinfo[irq]->orb.cpa);
		ccode = diag98 (irq, &(ioinfo[irq]->orb));
	} else {
		ccode = ssch (irq, &(ioinfo[irq]->orb));
	}

	sprintf (dbf_txt, "ccode:%d", ccode);
	CIO_TRACE_EVENT (4, dbf_txt);

	switch (ccode) {
	case 0:

		if (!ioinfo[irq]->ui.flags.w4sense) {
			/*
			 * init the device driver specific devstat irb area
			 *
			 * Note : don´t clear saved irb info in case of sense !
			 */
			memset (&((devstat_t *) ioinfo[irq]->irq_desc.dev_id)->
				ii.irb, '\0', sizeof (irb_t));
		}

		memset (&ioinfo[irq]->devstat.ii.irb, '\0', sizeof (irb_t));

		/*
		 * initialize device status information
		 */
		ioinfo[irq]->ui.flags.busy = 1;
		ioinfo[irq]->ui.flags.doio = 1;

		ioinfo[irq]->u_intparm = user_intparm;
		ioinfo[irq]->devstat.cstat = 0;
		ioinfo[irq]->devstat.dstat = 0;
		ioinfo[irq]->devstat.lpum = 0;
		ioinfo[irq]->devstat.flag = DEVSTAT_START_FUNCTION;
		ioinfo[irq]->devstat.scnt = 0;

		ioinfo[irq]->ui.flags.fast = 0;
		ioinfo[irq]->ui.flags.repall = 0;

		/*
		 * Check for either early (FAST) notification requests
		 *  or if we are to return all interrupt info.
		 * Default is to call IRQ handler at secondary status only
		 */
		if (flag & DOIO_EARLY_NOTIFICATION) {
			ioinfo[irq]->ui.flags.fast = 1;
		} else if (flag & DOIO_REPORT_ALL) {
			ioinfo[irq]->ui.flags.repall = 1;

		}

		/*
		 * If synchronous I/O processing is requested, we have
		 *  to wait for the corresponding interrupt to occur by
		 *  polling the interrupt condition. However, as multiple
		 *  interrupts may be outstanding, we must not just wait
		 *  for the first interrupt, but must poll until ours
		 *  pops up.
		 */
		if (flag & DOIO_WAIT_FOR_INTERRUPT) {
			unsigned long psw_mask;
			int ccode;
			uint64_t time_start;
			uint64_t time_curr;

			int ready = 0;
			int io_sub = -1;
			int do_retry = 1;

			/*
			 * We shouldn't perform a TPI loop, waiting for an
			 *  interrupt to occur, but should load a WAIT PSW
			 *  instead. Otherwise we may keep the channel subsystem
			 *  busy, not able to present the interrupt. When our
			 *  sync. interrupt arrived we reset the I/O old PSW to
			 *  its original value.
			 */

			ccode = iac ();

			switch (ccode) {
			case 0:	/* primary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_PRIM_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 1:	/* secondary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_SEC_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 2:	/* access-register */
				psw_mask = _IO_PSW_MASK
				    | _PSW_ACC_REG_MODE | _PSW_IO_WAIT;
				break;
			case 3:	/* home-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_HOME_SPACE_MODE | _PSW_IO_WAIT;
				break;
			default:
				panic ("start_IO() : unexpected "
				       "address-space-control %d\n", ccode);
				break;
			}

			/*
			 * Martin didn't like modifying the new PSW, now we take
			 *  a fast exit in do_IRQ() instead
			 */
			*(__u32 *) __LC_SYNC_IO_WORD = 1;

			asm volatile ("STCK %0":"=m" (time_start));

			time_start = time_start >> 32;

			do {
				if (flag & DOIO_TIMEOUT) {
					tpi_info_t tpi_info = { 0, };

					do {
						if (tpi (&tpi_info) == 1) {
							io_sub = tpi_info.irq;
							break;
						} else {
							udelay (100);	/* usecs */
							asm volatile
							 ("STCK %0":"=m"
							  (time_curr));

							if (((time_curr >> 32) -
							     time_start) >= 3)
								do_retry = 0;

						}

					} while (do_retry);
				} else {
					__load_psw_mask (psw_mask);

					io_sub =
					    (__u32) *
					    (__u16 *) __LC_SUBCHANNEL_NR;

				}

				if (do_retry)
					ready = s390_process_IRQ (io_sub);

				/*
				 * surrender when retry count's exceeded ...
				 */
			} while (!((io_sub == irq)
				   && (ready == 1))
				 && do_retry);

			*(__u32 *) __LC_SYNC_IO_WORD = 0;

			if (!do_retry)
				ret = -ETIMEDOUT;

		}

		break;

	case 1:		/* status pending */
		
		/* 
		 * Don't do an inline processing of pending interrupt conditions
		 * while doing async. I/O. The interrupt will pop up when we are
		 * enabled again and the I/O can be retried.
		 */
		if (!ioinfo[irq]->ui.flags.syncio) {
			ret = -EBUSY;
			break;
		}

		ioinfo[irq]->devstat.flag = DEVSTAT_START_FUNCTION
		    | DEVSTAT_STATUS_PENDING;

		/*
		 * initialize the device driver specific devstat irb area
		 */
		memset (&((devstat_t *) ioinfo[irq]->irq_desc.dev_id)->ii.irb,
			'\0', sizeof (irb_t));

		/*
		 * Let the common interrupt handler process the pending status.
		 *  However, we must avoid calling the user action handler, as
		 *  it won't be prepared to handle a pending status during
		 *  do_IO() processing inline. This also implies that process_IRQ
		 *  must terminate synchronously - especially if device sensing
		 *  is required.
		 */
		ioinfo[irq]->ui.flags.s_pend = 1;
		ioinfo[irq]->ui.flags.busy = 1;
		ioinfo[irq]->ui.flags.doio = 1;

		s390_process_IRQ (irq);

		ioinfo[irq]->ui.flags.s_pend = 0;
		ioinfo[irq]->ui.flags.busy = 0;
		ioinfo[irq]->ui.flags.doio = 0;

		ioinfo[irq]->ui.flags.repall = 0;
		ioinfo[irq]->ui.flags.w4final = 0;

		ioinfo[irq]->devstat.flag |= DEVSTAT_FINAL_STATUS;

		/*
		 * In multipath mode a condition code 3 implies the last path
		 *  has gone, except we have previously restricted the I/O to
		 *  a particular path. A condition code 1 (0 won't occur)
		 *  results in return code EIO as well as 3 with another path
		 *  than the one used (i.e. path available mask is non-zero).
		 */
		if (ioinfo[irq]->devstat.ii.irb.scsw.cc == 3) {

			if (ioinfo[irq]->opm == 0) {
				ret = -ENODEV;
				ioinfo[irq]->ui.flags.oper = 0;
			} else {
				ret = -EIO;

			}

			ioinfo[irq]->devstat.flag |= DEVSTAT_NOT_OPER;

#ifdef CONFIG_DEBUG_IO
			{
				char buffer[80];

				stsch (irq, &(ioinfo[irq]->schib));

				sprintf (buffer,
					 "s390_start_IO(%04X) - irb for "
					 "device %04X, after status pending\n",
					 irq, ioinfo[irq]->devstat.devno);

				s390_displayhex (buffer,
						 &(ioinfo[irq]->devstat.ii.irb),
						 sizeof (irb_t));

				sprintf (buffer,
					 "s390_start_IO(%04X) - schib for "
					 "device %04X, after status pending\n",
					 irq, ioinfo[irq]->devstat.devno);

				s390_displayhex (buffer,
						 &(ioinfo[irq]->schib),
						 sizeof (schib_t));

				if (ioinfo[irq]->devstat.
				    flag & DEVSTAT_FLAG_SENSE_AVAIL) {
					sprintf (buffer,
						 "s390_start_IO(%04X) "
						 "- sense data for device %04X,"
						 " after status pending\n",
						 irq,
						 ioinfo[irq]->devstat.devno);

					s390_displayhex (buffer,
							 ioinfo[irq]->irq_desc.
							 dev_id->ii.sense.data,
							 ioinfo[irq]->irq_desc.
							 dev_id->rescnt);

				}
			}
#endif
			if (cio_debug_initialized) {
				stsch (irq, &(ioinfo[irq]->schib));
				
				sprintf(dbf_txt, "sp%x", irq);
				CIO_TRACE_EVENT(2, dbf_txt);
				CIO_TRACE_EVENT(2, "irb:");
				CIO_HEX_EVENT(2, &(ioinfo[irq]->devstat.ii.irb),
					      sizeof (irb_t));
				CIO_TRACE_EVENT(2, "schib:");
				CIO_HEX_EVENT(2, &(ioinfo[irq]->schib),
					      sizeof (schib_t));

				if (ioinfo[irq]->devstat.
				    flag & DEVSTAT_FLAG_SENSE_AVAIL) {
					CIO_TRACE_EVENT(2, "sense:");
					CIO_HEX_EVENT(2, ioinfo[irq]->irq_desc.
						      dev_id->ii.sense.data,
						      ioinfo[irq]->irq_desc.
						      dev_id->rescnt);

				}
			}
		} else {
			ret = -EIO;
			ioinfo[irq]->devstat.flag &= ~DEVSTAT_NOT_OPER;
			ioinfo[irq]->ui.flags.oper = 1;

		}

		break;

	case 2:		/* busy */

		ret = -EBUSY;
		break;

	default:		/* device/path not operational */

		if (flag & DOIO_VALID_LPM) {
			ioinfo[irq]->opm &= ~lpm;
		} else {
			ioinfo[irq]->opm = 0;

		}

		if (ioinfo[irq]->opm == 0) {
			ioinfo[irq]->ui.flags.oper = 0;
			ioinfo[irq]->devstat.flag |= DEVSTAT_NOT_OPER;

		}

		ret = -ENODEV;

		memcpy (ioinfo[irq]->irq_desc.dev_id,
			&(ioinfo[irq]->devstat), sizeof (devstat_t));

#ifdef CONFIG_DEBUG_IO

		stsch (irq, &(ioinfo[irq]->schib));

		sprintf (buffer, "s390_start_IO(%04X) - schib for "
			 "device %04X, after 'not oper' status\n",
			 irq, ioinfo[irq]->devstat.devno);

		s390_displayhex (buffer,
				 &(ioinfo[irq]->schib), sizeof (schib_t));
#endif
		if (cio_debug_initialized) {
			stsch (irq, &(ioinfo[irq]->schib));
			sprintf(dbf_txt, "no%x", irq);
			CIO_TRACE_EVENT(2, dbf_txt);
			CIO_HEX_EVENT(2, &(ioinfo[irq]->schib),
				      sizeof (schib_t));
		}

		break;

	}

	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		disable_cpu_sync_isc (irq);

	}

	if (flag & DOIO_DONT_CALL_INTHDLR) {
		ioinfo[irq]->ui.flags.repnone = 0;

	}

	return (ret);
}

int
do_IO (int irq,			/* IRQ */
       ccw1_t * cpa,		/* channel program address */
       unsigned long user_intparm,	/* interruption parameter */
       __u8 lpm,		/* logical path mask */
       unsigned long flag)
{				/* flags : see above */
	int ret = 0;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	/* handler registered ? or free_irq() in process already ? */
	if (!ioinfo[irq]->ui.flags.ready || ioinfo[irq]->ui.flags.unready) {
		return (-ENODEV);

	}

	sprintf (dbf_txt, "doIO%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	if (ioinfo[irq]->ui.flags.noio)
		return -EBUSY;

	/*
	 * Note: We ignore the device operational status - if not operational,
	 *        the SSCH will lead to an -ENODEV condition ...
	 */
	if (!ioinfo[irq]->ui.flags.busy) {	/* last I/O completed ? */
		ret = s390_start_IO (irq, cpa, user_intparm, lpm, flag);
	} else {
		ret = -EBUSY;

	}

	return (ret);

}

/*
 * resume suspended I/O operation
 */
int
resume_IO (int irq)
{
	int ret = 0;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	sprintf (dbf_txt, "resIO%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * We allow for 'resume' requests only for active I/O operations
	 */
	if (ioinfo[irq]->ui.flags.busy) {
		int ccode;

		ccode = rsch (irq);

		sprintf (dbf_txt, "ccode:%d", ccode);
		CIO_TRACE_EVENT (4, dbf_txt);

		switch (ccode) {
		case 0:
			break;

		case 1:
			ret = -EBUSY;
			break;

		case 2:
			ret = -EINVAL;
			break;

		case 3:
			/*
			 * useless to wait for request completion
			 *  as device is no longer operational !
			 */
			ioinfo[irq]->ui.flags.oper = 0;
			ioinfo[irq]->ui.flags.busy = 0;
			ret = -ENODEV;
			break;

		}

	} else {
		ret = -ENOTCONN;

	}

	return (ret);
}

/*
 * Note: The "intparm" parameter is not used by the halt_IO() function
 *       itself, as no ORB is built for the HSCH instruction. However,
 *       it allows the device interrupt handler to associate the upcoming
 *       interrupt with the halt_IO() request.
 */
int
halt_IO (int irq, unsigned long user_intparm, unsigned long flag)
{				/* possible DOIO_WAIT_FOR_INTERRUPT */
	int ret;
	int ccode;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	if (ioinfo[irq]->ui.flags.noio)
		return -EBUSY;

	/*
	 * we only allow for halt_IO if the device has an I/O handler associated
	 */
	if (!ioinfo[irq]->ui.flags.ready) {
		return -ENODEV;
	}
	/*
	 * we ignore the halt_io() request if ending_status was received but
	 *  a SENSE operation is waiting for completion.
	 */
	if (ioinfo[irq]->ui.flags.w4sense) {
		return 0;
	}
	sprintf (dbf_txt, "haltIO%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	/*
	 * If sync processing was requested we lock the sync ISC,
	 *  modify the device to present interrupts for this ISC only
	 *  and switch the CPU to handle this ISC + the console ISC
	 *  exclusively.
	 */
	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		ret = enable_cpu_sync_isc (irq);

		if (ret)
			return (ret);
	}

	/*
	 * Issue "Halt subchannel" and process condition code
	 */
	ccode = hsch (irq);

	sprintf (dbf_txt, "ccode:%d", ccode);
	CIO_TRACE_EVENT (2, dbf_txt);

	switch (ccode) {
	case 0:

		ioinfo[irq]->ui.flags.haltio = 1;

		if (!ioinfo[irq]->ui.flags.doio) {
			ioinfo[irq]->ui.flags.busy = 1;
			ioinfo[irq]->u_intparm = user_intparm;
			ioinfo[irq]->devstat.cstat = 0;
			ioinfo[irq]->devstat.dstat = 0;
			ioinfo[irq]->devstat.lpum = 0;
			ioinfo[irq]->devstat.flag = DEVSTAT_HALT_FUNCTION;
			ioinfo[irq]->devstat.scnt = 0;

		} else {
			ioinfo[irq]->devstat.flag |= DEVSTAT_HALT_FUNCTION;

		}

		/*
		 * If synchronous I/O processing is requested, we have
		 *  to wait for the corresponding interrupt to occur by
		 *  polling the interrupt condition. However, as multiple
		 *  interrupts may be outstanding, we must not just wait
		 *  for the first interrupt, but must poll until ours
		 *  pops up.
		 */
		if (flag & DOIO_WAIT_FOR_INTERRUPT) {
			int io_sub;
			__u32 io_parm;
			unsigned long psw_mask;
			int ccode;

			int ready = 0;

			/*
			 * We shouldn't perform a TPI loop, waiting for
			 *  an interrupt to occur, but should load a
			 *  WAIT PSW instead. Otherwise we may keep the
			 *  channel subsystem busy, not able to present
			 *  the interrupt. When our sync. interrupt
			 *  arrived we reset the I/O old PSW to its
			 *  original value.
			 */

			ccode = iac ();

			switch (ccode) {
			case 0:	/* primary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_PRIM_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 1:	/* secondary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_SEC_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 2:	/* access-register */
				psw_mask = _IO_PSW_MASK
				    | _PSW_ACC_REG_MODE | _PSW_IO_WAIT;
				break;
			case 3:	/* home-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_HOME_SPACE_MODE | _PSW_IO_WAIT;
				break;
			default:
				panic ("halt_IO() : unexpected "
				       "address-space-control %d\n", ccode);
				break;
			}

			/*
			 * Martin didn't like modifying the new PSW, now we take
			 *  a fast exit in do_IRQ() instead
			 */
			*(__u32 *) __LC_SYNC_IO_WORD = 1;

			do {
				__load_psw_mask (psw_mask);

				io_parm = *(__u32 *) __LC_IO_INT_PARM;
				io_sub = (__u32) * (__u16 *) __LC_SUBCHANNEL_NR;

				ready = s390_process_IRQ (io_sub);

			} while (!((io_sub == irq) && (ready == 1)));

			*(__u32 *) __LC_SYNC_IO_WORD = 0;

		}

		ret = 0;
		break;

	case 1:		/* status pending */

		/* 
		 * Don't do an inline processing of pending interrupt conditions
		 * while doing async. I/O. The interrupt will pop up when we are
		 * enabled again and the I/O can be retried.
		 */
		if (!ioinfo[irq]->ui.flags.syncio) {
			ret = -EBUSY;
			break;
		}
		
		ioinfo[irq]->devstat.flag |= DEVSTAT_STATUS_PENDING;

		/*
		 * initialize the device driver specific devstat irb area
		 */
		memset (&ioinfo[irq]->irq_desc.dev_id->ii.irb,
			'\0', sizeof (irb_t));

		/*
		 * Let the common interrupt handler process the pending
		 *  status. However, we must avoid calling the user
		 *  action handler, as it won't be prepared to handle
		 *  a pending status during do_IO() processing inline.
		 *  This also implies that s390_process_IRQ must
		 *  terminate synchronously - especially if device
		 *  sensing is required.
		 */
		ioinfo[irq]->ui.flags.s_pend = 1;
		ioinfo[irq]->ui.flags.busy = 1;
		ioinfo[irq]->ui.flags.doio = 1;

		s390_process_IRQ (irq);

		ioinfo[irq]->ui.flags.s_pend = 0;
		ioinfo[irq]->ui.flags.busy = 0;
		ioinfo[irq]->ui.flags.doio = 0;
		ioinfo[irq]->ui.flags.repall = 0;
		ioinfo[irq]->ui.flags.w4final = 0;

		ioinfo[irq]->devstat.flag |= DEVSTAT_FINAL_STATUS;

		/*
		 * In multipath mode a condition code 3 implies the last
		 *  path has gone, except we have previously restricted
		 *  the I/O to a particular path. A condition code 1
		 *  (0 won't occur) results in return code EIO as well
		 *  as 3 with another path than the one used (i.e. path 
		 *  available mask is non-zero).
		 */
		if (ioinfo[irq]->devstat.ii.irb.scsw.cc == 3) {
			ret = -ENODEV;
			ioinfo[irq]->devstat.flag |= DEVSTAT_NOT_OPER;
			ioinfo[irq]->ui.flags.oper = 0;
		} else {
			ret = -EIO;
			ioinfo[irq]->devstat.flag &= ~DEVSTAT_NOT_OPER;
			ioinfo[irq]->ui.flags.oper = 1;

		}

		break;

	case 2:		/* busy */

		ret = -EBUSY;
		break;

	default:		/* device not operational */

		ret = -ENODEV;
		break;

	}

	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		disable_cpu_sync_isc (irq);

	}

	return (ret);
}

/*
 * Note: The "intparm" parameter is not used by the clear_IO() function
 *       itself, as no ORB is built for the CSCH instruction. However,
 *       it allows the device interrupt handler to associate the upcoming
 *       interrupt with the clear_IO() request.
 */
int
clear_IO (int irq, unsigned long user_intparm, unsigned long flag)
{				/* possible DOIO_WAIT_FOR_INTERRUPT */
	int ret = 0;
	int ccode;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	if (ioinfo[irq] == INVALID_STORAGE_AREA)
		return (-ENODEV);

	if (ioinfo[irq]->ui.flags.noio)
		return -EBUSY;
	/*
	 * we only allow for clear_IO if the device has an I/O handler associated
	 */
	if (!ioinfo[irq]->ui.flags.ready)
		return -ENODEV;
	/*
	 * we ignore the clear_io() request if ending_status was received but
	 *  a SENSE operation is waiting for completion.
	 */
	if (ioinfo[irq]->ui.flags.w4sense)
		return 0;

	sprintf (dbf_txt, "clearIO%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	/*
	 * If sync processing was requested we lock the sync ISC,
	 *  modify the device to present interrupts for this ISC only
	 *  and switch the CPU to handle this ISC + the console ISC
	 *  exclusively.
	 */
	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		ret = enable_cpu_sync_isc (irq);

		if (ret)
			return (ret);
	}

	/*
	 * Issue "Clear subchannel" and process condition code
	 */
	ccode = csch (irq);

	sprintf (dbf_txt, "ccode:%d", ccode);
	CIO_TRACE_EVENT (2, dbf_txt);

	switch (ccode) {
	case 0:

		ioinfo[irq]->ui.flags.haltio = 1;

		if (!ioinfo[irq]->ui.flags.doio) {
			ioinfo[irq]->ui.flags.busy = 1;
			ioinfo[irq]->u_intparm = user_intparm;
			ioinfo[irq]->devstat.cstat = 0;
			ioinfo[irq]->devstat.dstat = 0;
			ioinfo[irq]->devstat.lpum = 0;
			ioinfo[irq]->devstat.flag = DEVSTAT_CLEAR_FUNCTION;
			ioinfo[irq]->devstat.scnt = 0;

		} else {
			ioinfo[irq]->devstat.flag |= DEVSTAT_CLEAR_FUNCTION;

		}

		/*
		 * If synchronous I/O processing is requested, we have
		 *  to wait for the corresponding interrupt to occur by
		 *  polling the interrupt condition. However, as multiple
		 *  interrupts may be outstanding, we must not just wait
		 *  for the first interrupt, but must poll until ours
		 *  pops up.
		 */
		if (flag & DOIO_WAIT_FOR_INTERRUPT) {
			int io_sub;
			__u32 io_parm;
			unsigned long psw_mask;
			int ccode;

			int ready = 0;

			/*
			 * We shouldn't perform a TPI loop, waiting for
			 *  an interrupt to occur, but should load a
			 *  WAIT PSW instead. Otherwise we may keep the
			 *  channel subsystem busy, not able to present
			 *  the interrupt. When our sync. interrupt
			 *  arrived we reset the I/O old PSW to its
			 *  original value.
			 */

			ccode = iac ();

			switch (ccode) {
			case 0:	/* primary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_PRIM_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 1:	/* secondary-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_SEC_SPACE_MODE | _PSW_IO_WAIT;
				break;
			case 2:	/* access-register */
				psw_mask = _IO_PSW_MASK
				    | _PSW_ACC_REG_MODE | _PSW_IO_WAIT;
				break;
			case 3:	/* home-space */
				psw_mask = _IO_PSW_MASK
				    | _PSW_HOME_SPACE_MODE | _PSW_IO_WAIT;
				break;
			default:
				panic ("clear_IO() : unexpected "
				       "address-space-control %d\n", ccode);
				break;
			}

			/*
			 * Martin didn't like modifying the new PSW, now we take
			 *  a fast exit in do_IRQ() instead
			 */
			*(__u32 *) __LC_SYNC_IO_WORD = 1;

			do {
				__load_psw_mask (psw_mask);

				io_parm = *(__u32 *) __LC_IO_INT_PARM;
				io_sub = (__u32) * (__u16 *) __LC_SUBCHANNEL_NR;

				ready = s390_process_IRQ (io_sub);

			} while (!((io_sub == irq) && (ready == 1)));

			*(__u32 *) __LC_SYNC_IO_WORD = 0;

		}

		ret = 0;
		break;

	case 1:		/* no status pending for csh */
		BUG ();
		break;

	case 2:		/* no busy for csh */
		BUG ();
		break;

	default:		/* device not operational */

		ret = -ENODEV;
		break;

	}

	if (flag & DOIO_WAIT_FOR_INTERRUPT) {
		disable_cpu_sync_isc (irq);

	}

	return (ret);
}

/*
 * Function: cancel_IO
 * Issues a "Cancel Subchannel" on the specified subchannel
 * Note: We don't need any fancy intparms and flags here
 *       since xsch is executed synchronously.
 * Only for common I/O internal use as for now.
 */
int
cancel_IO (int irq)
{

	int ccode;
	char dbf_txt[15];
	int ret = 0;

	SANITY_CHECK (irq);

	sprintf (dbf_txt, "cancelIO%x", irq);
	CIO_TRACE_EVENT (2, dbf_txt);

	ccode = xsch (irq);

	sprintf (dbf_txt, "ccode:%d", ccode);
	CIO_TRACE_EVENT (2, dbf_txt);

	switch (ccode) {

	case 0:		/* success */
		ret = 0;
		break;

	case 1:		/* status pending */

		ret = -EBUSY;
		break;

	case 2:		/* not applicable */
		ret = -EINVAL;
		break;

	default:		/* not oper */
		ret = -ENODEV;
	}

	return ret;
}

/*
 * do_IRQ() handles all normal I/O device IRQ's (the special
 *          SMP cross-CPU interrupts have their own specific
 *          handlers).
 *
 */
asmlinkage void
do_IRQ (struct pt_regs regs)
{
	/*
	 * Get interrupt info from lowcore
	 */
	volatile tpi_info_t *tpi_info = (tpi_info_t *) (__LC_SUBCHANNEL_ID);
	int cpu = smp_processor_id ();

	/*
	 * take fast exit if CPU is in sync. I/O state
	 *
	 * Note: we have to turn off the WAIT bit and re-disable
	 *       interrupts prior to return as this was the initial
	 *       entry condition to synchronous I/O.
	 */
	if (*(__u32 *) __LC_SYNC_IO_WORD) {
		regs.psw.mask &= ~(_PSW_WAIT_MASK_BIT | _PSW_IO_MASK_BIT);
		return;
	}
	/* endif */
#ifdef CONFIG_FAST_IRQ
	do {
#endif				/* CONFIG_FAST_IRQ */

		/*
		 * Non I/O-subchannel thin interrupts are processed differently
		 */
		if (tpi_info->adapter_IO == 1 &&
		    tpi_info->int_type == IO_INTERRUPT_TYPE) {
			irq_enter (cpu, -1);
			do_adapter_IO (tpi_info->intparm);
			irq_exit (cpu, -1);
		} else {
			unsigned int irq = tpi_info->irq;

			/*
			 * fix me !!!
			 *
			 * instead of boxing the device, we need to schedule device
			 * recognition, the interrupt stays pending. We need to
			 * dynamically allocate an ioinfo structure, etc..
			 */
			if (ioinfo[irq] == INVALID_STORAGE_AREA) {
				return;	/* this keeps the device boxed ... */
			}

			if (ioinfo[irq]->st) {
				/* How can that be? */
				printk(KERN_WARNING "Received interrupt on "
				       "non-IO subchannel %x!\n", irq);
				return;
			}

			irq_enter (cpu, irq);
			s390irq_spin_lock (irq);
			s390_process_IRQ (irq);
			s390irq_spin_unlock (irq);
			irq_exit (cpu, irq);
		}

#ifdef CONFIG_FAST_IRQ

		/*
		 * Are more interrupts pending?
		 * If so, the tpi instruction will update the lowcore 
		 * to hold the info for the next interrupt.
		 */
	} while (tpi (NULL) != 0);

#endif				/* CONFIG_FAST_IRQ */

	return;
}

/*
 * s390_process_IRQ() handles status pending situations and interrupts
 *
 * Called by : do_IRQ()             - for "real" interrupts
 *             s390_start_IO, halt_IO()
 *                                  - status pending cond. after SSCH, or HSCH
 *             disable_subchannel() - status pending conditions (after MSCH)
 *
 * Returns: 0 - no ending status received, no further action taken
 *          1 - interrupt handler was called with ending status
 */
int
s390_process_IRQ (unsigned int irq)
{
	int ccode;		/* cond code from tsch() operation */
	int irb_cc;		/* cond code from irb */
	int sdevstat;		/* struct devstat size to copy */
	unsigned int fctl;	/* function control */
	unsigned int stctl;	/* status   control */
	unsigned int actl;	/* activity control */

	int issense = 0;
	int ending_status = 0;
	int allow4handler = 1;
	int chnchk = 0;
	devstat_t *dp;
	devstat_t *udp;

	char dbf_txt[15];
	char buffer[256];

	if (cio_count_irqs) {
		int cpu = smp_processor_id ();
		s390_irq_count[cpu]++;
	}

	CIO_TRACE_EVENT (3, "procIRQ");
	sprintf (dbf_txt, "%x", irq);
	CIO_TRACE_EVENT (3, dbf_txt);

	if (ioinfo[irq] == INVALID_STORAGE_AREA) {
		/* we can't properly process the interrupt ... */
#ifdef CONFIG_DEBUG_IO
		printk (KERN_CRIT "s390_process_IRQ(%04X) - got interrupt "
			"for non-initialized subchannel!\n", irq);
#endif /* CONFIG_DEBUG_IO */
		CIO_MSG_EVENT (0,
			       "s390_process_IRQ(%04X) - got interrupt "
			       "for non-initialized subchannel!\n",
			       irq);
		tsch (irq, p_init_irb);
		return (1);

	}

	if (ioinfo[irq]->st) {
		/* can't be */
		BUG();
		return 1;
	}

	dp = &ioinfo[irq]->devstat;
	udp = ioinfo[irq]->irq_desc.dev_id;
	
	/*
	 * It might be possible that a device was not-oper. at the time
	 *  of free_irq() processing. This means the handler is no longer
	 *  available when the device possibly becomes ready again. In
	 *  this case we perform delayed disable_subchannel() processing.
	 */
	if (!ioinfo[irq]->ui.flags.ready) {
		if (!ioinfo[irq]->ui.flags.d_disable) {
#ifdef CONFIG_DEBUG_IO
			printk (KERN_CRIT "s390_process_IRQ(%04X) "
				"- no interrupt handler registered "
				"for device %04X !\n",
				irq, ioinfo[irq]->devstat.devno);
#endif				/* CONFIG_DEBUG_IO */
			CIO_MSG_EVENT(0,
				      "s390_process_IRQ(%04X) "
				      "- no interrupt handler "
				      "registered for device "
				      "%04X !\n",
				      irq,
				      ioinfo[irq]->devstat.devno);
		}
	}

	/*
	 * retrieve the i/o interrupt information (irb),
	 *  update the device specific status information
	 *  and possibly call the interrupt handler.
	 *
	 * Note 1: At this time we don't process the resulting
	 *         condition code (ccode) from tsch(), although
	 *         we probably should.
	 *
	 * Note 2: Here we will have to check for channel
	 *         check conditions and call a channel check
	 *         handler.
	 *
	 * Note 3: If a start function was issued, the interruption
	 *         parameter relates to it. If a halt function was
	 *         issued for an idle device, the intparm must not
	 *         be taken from lowcore, but from the devstat area.
	 */
	ccode = tsch (irq, &(dp->ii.irb));

	sprintf (dbf_txt, "ccode:%d", ccode);
	CIO_TRACE_EVENT (3, dbf_txt);

	if (ccode == 1) {
#ifdef CONFIG_DEBUG_IO
		printk (KERN_INFO "s390_process_IRQ(%04X) - no status "
			 "pending...\n", irq);
#endif /* CONFIG_DEBUG_IO */
		CIO_MSG_EVENT(2,
			      "s390_process_IRQ(%04X) - no status pending\n",
			      irq);
	} else if (ccode == 3) {
#ifdef CONFIG_DEBUG_IO
		printk (KERN_WARNING "s390_process_IRQ(%04X) - subchannel "
			"is not operational!\n",
			irq);
#endif /* CONFIG_DEBUG_IO */
		CIO_MSG_EVENT(0,
			      "s390_process_IRQ(%04X) - subchannel "
			      "is not operational!\n",
			      irq);
	}

	/*
	 * We must only accumulate the status if the device is busy already
	 */
	if (ioinfo[irq]->ui.flags.busy) {
		dp->dstat |= dp->ii.irb.scsw.dstat;
		dp->cstat |= dp->ii.irb.scsw.cstat;
		dp->intparm = ioinfo[irq]->u_intparm;

	} else {
		dp->dstat = dp->ii.irb.scsw.dstat;
		dp->cstat = dp->ii.irb.scsw.cstat;

		dp->flag = 0;	/* reset status flags */
		dp->intparm = 0;

	}

	dp->lpum = dp->ii.irb.esw.esw1.lpum;

	/*
	 * reset device-busy bit if no longer set in irb
	 */
	if ((dp->dstat & DEV_STAT_BUSY)
	    && ((dp->ii.irb.scsw.dstat & DEV_STAT_BUSY) == 0)) {
		dp->dstat &= ~DEV_STAT_BUSY;

	}

	/*
	   * Save residual count and CCW information in case primary and
	   *  secondary status are presented with different interrupts.
	 */
	if (dp->ii.irb.scsw.stctl
	    & (SCSW_STCTL_PRIM_STATUS | SCSW_STCTL_INTER_STATUS)) {

		/*
		 * If the subchannel status shows status pending
		 * and we received a check condition, the count
		 * information is not meaningful.
		 */

		if (!((dp->ii.irb.scsw.stctl & SCSW_STCTL_STATUS_PEND)
		      && (dp->ii.irb.scsw.cstat
			  & (SCHN_STAT_CHN_DATA_CHK
			     | SCHN_STAT_CHN_CTRL_CHK
			     | SCHN_STAT_INTF_CTRL_CHK
			     | SCHN_STAT_PROG_CHECK
			     | SCHN_STAT_PROT_CHECK
			     | SCHN_STAT_CHAIN_CHECK)))) {

			dp->rescnt = dp->ii.irb.scsw.count;
		} else {
			dp->rescnt = SENSE_MAX_COUNT;
		}

		dp->cpa = dp->ii.irb.scsw.cpa;
		
	}
	irb_cc = dp->ii.irb.scsw.cc;

	/*
	 * check for any kind of channel or interface control check but don't
	 * issue the message for the console device
	 */
	if ((dp->ii.irb.scsw.cstat
	     & (SCHN_STAT_CHN_DATA_CHK
		| SCHN_STAT_CHN_CTRL_CHK | SCHN_STAT_INTF_CTRL_CHK))) {
		if (irq != cons_dev)
			printk (KERN_WARNING
				"Channel-Check or Interface-Control-Check "
				"received\n"
				" ... device %04X on subchannel %04X, dev_stat "
				": %02X sch_stat : %02X\n",
				ioinfo[irq]->devstat.devno, irq, dp->dstat,
				dp->cstat);
		CIO_MSG_EVENT(0,
			      "Channel-Check or "
			      "Interface-Control-Check received\n");
		CIO_MSG_EVENT(0,
			      "... device %04X on subchannel %04X,"
			      " dev_stat: %02X sch_stat: %02X\n",
			      ioinfo[irq]->devstat.devno, irq,
			      dp->dstat, dp->cstat);
	

		chnchk = 1;

	}

	if (dp->ii.irb.scsw.ectl == 0) {
		issense = 0;
	} else if ((dp->ii.irb.scsw.stctl == SCSW_STCTL_STATUS_PEND)
		   && (dp->ii.irb.scsw.eswf == 0)) {
		issense = 0;
	} else if ((dp->ii.irb.scsw.stctl ==
		    (SCSW_STCTL_STATUS_PEND | SCSW_STCTL_INTER_STATUS))
		   && ((dp->ii.irb.scsw.actl & SCSW_ACTL_SUSPENDED) == 0)) {
		issense = 0;
	} else {
		issense = dp->ii.irb.esw.esw0.erw.cons;

	}

	if (issense) {
		dp->scnt = dp->ii.irb.esw.esw0.erw.scnt;
		dp->flag |= DEVSTAT_FLAG_SENSE_AVAIL;

		sdevstat = sizeof (devstat_t);

#ifdef CONFIG_DEBUG_IO
		if (irq != cons_dev)
			printk (KERN_DEBUG "s390_process_IRQ( %04X ) : "
				"concurrent sense bytes avail %d\n",
				irq, dp->scnt);
#endif
		CIO_MSG_EVENT(4,
			      "s390_process_IRQ( %04X ): "
			      "concurrent sense bytes avail %d\n",
			      irq, dp->scnt);
	} else {
		/* don't copy the sense data area ! */
		sdevstat = sizeof (devstat_t) - SENSE_MAX_COUNT;

	}

	switch (irb_cc) {
	case 1:		/* status pending */

		dp->flag |= DEVSTAT_STATUS_PENDING;

	case 0:		/* normal i/o interruption */

		fctl = dp->ii.irb.scsw.fctl;
		stctl = dp->ii.irb.scsw.stctl;
		actl = dp->ii.irb.scsw.actl;

		if (chnchk) {
			sprintf (buffer, "s390_process_IRQ(%04X) - irb for "
				 "device %04X after channel check "
				 "or interface control check\n",
				 irq, dp->devno);

			s390_displayhex (buffer, &(dp->ii.irb), sizeof (irb_t));
			sprintf(dbf_txt, "chk%x", irq);
			CIO_TRACE_EVENT(0, dbf_txt);
			CIO_HEX_EVENT(0, &(dp->ii.irb), sizeof (irb_t));
		}

		ioinfo[irq]->stctl |= stctl;

		ending_status = (stctl & SCSW_STCTL_SEC_STATUS)
		    || (stctl ==
			(SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND))
		    || (stctl == SCSW_STCTL_STATUS_PEND);

		/*
		 * Check for unsolicited interrupts - for debug purposes only
		 *
		 * We only consider an interrupt as unsolicited, if the device was not
		 *  actively in use (busy) and an interrupt other than an ALERT status
		 *  was received.
		 *
		 * Note: We must not issue a message to the console, if the
		 *       unsolicited interrupt applies to the console device
		 *       itself !
		 */
		if (!(stctl & SCSW_STCTL_ALERT_STATUS)
		    && (ioinfo[irq]->ui.flags.busy == 0)) {

#ifdef CONFIG_DEBUG_IO
			if (irq != cons_dev)
				printk (KERN_INFO
					"Unsolicited interrupt received for "
					"device %04X on subchannel %04X\n"
					" ... device status : %02X "
					"subchannel status : %02X\n",
					dp->devno, irq, dp->dstat, dp->cstat);

			sprintf (buffer, "s390_process_IRQ(%04X) - irb for "
				 "device %04X, ending_status %d\n",
				 irq, dp->devno, ending_status);

			s390_displayhex (buffer, &(dp->ii.irb), sizeof (irb_t));
#endif
			CIO_MSG_EVENT(2,
				      "Unsolicited interrupt "
				      "received for device %04X "
				      "on subchannel %04X\n"
				      " ... device status : %02X "
				      "subchannel status : %02X\n",
				      dp->devno,
				      irq, dp->dstat, dp->cstat);
			sprintf(dbf_txt, "uint%x", irq);
			CIO_TRACE_EVENT(2, dbf_txt);
			CIO_HEX_EVENT(2, &(dp->ii.irb), sizeof (irb_t));
		}

		/*
		 * take fast exit if no handler is available
		 */
		if (!ioinfo[irq]->ui.flags.ready)
			return (ending_status);

		/*
		 * Check whether we must issue a SENSE CCW ourselves if there is no
		 *  concurrent sense facility installed for the subchannel.
		 *
		 * Note: We should check for ioinfo[irq]->ui.flags.consns but VM
		 *       violates the ESA/390 architecture and doesn't present an
		 *       operand exception for virtual devices without concurrent
		 *       sense facility available/supported when enabling the
		 *       concurrent sense facility.
		 */
		if (((dp->ii.irb.scsw.dstat & DEV_STAT_UNIT_CHECK)
		     && (!issense))
		    || (ioinfo[irq]->ui.flags.delsense && ending_status)) {
			int ret_io;
			ccw1_t *s_ccw = &ioinfo[irq]->senseccw;
			unsigned long s_flag = 0;

			if (ending_status) {
				/* there is a chance that the command
				 * that gave us the unit check actually
				 * was a basic sense, so we must not
				 * overwrite *udp in that case
				 */
				if (ioinfo[irq]->ui.flags.w4sense &&
					(dp->ii.irb.scsw.dstat & DEV_STAT_UNIT_CHECK)) {
					CIO_MSG_EVENT(4,"double unit check irq %04x, dstat %02x," 
							"flags %8x\n", irq, dp->ii.irb.scsw.dstat,
							 ioinfo[irq]->ui.info, ending_status);
				} else {
				/*
				 * We copy the current status information into the device driver
				 *  status area. Then we can use the local devstat area for device
				 *  sensing. When finally calling the IRQ handler we must not overlay
				 *  the original device status but copy the sense data only.
				 */
					memcpy (udp, dp, sizeof (devstat_t));
				}

				s_ccw->cmd_code = CCW_CMD_BASIC_SENSE;
				s_ccw->cda =
				    (__u32) virt_to_phys (ioinfo[irq]->
							  sense_data);
				s_ccw->count = SENSE_MAX_COUNT;
				s_ccw->flags = CCW_FLAG_SLI;

				/*
				 * If free_irq() or a sync do_IO/s390_start_IO() is in
				 *  process we have to sense synchronously
				 */
				if (ioinfo[irq]->ui.flags.unready
				    || ioinfo[irq]->ui.flags.syncio)
					s_flag = DOIO_WAIT_FOR_INTERRUPT
						|  DOIO_TIMEOUT
						|  DOIO_VALID_LPM;

				else
					s_flag = DOIO_VALID_LPM;

				/*
				 * Reset status info
				 *
				 * It does not matter whether this is a sync. or async.
				 *  SENSE request, but we have to assure we don't call
				 *  the irq handler now, but keep the irq in busy state.
				 *  In sync. mode s390_process_IRQ() is called recursively,
				 *  while in async. mode we re-enter do_IRQ() with the
				 *  next interrupt.
				 *
				 * Note : this may be a delayed sense request !
				 */
				allow4handler = 0;

				ioinfo[irq]->ui.flags.fast = 0;
				ioinfo[irq]->ui.flags.repall = 0;
				ioinfo[irq]->ui.flags.w4final = 0;
				ioinfo[irq]->ui.flags.delsense = 0;

				dp->cstat = 0;
				dp->dstat = 0;
				dp->rescnt = SENSE_MAX_COUNT;

				ioinfo[irq]->ui.flags.w4sense = 1;

				ret_io = s390_start_IO (irq, s_ccw, 0xE2C5D5E2,	/* = SENSe */
							0xff,
							s_flag);
				switch (ret_io) {
				case 0: /* OK */
					break;
				case -ENODEV:
					/*
					 * The device is no longer operational.
					 * We won't get any sense data.
					 */
					ioinfo[irq]->ui.flags.w4sense = 0;
					ioinfo[irq]->ui.flags.oper = 0;
					allow4handler = 1; /* to notify the driver */
					break;
				case -EBUSY:
					/*
					 * The channel subsystem is either busy, or we have
					 * a status pending. Retry later.
					 */
					ioinfo[irq]->ui.flags.w4sense = 0;
					ioinfo[irq]->ui.flags.delsense = 1;
					break;
				default:
					printk(KERN_ERR"irq %04X: Unexpected rc %d "
					       "for BASIC SENSE!\n", irq, ret_io);
					ioinfo[irq]->ui.flags.w4sense = 0;
					allow4handler = 1;
				}
			} else {
				/*
				 * we received an Unit Check but we have no final
				 *  status yet, therefore we must delay the SENSE
				 *  processing. However, we must not report this
				 *  intermediate status to the device interrupt
				 *  handler.
				 */
				ioinfo[irq]->ui.flags.fast = 0;
				ioinfo[irq]->ui.flags.repall = 0;

				ioinfo[irq]->ui.flags.delsense = 1;
				allow4handler = 0;

			}

		}

		/*
		 * we allow for the device action handler if .
		 *  - we received ending status
		 *  - the action handler requested to see all interrupts
		 *  - we received an intermediate status
		 *  - fast notification was requested (primary status)
		 *  - unsollicited interrupts
		 *
		 */
		if (allow4handler) {
			allow4handler = ending_status
			    || (ioinfo[irq]->ui.flags.repall)
			    || (stctl & SCSW_STCTL_INTER_STATUS)
			    || ((ioinfo[irq]->ui.flags.fast)
				&& (stctl & SCSW_STCTL_PRIM_STATUS))
			    || (ioinfo[irq]->ui.flags.oper == 0);

		}

		/*
		 * We used to copy the device status information right before
		 *  calling the device action handler. However, in status
		 *  pending situations during do_IO() or halt_IO(), as well as
		 *  enable_subchannel/disable_subchannel processing we must
		 *  synchronously return the status information and must not
		 *  call the device action handler.
		 *
		 */
		if (allow4handler) {
			/*
			 * if we were waiting for sense data we copy the sense
			 *  bytes only as the original status information was
			 *  saved prior to sense already.
			 */
			if (ioinfo[irq]->ui.flags.w4sense) {
				int sense_count =
				    SENSE_MAX_COUNT -
				    ioinfo[irq]->devstat.rescnt;

#ifdef CONFIG_DEBUG_IO
				if (irq != cons_dev)
					printk (KERN_DEBUG
						"s390_process_IRQ( %04X ) : "
						"BASIC SENSE bytes avail %d\n",
						irq, sense_count);
#endif
				CIO_MSG_EVENT(4,
					      "s390_process_IRQ( %04X ): "
					      "BASIC SENSE bytes avail %d\n",
					      irq, sense_count);
				ioinfo[irq]->ui.flags.w4sense = 0;
				udp->flag |= DEVSTAT_FLAG_SENSE_AVAIL;
				udp->scnt = sense_count;

				if (sense_count > 0) {
					memcpy (udp->ii.sense.data,
						ioinfo[irq]->sense_data,
						sense_count);
				} else if (sense_count == 0) {
					udp->flag &= ~DEVSTAT_FLAG_SENSE_AVAIL;
				} else {
					panic
					    ("s390_process_IRQ(%04x) encountered "
					     "negative sense count\n", irq);

				}
			} else {
				memcpy (udp, dp, sdevstat);

			}

		}

		/*
		 * for status pending situations other than deferred interrupt
		 *  conditions detected by s390_process_IRQ() itself we must not
		 *  call the handler. This will synchronously be reported back
		 *  to the caller instead, e.g. when detected during do_IO().
		 */
		if (ioinfo[irq]->ui.flags.s_pend
		    || ioinfo[irq]->ui.flags.unready
		    || ioinfo[irq]->ui.flags.repnone) {
			if (ending_status) {

				ioinfo[irq]->ui.flags.busy = 0;
				ioinfo[irq]->ui.flags.doio = 0;
				ioinfo[irq]->ui.flags.haltio = 0;
				ioinfo[irq]->ui.flags.fast = 0;
				ioinfo[irq]->ui.flags.repall = 0;
				ioinfo[irq]->ui.flags.w4final = 0;

				dp->flag |= DEVSTAT_FINAL_STATUS;
				udp->flag |= DEVSTAT_FINAL_STATUS;

			}

			allow4handler = 0;

		}

		/*
		 * Call device action handler if applicable
		 */
		if (allow4handler) {

			/*
			 *  We only reset the busy condition when we are sure that no further
			 *   interrupt is pending for the current I/O request (ending_status).
			 */
			if (ending_status || !ioinfo[irq]->ui.flags.oper) {
				ioinfo[irq]->ui.flags.oper = 1;	/* dev IS oper */

				ioinfo[irq]->ui.flags.busy = 0;
				ioinfo[irq]->ui.flags.doio = 0;
				ioinfo[irq]->ui.flags.haltio = 0;
				ioinfo[irq]->ui.flags.fast = 0;
				ioinfo[irq]->ui.flags.repall = 0;
				ioinfo[irq]->ui.flags.w4final = 0;

				dp->flag |= DEVSTAT_FINAL_STATUS;
				udp->flag |= DEVSTAT_FINAL_STATUS;

				if (!ioinfo[irq]->ui.flags.killio)
					ioinfo[irq]->irq_desc.handler (irq, udp, NULL);

				/*
				 * reset intparm after final status or we will badly present unsolicited
				 *  interrupts with a intparm value possibly no longer valid.
				 */
				dp->intparm = 0;

			} else {
				ioinfo[irq]->ui.flags.w4final = 1;

				/*
				 * Eventually reset subchannel PCI status and
				 *  set the PCI or SUSPENDED flag in the user
				 *  device status block if appropriate.
				 */
				if (dp->cstat & SCHN_STAT_PCI) {
					udp->flag |= DEVSTAT_PCI;
					dp->cstat &= ~SCHN_STAT_PCI;
				}

				if (actl & SCSW_ACTL_SUSPENDED) {
					udp->flag |= DEVSTAT_SUSPENDED;

				}
				
				ioinfo[irq]->irq_desc.handler (irq, udp, NULL);

			}

		}

		break;

	case 3:		/* device/path not operational */

		ioinfo[irq]->ui.flags.busy = 0;
		ioinfo[irq]->ui.flags.doio = 0;
		ioinfo[irq]->ui.flags.haltio = 0;

		dp->cstat = 0;
		dp->dstat = 0;

 		if ((dp->ii.irb.scsw.fctl != 0) &&
 		    ((dp->ii.irb.scsw.stctl & SCSW_STCTL_STATUS_PEND) != 0) &&
 		    (((dp->ii.irb.scsw.stctl & SCSW_STCTL_INTER_STATUS) == 0) ||
 		     ((dp->ii.irb.scsw.actl & SCSW_ACTL_SUSPENDED) != 0)))
 			if (dp->ii.irb.scsw.pno) {
 				stsch(irq, &ioinfo[irq]->schib);
 				ioinfo[irq]->opm &=
 					~ioinfo[irq]->schib.pmcw.pnom;
 			}

		if (ioinfo[irq]->opm == 0) {
			ioinfo[irq]->ui.flags.oper = 0;

		}

		ioinfo[irq]->devstat.flag |= DEVSTAT_NOT_OPER;
		ioinfo[irq]->devstat.flag |= DEVSTAT_FINAL_STATUS;


               /*
                * When we find a device "not oper" we save the status
                *  information into the device status area and call the
                *  device specific interrupt handler.
                *
                * Note: currently we don't have any way to reenable
                *       the device unless an unsolicited interrupt
                *       is presented. We don't check for spurious
                *       interrupts on "not oper" conditions.
                */

		
		ioinfo[irq]->ui.flags.fast = 0;
		ioinfo[irq]->ui.flags.repall = 0;
		ioinfo[irq]->ui.flags.w4final = 0;

		/*
		 * take fast exit if no handler is available
		 */
		if (!ioinfo[irq]->ui.flags.ready)
			return (ending_status);

		memcpy (udp, &(ioinfo[irq]->devstat), sdevstat);

		ioinfo[irq]->devstat.intparm = 0;

		if (!ioinfo[irq]->ui.flags.s_pend
		    && !ioinfo[irq]->ui.flags.repnone
		    && !ioinfo[irq]->ui.flags.killio) {

			ioinfo[irq]->irq_desc.handler (irq, udp, NULL);
		}

		ending_status = 1;

		break;

	}

	if (ending_status && 
	    ioinfo[irq]->ui.flags.noio &&
	    !ioinfo[irq]->ui.flags.syncio &&
	    !ioinfo[irq]->ui.flags.w4sense) {
		if(ioinfo[irq]->ui.flags.ready) {
			s390_schedule_path_verification(irq);
		} else {
			ioinfo[irq]->ui.flags.killio = 0;
			ioinfo[irq]->ui.flags.noio = 0;
		}
	}

	return (ending_status);
}

/*
 * Set the special i/o-interruption sublass 7 for the
 *  device specified by parameter irq. There can only
 *  be a single device been operated on this special
 *  isc. This function is aimed being able to check
 *  on special device interrupts in disabled state,
 *  without having to delay I/O processing (by queueing)
 *  for non-console devices.
 *
 * Setting of this isc is done by set_cons_dev(). 
 *  wait_cons_dev() allows
 *  to actively wait on an interrupt for this device in
 *  disabed state. When the interrupt condition is
 *  encountered, wait_cons_dev(9 calls do_IRQ() to have
 *  the console device driver processing the interrupt.
 */
int
set_cons_dev (int irq)
{
	int ccode;
	int rc = 0;
	char dbf_txt[15];

	SANITY_CHECK (irq);

	if (cons_dev != -1)
		return -EBUSY;

	sprintf (dbf_txt, "scons%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * modify the indicated console device to operate
	 *  on special console interrupt sublass 7
	 */
	ccode = stsch (irq, &(ioinfo[irq]->schib));

	if (ccode) {
		rc = -ENODEV;
		ioinfo[irq]->devstat.flag |= DEVSTAT_NOT_OPER;
	} else {
		ioinfo[irq]->schib.pmcw.isc = 7;

		ccode = msch (irq, &(ioinfo[irq]->schib));

		if (ccode) {
			rc = -EIO;
		} else {
			cons_dev = irq;

			/*
			 * enable console I/O-interrupt sublass 7
			 */
			ctl_set_bit (6, 24);

		}
	}

	return (rc);
}

int
wait_cons_dev (int irq)
{
	int rc = 0;
	long save_cr6;
	char dbf_txt[15];

	if (irq != cons_dev)
		return -EINVAL;

	sprintf (dbf_txt, "wcons%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * before entering the spinlock we may already have
	 *  processed the interrupt on a different CPU ...
	 */
	if (ioinfo[irq]->ui.flags.busy == 1) {
		long cr6 __attribute__ ((aligned (8)));

		/*
		 * disable all, but isc 7 (console device)
		 */
		__ctl_store (cr6, 6, 6);
		save_cr6 = cr6;
		cr6 &= 0x01FFFFFF;
		__ctl_load (cr6, 6, 6);

		do {
			tpi_info_t tpi_info = { 0, };
			if (tpi (&tpi_info) == 1) {
				s390_process_IRQ (tpi_info.irq);
			} else {
				s390irq_spin_unlock (irq);
				udelay (100);
				s390irq_spin_lock (irq);
			}
			eieio ();
		} while (ioinfo[irq]->ui.flags.busy == 1);

		/*
		 * restore previous isc value
		 */
		cr6 = save_cr6;
		__ctl_load (cr6, 6, 6);

	}

	return (rc);
}

int
enable_cpu_sync_isc (int irq)
{
	int ccode;
	long cr6 __attribute__ ((aligned (8)));

	int retry = 3;
	int rc = 0;
	char dbf_txt[15];

	sprintf (dbf_txt, "enisc%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/* This one spins until it can get the sync_isc lock for irq# irq */

	if ((irq <= highest_subchannel) && 
	    (ioinfo[irq] != INVALID_STORAGE_AREA) &&
	    (!ioinfo[irq]->st)) {
		if (atomic_read (&sync_isc) != irq)
			atomic_compare_and_swap_spin (-1, irq, &sync_isc);

		sync_isc_cnt++;

		if (sync_isc_cnt > 255) {	/* fixme : magic number */
			panic ("Too many recursive calls to enable_sync_isc");

		}
		/*
		 * we only run the STSCH/MSCH path for the first enablement
		 */
		else if (sync_isc_cnt == 1) {

			ccode = stsch (irq, &(ioinfo[irq]->schib));

			if (!ccode) {
				ioinfo[irq]->schib.pmcw.isc = 5;

				do {
					ccode = msch (irq,
						      &(ioinfo[irq]->schib));

					switch (ccode) {
					case 0:
						/*
						 * enable special isc
						 */
						__ctl_store (cr6, 6, 6);
						/* enable sync isc 5 */
						cr6 |= 0x04000000;
						/* disable standard isc 3 */
						cr6 &= 0xEFFFFFFF;
						/* disable console isc 7 */
						cr6 &= 0xFEFFFFFF;
						ioinfo[irq]->ui.flags.syncio = 1;
						__ctl_load (cr6, 6, 6);
						rc = 0;
						retry = 0;
						break;

					case 1:
						/*
						 * process pending status
						 */
						ioinfo[irq]->ui.flags.s_pend =
						    1;
						s390_process_IRQ (irq);
						ioinfo[irq]->ui.flags.s_pend =
						    0;

						rc = -EIO;	/* might be overwritten... */
						retry--;
						break;

					case 2:	/* busy */
						retry = 0;
						rc = -EBUSY;
						break;

					case 3:	/* not oper */
						retry = 0;
						rc = -ENODEV;
						break;

					}

				} while (retry);

			} else {
				rc = -ENODEV;	/* device is not-operational */

			}
		}

		if (rc) {	/* can only happen if stsch/msch fails */
			sync_isc_cnt = 0;
			atomic_set (&sync_isc, -1);
		} else if (sync_isc_cnt == 1) {
			int ccode;
			
			ccode = stsch(irq, &ioinfo[irq]->schib);
			if (!ccode && ioinfo[irq]->schib.pmcw.isc != 5) {
				ioinfo[irq]->ui.flags.syncio = 0;
				sync_isc_cnt = 0;
				atomic_set (&sync_isc, -1);
			}
		}
	} else {
#ifdef CONFIG_SYNC_ISC_PARANOIA
		panic ("enable_sync_isc: called with invalid %x\n", irq);
#endif

		rc = -EINVAL;

	}

	return (rc);
}

int
disable_cpu_sync_isc (int irq)
{
	int rc = 0;
	int retry1 = 5;
	int retry2 = 5;
	int clear_pend = 0;

	int ccode;
	long cr6 __attribute__ ((aligned (8)));

	char dbf_txt[15];

	sprintf (dbf_txt, "disisc%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	if ((irq <= highest_subchannel) && 
	    (ioinfo[irq] != INVALID_STORAGE_AREA) && 
	    (!ioinfo[irq]->st)) {
		/*
		 * We disable if we're the top user only, as we may
		 *  run recursively ... 
		 * We must not decrease the count immediately; during
		 *  msch() processing we may face another pending
		 *  status we have to process recursively (sync).
		 */

#ifdef CONFIG_SYNC_ISC_PARANOIA
		if (atomic_read (&sync_isc) != irq)
			panic
			    ("disable_sync_isc: called for %x while %x locked\n",
			     irq, atomic_read (&sync_isc));
#endif

		if (sync_isc_cnt == 1) {
			ccode = stsch (irq, &(ioinfo[irq]->schib));

			ioinfo[irq]->schib.pmcw.isc = 3;

			do {
				retry2 = 5;
				do {
					ccode =
					    msch (irq, &(ioinfo[irq]->schib));

					switch (ccode) {
					case 0:
						/*
						 * disable special interrupt subclass in CPU
						 */
						__ctl_store (cr6, 6, 6);
						/* disable sync isc 5 */
						cr6 &= 0xFBFFFFFF;
						/* enable standard isc 3 */
						cr6 |= 0x10000000;
						/* enable console isc 7 */
						cr6 |= 0x01000000;
						__ctl_load (cr6, 6, 6);

						retry2 = 0;
						break;

					case 1:	/* status pending */
						ioinfo[irq]->ui.flags.s_pend =
						    1;
						s390_process_IRQ (irq);
						ioinfo[irq]->ui.flags.s_pend =
						    0;

						retry2--;
						break;

					case 2:	/* busy */
						retry2--;
						udelay (100);	/* give it time */
						break;

					default:	/* not oper */
						retry2 = 0;
						break;
					}

				} while (retry2);

				retry1--;

				/* try stopping it ... */
				if ((ccode) && !clear_pend) {
					clear_IO (irq, 0x00004711, 0);
					clear_pend = 1;

				}

				udelay (100);

			} while (retry1 && ccode);

			ioinfo[irq]->ui.flags.syncio = 0;

			sync_isc_cnt = 0;
			atomic_set (&sync_isc, -1);
		
		} else {
			sync_isc_cnt--;

		}
	} else {
#ifdef CONFIG_SYNC_ISC_PARANOIA
		if (atomic_read (&sync_isc) != -1)
			panic
			    ("disable_sync_isc: called with invalid %x while %x locked\n",
			     irq, atomic_read (&sync_isc));
#endif

		rc = -EINVAL;

	}

	return (rc);
}

int diag210 (diag210_t *addr)
{
        int ccode;

        __asm__ __volatile__(
#ifdef CONFIG_ARCH_S390X
                "   sam31\n"
                "   diag  %1,0,0x210\n"
                "   sam64\n"
#else
                "   diag  %1,0,0x210\n"
#endif
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "a" (addr)
                : "cc" );
        return ccode;
}

/*
 * Input :
 *   devno - device number
 *   ps    - pointer to sense ID data area
 * Output : none
 */
void
VM_virtual_device_info (__u16 devno, senseid_t * ps)
{
	diag210_t *p_diag_data;
	int ccode;

	int error = 0;

	CIO_TRACE_EVENT (4, "VMvdinf");

	if (init_IRQ_complete) {
		p_diag_data = kmalloc (sizeof (diag210_t), GFP_DMA | GFP_ATOMIC);
	} else {
		p_diag_data = alloc_bootmem_low (sizeof (diag210_t));

	}
	if (!p_diag_data)
		return;

	p_diag_data->vrdcdvno = devno;
	p_diag_data->vrdclen = sizeof (diag210_t);
	ccode = diag210 ((diag210_t *) virt_to_phys (p_diag_data));
	ps->reserved = 0xff;

	switch (p_diag_data->vrdcvcla) {
	case 0x80:

		switch (p_diag_data->vrdcvtyp) {
		case 00:

			ps->cu_type = 0x3215;

			break;

		default:

			error = 1;

			break;

		}

		break;

	case 0x40:

		switch (p_diag_data->vrdcvtyp) {
		case 0xC0:

			ps->cu_type = 0x5080;

			break;

		case 0x80:

			ps->cu_type = 0x2250;

			break;

		case 0x04:

			ps->cu_type = 0x3277;

			break;

		case 0x01:

			ps->cu_type = 0x3278;

			break;

		default:

			error = 1;

			break;

		}

		break;

	case 0x20:

		switch (p_diag_data->vrdcvtyp) {
		case 0x84:

			ps->cu_type = 0x3505;

			break;

		case 0x82:

			ps->cu_type = 0x2540;

			break;

		case 0x81:

			ps->cu_type = 0x2501;

			break;

		default:

			error = 1;

			break;

		}

		break;

	case 0x10:

		switch (p_diag_data->vrdcvtyp) {
		case 0x84:

			ps->cu_type = 0x3525;

			break;

		case 0x82:

			ps->cu_type = 0x2540;

			break;

		case 0x4F:
		case 0x4E:
		case 0x48:

			ps->cu_type = 0x3820;

			break;

		case 0x4D:
		case 0x49:
		case 0x45:

			ps->cu_type = 0x3800;

			break;

		case 0x4B:

			ps->cu_type = 0x4248;

			break;

		case 0x4A:

			ps->cu_type = 0x4245;

			break;

		case 0x47:

			ps->cu_type = 0x3262;

			break;

		case 0x43:

			ps->cu_type = 0x3203;

			break;

		case 0x42:

			ps->cu_type = 0x3211;

			break;

		case 0x41:

			ps->cu_type = 0x1403;

			break;

		default:

			error = 1;

			break;

		}

		break;

	case 0x08:

		switch (p_diag_data->vrdcvtyp) {
		case 0x82:

			ps->cu_type = 0x3422;

			break;

		case 0x81:

			ps->cu_type = 0x3490;

			break;

		case 0x10:

			ps->cu_type = 0x3420;

			break;

		case 0x02:

			ps->cu_type = 0x3430;

			break;

		case 0x01:

			ps->cu_type = 0x3480;

			break;

		case 0x42:

			ps->cu_type = 0x3424;

			break;

		case 0x44:

			ps->cu_type = 0x9348;

			break;

		default:

			error = 1;

			break;

		}

		break;

	case 02:		/* special device class ... */

		switch (p_diag_data->vrdcvtyp) {
		case 0x20:	/* OSA */

			ps->cu_type = 0x3088;
			ps->cu_model = 0x60;

			break;

		default:

			error = 1;
			break;

		}

		break;

	default:

		error = 1;

		break;

	}

	if (init_IRQ_complete) {
		kfree (p_diag_data);
	} else {
		free_bootmem ((unsigned long) p_diag_data, sizeof (diag210_t));

	}

	if (error) {
		printk (KERN_ERR "DIAG X'210' for "
			"device %04X returned "
			"(cc = %d): vdev class : %02X, "
			"vdev type : %04X \n"
			" ...  rdev class : %02X, rdev type : %04X, "
			"rdev model: %02X\n",
			devno,
			ccode,
			p_diag_data->vrdcvcla,
			p_diag_data->vrdcvtyp,
			p_diag_data->vrdcrccl,
			p_diag_data->vrdccrty, p_diag_data->vrdccrmd);
		CIO_MSG_EVENT(0,
			      "DIAG X'210' for "
			      "device %04X returned "
			      "(cc = %d): vdev class : %02X, "
			      "vdev type : %04X \n ...  "
			      "rdev class : %02X, rdev type : %04X, "
			      "rdev model: %02X\n",
			      devno,
			      ccode,
			      p_diag_data->vrdcvcla,
			      p_diag_data->vrdcvtyp,
			      p_diag_data->vrdcrccl,
			      p_diag_data->vrdccrty,
			      p_diag_data->vrdccrmd);
		
	}
}

/*
 * This routine returns the characteristics for the device
 *  specified. Some old devices might not provide the necessary
 *  command code information during SenseID processing. In this
 *  case the function returns -EINVAL. Otherwise the function
 *  allocates a decice specific data buffer and provides the
 *  device characteristics together with the buffer size. Its
 *  the callers responability to release the kernel memory if
 *  not longer needed. In case of persistent I/O problems -EBUSY
 *  is returned.
 *
 *  The function may be called enabled or disabled. However, the
 *   caller must have locked the irq it is requesting data for.
 *
 * Note : It would have been nice to collect this information
 *         during init_IRQ() processing but this is not possible
 *
 *         a) without statically pre-allocation fixed size buffers
 *            as virtual memory management isn't available yet.
 *
 *         b) without unnecessarily increase system startup by
 *            evaluating devices eventually not used at all.
 */
int
read_dev_chars (int irq, void **buffer, int length)
{
	unsigned long flags;
	ccw1_t *rdc_ccw;
	devstat_t devstat;
	char *rdc_buf;
	int devflag = 0;

	int ret = 0;
	int emulated = 0;
	int retry = 5;

	char dbf_txt[15];

	if (!buffer || !length) {
		return (-EINVAL);

	}

	SANITY_CHECK (irq);

	if (ioinfo[irq]->ui.flags.oper == 0) {
		return (-ENODEV);

	}

 	if (ioinfo[irq]->ui.flags.unfriendly) {
 		/* don't even try it */
 		return -EUSERS;
 	}

	sprintf (dbf_txt, "rddevch%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * Before playing around with irq locks we should assure
	 *   running disabled on (just) our CPU. Sync. I/O requests
	 *   also require to run disabled.
	 *
	 * Note : as no global lock is required, we must not use
	 *        cli(), but __cli() instead.   
	 */
	__save_flags (flags);
	__cli ();

	rdc_ccw = &ioinfo[irq]->senseccw;

	if (!ioinfo[irq]->ui.flags.ready) {
		ret = request_irq (irq,
				   init_IRQ_handler, SA_PROBE, "RDC", &devstat);

		if (!ret) {
			emulated = 1;

		}

	}

	if (!ret) {
		if (!*buffer) {
			rdc_buf = kmalloc (length, GFP_KERNEL);
		} else {
			rdc_buf = *buffer;

		}

		if (!rdc_buf) {
			ret = -ENOMEM;
		} else {
			do {
				rdc_ccw->cmd_code = CCW_CMD_RDC;
				rdc_ccw->count = length;
				rdc_ccw->flags = CCW_FLAG_SLI;
				ret =
				    set_normalized_cda (rdc_ccw, rdc_buf);
				if (!ret) {

					memset (ioinfo[irq]->irq_desc.dev_id,
						'\0', sizeof (devstat_t));

					ret = s390_start_IO (irq, rdc_ccw, 0x00524443,	/* RDC */
							     0,	/* n/a */
							     DOIO_WAIT_FOR_INTERRUPT
							     |
							     DOIO_DONT_CALL_INTHDLR);
					retry--;
					devflag =
					    ioinfo[irq]->irq_desc.dev_id->flag;

					clear_normalized_cda (rdc_ccw);
				} else {
					udelay (100);	/* wait for recovery */
					retry--;
				}

			} while ((retry)
				 && (ret
				     || (devflag & DEVSTAT_STATUS_PENDING)));

		}

		if (!retry) {
			ret = (ret == -ENOMEM) ? -ENOMEM : -EBUSY;

		}

		__restore_flags (flags);

		/*
		 * on success we update the user input parms
		 */
		if (!ret) {
			*buffer = rdc_buf;

		}

		if (emulated) {
			free_irq (irq, &devstat);

		}

	} else {
		__restore_flags (flags);
	}

	return (ret);
}

/*
 *  Read Configuration data
 */
int
read_conf_data (int irq, void **buffer, int *length, __u8 lpm)
{
	unsigned long flags;
	int ciw_cnt;

	int found = 0;		/* RCD CIW found */
	int ret = 0;		/* return code */

	char dbf_txt[15];

	SANITY_CHECK (irq);

	if (!buffer || !length) {
		return (-EINVAL);
	} else if (ioinfo[irq]->ui.flags.oper == 0) {
		return (-ENODEV);
	} else if (ioinfo[irq]->ui.flags.esid == 0) {
		*buffer = NULL;
		*length = 0;
		return (-EOPNOTSUPP);

	}

 	if (ioinfo[irq]->ui.flags.unfriendly) {
 		/* don't even try it */
 		return -EUSERS;
 	}

	sprintf (dbf_txt, "rdconf%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * scan for RCD command in extended SenseID data
	 */

	for (ciw_cnt = 0; (found == 0) && (ciw_cnt < MAX_CIWS); ciw_cnt++) {
		if (ioinfo[irq]->senseid.ciw[ciw_cnt].ct == CIW_TYPE_RCD) {
			/*
			 * paranoia check ...
			 */
			if (ioinfo[irq]->senseid.ciw[ciw_cnt].cmd != 0
			    && ioinfo[irq]->senseid.ciw[ciw_cnt].count != 0) {
				found = 1;

			}

			break;

		}
	}

	if (found) {
		devstat_t devstat;	/* inline device status area */
		devstat_t *pdevstat;
		int ioflags;

		ccw1_t *rcd_ccw = &ioinfo[irq]->senseccw;
		char *rcd_buf = NULL;
		int emulated = 0;	/* no i/O handler installed */
		int retry = 5;	/* retry count */

		__save_flags (flags);
		__cli ();

		if (!ioinfo[irq]->ui.flags.ready) {
			pdevstat = &devstat;
			ret = request_irq (irq,
					   init_IRQ_handler,
					   SA_PROBE, "RCD", pdevstat);

			if (!ret) {
				emulated = 1;

			}	/* endif */
		} else {
			pdevstat = ioinfo[irq]->irq_desc.dev_id;

		}		/* endif */

		if (!ret) {
			if (init_IRQ_complete) {
				rcd_buf =
				    kmalloc (ioinfo[irq]->senseid.ciw[ciw_cnt].
					     count, GFP_DMA | GFP_ATOMIC);
			} else {
				rcd_buf =
				    alloc_bootmem_low (ioinfo[irq]->senseid.
						       ciw[ciw_cnt].count);

			}

			if (rcd_buf == NULL) {
				ret = -ENOMEM;

			}
			if (!ret) {
				memset (rcd_buf,
					'\0',
					ioinfo[irq]->senseid.ciw[ciw_cnt].
					count);

				do {
					rcd_ccw->cmd_code =
					    ioinfo[irq]->senseid.ciw[ciw_cnt].
					    cmd;
					rcd_ccw->cda =
					    (__u32) virt_to_phys (rcd_buf);
					rcd_ccw->count =
					    ioinfo[irq]->senseid.ciw[ciw_cnt].
					    count;
					rcd_ccw->flags = CCW_FLAG_SLI;

					memset (pdevstat, '\0',
						sizeof (devstat_t));

					if (lpm) {
						ioflags =
						    DOIO_WAIT_FOR_INTERRUPT |
						    DOIO_VALID_LPM |
						    DOIO_DONT_CALL_INTHDLR;
					} else {
						ioflags =
						    DOIO_WAIT_FOR_INTERRUPT |
						    DOIO_DONT_CALL_INTHDLR;

					}

					ret = s390_start_IO (irq, rcd_ccw, 0x00524344,	/* == RCD */
							     lpm, ioflags);
					switch (ret) {
					case 0:
					case -EIO:

						if (!
						    (pdevstat->
						     flag &
						     (DEVSTAT_STATUS_PENDING |
						      DEVSTAT_NOT_OPER |
						      DEVSTAT_FLAG_SENSE_AVAIL)))
						{
							retry = 0;	/* we got it ... */
						} else {
							retry--;	/* try again ... */

						}

						break;

					default:	/* -EBUSY, -ENODEV, ??? */
						retry = 0;

					}

				} while (retry);
			}
		}

		__restore_flags (flags);

		/*
		 * on success we update the user input parms
		 */
		if (ret == 0) {
			*length = ioinfo[irq]->senseid.ciw[ciw_cnt].count;
			*buffer = rcd_buf;
		} else {
			if (rcd_buf != NULL) {
				if (init_IRQ_complete) {
					kfree (rcd_buf);
				} else {
					free_bootmem ((unsigned long) rcd_buf,
						      ioinfo[irq]->senseid.
						      ciw[ciw_cnt].count);

				}

			}

			*buffer = NULL;
			*length = 0;

		}

		if (emulated)
			free_irq (irq, pdevstat);
	} else {
		*buffer = NULL;
		*length = 0;
		ret = -EOPNOTSUPP;

	}

	return (ret);

}

int
get_dev_info (int irq, s390_dev_info_t * pdi)
{
	return (get_dev_info_by_irq (irq, pdi));
}

static int __inline__
get_next_available_irq (ioinfo_t * pi)
{
	int ret_val = -ENODEV;

	while (pi != NULL) {
		if ((!pi->st) 
		    && (pi->ui.flags.oper)
		    && (!pi->ui.flags.unfriendly)) {
			ret_val = pi->irq;
			break;
		} else {
			pi = pi->next;
		}
	}

	return ret_val;
}

int
get_irq_first (void)
{
	int ret_irq;

	if (ioinfo_head) {
		if ((ioinfo_head->ui.flags.oper) && 
		    (!ioinfo_head->ui.flags.unfriendly) &&
		    (!ioinfo_head->st)) {
			ret_irq = ioinfo_head->irq;
		} else if (ioinfo_head->next) {
			ret_irq = get_next_available_irq (ioinfo_head->next);

		} else {
			ret_irq = -ENODEV;

		}
	} else {
		ret_irq = -ENODEV;

	}

	return ret_irq;
}

int
get_irq_next (int irq)
{
	int ret_irq;

	if (ioinfo[irq] != INVALID_STORAGE_AREA) {
		if (ioinfo[irq]->next) {
			if ((ioinfo[irq]->next->ui.flags.oper) &&
			    (!ioinfo[irq]->next->ui.flags.unfriendly) &&
			    (!ioinfo[irq]->next->st)) {
				ret_irq = ioinfo[irq]->next->irq;
			} else {
				ret_irq =
				    get_next_available_irq (ioinfo[irq]->next);

			}
		} else {
			ret_irq = -ENODEV;

		}
	} else {
		ret_irq = -EINVAL;

	}

	return ret_irq;
}

int
get_dev_info_by_irq (int irq, s390_dev_info_t * pdi)
{

	SANITY_CHECK (irq);

	if (pdi == NULL)
		return -EINVAL;

	pdi->devno = ioinfo[irq]->schib.pmcw.dev;
	pdi->irq = irq;

	if (ioinfo[irq]->ui.flags.oper && !ioinfo[irq]->ui.flags.unknown) {
		pdi->status = 0;
		memcpy (&(pdi->sid_data),
			&ioinfo[irq]->senseid, sizeof (senseid_t));
 
 	} else if (ioinfo[irq]->ui.flags.unfriendly) {
 		pdi->status = DEVSTAT_UNFRIENDLY_DEV;
 		memset (&(pdi->sid_data), '\0', sizeof (senseid_t));
 		pdi->sid_data.cu_type = 0xFFFF;

	} else if (ioinfo[irq]->ui.flags.unknown) {
		pdi->status = DEVSTAT_UNKNOWN_DEV;
		memset (&(pdi->sid_data), '\0', sizeof (senseid_t));
		pdi->sid_data.cu_type = 0xFFFF;

	} else {
		pdi->status = DEVSTAT_NOT_OPER;
		memset (&(pdi->sid_data), '\0', sizeof (senseid_t));
		pdi->sid_data.cu_type = 0xFFFF;

	}

	if (ioinfo[irq]->ui.flags.ready)
		pdi->status |= DEVSTAT_DEVICE_OWNED;

	return 0;
}

int
get_dev_info_by_devno (__u16 devno, s390_dev_info_t * pdi)
{
	int i;
	int rc = -ENODEV;

	if (devno > 0x0000ffff)
		return -ENODEV;
	if (pdi == NULL)
		return -EINVAL;

	for (i = 0; i <= highest_subchannel; i++) {

		if ((ioinfo[i] != INVALID_STORAGE_AREA) &&
		    (!ioinfo[i]->st) &&
		    (ioinfo[i]->schib.pmcw.dev == devno)) {

			pdi->irq = i;
			pdi->devno = devno;

			if (ioinfo[i]->ui.flags.oper
			    && !ioinfo[i]->ui.flags.unknown) {
				pdi->status = 0;
				memcpy (&(pdi->sid_data),
					&ioinfo[i]->senseid,
					sizeof (senseid_t));
 
 			} else if (ioinfo[i]->ui.flags.unfriendly) {
 				pdi->status = DEVSTAT_UNFRIENDLY_DEV;
 				memset (&(pdi->sid_data), '\0', 
 					sizeof (senseid_t));
 				pdi->sid_data.cu_type = 0xFFFF;
 

			} else if (ioinfo[i]->ui.flags.unknown) {
				pdi->status = DEVSTAT_UNKNOWN_DEV;

				memset (&(pdi->sid_data),
					'\0', sizeof (senseid_t));

				pdi->sid_data.cu_type = 0xFFFF;
			} else {
				pdi->status = DEVSTAT_NOT_OPER;

				memset (&(pdi->sid_data),
					'\0', sizeof (senseid_t));

				pdi->sid_data.cu_type = 0xFFFF;

			}

			if (ioinfo[i]->ui.flags.ready)
				pdi->status |= DEVSTAT_DEVICE_OWNED;

			if (!ioinfo[i]->ui.flags.unfriendly)
				rc = 0;	/* found */
			else
				rc = -EUSERS;
			break;

		}
	}

	return (rc);

}

int
get_irq_by_devno (__u16 devno)
{
	int i;
	int rc = -1;

	if (devno <= 0x0000ffff) {
		for (i = 0; i <= highest_subchannel; i++) {
			if ((ioinfo[i] != INVALID_STORAGE_AREA)
			    && (!ioinfo[i]->st)
			    && (ioinfo[i]->schib.pmcw.dev == devno)
			    && (ioinfo[i]->schib.pmcw.dnv == 1)) {
				rc = i;
				break;
			}
		}
	}

	return (rc);
}

unsigned int
get_devno_by_irq (int irq)
{

	if ((irq > highest_subchannel)
	    || (irq < 0)
	    || (ioinfo[irq] == INVALID_STORAGE_AREA)) {
		return -1;

	}

	if (ioinfo[irq]->st) 
		return -1;

	/*
	 * we don't need to check for the device be operational
	 *  as the initial STSCH will always present the device
	 *  number defined by the IOCDS regardless of the device
	 *  existing or not. However, there could be subchannels
	 *  defined who's device number isn't valid ...
	 */
	if (ioinfo[irq]->schib.pmcw.dnv)
		return (ioinfo[irq]->schib.pmcw.dev);
	else
		return -1;
}

/*
 * s390_device_recognition_irq
 *
 * Used for individual device recognition. Issues the device
 *  independant SenseID command to obtain info the device type.
 *
 */
void
s390_device_recognition_irq (int irq)
{
	int ret;
	char dbf_txt[15];

	sprintf (dbf_txt, "devrec%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * We issue the SenseID command on I/O subchannels we think are
	 *  operational only.
	 */
	if ((ioinfo[irq] != INVALID_STORAGE_AREA)
	    && (!ioinfo[irq]->st)
	    && (ioinfo[irq]->schib.pmcw.st == 0)
	    && (ioinfo[irq]->ui.flags.oper == 1)) {
		int irq_ret;
		devstat_t devstat;

		if (ioinfo[irq]->ui.flags.pgid_supp)
			irq_ret = request_irq (irq,
					       init_IRQ_handler,
					       SA_PROBE | SA_DOPATHGROUP,
					       "INIT", &devstat);
		else
			irq_ret = request_irq (irq,
					       init_IRQ_handler,
					       SA_PROBE, "INIT", &devstat);

		if (!irq_ret) {
			ret = enable_cpu_sync_isc (irq);

			if (!ret) {
				ioinfo[irq]->ui.flags.unknown = 0;
				
				memset (&ioinfo[irq]->senseid, '\0',
					sizeof (senseid_t));
				
				if (cio_sid_with_pgid) {
					
					ret = s390_DevicePathVerification(irq,0);
					
					if (ret == -EOPNOTSUPP) 
						/* 
						 * Doesn't prevent us from proceeding
						 */
						ret = 0;
				}
				
				/*
				 * we'll fallthrough here if we don't want
				 * to do SPID before SID
				 */
				if (!ret) {
					ret = s390_SenseID (irq, &ioinfo[irq]->senseid, 0xff);
					if (ret == -ETIMEDOUT) {
						/* SenseID timed out.
						 * We consider this device to be
						 * boxed for now.
						 */
						ioinfo[irq]->ui.flags.unfriendly = 1;
					}

#if 0				/* FIXME */
				/*
				 * We initially check the configuration data for
				 *  those devices with more than a single path
				 */
				if (ioinfo[irq]->schib.pmcw.pim != 0x80) {
					char *prcd;
					int lrcd;

					ret =
					    read_conf_data (irq,
							    (void **) &prcd,
							    &lrcd, 0);

					if (!ret)	// on success only ...
					{
						char buffer[80];
#ifdef CONFIG_DEBUG_IO
						sprintf (buffer,
							 "RCD for device(%04X)/"
							 "subchannel(%04X) returns :\n",
							 ioinfo[irq]->schib.
							 pmcw.dev, irq);

						s390_displayhex (buffer, prcd,
								 lrcd);
#endif
						CIO_TRACE_EVENT(2, "rcddata:");
						CIO_HEX_EVENT(2, prcd, lrcd);

						if (init_IRQ_complete) {
							kfree (prcd);
						} else {
							free_bootmem ((unsigned
								       long)
								      prcd,
								      lrcd);

						}
					}
				}
#endif
				}
				disable_cpu_sync_isc (irq);

			}

			free_irq (irq, &devstat);

		}			
	}
}

/*
 * s390_device_recognition_all
 *
 * Used for system wide device recognition.
 *
 */
void
s390_device_recognition_all (void)
{
	int irq = 0;		/* let's start with subchannel 0 ... */

	do {
		s390_device_recognition_irq (irq);

		irq++;

	} while (irq <= highest_subchannel);

}

/*
 * Function: s390_redo_validation
 * Look for no longer blacklisted devices
 * FIXME: there must be a better way to do this...
 */

void
s390_redo_validation (void)
{
	int irq = 0;
	int ret;

	CIO_TRACE_EVENT (0, "redoval");

	do {
		if (ioinfo[irq] == INVALID_STORAGE_AREA) {
			ret = s390_validate_subchannel (irq, 0);
			if (!ret) {
				s390_device_recognition_irq (irq);
				if (ioinfo[irq]->ui.flags.oper) {
					devreg_t *pdevreg;

					pdevreg =
					    s390_search_devreg (ioinfo[irq]);
					if (pdevreg != NULL) {
						if (pdevreg->oper_func != NULL)
							pdevreg->oper_func (irq,
									    pdevreg);

					}
				}
#ifdef CONFIG_PROC_FS
				if (cio_proc_devinfo)
					if (irq < MAX_CIO_PROCFS_ENTRIES) {
						cio_procfs_device_create (ioinfo
									  [irq]->
									  devno);
					}
#endif
			}
		}
		irq++;
	} while (irq <= highest_subchannel);
}


/*
 * s390_trigger_resense
 *
 * try to re-sense the device on subchannel irq
 * only to be called without interrupt handler
 */
int
s390_trigger_resense(int irq)
{

	SANITY_CHECK(irq);

	if (ioinfo[irq]->ui.flags.ready) {
		printk (KERN_WARNING "s390_trigger_resense(%04X): "
			"Device is in use!\n", irq);
		return -EBUSY;
	}

	/* 
	 * This function is called by dasd if it just executed a "steal lock".
	 * Therefore, re-initialize the 'unfriendly' flag to 0.
	 * We run into timeouts if the device is still boxed...
	 */
	ioinfo[irq]->ui.flags.unfriendly = 0;

	s390_device_recognition_irq(irq);

	return 0;
}

/*
 * s390_search_devices
 *
 * Determines all subchannels available to the system.
 *
 */
void
s390_process_subchannels (void)
{
	int ret;
	int irq = 0;		/* Evaluate all subchannels starting with 0 ... */

	do {
		ret = s390_validate_subchannel (irq, 0);

		if (ret != -ENXIO)
			irq++;

	} while ((ret != -ENXIO) && (irq < __MAX_SUBCHANNELS));

	highest_subchannel = (--irq);

	printk (KERN_INFO "Highest subchannel number detected (hex) : %04X\n",
		highest_subchannel);
	CIO_MSG_EVENT(0,
		      "Highest subchannel number detected "
		      "(hex) : %04X\n", highest_subchannel);
}

/*
 * s390_validate_subchannel()
 *
 * Process the subchannel for the requested irq. Returns 1 for valid
 *  subchannels, otherwise 0.
 */
int
s390_validate_subchannel (int irq, int enable)
{

	int retry;		/* retry count for status pending conditions */
	int ccode;		/* condition code for stsch() only */
	int ccode2;		/* condition code for other I/O routines */
	schib_t *p_schib;
	int ret;
#ifdef CONFIG_CHSC
	int      chp = 0;
	int      mask;
#endif /* CONFIG_CHSC */

	char dbf_txt[15];

	sprintf (dbf_txt, "valsch%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	/*
	 * The first subchannel that is not-operational (ccode==3)
	 *  indicates that there aren't any more devices available.
	 */
	if ((init_IRQ_complete)
	    && (ioinfo[irq] != INVALID_STORAGE_AREA)) {
		p_schib = &ioinfo[irq]->schib;
	} else {
		p_schib = p_init_schib;

	}

	/*
	 * If we knew the device before we assume the worst case ...    
	 */
	if (ioinfo[irq] != INVALID_STORAGE_AREA) {
		ioinfo[irq]->ui.flags.oper = 0;
		ioinfo[irq]->ui.flags.dval = 0;

	}

	ccode = stsch (irq, p_schib);

	if (ccode) {
		return -ENXIO;
	}
	/*
	 * ... just being curious we check for non I/O subchannels
	 */
	if (p_schib->pmcw.st) {
		if (cio_show_msg) {
			printk (KERN_INFO "Subchannel %04X reports "
				"non-I/O subchannel type %04X\n",
				irq, p_schib->pmcw.st);
		}
		CIO_MSG_EVENT(0,
			      "Subchannel %04X reports "
			      "non-I/O subchannel type %04X\n",
			      irq, p_schib->pmcw.st);

		if (ioinfo[irq] != INVALID_STORAGE_AREA)
			ioinfo[irq]->ui.flags.oper = 0;

	}

	if ((!p_schib->pmcw.dnv) && (!p_schib->pmcw.st)) {
		return -ENODEV;
	}
	if (!p_schib->pmcw.st) {
		if (is_blacklisted (p_schib->pmcw.dev)) {
			/* 
			 * This device must not be known to Linux. So we simply say that 
			 * there is no device and return ENODEV.
			 */
#ifdef CONFIG_DEBUG_IO
			printk (KERN_DEBUG
				"Blacklisted device detected at devno %04X\n",
				p_schib->pmcw.dev);
#endif
			CIO_MSG_EVENT(0,
				      "Blacklisted device detected at devno %04X\n",
				      p_schib->pmcw.dev);
			return -ENODEV;
		}
	}
	
	if (ioinfo[irq] == INVALID_STORAGE_AREA) {
		if (!init_IRQ_complete) {
			ioinfo[irq] = (ioinfo_t *)
			    alloc_bootmem_low (sizeof (ioinfo_t));
		} else {
			ioinfo[irq] = (ioinfo_t *)
			    kmalloc (sizeof (ioinfo_t), GFP_DMA | GFP_ATOMIC);

		}
		if (!ioinfo[irq])
			return -ENOMEM;
			

		memset (ioinfo[irq], '\0', sizeof (ioinfo_t));
		memcpy (&ioinfo[irq]->schib, p_init_schib, sizeof (schib_t));

		/*
		 * We have to insert the new ioinfo element
		 *  into the linked list, either at its head,
		 *  its tail or insert it.
		 */
		if (ioinfo_head == NULL) {	/* first element */
			ioinfo_head = ioinfo[irq];
			ioinfo_tail = ioinfo[irq];
		} else if (irq < ioinfo_head->irq) {	/* new head */
			ioinfo[irq]->next = ioinfo_head;
			ioinfo_head->prev = ioinfo[irq];
			ioinfo_head = ioinfo[irq];
		} else if (irq > ioinfo_tail->irq) {	/* new tail */
			ioinfo_tail->next = ioinfo[irq];
			ioinfo[irq]->prev = ioinfo_tail;
			ioinfo_tail = ioinfo[irq];
		} else {	/* insert element */

			ioinfo_t *pi = ioinfo_head;

			for (pi = ioinfo_head; pi != NULL; pi = pi->next) {

				if (irq < pi->next->irq) {
					ioinfo[irq]->next = pi->next;
					ioinfo[irq]->prev = pi;
					pi->next->prev = ioinfo[irq];
					pi->next = ioinfo[irq];
					break;

				}
			}
		}
	}

	/* initialize some values ... */
	ioinfo[irq]->irq = irq;
	ioinfo[irq]->st = ioinfo[irq]->schib.pmcw.st;
	if (ioinfo[irq]->st)
		return -ENODEV;

	ioinfo[irq]->opm = ioinfo[irq]->schib.pmcw.pim
	    & ioinfo[irq]->schib.pmcw.pam & ioinfo[irq]->schib.pmcw.pom;

#ifdef CONFIG_CHSC
	if (ioinfo[irq]->opm) {
		for (chp=0;chp<=7;chp++) {
			mask = 0x80 >> chp;
			if (ioinfo[irq]->opm & mask) {
				if (!test_bit
				    (ioinfo[irq]->schib.pmcw.chpid[chp], 
				     &chpids_logical)) {
					/* disable using this path */
					ioinfo[irq]->opm &= ~mask;
				}
			} else {
				/* This chpid is not available to us */
				clear_bit(ioinfo[irq]->schib.pmcw.chpid[chp],
					  &chpids);
			}
		}
	}
#endif /* CONFIG_CHSC */

	if (cio_show_msg) {
		printk (KERN_INFO
			"Detected device %04X "
			"on subchannel %04X"
			" - PIM = %02X, PAM = %02X, POM = %02X\n",
			ioinfo[irq]->schib.pmcw.dev,
			irq,
			ioinfo[irq]->schib.pmcw.pim,
			ioinfo[irq]->schib.pmcw.pam,
			ioinfo[irq]->schib.pmcw.pom);

	}
	CIO_MSG_EVENT(0,
		      "Detected device %04X "
		      "on subchannel %04X"
		      " - PIM = %02X, "
		      "PAM = %02X, POM = %02X\n",
		      ioinfo[irq]->schib.pmcw.dev, 
		      irq,
		      ioinfo[irq]->schib.pmcw.pim,
		      ioinfo[irq]->schib.pmcw.pam, 
		      ioinfo[irq]->schib.pmcw.pom);

	/*
	 * initialize ioinfo structure
	 */
	if (!ioinfo[irq]->ui.flags.ready) {
		ioinfo[irq]->nopfunc = NULL;
		ioinfo[irq]->ui.flags.busy = 0;
		ioinfo[irq]->ui.flags.dval = 1;
		ioinfo[irq]->devstat.intparm = 0;

	}
	ioinfo[irq]->devstat.devno = ioinfo[irq]->schib.pmcw.dev;
	ioinfo[irq]->devno = ioinfo[irq]->schib.pmcw.dev;

	/*
	 * We should have at least one CHPID ...
	 */
	if (ioinfo[irq]->opm) {
		/*
		 * We now have to initially ...
		 *  ... set "interruption sublass"
		 *  ... enable "concurrent sense"
		 *  ... enable "multipath mode" if more than one
		 *        CHPID is available. This is done regardless
		 *        whether multiple paths are available for us.
		 *
		 * Note : we don't enable the device here, this is temporarily
		 *        done during device sensing below.
		 */
		ioinfo[irq]->schib.pmcw.isc = 3;	/* could be smth. else */
		ioinfo[irq]->schib.pmcw.csense = 1;	/* concurrent sense */
		ioinfo[irq]->schib.pmcw.ena = enable;
		ioinfo[irq]->schib.pmcw.intparm = ioinfo[irq]->schib.pmcw.dev;

		if ((ioinfo[irq]->opm != 0x80)
		    && (ioinfo[irq]->opm != 0x40)
		    && (ioinfo[irq]->opm != 0x20)
		    && (ioinfo[irq]->opm != 0x10)
		    && (ioinfo[irq]->opm != 0x08)
		    && (ioinfo[irq]->opm != 0x04)
		    && (ioinfo[irq]->opm != 0x02)
		    && (ioinfo[irq]->opm != 0x01)) {
			ioinfo[irq]->schib.pmcw.mp = 1;	/* multipath mode */

		}

		retry = 5;

		do {
			ccode2 = msch_err (irq, &ioinfo[irq]->schib);

			switch (ccode2) {
			case 0:
				/*
				 * successful completion
				 *
				 * concurrent sense facility available
				 */
				ioinfo[irq]->ui.flags.oper = 1;
				ioinfo[irq]->ui.flags.consns = 1;
				ret = 0;
				break;

			case 1:
				/*
				 * status pending
				 *
				 * How can we have a pending status 
				 * as the device is disabled for 
				 * interrupts ?
				 * Anyway, process it ...
				 */
				ioinfo[irq]->ui.flags.s_pend = 1;
				s390_process_IRQ (irq);
				ioinfo[irq]->ui.flags.s_pend = 0;
				retry--;
				ret = -EIO;
				break;

			case 2:
				/*
				 * busy
				 *
				 * we mark it not-oper as we can't 
				 * properly operate it !
				 */
				ioinfo[irq]->ui.flags.oper = 0;
				udelay (100);	/* allow for recovery */
				retry--;
				ret = -EBUSY;
				break;

			case 3:	/* not operational */
				ioinfo[irq]->ui.flags.oper = 0;
				retry = 0;
				ret = -ENODEV;
				break;

			default:
#define PGMCHK_OPERAND_EXC      0x15

				if ((ccode2 & PGMCHK_OPERAND_EXC)
				    == PGMCHK_OPERAND_EXC) {
					/*
					 * re-issue the modify subchannel without trying to
					 *  enable the concurrent sense facility
					 */
					ioinfo[irq]->schib.pmcw.csense = 0;

					ccode2 =
					    msch_err (irq, &ioinfo[irq]->schib);

					if (ccode2 != 0) {
						printk (KERN_ERR
							" ... msch() (2) failed"
							" with CC = %X\n",
							ccode2);
						CIO_MSG_EVENT(0,
							      "msch() (2) failed"
							      " with CC=%X\n",
							      ccode2);
						ioinfo[irq]->ui.flags.oper = 0;
						ret = -EIO;
					} else {
						ioinfo[irq]->ui.flags.oper = 1;
						ioinfo[irq]->ui.
						    flags.consns = 0;
						ret = 0;

					}

				} else {
					printk (KERN_ERR
						" ... msch() (1) failed with "
						"CC = %X\n", ccode2);
					CIO_MSG_EVENT(0,
						      "msch() (1) failed with "
						      "CC = %X\n", ccode2);
					ioinfo[irq]->ui.flags.oper = 0;
					ret = -EIO;

				}

				retry = 0;
				break;

			}

		} while (ccode2 && retry);

		if ((ccode2 != 0) && (ccode2 != 3)
		    && (!retry)) {
			printk (KERN_ERR
				" ... msch() retry count for "
				"subchannel %04X exceeded, CC = %d\n",
				irq, ccode2);
			CIO_MSG_EVENT(0,
				      " ... msch() retry count for "
				      "subchannel %04X exceeded, CC = %d\n",
				      irq, ccode2);

		}
	} else {
		/* no path available ... */
		ioinfo[irq]->ui.flags.oper = 0;
		ret = -ENODEV;

	}

	return (ret);
}

/*
 * s390_SenseID
 *
 * Try to obtain the 'control unit'/'device type' information
 *  associated with the subchannel.
 *
 * The function is primarily meant to be called without irq
 *  action handler in place. However, it also allows for
 *  use with an action handler in place. If there is already
 *  an action handler registered assure it can handle the
 *  s390_SenseID() related device interrupts - interruption
 *  parameter used is 0x00E2C9C4 ( SID ).
 */
int
s390_SenseID (int irq, senseid_t * sid, __u8 lpm)
{
	ccw1_t *sense_ccw;	/* ccw area for SenseID command */
	senseid_t isid;		/* internal sid */
	devstat_t devstat;	/* required by request_irq() */
	__u8 pathmask;		/* calulate path mask */
	__u8 domask;		/* path mask to use */
	int inlreq;		/* inline request_irq() */
	int irq_ret;		/* return code */
	devstat_t *pdevstat;	/* ptr to devstat in use */
	int retry;		/* retry count */
	int io_retry;		/* retry indicator */

	senseid_t *psid = sid;	/* start with the external buffer */
	int sbuffer = 0;	/* switch SID data buffer */

	char dbf_txt[15];
	int i;
	int failure = 0;	/* nothing went wrong yet */

	SANITY_CHECK (irq);

	if (ioinfo[irq]->ui.flags.oper == 0) {
		return (-ENODEV);

	}

 	if (ioinfo[irq]->ui.flags.unfriendly) {
 		/* don't even try it */
 		return -EUSERS;
 	}

	sprintf (dbf_txt, "snsID%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	inlreq = 0;		/* to make the compiler quiet... */

	if (!ioinfo[irq]->ui.flags.ready) {

		pdevstat = &devstat;

		/*
		 * Perform SENSE ID command processing. We have to request device
		 *  ownership and provide a dummy I/O handler. We issue sync. I/O
		 *  requests and evaluate the devstat area on return therefore
		 *  we don't need a real I/O handler in place.
		 */
		irq_ret =
		    request_irq (irq, init_IRQ_handler, SA_PROBE, "SID",
				 &devstat);

		if (irq_ret == 0)
			inlreq = 1;
	} else {
		inlreq = 0;
		irq_ret = 0;
		pdevstat = ioinfo[irq]->irq_desc.dev_id;

	}

	if (irq_ret) {
		return irq_ret;
	}

	s390irq_spin_lock (irq);

	if (init_IRQ_complete) {
		sense_ccw = kmalloc (2 * sizeof (ccw1_t), GFP_DMA | GFP_ATOMIC);
	} else {
		sense_ccw = alloc_bootmem_low (2 * sizeof (ccw1_t));

	}
	if (!sense_ccw) {
		s390irq_spin_unlock (irq);
		if (inlreq)
			free_irq (irq, &devstat);
		return -ENOMEM;
	}

	/* more than one path installed ? */
	if (ioinfo[irq]->schib.pmcw.pim != 0x80) {
		sense_ccw[0].cmd_code = CCW_CMD_SUSPEND_RECONN;
		sense_ccw[0].cda = 0;
		sense_ccw[0].count = 0;
		sense_ccw[0].flags = CCW_FLAG_SLI | CCW_FLAG_CC;

		sense_ccw[1].cmd_code = CCW_CMD_SENSE_ID;
		sense_ccw[1].cda = (__u32) virt_to_phys (sid);
		sense_ccw[1].count = sizeof (senseid_t);
		sense_ccw[1].flags = CCW_FLAG_SLI;
	} else {
		sense_ccw[0].cmd_code = CCW_CMD_SENSE_ID;
		sense_ccw[0].cda = (__u32) virt_to_phys (sid);
		sense_ccw[0].count = sizeof (senseid_t);
		sense_ccw[0].flags = CCW_FLAG_SLI;

	}

	for (i = 0; (i < 8); i++) {
		pathmask = 0x80 >> i;

		domask = ioinfo[irq]->opm & pathmask;

		if (lpm)
			domask &= lpm;

		if (!domask)
			continue;

		failure = 0;

		memset(psid, 0, sizeof(senseid_t));
		psid->cu_type = 0xFFFF;	/* initialize fields ... */

		retry = 5;	/* retry count    */
		io_retry = 1;	/* enable retries */

		/*
		 * We now issue a SenseID request. In case of BUSY,
		 *  STATUS PENDING or non-CMD_REJECT error conditions
		 *  we run simple retries.
		 */
		do {
			memset (pdevstat, '\0', sizeof (devstat_t));

			irq_ret = s390_start_IO (irq, sense_ccw, 0x00E2C9C4,	/* == SID */
						 domask,
						 DOIO_WAIT_FOR_INTERRUPT
						 | DOIO_TIMEOUT
						 | DOIO_VALID_LPM
						 | DOIO_DONT_CALL_INTHDLR);

			if ((psid->cu_type != 0xFFFF)
			    && (psid->reserved == 0xFF)) {
				if (!sbuffer) {	/* switch buffers */
					/*
					 * we report back the
					 *  first hit only
					 */
					psid = &isid;

					if (ioinfo[irq]->schib.pmcw.pim != 0x80) {
						sense_ccw[1].cda = (__u32)
						    virt_to_phys (psid);
					} else {
						sense_ccw[0].cda = (__u32)
						    virt_to_phys (psid);

					}

					/*
					 * if just the very first
					 *  was requested to be
					 *  sensed disable further
					 *  scans.
					 */
					if (!lpm)
						lpm = domask;

					sbuffer = 1;

				}

				if (pdevstat->rescnt < (sizeof (senseid_t) - 8)) {
					ioinfo[irq]->ui.flags.esid = 1;

				}

				io_retry = 0;

				break;
			}

			failure = 1;

			if (pdevstat->flag & DEVSTAT_STATUS_PENDING) {
#ifdef CONFIG_DEBUG_IO
				printk (KERN_DEBUG
					"SenseID : device %04X on "
					"Subchannel %04X "
					"reports pending status, "
					"retry : %d\n",
					ioinfo[irq]->schib.pmcw.dev, irq,
					retry);
#endif
				CIO_MSG_EVENT(2,
					      "SenseID : device %04X on "
					      "Subchannel %04X "
					      "reports pending status, "
					      "retry : %d\n",
					      ioinfo
					      [irq]->schib.pmcw.dev, irq, retry);
			}

			else if (pdevstat->flag & DEVSTAT_FLAG_SENSE_AVAIL) {
				/*
				 * if the device doesn't support the SenseID
				 *  command further retries wouldn't help ...
				 */
				if (pdevstat->ii.sense.data[0]
				    & (SNS0_CMD_REJECT | SNS0_INTERVENTION_REQ)) {
#ifdef CONFIG_DEBUG_IO
					printk (KERN_ERR
						"SenseID : device %04X on "
						"Subchannel %04X "
						"reports cmd reject or "
						"intervention required\n",
						ioinfo[irq]->schib.pmcw.dev,
						irq);
#endif
					CIO_MSG_EVENT(2,
						      "SenseID : device %04X on "
						      "Subchannel %04X "
						      "reports cmd reject or "
						      "intervention required\n",
						      ioinfo[irq]->schib.pmcw.dev, 
						      irq);
					io_retry = 0;
				} else {
#ifdef CONFIG_DEBUG_IO
					printk
					    (KERN_WARNING
					     "SenseID : UC on "
					     "dev %04X, "
					     "retry %d, "
					     "lpum %02X, "
					     "cnt %02d, "
					     "sns :"
					     " %02X%02X%02X%02X "
					     "%02X%02X%02X%02X ...\n",
					     ioinfo[irq]->schib.pmcw.dev,
					     retry,
					     pdevstat->lpum,
					     pdevstat->scnt,
					     pdevstat->ii.sense.data[0],
					     pdevstat->ii.sense.data[1],
					     pdevstat->ii.sense.data[2],
					     pdevstat->ii.sense.data[3],
					     pdevstat->ii.sense.data[4],
					     pdevstat->ii.sense.data[5],
					     pdevstat->ii.sense.data[6],
					     pdevstat->ii.sense.data[7]);
#endif
					CIO_MSG_EVENT(2,
						      "SenseID : UC on "
						      "dev %04X, "
						      "retry %d, "
						      "lpum %02X, "
						      "cnt %02d, "
						      "sns :"
						      " %02X%02X%02X%02X "
						      "%02X%02X%02X%02X ...\n",
						      ioinfo[irq]->
						      schib.pmcw.dev,
						      retry,
						      pdevstat->lpum,
						      pdevstat->scnt,
						      pdevstat->
						      ii.sense.data[0],
						      pdevstat->
						      ii.sense.data[1],
						      pdevstat->
						      ii.sense.data[2],
						      pdevstat->
						      ii.sense.data[3],
						      pdevstat->
						      ii.sense.data[4],
						      pdevstat->
						      ii.sense.data[5],
						      pdevstat->
						      ii.sense.data[6],
						      pdevstat->
						      ii.sense.data[7]);

				}

			} else if ((pdevstat->flag & DEVSTAT_NOT_OPER)
				   || (irq_ret == -ENODEV)) {
#ifdef CONFIG_DEBUG_IO
				printk (KERN_ERR
					"SenseID : path %02X for "
					"device %04X on "
					"subchannel %04X "
					"is 'not operational'\n",
					domask,
					ioinfo[irq]->schib.pmcw.dev, irq);
#endif
				CIO_MSG_EVENT(2,
					      "SenseID : path %02X for "
					      "device %04X on "
					      "subchannel %04X "
					      "is 'not operational'\n",
					      domask,
					      ioinfo[irq]->schib.pmcw.dev, irq);
				
				io_retry = 0;
				ioinfo[irq]->opm &= ~domask;

			} else {
#ifdef CONFIG_DEBUG_IO
				printk (KERN_INFO
					"SenseID : start_IO() for "
					"device %04X on "
					"subchannel %04X "
					"returns %d, retry %d, "
					"status %04X\n",
					ioinfo[irq]->schib.pmcw.dev,
					irq, irq_ret, retry, pdevstat->flag);
#endif
				CIO_MSG_EVENT(2,
					     "SenseID : start_IO() for "
					     "device %04X on "
					     "subchannel %04X "
					     "returns %d, retry %d, "
					     "status %04X\n",
					     ioinfo[irq]->schib.pmcw.dev, irq,
					     irq_ret, retry, pdevstat->flag);

				if (irq_ret == -ETIMEDOUT) {
					int xret;

					/*
					 * Seems we need to cancel the first ssch sometimes...
					 * On the next try, the ssch will usually be fine.
					 */

					xret = cancel_IO (irq);

					if (!xret)
						CIO_MSG_EVENT(2,
							      "SenseID: sch canceled "
							      "successfully for irq %x\n",
							      irq);
				}

			}

			if (io_retry) {
				retry--;

				if (retry == 0) {
					io_retry = 0;

				}
			}

			if ((failure) && (io_retry)) {
				/* reset fields... */

				failure = 0;

				memset(psid, 0, sizeof(senseid_t));
				psid->cu_type = 0xFFFF;
			}

		} while ((io_retry));

	}

	if (init_IRQ_complete) {
		kfree (sense_ccw);
	} else {
		free_bootmem ((unsigned long) sense_ccw, 2 * sizeof (ccw1_t));

	}

	s390irq_spin_unlock (irq);

	/*
	 * If we installed the irq action handler we have to
	 *  release it too.
	 */
	if (inlreq)
		free_irq (irq, pdevstat);

	/*
	 * if running under VM check there ... perhaps we should do
	 *  only if we suffered a command reject, but it doesn't harm
	 */
	if ((sid->cu_type == 0xFFFF)
	    && (MACHINE_IS_VM)) {
		VM_virtual_device_info (ioinfo[irq]->schib.pmcw.dev, sid);
	}

	if (sid->cu_type == 0xFFFF) {
		/*
		 * SenseID CU-type of 0xffff indicates that no device
		 *  information could be retrieved (pre-init value).
		 *
		 * If we can't couldn't identify the device type we
		 *  consider the device "not operational".
		 */
#ifdef CONFIG_DEBUG_IO
		printk (KERN_WARNING
			"SenseID : unknown device %04X on subchannel %04X\n",
			ioinfo[irq]->schib.pmcw.dev, irq);
#endif
		CIO_MSG_EVENT(2,
			      "SenseID : unknown device %04X on subchannel %04X\n",
			      ioinfo[irq]->schib.pmcw.dev, irq);
		ioinfo[irq]->ui.flags.unknown = 1;

	}

	/*
	 * Issue device info message if unit was operational .
	 */
	if (!ioinfo[irq]->ui.flags.unknown) {
		if (sid->dev_type != 0) {
			if (cio_show_msg)
				printk (KERN_INFO
					"SenseID : device %04X reports: "
					"CU  Type/Mod = %04X/%02X,"
					" Dev Type/Mod = %04X/%02X\n",
					ioinfo[irq]->schib.pmcw.dev,
					sid->cu_type, sid->cu_model,
					sid->dev_type, sid->dev_model);
			CIO_MSG_EVENT(2,
				      "SenseID : device %04X reports: "
				      "CU  Type/Mod = %04X/%02X,"
				      " Dev Type/Mod = %04X/%02X\n",
				      ioinfo[irq]->schib.
				      pmcw.dev,
				      sid->cu_type,
				      sid->cu_model,
				      sid->dev_type,
				      sid->dev_model);
		} else {
			if (cio_show_msg)
				printk (KERN_INFO
					"SenseID : device %04X reports:"
					" Dev Type/Mod = %04X/%02X\n",
					ioinfo[irq]->schib.pmcw.dev,
					sid->cu_type, sid->cu_model);
			CIO_MSG_EVENT(2,
				      "SenseID : device %04X reports:"
				      " Dev Type/Mod = %04X/%02X\n",
				      ioinfo[irq]->schib.
				      pmcw.dev,
				      sid->cu_type,
				      sid->cu_model);
		}

	}

	if (!ioinfo[irq]->ui.flags.unknown)
		irq_ret = 0;
	else if (irq_ret != -ETIMEDOUT)
		irq_ret = -ENODEV;

	return (irq_ret);
}

static int __inline__
s390_SetMultiPath (int irq)
{
	int cc;

	cc = stsch (irq, &ioinfo[irq]->schib);

	if (!cc) {
		ioinfo[irq]->schib.pmcw.mp = 1;	/* multipath mode */

		cc = msch (irq, &ioinfo[irq]->schib);

	}

	return (cc);
}

static int
s390_do_path_verification(int irq, __u8 usermask)
{
	__u8 domask;
	int i;
	pgid_t pgid;
	__u8 dev_path;
	int first = 1;
	int ret = 0;
	char dbf_txt[15];

	sprintf(dbf_txt, "dopv%x", irq);
	CIO_TRACE_EVENT(2, dbf_txt);

	dev_path = usermask ? usermask : ioinfo[irq]->opm;

	if (ioinfo[irq]->ui.flags.pgid == 0) {
		memcpy (&ioinfo[irq]->pgid, global_pgid, sizeof (pgid_t));
		ioinfo[irq]->ui.flags.pgid = 1;
	}

	for (i = 0; i < 8 && !ret; i++) {

		domask = dev_path & (0x80>>i);

		if (!domask)
			continue;

		if (!test_bit(ioinfo[irq]->schib.pmcw.chpid[i],
			      &chpids_logical))
			/* Chpid is logically offline, don't do io */
			continue;

		ret = s390_SetPGID (irq, domask);

		/*
		 * For the *first* path we are prepared for recovery
		 *
		 *  - If we fail setting the PGID we assume its
		 *     using  a different PGID already (VM) we
		 *     try to sense.
		 */
		if (ret == -EOPNOTSUPP && first) {
			*(int *) &pgid = 0;
			
			ret = s390_SensePGID (irq, domask, &pgid);
			first = 0;

			if (ret == 0) {
				/*
				 * Check whether we retrieved
				 *  a reasonable PGID ...
				 */
				if (pgid.inf.ps.state1 == SNID_STATE1_GROUPED)
					memcpy (&ioinfo[irq]->pgid,
						&pgid, sizeof (pgid_t));
				else /* ungrouped or garbage ... */
					ret = -EOPNOTSUPP;
				
			} else {
				ioinfo[irq]->ui.flags.pgid_supp = 0;

#ifdef CONFIG_DEBUG_IO
				printk (KERN_WARNING
					"PathVerification(%04X) - Device %04X "
					"doesn't support path grouping\n",
					irq, ioinfo[irq]->schib.pmcw.dev);
#endif
				CIO_MSG_EVENT(2, "PathVerification(%04X) "
					      "- Device %04X doesn't "
					      " support path grouping\n",
					      irq,
					      ioinfo[irq]->schib.pmcw.dev);
					
			}
		} else if (ret == -EIO) {
#ifdef CONFIG_DEBUG_IO
			printk (KERN_ERR "PathVerification(%04X) - I/O error "
				"on device %04X\n", irq,
				ioinfo[irq]->schib.pmcw.dev);
#endif

			CIO_MSG_EVENT(2, "PathVerification(%04X) - I/O error "
				      "on device %04X\n", irq,
				      ioinfo[irq]->schib.pmcw.dev);
				
			ioinfo[irq]->ui.flags.pgid_supp = 0;

		} else if (ret == -ETIMEDOUT) {
#ifdef CONFIG_DEBUG_IO
			printk (KERN_ERR "PathVerification(%04X) - I/O timed "
				"out on device %04X\n", irq,
				ioinfo[irq]->schib.pmcw.dev);
#endif
			CIO_MSG_EVENT(2, "PathVerification(%04X) - I/O timed "
				      "out on device %04X\n", irq,
				      ioinfo[irq]->schib.pmcw.dev);
				
			ioinfo[irq]->ui.flags.pgid_supp = 0;

		} else if (ret == -EAGAIN) {
			
			ret = 0;
		} else if (ret == -EUSERS) {
			
#ifdef CONFIG_DEBUG_IO
			printk (KERN_ERR "PathVerification(%04X) "
				"- Device is locked by someone else!\n",
				irq);
#endif
			CIO_MSG_EVENT(2, "PathVerification(%04X) "
				      "- Device is locked by someone else!\n",
				      irq);
		} else if (ret == -ENODEV) {
#ifdef CONFIG_DEBUG_IO
			printk (KERN_ERR "PathVerification(%04X) "
				"- Device %04X is no longer there?!?\n",
				irq, ioinfo[irq]->schib.pmcw.dev);
#endif
			CIO_MSG_EVENT(2, "PathVerification(%04X) "
				      "- Device %04X is no longer there?!?\n",
				      irq, ioinfo[irq]->schib.pmcw.dev);

		} else if (ret == -EBUSY) {
			/* 
			 * The device is busy. Schedule the path verification 
			 * bottom half and we'll hopefully get in next time.
			 */
			if (!ioinfo[irq]->ui.flags.noio) {
				s390_schedule_path_verification(irq);
			}
			return -EINPROGRESS;
		} else if (ret) {
#ifdef CONFIG_DEBUG_IO
			printk (KERN_ERR "PathVerification(%04X) "
				"- Unexpected error %d on device %04X\n",
				irq, ret, ioinfo[irq]->schib.pmcw.dev);
#endif
			CIO_MSG_EVENT(2, "PathVerification(%04X) - "
				      "Unexpected error %d on device %04X\n",
				      irq, ret, ioinfo[irq]->schib.pmcw.dev);
				
			ioinfo[irq]->ui.flags.pgid_supp = 0;
		}
	}
	if (stsch(irq, &ioinfo[irq]->schib) != 0)
		/* FIXME: tell driver device is dead. */
		return -ENODEV;

	/*
	 * stsch() doesn't always yield the correct pim, pam, and pom
	 * values, if no device selection has been performed yet.
	 * However, after complete path verification they are up to date.
	 */
	ioinfo[irq]->opm = ioinfo[irq]->schib.pmcw.pim &
		ioinfo[irq]->schib.pmcw.pam &
		ioinfo[irq]->schib.pmcw.pom;

#ifdef CONFIG_CHSC	
	if (ioinfo[irq]->opm) {
		for (i=0;i<=7;i++) {
			int mask = 0x80 >> i;
			if ((ioinfo[irq]->opm & mask) &&
			    (!test_bit(ioinfo[irq]->schib.pmcw.chpid[i],
				       &chpids_logical)))
				/* disable using this path */
				ioinfo[irq]->opm &= ~mask;
		}
	}
#endif /* CONFIG_CHSC */	

	ioinfo[irq]->ui.flags.noio = 0;

	/* Eventually wake up the device driver. */
	if (ioinfo[irq]->opm != 0) {
		devreg_t *pdevreg;
		pdevreg = s390_search_devreg(ioinfo[irq]);

		if (pdevreg && pdevreg->oper_func)
			pdevreg->oper_func(irq, pdevreg);
	}
	return ret;

}

/*
 * Device Path Verification
 *
 * Path verification is accomplished by checking which paths (CHPIDs) are
 *  available. Further, a path group ID is set, if possible in multipath
 *  mode, otherwise in single path mode.
 *
 * Note : This function must not be called during normal device recognition,
 *         but during device driver initiated request_irq() processing only.
 */
int
s390_DevicePathVerification (int irq, __u8 usermask)
{
	int ccode;
#ifdef CONFIG_CHSC
	int chp;
	int mask;
	int old_opm = 0;
#endif /* CONFIG_CHSC */

	int ret = 0;

	char dbf_txt[15];
	devreg_t *pdevreg;

	sprintf (dbf_txt, "dpver%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	if (ioinfo[irq]->st) 
		return -ENODEV;

#ifdef CONFIG_CHSC
	old_opm = ioinfo[irq]->opm;
#endif /* CONFIG_CHSC */
	ccode = stsch (irq, &(ioinfo[irq]->schib));

	if (ccode)
		return -ENODEV;

	if (ioinfo[irq]->schib.pmcw.pim == 0x80) {
		/*
		 * no error, just not required for single path only devices
		 */
		ioinfo[irq]->ui.flags.pgid_supp = 0;
		ret = 0;
		ioinfo[irq]->ui.flags.noio = 0;

#ifdef CONFIG_CHSC
		/*
		 * disable if chpid is logically offline
		 */
		if (!test_bit(ioinfo[irq]->schib.pmcw.chpid[0], 
			      &chpids_logical)) {

			ioinfo[irq]->opm = 0;
			ioinfo[irq]->ui.flags.oper = 0;
			printk(KERN_WARNING 
			       "No logical path for sch %d...\n",
			       irq);

			if (ioinfo[irq]->nopfunc) {
				if (ioinfo[irq]->ui.flags.notacccap)
					ioinfo[irq]->nopfunc(irq,
							     DEVSTAT_NOT_ACC);
				else {
					not_oper_handler_func_t nopfunc =
						ioinfo[irq]->nopfunc;
#ifdef CONFIG_PROC_FS
					/* remove procfs entry */
					if (cio_proc_devinfo)
						cio_procfs_device_remove
							(ioinfo[irq]->devno);
#endif
					free_irq(irq,
						 ioinfo[irq]->irq_desc.dev_id);
					nopfunc(irq, DEVSTAT_DEVICE_GONE);
				}
			}
			return -ENODEV;
		}
		if (!old_opm) {

			ioinfo[irq]->opm = ioinfo[irq]->schib.pmcw.pim
				& ioinfo[irq]->schib.pmcw.pam
				& ioinfo[irq]->schib.pmcw.pom;
		
			if (ioinfo[irq]->opm) {

				ioinfo[irq]->ui.flags.oper = 1;
				pdevreg = s390_search_devreg(ioinfo[irq]);
			
				if (pdevreg && pdevreg->oper_func)
					pdevreg->oper_func(irq, pdevreg);
				ret = 0;
			} else {
				ret = -ENODEV;
			}
		}
#endif /* CONFIG_CHSC */
		return ret;
	}

	ioinfo[irq]->opm = ioinfo[irq]->schib.pmcw.pim
	    & ioinfo[irq]->schib.pmcw.pam & ioinfo[irq]->schib.pmcw.pom;

#ifdef CONFIG_CHSC
	if (ioinfo[irq]->opm) {
		for (chp=0;chp<=7;chp++) {
			mask = 0x80 >> chp;
			if ((ioinfo[irq]->opm & mask)
			    &&(!test_bit(ioinfo[irq]->schib.pmcw.chpid[chp],
					 &chpids_logical)))
				/* disable using this path */
				ioinfo[irq]->opm &= ~mask;
		}
	}
	
#endif /* CONFIG_CHSC */

	if (ioinfo[irq]->ui.flags.pgid_supp == 0) {		

		if (ioinfo[irq]->opm == 0)
			return -ENODEV;
			
		ioinfo[irq]->ui.flags.oper = 1;
		ioinfo[irq]->ui.flags.noio = 0;

		pdevreg = s390_search_devreg(ioinfo[irq]);
		
		if (pdevreg && pdevreg->oper_func)
			pdevreg->oper_func(irq, pdevreg);

		return 0;
	}

	if (ioinfo[irq]->ui.flags.ready)
		return s390_do_path_verification (irq, usermask);
	return 0;

}

void
s390_kick_path_verification (unsigned long irq)
{
	long cr6 __attribute__ ((aligned (8)));

	atomic_set (&ioinfo[irq]->pver_pending, 0);
	/* Do not enter path verification if sync_isc is enabled. */
	__ctl_store (cr6, 6, 6);
	if (cr6 & 0x04000000) {
		s390_schedule_path_verification (irq);
		return;
	}
	ioinfo[irq]->ui.flags.killio = 0;
	s390_DevicePathVerification(irq, 0xff);

}

static void
s390_schedule_path_verification(unsigned long irq)
{
	/* Protect against rescheduling, when already running */
	if (atomic_compare_and_swap (0, 1, &ioinfo[irq]->pver_pending)) {
		return;
	}

	/* 
	 * Call path verification.
	 * Note this is always called from inside the i/o layer, so we don't 
	 * need to care about the usermask.
	 */
	INIT_LIST_HEAD (&ioinfo[irq]->pver_bh.list);
	ioinfo[irq]->pver_bh.sync = 0;
	ioinfo[irq]->pver_bh.routine = (void*) (void*) s390_kick_path_verification;
	ioinfo[irq]->pver_bh.data = (void*) irq;
	queue_task (&ioinfo[irq]->pver_bh, &tq_immediate);
	mark_bh (IMMEDIATE_BH);
}

/*
 * s390_SetPGID
 *
 * Set Path Group ID
 *
 */
int
s390_SetPGID (int irq, __u8 lpm)
{
	ccw1_t *spid_ccw;	/* ccw area for SPID command */
	devstat_t devstat;	/* required by request_irq() */
	devstat_t *pdevstat = &devstat;
	unsigned long flags;
	char dbf_txt[15];

	int irq_ret = 0;	/* return code */
	int retry = 5;		/* retry count */
	int inlreq = 0;		/* inline request_irq() */
	int mpath = 1;		/* try multi-path first */

	SANITY_CHECK (irq);

	if (ioinfo[irq]->ui.flags.oper == 0) {
		return (-ENODEV);

	}

 	if (ioinfo[irq]->ui.flags.unfriendly) {
 		/* don't even try it */
 		return -EUSERS;
 	}

	sprintf (dbf_txt, "SPID%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	if (!ioinfo[irq]->ui.flags.ready) {
		/*
		 * Perform SetPGID command processing. We have to request device
		 *  ownership and provide a dummy I/O handler. We issue sync. I/O
		 *  requests and evaluate the devstat area on return therefore
		 *  we don't need a real I/O handler in place.
		 */
		irq_ret = request_irq (irq,
				       init_IRQ_handler,
				       SA_PROBE, "SPID", pdevstat);

		if (irq_ret == 0)
			inlreq = 1;
	} else {
		pdevstat = ioinfo[irq]->irq_desc.dev_id;

	}

	if (irq_ret) {
		return irq_ret;
	}

	s390irq_spin_lock_irqsave (irq, flags);

	if (init_IRQ_complete) {
		spid_ccw = kmalloc (2 * sizeof (ccw1_t), GFP_DMA | GFP_ATOMIC);
	} else {
		spid_ccw = alloc_bootmem_low (2 * sizeof (ccw1_t));
	}
	if (!spid_ccw) {
		s390irq_spin_unlock_irqrestore(irq, flags);
		if (inlreq)
			free_irq(irq, pdevstat);
		return -ENOMEM;
	}

	spid_ccw[0].cmd_code = CCW_CMD_SUSPEND_RECONN;
	spid_ccw[0].cda = 0;
	spid_ccw[0].count = 0;
	spid_ccw[0].flags = CCW_FLAG_SLI | CCW_FLAG_CC;

	spid_ccw[1].cmd_code = CCW_CMD_SET_PGID;
	spid_ccw[1].cda = (__u32) virt_to_phys (&ioinfo[irq]->pgid);
	spid_ccw[1].count = sizeof (pgid_t);
	spid_ccw[1].flags = CCW_FLAG_SLI;

	ioinfo[irq]->pgid.inf.fc = SPID_FUNC_MULTI_PATH | SPID_FUNC_ESTABLISH;

	/*
	 * We now issue a SetPGID request. In case of BUSY
	 *  or STATUS PENDING conditions we retry 5 times.
	 */
	do {
		memset (pdevstat, '\0', sizeof (devstat_t));

		irq_ret = s390_start_IO (irq, spid_ccw, 0xE2D7C9C4,	/* == SPID */
					 lpm,	/* n/a */
					 DOIO_WAIT_FOR_INTERRUPT
					 | DOIO_VALID_LPM
					 | DOIO_DONT_CALL_INTHDLR
					 | DOIO_TIMEOUT);

		if (!irq_ret) {
			if (pdevstat->flag & DEVSTAT_STATUS_PENDING) {
#ifdef CONFIG_DEBUG_IO
				printk (KERN_DEBUG "SPID - Device %04X "
					"on Subchannel %04X "
					"reports pending status, "
					"lpm = %x, "
					"retry : %d\n",
					ioinfo[irq]->schib.pmcw.dev,
					irq, lpm, retry);
#endif
				CIO_MSG_EVENT(2,
					      "SPID - Device %04X "
					      "on Subchannel %04X "
					      "reports pending status, "
					      "lpm = %x, "
					      "retry : %d\n",
					      ioinfo[irq]->schib.pmcw.
					      dev, irq, lpm, retry);
				retry--;
				irq_ret = -EIO;
			}

			if (pdevstat->flag == (DEVSTAT_START_FUNCTION
					       | DEVSTAT_FINAL_STATUS)) {
				retry = 0;	/* successfully set ... */
				irq_ret = 0;
			} else if (pdevstat->flag & DEVSTAT_FLAG_SENSE_AVAIL) {
				/*
				 * If the device doesn't support the
				 *  Sense Path Group ID command
				 *  further retries wouldn't help ...
				 */
				if (pdevstat->ii.sense.
				    data[0] & SNS0_CMD_REJECT) {
					if (mpath) {
						/*
						 * We now try single path mode.
						 * Note we must not issue the suspend
						 * multipath reconnect, or we will get
						 * a command reject by tapes.
						 */

						spid_ccw[0].cmd_code =
						    CCW_CMD_SET_PGID;
						spid_ccw[0].cda = (__u32)
						    virt_to_phys (&ioinfo[irq]->pgid);
						spid_ccw[0].count =
						    sizeof (pgid_t);
						spid_ccw[0].flags =
						    CCW_FLAG_SLI;

						ioinfo[irq]->pgid.inf.fc =
						    SPID_FUNC_SINGLE_PATH
						    | SPID_FUNC_ESTABLISH;
						mpath = 0;
						retry--;
						irq_ret = -EIO;
					} else {
						irq_ret = -EOPNOTSUPP;
						retry = 0;

					}
				} else {
#ifdef CONFIG_DEBUG_IO
					printk (KERN_WARNING
						"SPID - device %04X,"
						" unit check,"
						" retry %d, cnt %02d,"
						" lpm %x, sns :"
						" %02X%02X%02X%02X %02X%02X%02X%02X ...\n",
						ioinfo[irq]->schib.pmcw.
						dev, retry,
						pdevstat->scnt,
						lpm, 
						pdevstat->ii.sense.
						data[0],
						pdevstat->ii.sense.
						data[1],
						pdevstat->ii.sense.
						data[2],
						pdevstat->ii.sense.
						data[3],
						pdevstat->ii.sense.
						data[4],
						pdevstat->ii.sense.
						data[5],
						pdevstat->ii.sense.
						data[6],
						pdevstat->ii.sense.data[7]);
#endif

					CIO_MSG_EVENT(2,
						     "SPID - device %04X,"
						     " unit check,"
						     " retry %d, cnt %02d,"
						     " lpm %x, sns :"
						     " %02X%02X%02X%02X %02X%02X%02X%02X ...\n",
						     ioinfo[irq]->schib.
						     pmcw.dev, retry,
						     pdevstat->scnt,
						     lpm, 
						     pdevstat->ii.sense.
						     data[0],
						     pdevstat->ii.sense.
						     data[1],
						     pdevstat->ii.sense.
						     data[2],
						     pdevstat->ii.sense.
						     data[3],
						     pdevstat->ii.sense.
						     data[4],
						     pdevstat->ii.sense.
						     data[5],
						     pdevstat->ii.sense.
						     data[6],
						     pdevstat->ii.sense.
						     data[7]);

					retry--;
					irq_ret = -EIO;

				}

			} else if (pdevstat->flag & DEVSTAT_NOT_OPER) {
				/* don't issue warnings during startup unless requested */
				if (init_IRQ_complete || cio_notoper_msg) {

					printk (KERN_INFO
						"SPID - Device %04X "
						"on Subchannel %04X, "
						"lpm %02X, "
						"became 'not operational'\n",
						ioinfo[irq]->schib.pmcw.
						dev, irq,
						lpm);
					CIO_MSG_EVENT(2,
						     "SPID - Device %04X "
						     "on Subchannel %04X, "
						      "lpm %02X, "
						     "became 'not operational'\n",
						     ioinfo[irq]->schib.
						     pmcw.dev, irq,
						     lpm);
				}

				retry = 0;
				ioinfo[irq]->opm &= ~lpm;
				irq_ret = -EAGAIN;

			}

		} else if (irq_ret == -ETIMEDOUT) {
			/* 
			 * SetPGID timed out, so we cancel it before
			 * we retry
			 */
			int xret;

			xret = cancel_IO(irq);

			if (!xret) 
				CIO_MSG_EVENT(2,
					      "SetPGID: sch canceled "
					      "successfully for irq %x\n",
					      irq);
			retry--;

		} else if (irq_ret == -EBUSY) {
#ifdef CONFIG_DEBUG_IO
			printk(KERN_WARNING
			       "SPID - device %x, irq %x is busy!\n",
			       ioinfo[irq]->schib.pmcw.dev, irq);
#endif /* CONFIG_DEBUG_IO */
			CIO_MSG_EVENT(2,
				      "SPID - device %x, irq %x is busy!\n",
				      ioinfo[irq]->schib.pmcw.dev, irq);
			retry = 0;

		} else if (irq_ret != -ENODEV) {
			retry--;
			irq_ret = -EIO;
		} else if (!pdevstat->flag & DEVSTAT_NOT_OPER) {
			retry = 0;
			irq_ret = -ENODEV;
		} else {
			/* don't issue warnings during startup unless requested */
			if (init_IRQ_complete || cio_notoper_msg) {
				
				printk (KERN_INFO
					"SPID - Device %04X "
					"on Subchannel %04X, "
					"lpm %02X, "
					"became 'not operational'\n",
					ioinfo[irq]->schib.pmcw.
					dev, irq,
					lpm);
				CIO_MSG_EVENT(2,
					      "SPID - Device %04X "
					      "on Subchannel %04X, "
					      "lpm %02X, "
					      "became 'not operational'\n",
					      ioinfo[irq]->schib.
					      pmcw.dev, irq,
					      lpm);
			}

			retry = 0;
			ioinfo[irq]->opm &= ~lpm;

			if (ioinfo[irq]->opm != 0)
				irq_ret = -EAGAIN;
			else
				irq_ret = -ENODEV;

		}

	} while (retry > 0);

	if (init_IRQ_complete) {
		kfree (spid_ccw);
	} else {
		free_bootmem ((unsigned long) spid_ccw, 2 * sizeof (ccw1_t));

	}

	s390irq_spin_unlock_irqrestore (irq, flags);

	/*
	 * If we installed the irq action handler we have to
	 *  release it too.
	 */
	if (inlreq)
		free_irq (irq, pdevstat);

	return (irq_ret);
}

/*
 * s390_SensePGID
 *
 * Sense Path Group ID
 *
 */
int
s390_SensePGID (int irq, __u8 lpm, pgid_t * pgid)
{
	ccw1_t *snid_ccw;	/* ccw area for SNID command */
	devstat_t devstat;	/* required by request_irq() */
	devstat_t *pdevstat = &devstat;
	char dbf_txt[15];
	pgid_t * tmp_pgid;

	int irq_ret = 0;	/* return code */
	int retry = 5;		/* retry count */
	int inlreq = 0;		/* inline request_irq() */
	unsigned long flags;

	SANITY_CHECK (irq);

	if (ioinfo[irq]->ui.flags.oper == 0) {
		return (-ENODEV);

	}

	sprintf (dbf_txt, "SNID%x", irq);
	CIO_TRACE_EVENT (4, dbf_txt);

	if (!ioinfo[irq]->ui.flags.ready) {
		/*
		 * Perform SENSE PGID command processing. We have to request device
		 *  ownership and provide a dummy I/O handler. We issue sync. I/O
		 *  requests and evaluate the devstat area on return therefore
		 *  we don't need a real I/O handler in place.
		 */
		irq_ret = request_irq (irq,
				       init_IRQ_handler,
				       SA_PROBE, "SNID", pdevstat);

		if (irq_ret == 0)
			inlreq = 1;

	} else {
		pdevstat = ioinfo[irq]->irq_desc.dev_id;

	}

	if (irq_ret) {
		return irq_ret;
	}

	s390irq_spin_lock_irqsave (irq, flags);

	ioinfo[irq]->ui.flags.unfriendly = 0; /* assume it's friendly... */

	if (init_IRQ_complete) {
		snid_ccw = kmalloc (sizeof (ccw1_t), GFP_DMA | GFP_ATOMIC);
		tmp_pgid = kmalloc (sizeof (pgid_t), GFP_DMA | GFP_ATOMIC);
	} else {
		snid_ccw = alloc_bootmem_low (sizeof (ccw1_t));
		tmp_pgid = alloc_bootmem_low (sizeof (pgid_t));
	}

	if (!snid_ccw || !tmp_pgid) {
		if (snid_ccw) {
			if (init_IRQ_complete)
				kfree(snid_ccw);
			else
				free_bootmem((unsigned long) snid_ccw, sizeof(ccw1_t));
		}
		if (tmp_pgid) {
			if (init_IRQ_complete)
				kfree(tmp_pgid);
			else
				free_bootmem((unsigned long) tmp_pgid, sizeof(pgid_t));
		}
		s390irq_spin_unlock_irqrestore(irq, flags);
		if (inlreq) 
			free_irq (irq, pdevstat);
		return -ENOMEM;
	}

	snid_ccw->cmd_code = CCW_CMD_SENSE_PGID;
	snid_ccw->cda = (__u32) virt_to_phys (tmp_pgid);
	snid_ccw->count = sizeof (pgid_t);
	snid_ccw->flags = CCW_FLAG_SLI;

	/*
	 * We now issue a SensePGID request. In case of BUSY
	 *  or STATUS PENDING conditions we retry 5 times.
	 */
	do {
		memset (pdevstat, '\0', sizeof (devstat_t));

		irq_ret = s390_start_IO (irq, snid_ccw, 0xE2D5C9C4,	/* == SNID */
					 lpm,	/* n/a */
					 DOIO_WAIT_FOR_INTERRUPT
 					 | DOIO_TIMEOUT
					 | DOIO_VALID_LPM
					 | DOIO_DONT_CALL_INTHDLR);

		if (irq_ret == 0) {
			if (pdevstat->flag & DEVSTAT_FLAG_SENSE_AVAIL) {
				/*
				 * If the device doesn't support the
				 *  Sense Path Group ID command
				 *  further retries wouldn't help ...
				 */
				if (pdevstat->ii.sense.data[0] 
				    & (SNS0_CMD_REJECT | SNS0_INTERVENTION_REQ)) {
					retry = 0;
					irq_ret = -EOPNOTSUPP;
				} else {
#ifdef CONFIG_DEBUG_IO
					printk (KERN_WARNING
						"SNID - device %04X,"
						" unit check,"
						" flag %04X, "
						" retry %d, cnt %02d,"
						" sns :"
						" %02X%02X%02X%02X %02X%02X%02X%02X ...\n",
						ioinfo[irq]->schib.pmcw.
						dev, pdevstat->flag,
						retry, pdevstat->scnt,
						pdevstat->ii.sense.
						data[0],
						pdevstat->ii.sense.
						data[1],
						pdevstat->ii.sense.
						data[2],
						pdevstat->ii.sense.
						data[3],
						pdevstat->ii.sense.
						data[4],
						pdevstat->ii.sense.
						data[5],
						pdevstat->ii.sense.
						data[6],
						pdevstat->ii.sense.data[7]);

#endif
					CIO_MSG_EVENT(2,
						     "SNID - device %04X,"
						     " unit check,"
						     " flag %04X, "
						     " retry %d, cnt %02d,"
						     " sns :"
						     " %02X%02X%02X%02X %02X%02X%02X%02X ...\n",
						     ioinfo[irq]->schib.
						     pmcw.dev,
						     pdevstat->flag,
						     retry,
						     pdevstat->scnt,
						     pdevstat->ii.sense.
						     data[0],
						     pdevstat->ii.sense.
						     data[1],
						     pdevstat->ii.sense.
						     data[2],
						     pdevstat->ii.sense.
						     data[3],
						     pdevstat->ii.sense.
						     data[4],
						     pdevstat->ii.sense.
						     data[5],
						     pdevstat->ii.sense.
						     data[6],
						     pdevstat->ii.sense.
						     data[7]);
					retry--;
					irq_ret = -EIO;

				}
			} else if (pdevstat->flag & DEVSTAT_NOT_OPER) {
				/* don't issue warnings during startup unless requested */
				if (init_IRQ_complete || cio_notoper_msg) {
					printk (KERN_INFO
						"SNID - Device %04X "
						"on Subchannel %04X, "
						"lpm %02X, "
						"became 'not operational'\n",
						ioinfo[irq]->schib.pmcw.
						dev, irq,
						lpm);
					CIO_MSG_EVENT(2,
						     "SNID - Device %04X "
						     "on Subchannel %04X, "
						     "lpm %02X, "
						     "became 'not operational'\n",
						     ioinfo[irq]->schib.
						     pmcw.dev, irq,
						     lpm);
				}

				retry = 0;
				ioinfo[irq]->opm &= ~lpm;
				irq_ret = -EAGAIN;

			} else {
				retry = 0;	/* success ... */
				irq_ret = 0;
 				/*
 				 * Check if device is locked by someone else
 				 * -- we'll fail other commands if that is
 				 * the case
 				 */
 				if (tmp_pgid->inf.ps.state2 ==
 				    SNID_STATE2_RESVD_ELSE) {
 					printk (KERN_WARNING 
 						"SNID - Device %04X "
 						"on Subchannel %04X "
 						"is reserved by "
 						"someone else\n",
 						ioinfo[irq]->schib.pmcw.dev,
 						irq);
 					CIO_MSG_EVENT(2,
 						      "SNID - Device %04X "
 						      "on Subchannel %04X "
 						      "is reserved by "
 						      "someone else\n",
 						      ioinfo[irq]->schib.
 						      pmcw.dev,
 						      irq);
 				
 					ioinfo[irq]->ui.flags.unfriendly = 1;
 				} else {
 					/*
 					 * device is friendly to us :)
 					 */
 					ioinfo[irq]->ui.flags.unfriendly = 0;
 				}
				memcpy(pgid, tmp_pgid, sizeof(pgid_t));
			}

 		} else if (irq_ret == -ETIMEDOUT) {
#ifdef CONFIG_DEBUG_IO
 			printk(KERN_INFO "SNID - Operation timed out "
 			       "on Device %04X, Subchannel %04X... "
 			       "cancelling IO\n",
 			       ioinfo[irq]->schib.pmcw.dev,
 			       irq);
#endif /* CONFIG_DEBUG_IO */
 			CIO_MSG_EVENT(2,
 				      "SNID - Operation timed out "
 				      "on Device %04X, Subchannel %04X... "
 				      "cancelling IO\n",
 				      ioinfo[irq]->schib.pmcw.dev,
 				      irq);
 			cancel_IO(irq);
 			retry--;

		} else if (irq_ret != -ENODEV) {	/* -EIO, or -EBUSY */

			if (pdevstat->flag & DEVSTAT_STATUS_PENDING) {
#ifdef CONFIG_DEBUG_IO
				printk (KERN_INFO "SNID - Device %04X "
					"on Subchannel %04X "
					"reports pending status, "
					"retry : %d\n",
					ioinfo[irq]->schib.pmcw.dev,
					irq, retry);
#endif
				CIO_MSG_EVENT(2,
					     "SNID - Device %04X "
					     "on Subchannel %04X "
					     "reports pending status, "
					     "retry : %d\n",
					     ioinfo[irq]->schib.pmcw.
					     dev, irq, retry);
			}

			printk (KERN_WARNING "SNID - device %04X,"
				" start_io() reports rc : %d, retrying ...\n",
				ioinfo[irq]->schib.pmcw.dev, irq_ret);
			CIO_MSG_EVENT(2,
				      "SNID - device %04X,"
				      " start_io() reports rc : %d,"
				      " retrying ...\n",
				      ioinfo[irq]->schib.pmcw.dev, irq_ret);
			retry--;
			irq_ret = -EIO;
		} else if (!pdevstat->flag & DEVSTAT_NOT_OPER) {
			retry = 0;
			irq_ret = -ENODEV;
		} else {
			/* don't issue warnings during startup unless requested */
			if (init_IRQ_complete || cio_notoper_msg) {
				
				printk (KERN_INFO
					"SNID - Device %04X "
					"on Subchannel %04X, "
					"lpm %02X, "
					"became 'not operational'\n",
					ioinfo[irq]->schib.pmcw.
					dev, irq,
					lpm);
				CIO_MSG_EVENT(2,
					      "SNID - Device %04X "
					      "on Subchannel %04X, "
					      "lpm %02X, "
					      "became 'not operational'\n",
					      ioinfo[irq]->schib.
					      pmcw.dev, irq,
					      lpm);
			}

			retry = 0;
			ioinfo[irq]->opm &= ~lpm;

			if (ioinfo[irq]->opm != 0)
				irq_ret = -EAGAIN;
			else
				irq_ret = -ENODEV;

		}

	} while (retry > 0);

	if (init_IRQ_complete) {
		kfree (snid_ccw);
		kfree (tmp_pgid);
	} else {
		free_bootmem ((unsigned long) snid_ccw, sizeof (ccw1_t));
		free_bootmem ((unsigned long) tmp_pgid, sizeof (pgid_t));

	}

	s390irq_spin_unlock_irqrestore (irq, flags);

	/*
	 * If we installed the irq action handler we have to
	 *  release it too.
	 */
	if (inlreq)
		free_irq (irq, pdevstat);

	return (irq_ret);
}

void
s390_process_subchannel_source (int irq)
{
	int dev_oper = 0;
	int dev_no = -1;
	int lock = 0;
	int is_owned = 0;

	/*
	 * If the device isn't known yet
	 *   we can't lock it ...
	 */
	if (ioinfo[irq] != INVALID_STORAGE_AREA) {
		s390irq_spin_lock (irq);
		lock = 1;

		if (!ioinfo[irq]->st) {
			dev_oper = ioinfo[irq]->ui.flags.oper;
			
			if (ioinfo[irq]->ui.flags.dval)
				dev_no = ioinfo[irq]->devno;
			
			is_owned = ioinfo[irq]->ui.flags.ready;
		}

	}
#ifdef CONFIG_DEBUG_CRW
	printk (KERN_DEBUG
		"do_crw_pending : subchannel validation - start ...\n");
#endif
	CIO_CRW_EVENT(4, "subchannel validation - start\n");
	s390_validate_subchannel (irq, is_owned);

	if (irq > highest_subchannel)
		highest_subchannel = irq;

#ifdef CONFIG_DEBUG_CRW
	printk (KERN_DEBUG "do_crw_pending : subchannel validation - done\n");
#endif
	CIO_CRW_EVENT(4, "subchannel validation - done\n");
	/*
	 * After the validate processing
	 *   the ioinfo control block
	 *   should be allocated ...
	 */
	if (lock) {
		s390irq_spin_unlock (irq);
	}

	if (ioinfo[irq] != INVALID_STORAGE_AREA) {
#ifdef CONFIG_DEBUG_CRW
		printk (KERN_DEBUG "do_crw_pending : ioinfo at "
#ifdef CONFIG_ARCH_S390X
			"%08lX\n", (unsigned long) ioinfo[irq]
#else				/* CONFIG_ARCH_S390X */
			"%08X\n", (unsigned) ioinfo[irq]
#endif				/* CONFIG_ARCH_S390X */
			);
#endif
#ifdef CONFIG_ARCH_S390X
		CIO_CRW_EVENT(4, "ioinfo at %08lX\n", 
			      (unsigned long)ioinfo[irq]);
#else				/* CONFIG_ARCH_S390X */
		CIO_CRW_EVENT(4, "ioinfo at %08X\n", 
			      (unsigned)ioinfo[irq]);
#endif				/* CONFIG_ARCH_S390X */

		if (ioinfo[irq]->st)
			return;

		if (ioinfo[irq]->ui.flags.oper == 0) {
			not_oper_handler_func_t nopfunc = ioinfo[irq]->nopfunc;
#ifdef CONFIG_PROC_FS
			/* remove procfs entry */
			if (cio_proc_devinfo)
				cio_procfs_device_remove (dev_no);
#endif
			/*
			 * If the device has gone
			 *  call not oper handler               
			 */
			if ((dev_oper == 1)
			    && (nopfunc != NULL)) {

				free_irq (irq, ioinfo[irq]->irq_desc.dev_id);
				nopfunc (irq, DEVSTAT_DEVICE_GONE);

			}
		} else {
#ifdef CONFIG_DEBUG_CRW
			printk (KERN_DEBUG
				"do_crw_pending : device "
				"recognition - start ...\n");
#endif
			CIO_CRW_EVENT( 4,
				       "device recognition - start\n");
			s390_device_recognition_irq (irq);

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_DEBUG
				"do_crw_pending : device "
				"recognition - done\n");
#endif
			CIO_CRW_EVENT( 4,
				       "device recognition - done\n");
			/*
			 * the device became operational
			 */
			if (dev_oper == 0) {
				devreg_t *pdevreg;

				pdevreg = s390_search_devreg (ioinfo[irq]);

				if (pdevreg && pdevreg->oper_func)
					pdevreg->oper_func(irq, pdevreg);

#ifdef CONFIG_PROC_FS
				/* add new procfs entry */
				if (cio_proc_devinfo)
					if (highest_subchannel <
					    MAX_CIO_PROCFS_ENTRIES) {
						cio_procfs_device_create
						    (ioinfo[irq]->devno);
					}
#endif
			}
			/*
			 * ... it is and was operational, but
			 *      the devno may have changed
			 */
			else if ((ioinfo[irq]->devno != dev_no)
				 && (ioinfo[irq]->nopfunc != NULL)) {
#ifdef CONFIG_PROC_FS
				int devno_old = ioinfo[irq]->devno;
#endif
				ioinfo[irq]->nopfunc (irq, DEVSTAT_REVALIDATE);
#ifdef CONFIG_PROC_FS
				/* remove old entry, add new */
				if (cio_proc_devinfo) {
					cio_procfs_device_remove (devno_old);
					cio_procfs_device_create
					    (ioinfo[irq]->devno);
				}
#endif
			}
		}
#ifdef CONFIG_PROC_FS
		/* get rid of dead procfs entries */
		if (cio_proc_devinfo)
			cio_procfs_device_purge ();
#endif
	}
}

#ifdef CONFIG_CHSC
static int 
chsc_get_sch_desc_irq(int irq)
{
	int j = 0;
	int ccode;

	spin_lock(&chsc_lock_ssd);
		
	if (!chsc_area_ssd)
		chsc_area_ssd = kmalloc(sizeof(chsc_area_t),GFP_KERNEL);
	
	if (!chsc_area_ssd) {
		printk( KERN_CRIT "No memory to determine sch descriptions...\n");
		spin_unlock(&chsc_lock_ssd);
		return -ENOMEM;
	}
	
	memset(chsc_area_ssd, 0, sizeof(chsc_area_t));
	
	chsc_area_ssd->request_block.command_code1=0x0010;
	chsc_area_ssd->request_block.command_code2=0x0004;
	chsc_area_ssd->request_block.request_block_data.ssd_req.f_sch=irq;
	chsc_area_ssd->request_block.request_block_data.ssd_req.l_sch=irq;
	
	ccode = chsc(chsc_area_ssd);
#ifdef CONFIG_DEBUG_CHSC
	if (ccode)
		printk( KERN_DEBUG "chsc returned with ccode = %d\n",ccode);
#endif /* CONFIG_DEBUG_CHSC */
	if (!ccode) {
		if (chsc_area_ssd->response_block.response_code == 0x0003) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING "Error in chsc request block!\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, "Error in chsc request block!\n");
			spin_unlock(&chsc_lock_ssd);
			return -EINVAL;
			
		} else if (chsc_area_ssd->response_block.response_code == 0x0004) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING "Model does not provide ssd\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, "Model does not provide ssd\n");
			spin_unlock(&chsc_lock_ssd);
			return -EOPNOTSUPP;

		} else if (chsc_area_ssd->response_block.response_code == 0x0002) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING "chsc: Invalid command!\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2,
				       "chsc: Invalid command!\n");
			return -EINVAL;

		} else if (chsc_area_ssd->response_block.response_code == 0x0001) {
			/* everything ok */
					
			switch (chsc_area_ssd->response_block.response_block_data.ssd_res.st) {
				
			case 0:  /* I/O subchannel */
				
				/* 
				 * All fields have meaning
				 */
#ifdef CONFIG_DEBUG_CHSC
				if (cio_show_msg) 
					printk( KERN_DEBUG 
						"ssd: sch %x is I/O subchannel\n",
						irq);
#endif /* CONFIG_DEBUG_CHSC */
				CIO_CRW_EVENT( 6,
					       "ssd: sch %x is I/O subchannel\n",
					       irq);

				if (ioinfo[irq] == INVALID_STORAGE_AREA)
					/* FIXME: we should do device rec. here... */
					break;

				ioinfo[irq]->ssd_info.valid = 1;
				ioinfo[irq]->ssd_info.type = 0;
				for (j=0;j<8;j++) {
					if ((0x80 >> j) & 
					    chsc_area_ssd->response_block.
					    response_block_data.ssd_res.path_mask & 
					    chsc_area_ssd->response_block.
					    response_block_data.ssd_res.fla_valid_mask) {

						if (chsc_area_ssd->response_block.
						    response_block_data.ssd_res.chpid[j]) 

							if (!test_and_set_bit
							    (chsc_area_ssd->response_block.
							     response_block_data.
							     ssd_res.chpid[j],
							     &chpids_known)) 

								if (test_bit
								    (chsc_area_ssd->response_block.
								     response_block_data.
								     ssd_res.chpid[j],
								     &chpids_logical))

									set_bit(chsc_area_ssd->response_block.
										response_block_data.
										ssd_res.chpid[j],
										&chpids);

						ioinfo[irq]->ssd_info.chpid[j] = 
							chsc_area_ssd->response_block.
							response_block_data.ssd_res.chpid[j];
						ioinfo[irq]->ssd_info.fla[j] = 
							chsc_area_ssd->response_block.
							response_block_data.ssd_res.fla[j];
					}
				}
				break;
				
			case 1:  /* CHSC subchannel */
				
				/*
				 * Only sch_val, st and sch have meaning
				 */
#ifdef CONFIG_DEBUG_CHSC
				if (cio_show_msg)
					printk( KERN_DEBUG 
						"ssd: sch %x is chsc subchannel\n",
						irq);
#endif /* CONFIG_DEBUG_CHSC */
				CIO_CRW_EVENT( 6,
					       "ssd: sch %x is chsc subchannel\n",
					       irq);

				if (ioinfo[irq] == INVALID_STORAGE_AREA)
					/* FIXME: we should do device rec. here... */
					break;

				ioinfo[irq]->ssd_info.valid = 1;
				ioinfo[irq]->ssd_info.type = 1;
				break;
				
			case 2: /* Message subchannel */
				
				/*
				 * All fields except unit_addr have meaning
				 */
#ifdef CONFIG_DEBUG_CHSC
				if (cio_show_msg)
					printk( KERN_DEBUG 
						"ssd: sch %x is message subchannel\n",
						irq);
#endif
				CIO_CRW_EVENT( 6,
					       "ssd: sch %x is message subchannel\n",
					       irq);

				if (ioinfo[irq] == INVALID_STORAGE_AREA)
					/* FIXME: we should do device rec. here... */
					break;

				ioinfo[irq]->ssd_info.valid = 1;
				ioinfo[irq]->ssd_info.type = 2;
				for (j=0;j<8;j++) {
					if ((0x80 >> j) & 
					    chsc_area_ssd->response_block.
					    response_block_data.ssd_res.path_mask & 
					    chsc_area_ssd->response_block.
					    response_block_data.ssd_res.fla_valid_mask) {
						if (chsc_area_ssd->response_block.
						    response_block_data.ssd_res.chpid[j])

							if (!test_and_set_bit
							    (chsc_area_ssd->response_block.
							     response_block_data.
							     ssd_res.chpid[j],
							     &chpids_known)) 

								if (test_bit
								    (chsc_area_ssd->response_block.
								     response_block_data.
								     ssd_res.chpid[j],
								     &chpids_logical))

									set_bit(chsc_area_ssd->response_block.
										response_block_data.
										ssd_res.chpid[j],
										&chpids);

						ioinfo[irq]->ssd_info.chpid[j] = 
							chsc_area_ssd->response_block.
							response_block_data.ssd_res.chpid[j];
						ioinfo[irq]->ssd_info.fla[j] = 
							chsc_area_ssd->response_block.
							response_block_data.ssd_res.fla[j];
					}
				}
				break;
				
			case 3: /* ADM subchannel */
				
				/*
				 * Only sch_val, st and sch have meaning
				 */
#ifdef CONFIG_DEBUG_CHSC
				if (cio_show_msg) 
					printk( KERN_DEBUG 
						"ssd: sch %x is ADM subchannel\n",
						irq);
#endif /* CONFIG_DEBUG_CHSC */
				CIO_CRW_EVENT( 6,
					       "ssd: sch %x is ADM subchannel\n",
					       irq);

				if (ioinfo[irq] == INVALID_STORAGE_AREA)
					/* FIXME: we should do device rec. here... */
					break;

				ioinfo[irq]->ssd_info.valid = 1;
				ioinfo[irq]->ssd_info.type = 3;
				break;
				
			default: /* uhm, that looks strange... */
#ifdef CONFIG_DEBUG_CHSC
				if (cio_show_msg) 
					printk( KERN_DEBUG 
						"Strange subchannel type %d for sch %x\n", 
						chsc_area_ssd->response_block.
						response_block_data.ssd_res.st,
						irq);
#endif /* CONFIG_DEBUG_CHSC */
				CIO_CRW_EVENT( 0, 
					       "Strange subchannel type %d for "
					       "sch %x\n", 
					       chsc_area_ssd->response_block.
					       response_block_data.ssd_res.st,
					       irq);
			}
			spin_unlock(&chsc_lock_ssd);
			return 0;
		}
	} else {
		spin_unlock(&chsc_lock_ssd);
		if (ccode == 3)
			return -ENODEV;
		return -EBUSY;
	}
	return -EIO;
}


static int 
chsc_get_sch_descriptions( void )
{

	int irq = 0;
	int err = 0;

	CIO_TRACE_EVENT( 4, "gsdesc");

	/*
	 * get information about chpids and link addresses 
	 * by executing the chsc command 'store subchannel description'
	 */

	if (init_IRQ_complete) {
		
		for (irq=0; irq<=highest_subchannel; irq++) {

			/*
			 * retrieve information for each sch
			 */
			err = chsc_get_sch_desc_irq(irq);
			if (err) {
				if (!cio_chsc_err_msg) {
					printk( KERN_ERR
						"chsc_get_sch_descriptions:"
						" Error %d while doing chsc; "
						"processing "
						"some machine checks may "
						"not work\n", 
						err);
					cio_chsc_err_msg=1;
				}
				return err;
			}
		}
		cio_chsc_desc_avail = 1;
		return 0;
	} else {
		/* Paranoia... */
		
		printk( KERN_ERR 
			"Error: chsc_get_sch_descriptions called before "
		       "initialization complete\n");
		return -EINVAL;
	}
	
}

__initcall(chsc_get_sch_descriptions);

static int
__check_for_io_and_kill(int irq, __u8 mask, int fatal)
{
	schib_t *schib = &ioinfo[irq]->schib;
	int ret = 0;

	if (schib->scsw.actl & SCSW_ACTL_DEVACT) {
		if ((ioinfo[irq]->opm != mask) ||
		     (fatal == 0)) {
			ret = CIO_PATHGONE_WAIT4INT;
		}
		if ((schib->scsw.actl & SCSW_ACTL_SCHACT) &&
		    (schib->pmcw.lpum == mask) &&
		    (fatal != 0)) {
			int cc;
			/* Kill the IO. It won't complete. */
			ioinfo[irq]->ui.flags.noio = 0;
			ioinfo[irq]->ui.flags.killio = 1;
			cc = clear_IO(irq, 0xD2C9D3D3, 0);
			if (cc != 0) {
				/* Eek, can't kill io. */
				CIO_CRW_EVENT(0, 
					      "Can't kill io on "
					      "sch %x, clear_IO "
					      "returned %d!\n",
					      irq, cc);
				ioinfo[irq]->ui.flags.killio = 0;
				s390irq_spin_unlock(irq);
				if ((cc == -ENODEV) && 
				    (ioinfo[irq]->nopfunc)) {
					ioinfo[irq]->ui.flags.oper = 0;
					ioinfo[irq]->nopfunc(irq,
							     DEVSTAT_DEVICE_GONE);
				}
				ret = CIO_PATHGONE_DEVGONE;
			} else {
				ret |= CIO_PATHGONE_WAIT4INT;
			}
			ioinfo[irq]->ui.flags.noio = 1;
			ret |= CIO_PATHGONE_IOERR;
		}

	} else if (schib->scsw.actl & (SCSW_ACTL_CLEAR_PEND |
				SCSW_ACTL_HALT_PEND |
				SCSW_ACTL_START_PEND |
				SCSW_ACTL_RESUME_PEND)) {
		if ((schib->pmcw.lpum != mask) ||
		    (fatal == 0)) {
			ret = CIO_PATHGONE_WAIT4INT;
		} else {
			int cc;
			/* Cancel the i/o. */
			cc = cancel_IO(irq);
			switch (cc) {
			case 0: 
				/* i/o cancelled; we can do path verif. */
				ret = CIO_PATHGONE_IOERR;
				break;
			case -EBUSY:
				/* Status pending, we'll get an interrupt */
				ret = CIO_PATHGONE_WAIT4INT;
				break;
			case -EINVAL:
				/*
				 * There is either not only the start function
				 * specified or we are subchannel active.
				 * Do a clear sch.
				 */
				ioinfo[irq]->ui.flags.noio = 0;
				ioinfo[irq]->ui.flags.killio = 1;
				cc = clear_IO(irq, 0xD2C9D3D3, 0);
				if (cc != 0) {
				/* Eek, can't kill io. */
					CIO_CRW_EVENT(0, 
						      "Can't kill io on "
						      "sch %x, clear_IO "
						      "returned %d!\n",
						      irq, cc);
					ioinfo[irq]->ui.flags.killio = 0;
					s390irq_spin_unlock(irq);
					if ((cc == -ENODEV) && 
					    (ioinfo[irq]->nopfunc)) {
						ioinfo[irq]->nopfunc(irq,
								     DEVSTAT_DEVICE_GONE);
						ioinfo[irq]->ui.flags.oper = 0;
					}
					ret = CIO_PATHGONE_DEVGONE;
				} else {
					ret = CIO_PATHGONE_WAIT4INT
						| CIO_PATHGONE_IOERR;
					ioinfo[irq]->ui.flags.noio = 1;
				}
				break;
			default: /* -ENODEV */
				s390irq_spin_unlock(irq);
				if (ioinfo[irq]->nopfunc) {
						ioinfo[irq]->ui.flags.oper = 0;
						ioinfo[irq]->nopfunc(irq,
								     DEVSTAT_DEVICE_GONE);
				}
				ret = CIO_PATHGONE_DEVGONE;
			}
		}
	}
	return ret;
}

void 
s390_do_chpid_processing( __u8 chpid)
{

	int irq;
	int j;
	char dbf_txt[15];

	sprintf(dbf_txt, "chpr%x", chpid);
	CIO_TRACE_EVENT( 2, dbf_txt);

	/* 
	 * TODO: the chpid may be not the chpid with the link incident,
	 * but the chpid the report came in through. How to handle???
	 */
	clear_bit(chpid, &chpids);
	if (!test_and_clear_bit(chpid, &chpids_known)) {
#ifdef CONFIG_DEBUG_CHSC
		pr_debug(KERN_DEBUG"Got link incident for unknown chpid %x\n",
		       chpid);
#endif /* CONFIG_DEBUG_CHSC */
		return;  /* we didn't know the chpid anyway */
	}

	for (irq=0;irq<=highest_subchannel;irq++) {
		schib_t *schib;
		
		if (ioinfo[irq] == INVALID_STORAGE_AREA) 
			continue;  /* we don't know the device anyway */
		if (ioinfo[irq]->st)
			continue; /* non-io subchannel */
		schib = &ioinfo[irq]->schib;
		for (j=0; j<8;j++) {
			int mask = 0x80 >> j;
			int out = 0;
			int err = 0;
			
			if (schib->pmcw.chpid[j] != chpid)
				continue;
			
			if (stsch(irq, schib) != 0) {
				ioinfo[irq]->ui.flags.oper = 0;
				if (ioinfo[irq]->nopfunc)
					ioinfo[irq]->nopfunc(irq, DEVSTAT_DEVICE_GONE);
				break;
			}
			
			s390irq_spin_lock(irq);
			
			ioinfo[irq]->ui.flags.noio = 1;
			
			/* Do we still expect an interrupt for outstanding io? */
			if (ioinfo[irq]->ui.flags.busy) {
				int rck = __check_for_io_and_kill(irq, mask, 1);
				if (rck & CIO_PATHGONE_WAIT4INT)
					out=1;
				if (rck & CIO_PATHGONE_IOERR)
					err=1;
				if (rck & CIO_PATHGONE_DEVGONE)
					break;
			}
			
			s390irq_spin_unlock(irq);
			
			/* 
			 * Tell the device driver not to disturb us. 
			 * If the driver is not capable of handling
			 * DEVSTAT_NOT_ACC, it doesn't want path grouping anyway.
			 */
			if (ioinfo[irq]->ui.flags.ready &&
			    schib->pmcw.pim != 0x80 &&
			    ioinfo[irq]->nopfunc &&
			    ioinfo[irq]->ui.flags.notacccap) {
				if (err)
					ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC_ERR);
				else
					ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);
			}

			ioinfo[irq]->opm &= ~mask;
			
			if (out)
				break;

			/* 
			 * Always schedule the path verification, even if opm=0.
			 * Reason: We can't rely on stsch() to return latest&greatest
			 * values, if a device selections hasn't been performed, and
			 * we might miss a path we didn't get a mchk for.
			 */
			if (ioinfo[irq]->ui.flags.ready)
				s390_schedule_path_verification(irq);
			else {
				ioinfo[irq]->ui.flags.noio = 0;
				ioinfo[irq]->ui.flags.killio = 0;
			}
			break;
		}
	}	
}


void 
s390_do_res_acc_processing( __u8 chpid, __u16 fla, int info)
{

	char dbf_txt[15];
	int irq = 0;
	__u32 fla_mask = 0xffff;
	int chp;
	int mask;

	sprintf(dbf_txt, "accpr%x", chpid);
	CIO_TRACE_EVENT( 2, dbf_txt);
	if (info != CHSC_SEI_ACC_CHPID) {
		sprintf(dbf_txt, "fla%x", fla);
		CIO_TRACE_EVENT( 2, dbf_txt);
	}
	sprintf(dbf_txt, "info:%d", info);
	CIO_TRACE_EVENT( 2, dbf_txt);
	
	/*
	 * I/O resources may have become accessible.
	 * Scan through all subchannels that may be concerned and
	 * do a validation on those.
	 * The more information we have (info), the less scanning
	 * will we have to do.
	 */

	if (!cio_chsc_desc_avail) 
		chsc_get_sch_descriptions();

	if (!cio_chsc_desc_avail) {
		/*
		 * Something went wrong...
		 */
#ifdef CONFIG_DEBUG_CRW
		printk( KERN_WARNING 
			"Error: Could not retrieve subchannel descriptions, "
		       "will not process css machine check...\n");
#endif /* CONFIG_DEBUG_CRW */
		CIO_CRW_EVENT( 0,
			       "Error: Could not retrieve subchannel descriptions, "
			       "will not process css machine check...\n");
		return;
	} 

	if (!test_bit(chpid, &chpids_logical)) {
#ifdef CONFIG_DEBUG_CHSC
		printk(KERN_DEBUG"chpid %x is logically offline, "
		       "skipping res acc processing\n", chpid);
#endif /* CONFIG_DEBUG_CHSC */
		return; /* no need to do the rest */
	}

	switch (info) {
	case CHSC_SEI_ACC_CHPID: /*
				  * worst case, we only know about the chpid
				  * the devices are attached to
				  */
#ifdef CONFIG_DEBUG_CHSC
		printk( KERN_DEBUG "Looking at chpid %x...\n", chpid);
#endif /* CONFIG_DEBUG_CHSC */
		
		for (irq=0; irq<__MAX_SUBCHANNELS; irq++) {
			
			if((ioinfo[irq] != INVALID_STORAGE_AREA) 
			   && (ioinfo[irq]->st != 0))
				continue;
				
			if (ioinfo[irq] == INVALID_STORAGE_AREA) {
				/*
				 * We don't know the device yet, but since a path
				 * may be available now to the device we'll have
				 * to do recognition again.
				 * Since we don't have any idea about which chpid
				 * that beast may be on we'll have to do a stsch
				 * on all devices, grr...
				 */
				int valret = 0;
				
				valret = s390_validate_subchannel(irq,0);
				if (valret == -ENXIO) {
					/* We're through */
					return;
				}
				if (irq > highest_subchannel)
					highest_subchannel = irq;
				if (valret == 0)
					s390_device_recognition_irq(irq);
				continue;
			}

			for (chp=0;chp<=7;chp++) {
				mask = 0x80 >> chp;
				
				/*
				 * check if chpid is in information
				 * updated by ssd
				 */
				if ((!ioinfo[irq]->ssd_info.valid) ||
				    (ioinfo[irq]->ssd_info.chpid[chp] != chpid))
					continue;

				/* Tell the device driver not to disturb us. */
				if (ioinfo[irq]->ui.flags.ready &&
				    ioinfo[irq]->schib.pmcw.pim != 0x80 &&
				    ioinfo[irq]->nopfunc &&
				    ioinfo[irq]->ui.flags.notacccap)
					ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);

				ioinfo[irq]->ui.flags.noio = 1;

				/* Do we still expect an interrupt for outstanding io? */
				if (ioinfo[irq]->ui.flags.busy)
					/* Wait for interrupt. */
					break;

				if (ioinfo[irq]->ui.flags.ready) {
					s390_schedule_path_verification(irq);
				} else
					ioinfo[irq]->ui.flags.noio = 0;

				break;
			}
		}
		break;
		
	case CHSC_SEI_ACC_LINKADDR: /*
				     * better, we know the link determined by
				     * the link address and the chpid
				     */
		fla_mask = 0xff00;
		/* fallthrough */

	case CHSC_SEI_ACC_FULLLINKADDR: /*
					 * best case, we know the CU image
					 * by chpid and full link address
					 */

#ifdef CONFIG_DEBUG_CHSC
		printk( KERN_DEBUG "Looking at chpid %x, link addr %x...\n", 
			chpid, fla);
#endif /* CONFIG_DEBUG_CHSC */
		
		for (irq=0; irq<__MAX_SUBCHANNELS; irq++) {
			int j;
			/*
			 * Walk through all subchannels and
			 * look if our chpid and our (masked) link 
			 * address are in somewhere
			 * Do a stsch for the found subchannels and
			 * perform path grouping  
			 */
			if (ioinfo[irq] == INVALID_STORAGE_AREA) {
				/* The full program again (see above), grr... */
				int valret = 0;
				
				valret = s390_validate_subchannel(irq,0);
				if (valret == -ENXIO) {
					/* We're done */
					return;
				}
				if (irq > highest_subchannel)
					highest_subchannel = irq;
				if (valret == 0)
					s390_device_recognition_irq(irq);
				continue;
			}
			if (ioinfo[irq]->st != 0)
				continue;
				
			/* Update our ssd_info */
			if (chsc_get_sch_desc_irq(irq)) 
				break;
			
			for (j=0;j<8;j++) {
				if ((ioinfo[irq]->ssd_info.chpid[j] != chpid) ||
				    ((ioinfo[irq]->ssd_info.fla[j]&fla_mask) != fla))
					continue;
					
				/* Tell the device driver not to disturb us. */
				if (ioinfo[irq]->ui.flags.ready &&
				    ioinfo[irq]->schib.pmcw.pim != 0x80 &&
				    ioinfo[irq]->nopfunc &&
				    ioinfo[irq]->ui.flags.notacccap)
					ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);

				ioinfo[irq]->ui.flags.noio = 1;

				/* Do we still expect an interrupt for outstanding io? */
				if (ioinfo[irq]->ui.flags.busy)
					/* Wait for interrupt. */
					break;

				if (ioinfo[irq]->ui.flags.ready) {
					s390_schedule_path_verification(irq);
				} else
					ioinfo[irq]->ui.flags.noio = 0;
				
				break;
			}
			break;
			
		}
		break;

	default: BUG();
	}
}

static int
__get_chpid_from_lir(void *data)
{
	struct lir {
		u8  iq;
		u8  ic;
		u16 sci;
		/* incident-node descriptor */
		u32 indesc[28];
		/* attached-node descriptor */
		u32 andesc[28];
		/* incident-specific information */
		u32 isinfo[28];
	} *lir;

	lir = (struct lir*) data;
	if (!(lir->iq&0x80))
		/* NULL link incident record */
		return -EINVAL;
	if (!(lir->indesc[0]&0xc0000000))
		/* node descriptor not valid */
		return -EINVAL;
	if (!(lir->indesc[0]&0x10000000))
		/* don't handle device-type nodes - FIXME */
		return -EINVAL;
	/* Byte 3 contains the chpid. Could also be CTCA, but we don't care */

	return (u16) (lir->indesc[0]&0x000000ff);
}

void 
s390_process_css( void ) 
{

	int ccode, do_sei, chpid;

	CIO_TRACE_EVENT( 2, "prcss");

	spin_lock(&chsc_lock_sei);

	if (!chsc_area_sei) {
		if (init_IRQ_complete) 
			chsc_area_sei = kmalloc(sizeof(chsc_area_t),GFP_KERNEL);
		else
			chsc_area_sei = alloc_bootmem(sizeof(chsc_area_t));
	}
	
	if (!chsc_area_sei) {
		printk( KERN_CRIT 
			"No memory to store event information...\n");
		spin_unlock(&chsc_lock_sei);
		return;
	}

	do_sei = 1;

	while (do_sei) {
		
		do_sei = 0;

		/* 
		 * build the chsc request block for store event information 
		 * and do the call 
		 */
		memset(chsc_area_sei,0,sizeof(chsc_area_t));
		chsc_area_sei->request_block.command_code1=0x0010;
		chsc_area_sei->request_block.command_code2=0x000E;
		
		ccode = chsc(chsc_area_sei);
		
		
		if (ccode)
			break;

		/* for debug purposes, check for problems */
		if (chsc_area_sei->response_block.response_code == 0x0003) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING 
				"s390_process_css: error in chsc request block!\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, 
				       "s390_process_css: "
				       "error in chsc request block!\n");
			break;
		}
		if (chsc_area_sei->response_block.response_code == 0x0005) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING 
				"s390_process_css: no event information stored\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, 
				       "s390_process_css: "
				       "no event information stored\n");
			break;
		}
		if (chsc_area_sei->response_block.response_code == 0x0002) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING
				"s390_process_css: invalid command!\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2,
				       "s390_process_css: "
				       "invalid command!\n");
			break;
		}
		if (chsc_area_sei->response_block.response_code != 0x0001) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_WARNING
				"s390_process_css: unknown response code %d\n",
				chsc_area_sei->response_block.response_code);
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2,
				       "s390_process_css: unknown response "
				       "code %d\n",
				       chsc_area_sei->response_block.response_code);
			break;
		}
		/* everything ok */
#ifdef CONFIG_DEBUG_CHSC
		printk( KERN_DEBUG 
			"s390_process_css: "
			"event information successfully stored\n");
#endif /* CONFIG_DEBUG_CHSC */
		CIO_CRW_EVENT( 4, 
			       "s390_process_css: "
			       "event information successfully stored\n");
					
		/* Check if there is more event information pending. */
		if (chsc_area_sei->response_block.response_block_data.
		    sei_res.flags & 0x80) {
#ifdef CONFIG_DEBUG_CHSC
			printk(KERN_INFO"s390_process_css: further event "
			       "information pending...\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, "further event information pending\n");

			do_sei = 1;
		}

		/* Check if we might have lost some information. */
		if (chsc_area_sei->response_block.response_block_data.
		    sei_res.flags & 0x40) {
#ifdef CONFIG_DEBUG_CHSC
			printk(KERN_ERR"s390_process_css: Event information has "
			       "been lost due to overflow!\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2, "Event information has "
				       "been lost due to overflow!\n");
		}
		
		if (chsc_area_sei->response_block.
		    response_block_data.sei_res.rs != 4) {
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_ERR
				"s390_process_css: "
				"reporting source (%04X) isn't a chpid!\n",
				chsc_area_sei->response_block.
				response_block_data.sei_res.rsid);
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 2,
				       "s390_process_css: "
				       "reporting source (%04X) isn't a chpid!\n",
				       chsc_area_sei->response_block.
				       response_block_data.sei_res.rsid);
			continue;
		}

		/* which kind of information was stored? */
		switch (chsc_area_sei->response_block.
			response_block_data.sei_res.cc) {
		case 1: /* link incident*/
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_DEBUG 
				"s390_process_css: "
				"channel subsystem reports link incident,"
				" source is chpid %x\n", 
				chsc_area_sei->response_block.
				response_block_data.sei_res.rsid);
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 4,
				       "s390_process_css: "
				       "channel subsystem reports "
				       "link incident, "
				       "source is chpid %x\n", 
				       chsc_area_sei->response_block.
				       response_block_data.sei_res.rsid);
			
			chpid = __get_chpid_from_lir(chsc_area_sei->response_block.
						     response_block_data.sei_res.
						     ccdf);
			if (chpid >= 0)
				s390_do_chpid_processing(chpid);
			break;
			
		case 2: /* i/o resource accessibiliy */
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_DEBUG 
				"s390_process_css: channel subsystem "
				"reports some I/O devices "
				"may have become accessible\n");
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 4,
				       "s390_process_css: "
				       "channel subsystem reports "
				       "some I/O devices "
				       "may have become accessible\n");
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_DEBUG 
				"Data received after sei: \n");
			printk( KERN_DEBUG 
				"Validity flags: %x\n", 
				chsc_area_sei->response_block.
				response_block_data.sei_res.vf);
#endif /* CONFIG_DEBUG_CHSC */
			if ((chsc_area_sei->response_block.
			     response_block_data.sei_res.vf&0x80)
			    == 0) {
#ifdef CONFIG_DEBUG_CHSC
				printk( KERN_DEBUG "chpid: %x\n",
					chsc_area_sei->response_block.
					response_block_data.sei_res.rsid);
#endif /* CONFIG_DEBUG_CHSC */
				s390_do_res_acc_processing
					(chsc_area_sei->response_block.
					 response_block_data.sei_res.rsid,
					 0,
					 CHSC_SEI_ACC_CHPID);
			} else if ((chsc_area_sei->response_block.
				    response_block_data.sei_res.vf&0xc0)
				   == 0x80) {
#ifdef CONFIG_DEBUG_CHSC
				printk( KERN_DEBUG 
					"chpid: %x  link addr: %x\n",
					chsc_area_sei->response_block.
					response_block_data.sei_res.rsid,
					chsc_area_sei->response_block.
					response_block_data.sei_res.fla);
#endif /* CONFIG_DEBUG_CHSC */
				s390_do_res_acc_processing
					(chsc_area_sei->response_block.
					 response_block_data.sei_res.rsid, 
					 chsc_area_sei->response_block.
					 response_block_data.sei_res.fla,
					 CHSC_SEI_ACC_LINKADDR);
			} else if ((chsc_area_sei->response_block.
				    response_block_data.sei_res.vf & 0xc0)
				   == 0xc0) {
#ifdef CONFIG_DEBUG_CHSC
				printk( KERN_DEBUG 
					"chpid: %x  "
					"full link addr: %x\n",
					chsc_area_sei->response_block.
					response_block_data.sei_res.rsid,
					chsc_area_sei->response_block.
					response_block_data.sei_res.fla);
#endif /* CONFIG_DEBUG_CHSC */
				s390_do_res_acc_processing
					(chsc_area_sei->response_block.
					 response_block_data.sei_res.rsid, 
					 chsc_area_sei->response_block.
					 response_block_data.sei_res.fla,
					 CHSC_SEI_ACC_FULLLINKADDR);
			}
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_DEBUG "\n");
#endif /* CONFIG_DEBUG_CHSC */
			
			break;
			
		default: /* other stuff */
#ifdef CONFIG_DEBUG_CHSC
			printk( KERN_DEBUG 
				"s390_process_css: event %d\n",
				chsc_area_sei->response_block.
				response_block_data.sei_res.cc);
#endif /* CONFIG_DEBUG_CHSC */
			CIO_CRW_EVENT( 4, 
				       "s390_process_css: event %d\n",
				       chsc_area_sei->response_block.
				       response_block_data.sei_res.cc);
			
			break;
			
		}
	}
		
	spin_unlock(&chsc_lock_sei);
}
#endif

static void
__process_chp_gone(int irq, int chpid)
{
	schib_t *schib = &ioinfo[irq]->schib;
	int i;
	
	for (i=0;i<8;i++) {
		int mask = 0x80>>i;
		int out = 0;
		int err = 0;
		
		if (schib->pmcw.chpid[i] != chpid)
			continue;
		
		if (stsch(irq, schib) != 0) {
			ioinfo[irq]->ui.flags.oper = 0;
			if (ioinfo[irq]->nopfunc)
				ioinfo[irq]->nopfunc(irq, DEVSTAT_DEVICE_GONE);
			break;
		}
		
		s390irq_spin_lock(irq);
		
		ioinfo[irq]->ui.flags.noio = 1;
		
		/* Do we still expect an interrupt for outstanding io? */
		if (ioinfo[irq]->ui.flags.busy) {
			int rck = __check_for_io_and_kill(irq, mask, 1);
			if (rck & CIO_PATHGONE_WAIT4INT)
				out=1;
			if (rck & CIO_PATHGONE_IOERR)
				err=1;
			if (rck & CIO_PATHGONE_DEVGONE)
				break;
		}
		
		s390irq_spin_unlock(irq);

		/* Tell the device driver not to disturb us. */
		if (ioinfo[irq]->ui.flags.ready &&
		    schib->pmcw.pim != 0x80 &&
		    ioinfo[irq]->nopfunc &&
		    ioinfo[irq]->ui.flags.notacccap) {
			if (err)
				ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC_ERR);
			else
				ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);
		}
		
		if (out)
			break;
		
		if (ioinfo[irq]->ui.flags.ready) {
			s390_schedule_path_verification(irq);
		} else {
			ioinfo[irq]->ui.flags.noio = 0;
			ioinfo[irq]->ui.flags.killio = 0;
		}
		break;
	}

}

static void
__process_chp_come(int irq, int chpid)
{
	schib_t *schib = &ioinfo[irq]->schib;
	int i;

	for (i=0;i<8;i++) {

		if (schib->pmcw.chpid[i] != chpid)
			continue;
		
		/* Tell the device driver not to disturb us. */
		if (ioinfo[irq]->ui.flags.ready &&
		    schib->pmcw.pim != 0x80 &&
		    ioinfo[irq]->nopfunc &&
		    ioinfo[irq]->ui.flags.notacccap)
			ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);
		
		ioinfo[irq]->ui.flags.noio = 1;
		
		/* Do we still expect an interrupt for outstanding io? */
		if (ioinfo[irq]->ui.flags.busy)
			/* Wait for interrupt. */
			break;
		
		if (ioinfo[irq]->ui.flags.ready)
			s390_schedule_path_verification(irq);
		else
			ioinfo[irq]->ui.flags.noio = 0;
		
		break;
	}
}

static void
s390_process_chp_source(int chpid, int onoff)
{
	int irq;
	int ret;
	char dbf_txt[15];

	sprintf(dbf_txt, "prchp%x", chpid);
	CIO_TRACE_EVENT(2, dbf_txt);

#ifdef CONFIG_CHSC
	if (onoff == 0) {
		clear_bit(chpid, &chpids);
	} else {
		set_bit(chpid, &chpids);
		set_bit(chpid, &chpids_known);
	}
#endif /* CONFIG_CHSC */

	if (onoff == 0) {
		for (irq=0;irq<=highest_subchannel;irq++) {

			if ((ioinfo[irq] == INVALID_STORAGE_AREA)
			    || (ioinfo[irq]->st != 0))
				continue;

			__process_chp_gone(irq, chpid);
		}
		return;
	}

	for (irq=0;irq<__MAX_SUBCHANNELS;irq++) {
		
		if (ioinfo[irq] == INVALID_STORAGE_AREA) {
			ret = s390_validate_subchannel(irq,0);
			if (ret == 0) {
				if (irq > highest_subchannel)
					highest_subchannel = irq;
#ifdef CONFIG_DEBUG_CRW
				printk(KERN_DEBUG"process_chp_source: Found "
				       "device on irq %x\n", irq);
#endif /* CONFIG_DEBUG_CRW */
				CIO_CRW_EVENT(4, "Found device on irq %x\n",
					      irq);
				s390_device_recognition_irq(irq);
			}
		} else if (ioinfo[irq]->st == 0) {
			ret = stsch(irq, &ioinfo[irq]->schib);
			if (ret != 0)
				ret = -ENXIO;
		} else
			continue;

		if (ret == -ENXIO)
			/* We're through. */
			return;

		if (ret != 0)
			continue;
		
		__process_chp_come(irq, chpid);
	}

}

/*
 * s390_do_crw_pending
 *
 * Called by the machine check handler to process CRW pending
 *  conditions. It may be a single CRW, or CRWs may be chained.
 *
 * Note : we currently process CRWs for subchannel source only
 */
void
s390_do_crw_pending (crwe_t * pcrwe)
{
	int irq;
	int chpid;
	
#ifdef CONFIG_DEBUG_CRW
	printk (KERN_DEBUG "do_crw_pending : starting ...\n");
#endif
	CIO_CRW_EVENT( 2, "do_crw_pending: starting\n");
	while (pcrwe != NULL) {

		switch (pcrwe->crw.rsc) {
		case CRW_RSC_SCH:
			
			irq = pcrwe->crw.rsid;
			
#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE "do_crw_pending : source is "
				"subchannel %04X\n", irq);
#endif
			CIO_CRW_EVENT(2, "source is subchannel %04X\n",
				      irq);
			s390_process_subchannel_source (irq);

			break;

		case CRW_RSC_MONITOR:

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE "do_crw_pending : source is "
				"monitoring facility\n");
#endif
			CIO_CRW_EVENT(2, "source is monitoring facility\n");
			break;

		case CRW_RSC_CPATH:

			chpid = pcrwe->crw.rsid;

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE "do_crw_pending : source is "
				"channel path %02X\n", chpid);
#endif
			CIO_CRW_EVENT(2, "source is channel path %02X\n",
				      chpid);
			switch (pcrwe->crw.erc) {
			case CRW_ERC_IPARM: /* Path has come. */
				s390_process_chp_source(chpid, 1);
				break;
			case CRW_ERC_PERRI: /* Path has gone. */
				s390_process_chp_source(chpid, 0);
				break;
			default:
#ifdef CONFIG_DEBUG_CRW
				printk(KERN_WARNING"do_crw_pending: don't "
				       "know how to handle erc=%x\n",
				       pcrwe->crw.erc);
#endif /* CONFIG_DEBUG_CRW */
				CIO_CRW_EVENT(0, "don't know how to handle "
					      "erc=%x\n", pcrwe->crw.erc);
			}
			break;

		case CRW_RSC_CONFIG:

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE "do_crw_pending : source is "
				"configuration-alert facility\n");
#endif
			CIO_CRW_EVENT(2, "source is configuration-alert facility\n");
			break;

		case CRW_RSC_CSS:

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE "do_crw_pending : source is "
				"channel subsystem\n");
#endif
			CIO_CRW_EVENT(2, "source is channel subsystem\n");
#ifdef CONFIG_CHSC
			s390_process_css();
#endif
			break;

		default:

#ifdef CONFIG_DEBUG_CRW
			printk (KERN_NOTICE
				"do_crw_pending : unknown source\n");
#endif
			CIO_CRW_EVENT( 2, "unknown source\n");
			break;

		}

		pcrwe = pcrwe->crwe_next;

	}

#ifdef CONFIG_DEBUG_CRW
	printk (KERN_DEBUG "do_crw_pending : done\n");
#endif
	CIO_CRW_EVENT(2, "do_crw_pending: done\n");
	return;
}

/* added by Holger Smolinski for reipl support in reipl.S */
extern void do_reipl (int);
void
reipl (int sch)
{
	int i;
	s390_dev_info_t dev_info;

	for (i = 0; i <= highest_subchannel; i++) {
		if (get_dev_info_by_irq (i, &dev_info) == 0
		    && (dev_info.status & DEVSTAT_DEVICE_OWNED)) {
			free_irq (i, ioinfo[i]->irq_desc.dev_id);
		}
	}
	if (MACHINE_IS_VM)
		cpcmd ("IPL", NULL, 0);
	else
		do_reipl (0x10000 | sch);
}

/*
 * Function: cio_debug_init
 * Initializes three debug logs (under /proc/s390dbf) for common I/O:
 * - cio_msg logs the messages which are printk'ed when CONFIG_DEBUG_IO is on
 * - cio_trace logs the calling of different functions
 * - cio_crw logs the messages which are printk'ed when CONFIG_DEBUG_CRW is on
 * debug levels depend on CONFIG_DEBUG_IO resp. CONFIG_DEBUG_CRW
 */
int
cio_debug_init (void)
{
	int ret = 0;

	cio_debug_msg_id = debug_register ("cio_msg", 4, 4, 16 * sizeof (long));
	if (cio_debug_msg_id != NULL) {
		debug_register_view (cio_debug_msg_id, &debug_sprintf_view);
		debug_set_level (cio_debug_msg_id, 6);
	} else {
		ret = -1;
	}
	cio_debug_trace_id = debug_register ("cio_trace", 4, 4, 8);
	if (cio_debug_trace_id != NULL) {
		debug_register_view (cio_debug_trace_id, &debug_hex_ascii_view);
		debug_set_level (cio_debug_trace_id, 6);
	} else {
		ret = -1;
	}
	cio_debug_crw_id = debug_register ("cio_crw", 2, 4, 16 * sizeof (long));
	if (cio_debug_crw_id != NULL) {
		debug_register_view (cio_debug_crw_id, &debug_sprintf_view);
		debug_set_level (cio_debug_crw_id, 6);
	} else {
		ret = -1;
	}
	if (ret)
		return ret;
	cio_debug_initialized = 1;
	return 0;
}

__initcall (cio_debug_init);

#ifdef CONFIG_PROC_FS
#ifdef CONFIG_CHSC
/*
 * Function: cio_parse_chpids_proc_parameters
 * parse the stuff piped to /proc/chpids
 */
void 
cio_parse_chpids_proc_parameters(char* buf)
{
	int i;
	int cp;
	int ret;

	if (strstr(buf, "on ")) {
		for (i=0; i<3; i++) {
			buf++;
		}
		cp = blacklist_strtoul(buf, &buf);

		chsc_get_sch_descriptions();
		if (!cio_chsc_desc_avail) {
			printk(KERN_ERR "Could not get chpid status, "
			       "vary on/off not available\n");
			return;
		}
 
		if (!test_bit(cp, &chpids)) {
			ret = s390_vary_chpid(cp, 1);
			if (ret == -EINVAL) {
#ifdef CONFIG_DEBUG_CHSC
				printk(KERN_ERR "/proc/chpids: "
				       "Invalid chpid specified\n");
#else /* CONFIG_DEBUG_CHSC */
				printk(KERN_DEBUG "/proc/chpids: "
				       "Invalid chpid specified\n");
#endif /* CONFIG_DEBUG_CHSC */
			} else if (ret == 0) {
				printk(KERN_INFO "/proc/chpids: "
				       "Varied chpid %x logically online\n",
				       cp);
			}
		} else {
			printk(KERN_ERR "/proc/chpids: chpid %x is "
			       "already online\n",
			       cp);
		}
	} else if (strstr(buf, "off ")) {
		for (i=0; i<4; i++) {
			buf++;
		}
		cp = blacklist_strtoul(buf, &buf);

		chsc_get_sch_descriptions();
		if (!cio_chsc_desc_avail) {
			printk(KERN_ERR "Could not get chpid status, "
			       "vary on/off not available\n");
			return;
		}
		
		if (test_bit(cp, &chpids)) {
			ret = s390_vary_chpid(cp, 0);
			if (ret == -EINVAL) {
#ifdef CONFIG_DEBUG_CHSC
				printk(KERN_ERR "/proc/chpids: "
				       "Invalid chpid specified\n");
#else /* CONFIG_DEBUG_CHSC */
				printk(KERN_DEBUG "/proc/chpids: "
				       "Invalid chpid specified\n");
#endif /* CONFIG_DEBUG_CHSC */
			} else if (ret == 0) {
				printk(KERN_INFO "/proc/chpids: "
				       "Varied chpid %x logically offline\n",
				       cp);
			}
		} else { 
			printk(KERN_ERR "/proc/chpids: "
			       "chpid %x is already offline\n",
			       cp);
		}
	} else {
		printk(KERN_ERR "/proc/chpids: Parse error; "
		       "try using '{on,off} <chpid>'\n");
	}
}

static void
__vary_chpid_offline(int irq, int chpid)
{
	schib_t *schib = &ioinfo[irq]->schib;
	int i;
	
	for (i=0;i<8;i++) {
		int mask = 0x80>>i;
		int out = 0;
		unsigned long flags;
		
		if (ioinfo[irq]->ssd_info.chpid[i] != chpid)
			continue;
		
		s390irq_spin_lock_irqsave(irq, flags);
		
		ioinfo[irq]->ui.flags.noio = 1;
		
		/* Hmm, the path is not really gone... */
		if (ioinfo[irq]->ui.flags.busy) {
			if (__check_for_io_and_kill(irq, mask, 0) != 0)
				out=1;
		}
		
		s390irq_spin_unlock_irqrestore(irq, flags);

		/* Tell the device driver not to disturb us. */
		if (ioinfo[irq]->ui.flags.ready &&
		    schib->pmcw.pim != 0x80 &&
		    ioinfo[irq]->nopfunc &&
		    ioinfo[irq]->ui.flags.notacccap)
			ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);
		
		if (out)
			break;
		
		if (ioinfo[irq]->ui.flags.ready)
			s390_schedule_path_verification(irq);
		else
			ioinfo[irq]->ui.flags.noio = 0;

		break;
	}

}

static void
__vary_chpid_online(int irq, int chpid)
{
	schib_t *schib = &ioinfo[irq]->schib;
	int i;

	for (i=0;i<8;i++) {

		if (schib->pmcw.chpid[i] != chpid)
			continue;
		
		/* Tell the device driver not to disturb us. */
		if (ioinfo[irq]->ui.flags.ready &&
		    schib->pmcw.pim != 0x80 &&
		    ioinfo[irq]->nopfunc &&
		    ioinfo[irq]->ui.flags.notacccap)
			ioinfo[irq]->nopfunc(irq, DEVSTAT_NOT_ACC);
		
		ioinfo[irq]->ui.flags.noio = 1;
		
		/* Do we still expect an interrupt for outstanding io? */
		if (ioinfo[irq]->ui.flags.busy)
			/* Wait for interrupt. */
			break;
		
		s390_schedule_path_verification(irq);
		
		break;
	}
}


/*
 * Function: s390_vary_chpid
 * Varies the specified chpid online or offline
 */
int 
s390_vary_chpid( __u8 chpid, int on) 
{
	char dbf_text[15];
	int irq;

	if ((chpid <=0) || (chpid >= NR_CHPIDS))
		return -EINVAL;

	sprintf(dbf_text, on?"varyon%x":"varyoff%x", chpid);
	CIO_TRACE_EVENT( 2, dbf_text);

	if (!test_bit(chpid, &chpids_known)) {
		printk(KERN_ERR "Can't vary unknown chpid %02X\n", chpid);
		return -EINVAL;
	}

	if (on && test_bit(chpid, &chpids_logical)) {
		printk(KERN_ERR "chpid %02X already logically online\n", 
		       chpid);
		return -EINVAL;
	}

	if (!on && !test_bit(chpid, &chpids_logical)) {
		printk(KERN_ERR "chpid %02X already logically offline\n", 
		       chpid);
		return -EINVAL;
	}

	if (on) {
		set_bit(chpid, &chpids_logical);
		set_bit(chpid, &chpids);

	} else {
		clear_bit(chpid, &chpids_logical);
		clear_bit(chpid, &chpids);
	}

	/*
	 * Redo PathVerification on the devices the chpid connects to 
	 */
	
	for (irq=0;irq<=highest_subchannel;irq++) {

		if (ioinfo[irq] == INVALID_STORAGE_AREA)
			continue;

		if (ioinfo[irq]->st)
			continue;
		
		if (!ioinfo[irq]->ssd_info.valid)
			continue;

		if (on)
			__vary_chpid_online(irq, chpid);
		else
			__vary_chpid_offline(irq, chpid);

	}

	return 0;
}
#endif /* CONFIG_CHSC */

/* 
 * Display info on subchannels in /proc/subchannels. 
 * Adapted from procfs stuff in dasd.c by Cornelia Huck, 02/28/01.      
 */

typedef struct {
	char *data;
	int len;
} tempinfo_t;

#define MIN(a,b) ((a)<(b)?(a):(b))

static struct proc_dir_entry *chan_subch_entry;

static int
chan_subch_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	int i = 0;
	int j = 0;
	tempinfo_t *info;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		return -ENOMEM;
	} else {
		file->private_data = (void *) info;
	}

	size += (highest_subchannel + 1) * 128;
	info->data = (char *) vmalloc (size);

	if (size && info->data == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		vfree (info);
		return -ENOMEM;
	}

	len += sprintf (info->data + len,
			"Device sch.  Dev Type/Model CU  in use  PIM PAM POM CHPIDs\n");
	len += sprintf (info->data + len,
			"---------------------------------------------------------------------\n");

	for (i = 0; i <= highest_subchannel; i++) {
		if (!((ioinfo[i] == NULL) || (ioinfo[i] == INVALID_STORAGE_AREA)
		      || (ioinfo[i]->st )|| !(ioinfo[i]->ui.flags.oper))) {
			len +=
			    sprintf (info->data + len, "%04X   %04X  ",
				     ioinfo[i]->schib.pmcw.dev, i);
			if (ioinfo[i]->senseid.dev_type != 0) {
				len += sprintf (info->data + len,
						"%04X/%02X   %04X/%02X",
						ioinfo[i]->senseid.dev_type,
						ioinfo[i]->senseid.dev_model,
						ioinfo[i]->senseid.cu_type,
						ioinfo[i]->senseid.cu_model);
			} else {
				len += sprintf (info->data + len,
						"          %04X/%02X",
						ioinfo[i]->senseid.cu_type,
						ioinfo[i]->senseid.cu_model);
			}
			if (ioinfo[i]->ui.flags.ready) {
				len += sprintf (info->data + len, "  yes ");
			} else {
				len += sprintf (info->data + len, "      ");
			}
			len += sprintf (info->data + len,
					"    %02X  %02X  %02X  ",
					ioinfo[i]->schib.pmcw.pim,
					ioinfo[i]->schib.pmcw.pam,
					ioinfo[i]->schib.pmcw.pom);
			for (j = 0; j < 8; j++) {
				len += sprintf (info->data + len,
						"%02X",
						ioinfo[i]->schib.pmcw.chpid[j]);
				if (j == 3) {
					len += sprintf (info->data + len, " ");
				}
			}
			len += sprintf (info->data + len, "\n");
		}
	}
	info->len = len;

	return rc;
}

static int
chan_subch_close (struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		vfree (p_info);
	}

	return rc;
}

static ssize_t
chan_subch_read (struct file *file, char *user_buf, size_t user_len,
		 loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (*offset >= p_info->len) {
		return 0;
	} else {
		len = MIN (user_len, (p_info->len - *offset));
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;
	}
}

static struct file_operations chan_subch_file_ops = {
	read:chan_subch_read, open:chan_subch_open, release:chan_subch_close,
};

static int
chan_proc_init (void)
{
	chan_subch_entry =
	    create_proc_entry ("subchannels", S_IFREG | S_IRUGO, &proc_root);
	chan_subch_entry->proc_fops = &chan_subch_file_ops;

	return 1;
}

__initcall (chan_proc_init);

void
chan_proc_cleanup (void)
{
	remove_proc_entry ("subchannels", &proc_root);
}

/* 
 * Display device specific information under /proc/deviceinfo/<devno>
 */ static struct proc_dir_entry *cio_procfs_deviceinfo_root = NULL;

/* 
 * cio_procfs_device_list holds all devno-specific procfs directories
 */

typedef struct {
	int devno;
	struct proc_dir_entry *cio_device_entry;
	struct proc_dir_entry *cio_sensedata_entry;
	struct proc_dir_entry *cio_in_use_entry;
	struct proc_dir_entry *cio_chpid_entry;
} cio_procfs_entry_t;

typedef struct _cio_procfs_device {
	struct _cio_procfs_device *next;
	cio_procfs_entry_t *entry;
} cio_procfs_device_t;

cio_procfs_device_t *cio_procfs_device_list = NULL;

/*
 * File operations
 */

static int
cio_device_entry_close (struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		vfree (p_info);
	}

	return rc;
}

static ssize_t
cio_device_entry_read (struct file *file, char *user_buf, size_t user_len,
		       loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (*offset >= p_info->len) {
		return 0;
	} else {
		len = MIN (user_len, (p_info->len - *offset));
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;
	}
}

static int
cio_sensedata_entry_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	int irq;
	int devno;
	char *devno_str;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += 2 * 32;
		info->data = (char *) vmalloc (size);
		if (size && info->data == NULL) {
			printk (KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			devno_str = kmalloc (6 * sizeof (char), GFP_KERNEL);
			memset (devno_str, 0, 6 * sizeof (char));
			memcpy (devno_str,
				file->f_dentry->d_parent->d_name.name,
				strlen (file->f_dentry->d_parent->d_name.name) +
				1);
			devno = simple_strtoul (devno_str, &devno_str, 16);
			irq = get_irq_by_devno (devno);
			if (irq != -1) {
				len +=
				    sprintf (info->data + len,
					     "Dev Type/Mod: ");
				if (ioinfo[irq]->senseid.dev_type == 0) {
					len +=
					    sprintf (info->data + len,
						     "%04X/%02X\n",
						     ioinfo[irq]->senseid.
						     cu_type,
						     ioinfo[irq]->senseid.
						     cu_model);
				} else {
					len +=
					    sprintf (info->data + len,
						     "%04X/%02X\n",
						     ioinfo[irq]->senseid.
						     dev_type,
						     ioinfo[irq]->senseid.
						     dev_model);
					len +=
					    sprintf (info->data + len,
						     "CU Type/Mod:  %04X/%02X\n",
						     ioinfo[irq]->senseid.
						     cu_type,
						     ioinfo[irq]->senseid.
						     cu_model);
				}
			}
			info->len = len;
		}
	}

	return rc;
}

static int
cio_in_use_entry_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	int irq;
	int devno;
	char *devno_str;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += 8;
		info->data = (char *) vmalloc (size);
		if (size && info->data == NULL) {
			printk (KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			devno_str = kmalloc (6 * sizeof (char), GFP_KERNEL);
			memset (devno_str, 0, 6 * sizeof (char));
			memcpy (devno_str,
				file->f_dentry->d_parent->d_name.name,
				strlen (file->f_dentry->d_parent->d_name.name) +
				1);
			devno = simple_strtoul (devno_str, &devno_str, 16);
			irq = get_irq_by_devno (devno);
			if (irq != -1) {
				len +=
				    sprintf (info->data + len, "%s\n",
					     ioinfo[irq]->ui.flags.
					     ready ? "yes" : "no");
			}
			info->len = len;
		}
	}

	return rc;
}

static int
cio_chpid_entry_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	int irq;
	int devno;
	int i;
	char *devno_str;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += 8 * 16;
		info->data = (char *) vmalloc (size);
		if (size && info->data == NULL) {
			printk (KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			devno_str = kmalloc (6 * sizeof (char), GFP_KERNEL);
			memset (devno_str, 0, 6 * sizeof (char));
			memcpy (devno_str,
				file->f_dentry->d_parent->d_name.name,
				strlen (file->f_dentry->d_parent->d_name.name) +
				1);
			devno = simple_strtoul (devno_str, &devno_str, 16);
			irq = get_irq_by_devno (devno);
			if (irq != -1) {
				for (i = 0; i < 8; i++) {
					len +=
					    sprintf (info->data + len,
						     "CHPID[%d]: ", i);
					len +=
					    sprintf (info->data + len, "%02X\n",
						     ioinfo[irq]->schib.pmcw.
						     chpid[i]);
				}
			}
			info->len = len;
		}
	}

	return rc;
}

static struct file_operations cio_sensedata_entry_file_ops = {
	read:cio_device_entry_read, open:cio_sensedata_entry_open,
	release:cio_device_entry_close,
};

static struct file_operations cio_in_use_entry_file_ops = {
	read:cio_device_entry_read, open:cio_in_use_entry_open,
	release:cio_device_entry_close,
};

static struct file_operations cio_chpid_entry_file_ops = {
	read:cio_device_entry_read, open:cio_chpid_entry_open,
	release:cio_device_entry_close,
};

/*
 * Function: cio_procfs_device_create
 * create procfs entry for given device number
 * and insert it into list
 */
int
cio_procfs_device_create (int devno)
{
	cio_procfs_entry_t *entry;
	cio_procfs_device_t *tmp;
	cio_procfs_device_t *where;
	char buf[8];
	int i;
	int rc = 0;

	/* create the directory entry */
	entry =
	    (cio_procfs_entry_t *) kmalloc (sizeof (cio_procfs_entry_t),
					    GFP_KERNEL);
	if (entry) {
		entry->devno = devno;
		sprintf (buf, "%x", devno);
		entry->cio_device_entry =
		    proc_mkdir (buf, cio_procfs_deviceinfo_root);

		if (entry->cio_device_entry) {
			tmp = (cio_procfs_device_t *)
			    kmalloc (sizeof (cio_procfs_device_t), GFP_KERNEL);
			if (tmp) {
				tmp->entry = entry;

				if (cio_procfs_device_list == NULL) {
					cio_procfs_device_list = tmp;
					tmp->next = NULL;
				} else {
					where = cio_procfs_device_list;
					i = where->entry->devno;
					while ((devno > i)
					       && (where->next != NULL)) {
						where = where->next;
						i = where->entry->devno;
					}
					if (where->next == NULL) {
						where->next = tmp;
						tmp->next = NULL;
					} else {
						tmp->next = where->next;
						where->next = tmp;
					}
				}
				/* create the different entries */
				entry->cio_sensedata_entry =
				    create_proc_entry ("sensedata",
						       S_IFREG | S_IRUGO,
						       entry->cio_device_entry);
				entry->cio_sensedata_entry->proc_fops =
				    &cio_sensedata_entry_file_ops;
				entry->cio_in_use_entry =
				    create_proc_entry ("in_use",
						       S_IFREG | S_IRUGO,
						       entry->cio_device_entry);
				entry->cio_in_use_entry->proc_fops =
				    &cio_in_use_entry_file_ops;
				entry->cio_chpid_entry =
				    create_proc_entry ("chpids",
						       S_IFREG | S_IRUGO,
						       entry->cio_device_entry);
				entry->cio_chpid_entry->proc_fops =
				    &cio_chpid_entry_file_ops;
			} else {
				printk (KERN_WARNING
					"Error, could not allocate procfs structure!\n");
				remove_proc_entry (buf,
						   cio_procfs_deviceinfo_root);
				kfree (entry);
				rc = -ENOMEM;
			}
		} else {
			printk (KERN_WARNING
				"Error, could not allocate procfs structure!\n");
			kfree (entry);
			rc = -ENOMEM;
		}

	} else {
		printk (KERN_WARNING
			"Error, could not allocate procfs structure!\n");
		rc = -ENOMEM;
	}
	return rc;
}

/*
 * Function: cio_procfs_device_remove
 * remove procfs entry for given device number
 */
int
cio_procfs_device_remove (int devno)
{
	int rc = 0;
	cio_procfs_device_t *tmp;
	cio_procfs_device_t *prev = NULL;

	tmp = cio_procfs_device_list;
	while (tmp) {
		if (tmp->entry->devno == devno)
			break;
		prev = tmp;
		tmp = tmp->next;
	}
	if (tmp) {
		char buf[8];

		remove_proc_entry ("sensedata", tmp->entry->cio_device_entry);
		remove_proc_entry ("in_use", tmp->entry->cio_device_entry);
		remove_proc_entry ("chpid", tmp->entry->cio_device_entry);
		sprintf (buf, "%x", devno);
		remove_proc_entry (buf, cio_procfs_deviceinfo_root);

		if (tmp == cio_procfs_device_list) {
			cio_procfs_device_list = tmp->next;
		} else {
			prev->next = tmp->next;
		}
		kfree (tmp->entry);
		kfree (tmp);
	} else {
		rc = -ENODEV;
	}

	return rc;
}

/*
 * Function: cio_procfs_purge
 * purge /proc/deviceinfo of entries for gone devices
 */

int
cio_procfs_device_purge (void)
{
	int i;

	for (i = 0; i <= highest_subchannel; i++) {
		if (ioinfo[i] != INVALID_STORAGE_AREA) {
			if (!ioinfo[i]->ui.flags.oper)
				cio_procfs_device_remove (ioinfo[i]->devno);
		}
	}
	return 0;
}

/*
 * Function: cio_procfs_create
 * create /proc/deviceinfo/ and subdirs for the devices
 */
static int
cio_procfs_create (void)
{
	int irq;

	if (cio_proc_devinfo) {

		cio_procfs_deviceinfo_root =
		    proc_mkdir ("deviceinfo", &proc_root);

		if (highest_subchannel >= MAX_CIO_PROCFS_ENTRIES) {
			printk (KERN_ALERT
				"Warning: Not enough inodes for creating all "
				"entries under /proc/deviceinfo/. "
				"Not every device will get an entry.\n");
		}

		for (irq = 0; irq <= highest_subchannel; irq++) {
			if (irq >= MAX_CIO_PROCFS_ENTRIES)
				break;
			if (ioinfo[irq] != INVALID_STORAGE_AREA) {
				if (ioinfo[irq]->ui.flags.oper)
					if (cio_procfs_device_create
					    (ioinfo[irq]->devno) == -ENOMEM) {
						printk (KERN_CRIT
							"Out of memory while creating "
							"entries in /proc/deviceinfo/, "
							"not all devices might show up\n");
						break;
					}
			}
		}

	}

	return 1;
}

__initcall (cio_procfs_create);

/*
 * Entry /proc/cio_ignore to display blacklisted ranges of devices.
 * un-ignore devices by piping to /proc/cio_ignore:
 * free all frees all blacklisted devices, free <range>,<range>,...
 * frees specified ranges of devnos
 * add <range>,<range>,... will add a range of devices to blacklist -
 * but only for devices not already known
 */

static struct proc_dir_entry *cio_ignore_proc_entry;
static int
cio_ignore_proc_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	long flags;
	int i, j;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += nr_ignored * 6;
		info->data = (char *) vmalloc (size);
		if (size && info->data == NULL) {
			printk (KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			spin_lock_irqsave (&blacklist_lock, flags);
			for (i = 0; i <= highest_ignored; i++)
				if (test_bit (i, &bl_dev)) {
					len +=
					    sprintf (info->data + len, "%04x ",
						     i);
					for (j = i; (j <= highest_ignored)
					     && (test_bit (j, &bl_dev)); j++) ;
					j--;
					if (i != j)
						len +=
						    sprintf (info->data + len,
							     "- %04x", j);
					len += sprintf (info->data + len, "\n");
					i = j;
				}
			spin_unlock_irqrestore (&blacklist_lock, flags);
			info->len = len;
		}
	}
	return rc;
}

static int
cio_ignore_proc_close (struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		vfree (p_info);
	}

	return rc;
}

static ssize_t
cio_ignore_proc_read (struct file *file, char *user_buf, size_t user_len,
		      loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (*offset >= p_info->len) {
		return 0;
	} else {
		len = MIN (user_len, (p_info->len - *offset));
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;
	}
}

static ssize_t
cio_ignore_proc_write (struct file *file, const char *user_buf,
		       size_t user_len, loff_t * offset)
{
	char *buffer;
	
	if(user_len > 65536)
		user_len = 65536;
	
	buffer = vmalloc (user_len + 1);

	if (buffer == NULL)
		return -ENOMEM;
	if (copy_from_user (buffer, user_buf, user_len)) {
		vfree (buffer);
		return -EFAULT;
	}
	buffer[user_len] = '\0';
#ifdef CONFIG_DEBUG_IO
	printk (KERN_DEBUG "/proc/cio_ignore: '%s'\n", buffer);
#endif /* CONFIG_DEBUG_IO */

	blacklist_parse_proc_parameters (buffer);

	vfree (buffer);
	return user_len;
}

static struct file_operations cio_ignore_proc_file_ops = {
	read:cio_ignore_proc_read, open:cio_ignore_proc_open,
	write:cio_ignore_proc_write, release:cio_ignore_proc_close,
};

static int
cio_ignore_proc_init (void)
{
	cio_ignore_proc_entry =
	    create_proc_entry ("cio_ignore", S_IFREG | S_IRUGO | S_IWUSR,
			       &proc_root);
	cio_ignore_proc_entry->proc_fops = &cio_ignore_proc_file_ops;

	return 1;
}

__initcall (cio_ignore_proc_init);

/*
 * Entry /proc/irq_count
 * display how many irqs have occured per cpu...
 */

static struct proc_dir_entry *cio_irq_proc_entry;

static int
cio_irq_proc_open (struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	int i;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		printk (KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += NR_CPUS * 16;
		info->data = (char *) vmalloc (size);
		if (size && info->data == NULL) {
			printk (KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			for (i = 0; i < NR_CPUS; i++) {
				if (s390_irq_count[i] != 0)
					len +=
					    sprintf (info->data + len, "%lx\n",
						     s390_irq_count[i]);
			}
			info->len = len;
		}
	}
	return rc;
}

static int
cio_irq_proc_close (struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		vfree (p_info);
	}

	return rc;
}

static ssize_t
cio_irq_proc_read (struct file *file, char *user_buf, size_t user_len,
		   loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (*offset >= p_info->len) {
		return 0;
	} else {
		len = MIN (user_len, (p_info->len - *offset));
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;
	}
}

static struct file_operations cio_irq_proc_file_ops = {
	read:cio_irq_proc_read, open:cio_irq_proc_open,
	release:cio_irq_proc_close,
};

static int
cio_irq_proc_init (void)
{

	int i;

	if (cio_count_irqs) {
		for (i = 0; i < NR_CPUS; i++)
			s390_irq_count[i] = 0;
		cio_irq_proc_entry =
		    create_proc_entry ("irq_count", S_IFREG | S_IRUGO,
				       &proc_root);
		cio_irq_proc_entry->proc_fops = &cio_irq_proc_file_ops;
	}

	return 1;
}

__initcall (cio_irq_proc_init);


#ifdef CONFIG_CHSC
/*
 * /proc/chpids to display available chpids
 * vary chpids on/off by piping to it
 */

static struct proc_dir_entry *cio_chpids_proc_entry;

static int 
cio_chpids_proc_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	int size = 1;
	int len = 0;
	tempinfo_t *info;
	int i;

	if (!cio_chsc_desc_avail) {
		/* 
		 * We have not yet retrieved the link addresses,
		 * so we do it now.
		 */
		chsc_get_sch_descriptions();
	}


	info = (tempinfo_t *) vmalloc(sizeof(tempinfo_t));
	if (info == NULL) {
		printk( KERN_WARNING "No memory available for data\n");
		rc = -ENOMEM;
	} else {
		file->private_data = (void *) info;
		size += NR_CHPIDS * 16;
		info->data = (char *) vmalloc(size);
		if ( size && info->data == NULL) {
			printk( KERN_WARNING "No memory available for data\n");
			vfree (info);
			rc = -ENOMEM;
		} else {
			/* update our stuff */
			chsc_get_sch_descriptions();
			if (!cio_chsc_desc_avail) {
				len += sprintf(info->data+len, "no info available\n");
				goto cont;
			}

			for (i=0;i<NR_CHPIDS;i++) {
				if (test_bit(i, &chpids_known)) {
					if (!test_bit(i, &chpids))
						len += sprintf(info->data+len,
							       "%02X n/a\n",
							       i);
					else if (test_bit(i, &chpids_logical))
						len += sprintf(info->data+len, 
							       "%02X online\n", 
							       i);
					else 
						len += sprintf(info->data+len, 
							       "%02X logically "
							       "offline\n", 
							       i);
				}

			}
		cont:
			info->len = len;
		}
	}
	return rc;
}

static int 
cio_chpids_proc_close(struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

     if (p_info) {
	  if (p_info->data)
	       vfree( p_info->data );
	  vfree( p_info );
     }
     
     return rc;
}

static ssize_t 
cio_chpids_proc_read( struct file *file, char *user_buf, size_t user_len, loff_t * offset)
{
     loff_t len;
     tempinfo_t *p_info = (tempinfo_t *) file->private_data;
     
     if ( *offset>=p_info->len) {
	  return 0;
     } else {
	  len = MIN(user_len, (p_info->len - *offset));
	  if (copy_to_user( user_buf, &(p_info->data[*offset]), len))
	       return -EFAULT; 
	  (* offset) += len;
	  return len;
     }
}

static ssize_t 
cio_chpids_proc_write (struct file *file, const char *user_buf,
		       size_t user_len, loff_t * offset)
{
	char *buffer;
	
	if(user_len > 65536)
		user_len = 65536;
	
	buffer = vmalloc (user_len + 1);

	if (buffer == NULL)
		return -ENOMEM;
	if (copy_from_user (buffer, user_buf, user_len)) {
		vfree (buffer);
		return -EFAULT;
	}
	buffer[user_len]='\0';
#ifdef CIO_DEBUG_IO
	printk("/proc/chpids: '%s'\n", buffer);
#endif /* CIO_DEBUG_IO */

	cio_parse_chpids_proc_parameters(buffer);

	vfree (buffer);
	return user_len;
}

static struct file_operations cio_chpids_proc_file_ops =
{
	read:cio_chpids_proc_read,
	open:cio_chpids_proc_open,
	write:cio_chpids_proc_write,
	release:cio_chpids_proc_close,
};

static int 
cio_chpids_proc_init(void)
{

	cio_chpids_proc_entry = create_proc_entry("chpids", S_IFREG|S_IRUGO|S_IWUSR, &proc_root);
	cio_chpids_proc_entry->proc_fops = &cio_chpids_proc_file_ops;
	
	return 1;


}

__initcall(cio_chpids_proc_init);
#endif
/* end of procfs stuff */
#endif

schib_t *
s390_get_schib (int irq)
{
	if ((irq > highest_subchannel) || (irq < 0))
		return NULL;
	if (ioinfo[irq] == INVALID_STORAGE_AREA)
		return NULL;
	if (ioinfo[irq]->st)
		return NULL;
	return &ioinfo[irq]->schib;

}

int
s390_set_private_data(int irq, void *data)
{
	SANITY_CHECK(irq);
	
	ioinfo[irq]->private_data = data;
		
	return 0;
}

void *
s390_get_private_data(int irq)
{
	if ((irq > highest_subchannel) || (irq < 0))
		return NULL;
	if (ioinfo[irq] == INVALID_STORAGE_AREA)
		return NULL;
	if (ioinfo[irq]->st)
		return NULL;
	return ioinfo[irq]->private_data;
}

EXPORT_SYMBOL (halt_IO);
EXPORT_SYMBOL (clear_IO);
EXPORT_SYMBOL (do_IO);
EXPORT_SYMBOL (resume_IO);
EXPORT_SYMBOL (ioinfo);
EXPORT_SYMBOL (diag210);
EXPORT_SYMBOL (get_dev_info_by_irq);
EXPORT_SYMBOL (get_dev_info_by_devno);
EXPORT_SYMBOL (get_irq_by_devno);
EXPORT_SYMBOL (get_devno_by_irq);
EXPORT_SYMBOL (get_irq_first);
EXPORT_SYMBOL (get_irq_next);
EXPORT_SYMBOL (read_conf_data);
EXPORT_SYMBOL (read_dev_chars);
EXPORT_SYMBOL (s390_request_irq_special);
EXPORT_SYMBOL (s390_get_schib);
EXPORT_SYMBOL (s390_register_adapter_interrupt);
EXPORT_SYMBOL (s390_unregister_adapter_interrupt);
EXPORT_SYMBOL (s390_set_private_data);
EXPORT_SYMBOL (s390_get_private_data);
EXPORT_SYMBOL (s390_trigger_resense);

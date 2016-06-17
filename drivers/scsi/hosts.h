/*
 *  hosts.h Copyright (C) 1992 Drew Eckhardt
 *          Copyright (C) 1993, 1994, 1995, 1998, 1999 Eric Youngdale
 *
 *  mid to low-level SCSI driver interface header
 *      Initial versions: Drew Eckhardt
 *      Subsequent revisions: Eric Youngdale
 *
 *  <drew@colorado.edu>
 *
 *	 Modified by Eric Youngdale eric@andante.org to
 *	 add scatter-gather, multiple outstanding request, and other
 *	 enhancements.
 *
 *  Further modified by Eric Youngdale to support multiple host adapters
 *  of the same type.
 *
 *  Jiffies wrap fixes (host->resetting), 3 Dec 1998 Andrea Arcangeli
 */

#ifndef _HOSTS_H
#define _HOSTS_H

/*
    $Header: /vger/u4/cvs/linux/drivers/scsi/hosts.h,v 1.6 1997/01/19 23:07:13 davem Exp $
*/

#include <linux/config.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>

/* It is senseless to set SG_ALL any higher than this - the performance
 *  does not get any better, and it wastes memory
 */
#define SG_NONE 0
#define SG_ALL 0xff

#define DISABLE_CLUSTERING 0
#define ENABLE_CLUSTERING 1

/* The various choices mean:
 * NONE: Self evident.	Host adapter is not capable of scatter-gather.
 * ALL:	 Means that the host adapter module can do scatter-gather,
 *	 and that there is no limit to the size of the table to which
 *	 we scatter/gather data.
 * Anything else:  Indicates the maximum number of chains that can be
 *	 used in one scatter-gather request.
 */

/*
 * The Scsi_Host_Template type has all that is needed to interface with a SCSI
 * host in a device independent matter.	 There is one entry for each different
 * type of host adapter that is supported on the system.
 */

typedef struct scsi_disk Disk;

typedef struct	SHT
{

    /* Used with loadable modules so we can construct a linked list. */
    struct SHT * next;

    /* Used with loadable modules so that we know when it is safe to unload */
    struct module * module;

    /* The pointer to the /proc/scsi directory entry */
    struct proc_dir_entry *proc_dir;

    /* proc-fs info function.
     * Can be used to export driver statistics and other infos to the world
     * outside the kernel ie. userspace and it also provides an interface
     * to feed the driver with information. Check eata_dma_proc.c for reference
     */
    int (*proc_info)(char *, char **, off_t, int, int, int);

    /*
     * The name pointer is a pointer to the name of the SCSI
     * device detected.
     */
    const char *name;

    /*
     * The detect function shall return non zero on detection,
     * indicating the number of host adapters of this particular
     * type were found.	 It should also
     * initialize all data necessary for this particular
     * SCSI driver.  It is passed the host number, so this host
     * knows where the first entry is in the scsi_hosts[] array.
     *
     * Note that the detect routine MUST not call any of the mid level
     * functions to queue commands because things are not guaranteed
     * to be set up yet.  The detect routine can send commands to
     * the host adapter as long as the program control will not be
     * passed to scsi.c in the processing of the command.  Note
     * especially that scsi_malloc/scsi_free must not be called.
     */
    int (* detect)(struct SHT *);

    int (*revoke)(Scsi_Device *);

    /* Used with loadable modules to unload the host structures.  Note:
     * there is a default action built into the modules code which may
     * be sufficient for most host adapters.  Thus you may not have to supply
     * this at all.
     */
    int (*release)(struct Scsi_Host *);

    /*
     * The info function will return whatever useful
     * information the developer sees fit.  If not provided, then
     * the name field will be used instead.
     */
    const char *(* info)(struct Scsi_Host *);

    /*
     * ioctl interface
     */
    int (*ioctl)(Scsi_Device *dev, int cmd, void *arg);

    /*
     * The command function takes a target, a command (this is a SCSI
     * command formatted as per the SCSI spec, nothing strange), a
     * data buffer pointer, and data buffer length pointer.  The return
     * is a status int, bit fielded as follows :
     * Byte What
     * 0    SCSI status code
     * 1    SCSI 1 byte message
     * 2    host error return.
     * 3    mid level error return
     */
    int (* command)(Scsi_Cmnd *);

    /*
     * The QueueCommand function works in a similar manner
     * to the command function.	 It takes an additional parameter,
     * void (* done)(int host, int code) which is passed the host
     * # and exit result when the command is complete.
     * Host number is the POSITION IN THE hosts array of THIS
     * host adapter.
     *
     * The done() function must only be called after QueueCommand() 
     * has returned.
     */
    int (* queuecommand)(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));

    /*
     * This is an error handling strategy routine.  You don't need to
     * define one of these if you don't want to - there is a default
     * routine that is present that should work in most cases.  For those
     * driver authors that have the inclination and ability to write their
     * own strategy routine, this is where it is specified.  Note - the
     * strategy routine is *ALWAYS* run in the context of the kernel eh
     * thread.  Thus you are guaranteed to *NOT* be in an interrupt handler
     * when you execute this, and you are also guaranteed to *NOT* have any
     * other commands being queued while you are in the strategy routine.
     * When you return from this function, operations return to normal.
     *
     * See scsi_error.c scsi_unjam_host for additional comments about what
     * this function should and should not be attempting to do.
     */
     int (*eh_strategy_handler)(struct Scsi_Host *);
     int (*eh_abort_handler)(Scsi_Cmnd *);
     int (*eh_device_reset_handler)(Scsi_Cmnd *);
     int (*eh_bus_reset_handler)(Scsi_Cmnd *);
     int (*eh_host_reset_handler)(Scsi_Cmnd *);

    /*
     * Since the mid level driver handles time outs, etc, we want to
     * be able to abort the current command.  Abort returns 0 if the
     * abortion was successful.	 The field SCpnt->abort reason
     * can be filled in with the appropriate reason why we wanted
     * the abort in the first place, and this will be used
     * in the mid-level code instead of the host_byte().
     * If non-zero, the code passed to it
     * will be used as the return code, otherwise
     * DID_ABORT  should be returned.
     *
     * Note that the scsi driver should "clean up" after itself,
     * resetting the bus, etc.	if necessary.
     *
     * NOTE - this interface is depreciated, and will go away.  Use
     * the eh_ routines instead.
     */
    int (* abort)(Scsi_Cmnd *);

    /*
     * The reset function will reset the SCSI bus.  Any executing
     * commands should fail with a DID_RESET in the host byte.
     * The Scsi_Cmnd  is passed so that the reset routine can figure
     * out which host adapter should be reset, and also which command
     * within the command block was responsible for the reset in
     * the first place.	 Some hosts do not implement a reset function,
     * and these hosts must call scsi_request_sense(SCpnt) to keep
     * the command alive.
     *
     * NOTE - this interface is depreciated, and will go away.  Use
     * the eh_ routines instead.
     */
    int (* reset)(Scsi_Cmnd *, unsigned int);

    /*
     * This function is used to select synchronous communications,
     * which will result in a higher data throughput.  Not implemented
     * yet.
     */
    int (* slave_attach)(int, int);

    /*
     * This function determines the bios parameters for a given
     * harddisk.  These tend to be numbers that are made up by
     * the host adapter.  Parameters:
     * size, device number, list (heads, sectors, cylinders)
     */
    int (* bios_param)(Disk *, kdev_t, int []);


    /*
     * Used to set the queue depth for a specific device.
     */
    void (*select_queue_depths)(struct Scsi_Host *, Scsi_Device *);

    /*
     * This determines if we will use a non-interrupt driven
     * or an interrupt driven scheme,  It is set to the maximum number
     * of simultaneous commands a given host adapter will accept.
     */
    int can_queue;

    /*
     * In many instances, especially where disconnect / reconnect are
     * supported, our host also has an ID on the SCSI bus.  If this is
     * the case, then it must be reserved.  Please set this_id to -1 if
     * your setup is in single initiator mode, and the host lacks an
     * ID.
     */
    int this_id;

    /*
     * This determines the degree to which the host adapter is capable
     * of scatter-gather.
     */
    short unsigned int sg_tablesize;

    /*
     * if the host adapter has limitations beside segment count
     */
    short unsigned int max_sectors;

    /*
     * True if this host adapter can make good use of linked commands.
     * This will allow more than one command to be queued to a given
     * unit on a given host.  Set this to the maximum number of command
     * blocks to be provided for each device.  Set this to 1 for one
     * command block per lun, 2 for two, etc.  Do not set this to 0.
     * You should make sure that the host adapter will do the right thing
     * before you try setting this above 1.
     */
    short cmd_per_lun;

    /*
     * present contains counter indicating how many boards of this
     * type were found when we did the scan.
     */
    unsigned char present;

    /*
     * true if this host adapter uses unchecked DMA onto an ISA bus.
     */
    unsigned unchecked_isa_dma:1;

    /*
     * true if this host adapter can make good use of clustering.
     * I originally thought that if the tablesize was large that it
     * was a waste of CPU cycles to prepare a cluster list, but
     * it works out that the Buslogic is faster if you use a smaller
     * number of segments (i.e. use clustering).  I guess it is
     * inefficient.
     */
    unsigned use_clustering:1;

    /*
     * True if this driver uses the new error handling code.  This flag is
     * really only temporary until all of the other drivers get converted
     * to use the new error handling code.
     */
    unsigned use_new_eh_code:1;

    /*
     * True for emulated SCSI host adapters (e.g. ATAPI)
     */
    unsigned emulated:1;

    /*
     * True for drivers that can do I/O from highmem
     */
    unsigned highmem_io:1;

    /*
     * Name of proc directory
     */
    char *proc_name;

} Scsi_Host_Template;

/*
 * The scsi_hosts array is the array containing the data for all
 * possible <supported> scsi hosts.   This is similar to the
 * Scsi_Host_Template, except that we have one entry for each
 * actual physical host adapter on the system, stored as a linked
 * list.  Note that if there are 2 aha1542 boards, then there will
 * be two Scsi_Host entries, but only 1 Scsi_Host_Template entry.
 */

struct Scsi_Host
{
/* private: */
    /*
     * This information is private to the scsi mid-layer.  Wrapping it in a
     * struct private is a way of marking it in a sort of C++ type of way.
     */
    struct Scsi_Host      * next;
    Scsi_Device           * host_queue;


    struct task_struct    * ehandler;  /* Error recovery thread. */
    struct semaphore      * eh_wait;   /* The error recovery thread waits on
                                          this. */
    struct semaphore      * eh_notify; /* wait for eh to begin */
    struct semaphore      * eh_action; /* Wait for specific actions on the
                                          host. */
    unsigned int            eh_active:1; /* Indicates the eh thread is awake and active if
                                          this is true. */
    wait_queue_head_t       host_wait;
    Scsi_Host_Template    * hostt;
    atomic_t                host_active; /* commands checked out */
    volatile unsigned short host_busy;   /* commands actually active on low-level */
    volatile unsigned short host_failed; /* commands that failed. */
    
/* public: */
    unsigned short extra_bytes;
    unsigned short host_no;  /* Used for IOCTL_GET_IDLUN, /proc/scsi et al. */
    int resetting; /* if set, it means that last_reset is a valid value */
    unsigned long last_reset;


    /*
     *	These three parameters can be used to allow for wide scsi,
     *	and for host adapters that support multiple busses
     *	The first two should be set to 1 more than the actual max id
     *	or lun (i.e. 8 for normal systems).
     */
    unsigned int max_id;
    unsigned int max_lun;
    unsigned int max_channel;

    /* These parameters should be set by the detect routine */
    unsigned long base;
    unsigned long io_port;
    unsigned char n_io_port;
    unsigned char dma_channel;
    unsigned int  irq;

    /*
     * This is a unique identifier that must be assigned so that we
     * have some way of identifying each detected host adapter properly
     * and uniquely.  For hosts that do not support more than one card
     * in the system at one time, this does not need to be set.  It is
     * initialized to 0 in scsi_register.
     */
    unsigned int unique_id;

    /*
     * The rest can be copied from the template, or specifically
     * initialized, as required.
     */

    /*
     * The maximum length of SCSI commands that this host can accept.
     * Probably 12 for most host adapters, but could be 16 for others.
     * For drivers that don't set this field, a value of 12 is
     * assumed.  I am leaving this as a number rather than a bit
     * because you never know what subsequent SCSI standards might do
     * (i.e. could there be a 20 byte or a 24-byte command a few years
     * down the road?).  
     */
    unsigned char max_cmd_len;

    int this_id;
    int can_queue;
    short cmd_per_lun;
    short unsigned int sg_tablesize;
    short unsigned int max_sectors;

    unsigned in_recovery:1;
    unsigned unchecked_isa_dma:1;
    unsigned use_clustering:1;
    unsigned highmem_io:1;

    /*
     * True if this host was loaded as a loadable module
     */
    unsigned loaded_as_module:1;

    /*
     * Host has rejected a command because it was busy.
     */
    unsigned host_blocked:1;

    /*
     * Host has requested that no further requests come through for the
     * time being.
     */
    unsigned host_self_blocked:1;
    
    /*
     * Host uses correct SCSI ordering not PC ordering. The bit is
     * set for the minority of drivers whose authors actually read the spec ;)
     */
    unsigned reverse_ordering:1;

    /*
     * Indicates that one or more devices on this host were starved, and
     * when the device becomes less busy that we need to feed them.
     */
    unsigned some_device_starved:1;
   
    void (*select_queue_depths)(struct Scsi_Host *, Scsi_Device *);

    /*
     * For SCSI hosts which are PCI devices, set pci_dev so that
     * we can do BIOS EDD 3.0 mappings
     */
    struct pci_dev *pci_dev;

    /*
     * We should ensure that this is aligned, both for better performance
     * and also because some compilers (m68k) don't automatically force
     * alignment to a long boundary.
     */
    unsigned long hostdata[0]  /* Used for storage of host specific stuff */
        __attribute__ ((aligned (sizeof(unsigned long))));
};

/*
 * These two functions are used to allocate and free a pseudo device
 * which will connect to the host adapter itself rather than any
 * physical device.  You must deallocate when you are done with the
 * thing.  This physical pseudo-device isn't real and won't be available
 * from any high-level drivers.
 */
extern void scsi_free_host_dev(Scsi_Device * SDpnt);
extern Scsi_Device * scsi_get_host_dev(struct Scsi_Host * SHpnt);

extern void scsi_unblock_requests(struct Scsi_Host * SHpnt);
extern void scsi_block_requests(struct Scsi_Host * SHpnt);
extern void scsi_report_bus_reset(struct Scsi_Host * SHpnt, int channel);

typedef struct SHN
    {
    struct SHN * next;
    char * name;
    unsigned short host_no;
    unsigned short host_registered;
    unsigned loaded_as_module;
    } Scsi_Host_Name;
	
extern Scsi_Host_Name * scsi_host_no_list;
extern struct Scsi_Host * scsi_hostlist;
extern struct Scsi_Device_Template * scsi_devicelist;

extern Scsi_Host_Template * scsi_hosts;

extern void build_proc_dir_entries(Scsi_Host_Template  *);

/*
 *  scsi_init initializes the scsi hosts.
 */

extern int next_scsi_host;

unsigned int scsi_init(void);
extern struct Scsi_Host * scsi_register(Scsi_Host_Template *, int j);
extern void scsi_unregister(struct Scsi_Host * i);

extern void scsi_register_blocked_host(struct Scsi_Host * SHpnt);
extern void scsi_deregister_blocked_host(struct Scsi_Host * SHpnt);

static inline void scsi_set_pci_device(struct Scsi_Host *SHpnt,
                                       struct pci_dev *pdev)
{
	SHpnt->pci_dev = pdev;
}


/*
 * Prototypes for functions/data in scsi_scan.c
 */
extern void scan_scsis(struct Scsi_Host *shpnt,
		       uint hardcoded,
		       uint hchannel,
		       uint hid,
                       uint hlun);

extern void scsi_mark_host_reset(struct Scsi_Host *Host);

#define BLANK_HOST {"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

struct Scsi_Device_Template
{
    struct Scsi_Device_Template * next;
    const char * name;
    const char * tag;
    struct module * module;	  /* Used for loadable modules */
    unsigned char scsi_type;
    unsigned int major;
    unsigned int min_major;      /* Minimum major in range. */ 
    unsigned int max_major;      /* Maximum major in range. */
    unsigned int nr_dev;	  /* Number currently attached */
    unsigned int dev_noticed;	  /* Number of devices detected. */
    unsigned int dev_max;	  /* Current size of arrays */
    unsigned blk:1;		  /* 0 if character device */
    int (*detect)(Scsi_Device *); /* Returns 1 if we can attach this device */
    int (*init)(void);		  /* Sizes arrays based upon number of devices
		   *  detected */
    void (*finish)(void);	  /* Perform initialization after attachment */
    int (*attach)(Scsi_Device *); /* Attach devices to arrays */
    void (*detach)(Scsi_Device *);
    int (*init_command)(Scsi_Cmnd *);     /* Used by new queueing code. 
                                           Selects command for blkdevs */
};

void  scsi_initialize_queue(Scsi_Device * SDpnt, struct Scsi_Host * SHpnt);

int scsi_register_device(struct Scsi_Device_Template * sdpnt);
void scsi_deregister_device(struct Scsi_Device_Template * tpnt);

/* These are used by loadable modules */
extern int scsi_register_module(int, void *);
extern int scsi_unregister_module(int, void *);

/* The different types of modules that we can load and unload */
#define MODULE_SCSI_HA 1
#define MODULE_SCSI_CONST 2
#define MODULE_SCSI_IOCTL 3
#define MODULE_SCSI_DEV 4


/*
 * This is an ugly hack.  If we expect to be able to load devices at run time,
 * we need to leave extra room in some of the data structures.	Doing a
 * realloc to enlarge the structures would be riddled with race conditions,
 * so until a better solution is discovered, we use this crude approach
 *
 * Even bigger hack for SparcSTORAGE arrays. Those are at least 6 disks, but
 * usually up to 30 disks, so everyone would need to change this. -jj
 *
 * Note: These things are all evil and all need to go away.  My plan is to
 * tackle the character devices first, as there aren't any locking implications
 * in the block device layer.   The block devices will require more work.
 *
 * The generics driver has been updated to resize as required.  So as the tape
 * driver. Two down, two more to go.
 */
#ifndef CONFIG_SD_EXTRA_DEVS
#define CONFIG_SD_EXTRA_DEVS 2
#endif
#ifndef CONFIG_SR_EXTRA_DEVS
#define CONFIG_SR_EXTRA_DEVS 2
#endif
#define SD_EXTRA_DEVS CONFIG_SD_EXTRA_DEVS
#define SR_EXTRA_DEVS CONFIG_SR_EXTRA_DEVS

#endif
/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

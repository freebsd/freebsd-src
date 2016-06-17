/* 
 * File...........: linux/drivers/s390/block/dasd_int.h
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *                  Horst Hummel <Horst.Hummel@de.ibm.com> 
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 1999,2000
 *
 * $Revision: 1.36 $
 *
 * History of changes (starts July 2000)
 * 02/01/01 added dynamic registration of ioctls
 */

#ifndef DASD_INT_H
#define DASD_INT_H

#include <asm/dasd.h>

#define CONFIG_DASD_DYNAMIC

typedef int(*dasd_ioctl_fn_t) (void *inp, int no, long args);
int dasd_ioctl_no_register(struct module *, int no, dasd_ioctl_fn_t handler);
int dasd_ioctl_no_unregister(struct module *, int no, dasd_ioctl_fn_t handler);

#define DASD_NAME "dasd"
#define DASD_PER_MAJOR ( 1U<<(MINORBITS-DASD_PARTN_BITS))


#define DASD_FORMAT_INTENS_WRITE_RECZERO 0x01
#define DASD_FORMAT_INTENS_WRITE_HOMEADR 0x02

#define DASD_STATE_DEL   -1     /* "unknown" */
#define DASD_STATE_NEW    0     /* memory for dasd_device_t and lowmem ccw/idals allocated */  
#define DASD_STATE_BOXED  1     /* boxed dasd could not be analysed "plugged" */
#define DASD_STATE_KNOWN  2     /* major_info/devinfo/discipline/devfs-'device'/gendisk - "detected" */
#define DASD_STATE_ACCEPT 3     /* irq requested - "accepted" */
#define DASD_STATE_INIT   4     /* init_cqr started - "busy" */
#define DASD_STATE_READY  5     /* init finished  - "fenced(plugged)" */
#define DASD_STATE_ONLINE 6     /* unplugged "active" */

#define DASD_HOTPLUG_EVENT_ADD        0
#define DASD_HOTPLUG_EVENT_REMOVE     1
#define DASD_HOTPLUG_EVENT_PARTCHK    2
#define DASD_HOTPLUG_EVENT_PARTREMOVE 3

#define DASD_FORMAT_INTENS_WRITE_RECZERO 0x01
#define DASD_FORMAT_INTENS_WRITE_HOMEADR 0x02
#define DASD_FORMAT_INTENS_INVALIDATE    0x04
#define DASD_FORMAT_INTENS_CDL 0x08
#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/version.h>
#include <linux/major.h>
#include <linux/wait.h>
#include <linux/blk.h> 
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
#include <linux/blkdev.h> 
#include <linux/devfs_fs_kernel.h>
#endif
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/compatmac.h>

#include <asm/ccwcache.h>
#include <asm/irq.h>
#include <asm/s390dyn.h>
#include <asm/todclk.h>
#include <asm/debug.h>

/********************************************************************************
 * SECTION: Kernel Version Compatibility section 
 ********************************************************************************/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,98))
typedef struct request *request_queue_t;
#define block_device_operations file_operations
#define __setup(x,y) struct dasd_device_t
#define devfs_register_blkdev(major,name,ops) register_blkdev(major,name,ops)
#define register_disk(dd,dev,partn,ops,size) \
do { \
	dd->sizes[MINOR(dev)] = size >> 1; \
	resetup_one_dev(dd,MINOR(dev)>>DASD_PARTN_BITS); \
} while(0)
#define init_waitqueue_head(x) do { *x = NULL; } while(0)
#define blk_cleanup_queue(x) do {} while(0)
#define blk_init_queue(x...) do {} while(0)
#define blk_queue_headactive(x...) do {} while(0)
#define blk_queue_make_request(x) do {} while(0)
#define list_empty(x) (0)
#define INIT_BLK_DEV(d_major,d_request_fn,d_queue_fn,d_current) \
do { \
        blk_dev[d_major].request_fn = d_request_fn; \
        blk_dev[d_major].queue = d_queue_fn; \
        blk_dev[d_major].current_request = d_current; \
} while(0)
#define INIT_GENDISK(D_MAJOR,D_NAME,D_PARTN_BITS,D_PER_MAJOR) \
	major:D_MAJOR, \
	major_name:D_NAME, \
	minor_shift:D_PARTN_BITS, \
	max_p:1 << D_PARTN_BITS, \
	max_nr:D_PER_MAJOR, \
	nr_real:D_PER_MAJOR,
static inline struct request * 
dasd_next_request( request_queue_t *queue ) 
{
    return *queue;
}
static inline void 
dasd_dequeue_request( request_queue_t * q, struct request *req )
{
        *q = req->next;
        req->next = NULL;
}

#else
#define INIT_BLK_DEV(d_major,d_request_fn,d_queue_fn,d_current) \
do { \
        blk_dev[d_major].queue = d_queue_fn; \
} while(0)
#define INIT_GENDISK(D_MAJOR,D_NAME,D_PARTN_BITS,D_PER_MAJOR) \
	major:D_MAJOR, \
	major_name:D_NAME, \
	minor_shift:D_PARTN_BITS, \
	max_p:1 << D_PARTN_BITS, \
	nr_real:D_PER_MAJOR, \
        fops:&dasd_device_operations, 
static inline struct request * 
dasd_next_request( request_queue_t *queue ) 
{
        return blkdev_entry_next_request(&queue->queue_head);
}
static inline void 
dasd_dequeue_request( request_queue_t * q, struct request *req )
{
        blkdev_dequeue_request (req);
}
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,98)) */

/********************************************************************************
 * SECTION: Type definitions
 ********************************************************************************/

typedef struct dasd_devreg_t {
        devreg_t devreg; /* the devreg itself */
        /* build a linked list of devregs, needed for cleanup */
        struct list_head list;
} dasd_devreg_t;

typedef struct {
	struct list_head list;
	struct module *owner;
	int no;
	dasd_ioctl_fn_t handler;
} dasd_ioctl_list_t;

typedef enum {
	dasd_era_fatal = -1,	/* no chance to recover              */
	dasd_era_none = 0,	/* don't recover, everything alright */
	dasd_era_msg = 1,	/* don't recover, just report...     */
	dasd_era_recover = 2	/* recovery action recommended       */
} dasd_era_t;

/* BIT DEFINITIONS FOR SENSE DATA */
#define DASD_SENSE_BIT_0 0x80
#define DASD_SENSE_BIT_1 0x40
#define DASD_SENSE_BIT_2 0x20
#define DASD_SENSE_BIT_3 0x10

/* 
 * struct dasd_sizes_t
 * represents all data needed to access dasd with properly set up sectors
 */
typedef
struct dasd_sizes_t {
	unsigned long blocks; /* size of volume in blocks */
	unsigned int bp_block; /* bytes per block */
	unsigned int s2b_shift; /* log2 (bp_block/512) */
        unsigned int pt_block; /* from which block to read the partn table */
} dasd_sizes_t;

/* 
 * struct dasd_chanq_t 
 * represents a queue of channel programs related to a single device
 */
typedef
struct dasd_chanq_t {
	ccw_req_t *head;
	ccw_req_t *tail;
} dasd_chanq_t;

/* 
 * struct dasd_lowmem_t 
 * represents a queue of pages for lowmem request
 */
typedef struct {
        struct list_head list;
} dasd_lowmem_t;

#define DASD_LOWMEM_PAGES 2     /* # of lowmem pages per device (min 2) */

/********************************************************************************
 * SECTION: MACROS
 ********************************************************************************/

/*
 * CHECK_THEN_SET
 *
 * Change 'where' value from 'from' to 'to'.
 ' BUG if the 'from' value doesn't match.
 */
#define check_then_set(where,from,to) \
do { \
        if ((*(where)) != (from) ) { \
                printk (KERN_ERR PRINTK_HEADER "was %d\n", *(where)); \
                BUG(); \
        } \
        (*(where)) = (to); \
} while(0)


/********************************************************************************
 * SECION: MACROs for klogd and s390 debug feature (dbf)
 ********************************************************************************/

#define DBF_DEV_EVENT(d_level, d_device, d_str, d_data...) \
do { \
        if (d_device->debug_area != NULL) \
                debug_sprintf_event(d_device->debug_area, \
                                    d_level, \
                                    d_str "\n", \
                                    d_data); \
} while(0)

#define DBF_DEV_EXC(d_level, d_device, d_str, d_data...) \
do { \
        if (d_device->debug_area != NULL) \
                debug_sprintf_exception(d_device->debug_area, \
                                        d_level, \
                                        d_str "\n", \
                                        d_data); \
} while(0)

#define DBF_EVENT(d_level, d_str, d_data...)\
do { \
        if (dasd_debug_area != NULL) \
                debug_sprintf_event(dasd_debug_area, \
                                    d_level,\
                                    d_str "\n", \
                                    d_data); \
} while(0)

#define DBF_EXC(d_level, d_str, d_data...)\
do { \
        if (dasd_debug_area != NULL) \
                debug_sprintf_exception(dasd_debug_area, \
                                        d_level,\
                                        d_str "\n", \
                                        d_data); \
} while(0)

/* definition of dbf debug levels */
#define	DBF_EMERG	0	/* system is unusable		*/
#define	DBF_ALERT	1	/* action must be taken immediately	*/
#define	DBF_CRIT	2	/* critical conditions		*/
#define	DBF_ERR  	3	/* error conditions			*/
#define	DBF_WARNING	4	/* warning conditions		*/
#define	DBF_NOTICE	5	/* normal but significant condition	*/
#define	DBF_INFO	6	/* informational			*/
#define	DBF_DEBUG	6	/* debug-level messages		*/

/* messages to be written via klogd and dbf */
#define DEV_MESSAGE(d_loglevel,d_device,d_string,d_args...)\
do { \
        int d_devno = d_device->devinfo.devno; \
        int d_irq = d_device->devinfo.irq; \
        char *d_name = d_device->name; \
        int d_major = MAJOR(d_device->kdev); \
        int d_minor = MINOR(d_device->kdev); \
\
        printk(d_loglevel PRINTK_HEADER \
               " /dev/%-7s(%3d:%3d),%04x@%02x: " \
               d_string "\n", \
               d_name, \
               d_major, \
               d_minor, \
               d_devno, \
               d_irq, \
               d_args); \
\
        DBF_DEV_EVENT(DBF_ALERT, \
                      d_device, \
                      d_string, \
                      d_args); \
} while(0)

/* general messages to be written via klogd and dbf */
#define MESSAGE(d_loglevel,d_string,d_args...)\
do { \
        printk(d_loglevel PRINTK_HEADER \
               " " d_string "\n", \
               d_args); \
\
        DBF_EVENT(DBF_ALERT, \
                  d_string, \
                  d_args); \
} while(0)

/* general messages to be written via klogd only */
#define MESSAGE_LOG(d_loglevel,d_string,d_args...)\
do { \
        printk(d_loglevel PRINTK_HEADER \
               " " d_string "\n", \
               d_args); \
} while(0)

struct dasd_device_t;
struct request;

/********************************************************************************
 * SECTION: signatures for the functions of dasd_discipline_t 
 * make typecasts much easier
 ********************************************************************************/

typedef int    (*dasd_ck_id_fn_t)              (s390_dev_info_t *);
typedef int    (*dasd_ck_characteristics_fn_t) (struct dasd_device_t *);
typedef int    (*dasd_fill_geometry_fn_t)      (struct dasd_device_t *, 
                                                struct hd_geometry *);
typedef int    (*dasd_do_analysis_fn_t)        (struct dasd_device_t *);
typedef int    (*dasd_io_starter_fn_t)         (ccw_req_t *);
typedef int    (*dasd_io_stopper_fn_t)         (ccw_req_t *);
typedef int    (*dasd_info_fn_t)               (struct dasd_device_t *, 
                                                dasd_information2_t *);
typedef int    (*dasd_use_count_fn_t)          (int);
typedef int    (*dasd_get_attrib_fn_t)         (struct dasd_device_t *, 
                                                struct attrib_data_t *);
typedef int    (*dasd_set_attrib_fn_t)         (struct dasd_device_t *, 
                                                struct attrib_data_t *);
typedef void   (*dasd_int_handler_fn_t)        (int irq, void *, 
                                                struct pt_regs *);
typedef void   (*dasd_dump_sense_fn_t)         (struct dasd_device_t *,
                                                ccw_req_t *);
typedef ccw_req_t *(*dasd_format_fn_t)         (struct dasd_device_t *, 
                                                struct format_data_t *);
typedef ccw_req_t *(*dasd_init_analysis_fn_t ) (struct dasd_device_t *);
typedef ccw_req_t *(*dasd_cp_builder_fn_t)     (struct dasd_device_t *,
                                                struct request *);
typedef ccw_req_t *(*dasd_reserve_fn_t)        (struct dasd_device_t *);
typedef ccw_req_t *(*dasd_release_fn_t)        (struct dasd_device_t *);
typedef ccw_req_t *(*dasd_steal_lock_fn_t)     (struct dasd_device_t *);
typedef ccw_req_t *(*dasd_merge_cp_fn_t)       (struct dasd_device_t *);
typedef ccw_req_t *(*dasd_erp_action_fn_t)     (ccw_req_t * cqr);
typedef ccw_req_t *(*dasd_erp_postaction_fn_t) (ccw_req_t * cqr);
typedef ccw_req_t *(*dasd_read_stats_fn_t)     (struct dasd_device_t *);

typedef dasd_rssd_perf_stats_t * (*dasd_ret_stats_fn_t)     (ccw_req_t *);
typedef dasd_era_t               (*dasd_error_examine_fn_t) (ccw_req_t *, 
                                                             devstat_t * stat);
typedef dasd_erp_action_fn_t     (*dasd_error_analyse_fn_t) (ccw_req_t *);
typedef dasd_erp_postaction_fn_t (*dasd_erp_analyse_fn_t)   (ccw_req_t *);

/*
 * the dasd_discipline_t is
 * sth like a table of virtual functions, if you think of dasd_eckd
 * inheriting dasd...
 * no, currently we are not planning to reimplement the driver in C++
 */
typedef struct dasd_discipline_t {
        struct module *owner;
	char ebcname[8]; /* a name used for tagging and printks */
        char name[8];		/* a name used for tagging and printks */
	int max_blocks;	/* maximum number of blocks to be chained */
	dasd_ck_id_fn_t              id_check;	            /* check sense data */
	dasd_ck_characteristics_fn_t check_characteristics; /* check the characteristics */
	dasd_init_analysis_fn_t      init_analysis;	    /* start the analysis of the volume */
	dasd_do_analysis_fn_t        do_analysis;	    /* complete the analysis of the volume */
	dasd_fill_geometry_fn_t      fill_geometry;	    /* set up hd_geometry */
	dasd_io_starter_fn_t         start_IO;                 
	dasd_io_stopper_fn_t         term_IO;                  
        dasd_format_fn_t             format_device;	    /* format the device */
	dasd_error_examine_fn_t      examine_error;
	dasd_error_analyse_fn_t      erp_action;
	dasd_erp_analyse_fn_t        erp_postaction;
        dasd_cp_builder_fn_t         build_cp_from_req;
        dasd_dump_sense_fn_t         dump_sense;
        dasd_int_handler_fn_t        int_handler;
        dasd_reserve_fn_t            reserve;
        dasd_release_fn_t            release;
        dasd_steal_lock_fn_t         steal_lock;
        dasd_merge_cp_fn_t           merge_cp;
        dasd_info_fn_t               fill_info;
        dasd_read_stats_fn_t         read_stats;
        dasd_ret_stats_fn_t          ret_stats;             /* return performance statistics */
        dasd_get_attrib_fn_t         get_attrib;            /* get attributes (cache operations */ 
        dasd_set_attrib_fn_t         set_attrib;            /* set attributes (cache operations */ 
	struct list_head list;	/* used for list of disciplines */
} dasd_discipline_t;

/* dasd_range_t are used for ordering the DASD devices */
typedef struct dasd_range_t {
	unsigned int from;	/* first DASD in range */
	unsigned int to;	/* last DASD in range */
	char discipline[4];	/* placeholder to force discipline */
        int features;
	struct list_head list;	/* next one in linked list */
} dasd_range_t;



#define DASD_MAJOR_INFO_REGISTERED 1
#define DASD_MAJOR_INFO_IS_STATIC 2

typedef struct major_info_t {
	struct list_head list;
	struct dasd_device_t **dasd_device;
	int flags;
	struct gendisk gendisk; /* actually contains the major number */
} __attribute__ ((packed)) major_info_t;

typedef struct dasd_device_t {
	s390_dev_info_t devinfo;
	dasd_discipline_t *discipline;
	int level;
        atomic_t open_count;
        kdev_t kdev;
        major_info_t *major_info;
	struct dasd_chanq_t queue;
        wait_queue_head_t wait_q;
        request_queue_t *request_queue;
        struct timer_list timer;             /* used for start_IO */
        struct timer_list late_timer;        /* to get late devices online */  
        struct timer_list blocking_timer;    /* used for ERP */
	devstat_t dev_status; /* needed ONLY!! for request_irq */
        dasd_sizes_t sizes;
        char name[16]; /* The name of the device in /dev */
	char *private;	/* to be used by the discipline internally */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
        devfs_handle_t devfs_entry;
#endif /* LINUX_IS_24 */
	struct tq_struct bh_tq;
        atomic_t bh_scheduled;
        debug_info_t *debug_area;
        dasd_profile_info_t profile;
        ccw_req_t *init_cqr;
        atomic_t plugged;
        int stopped;                         /* device (do_IO) was stopped */
        struct list_head lowmem_pool;
}  dasd_device_t;

/* reasons why device (do_IO) was stopped */
#define DASD_STOPPED_NOT_ACC 1         /* not accessible */
#define DASD_STOPPED_PENDING 2         /* long busy */


int  dasd_init               (void);
void dasd_discipline_add     (dasd_discipline_t *);
void dasd_discipline_del     (dasd_discipline_t *);
int  dasd_start_IO           (ccw_req_t *);
int  dasd_term_IO            (ccw_req_t *);
void dasd_int_handler        (int , void *, struct pt_regs *);
void dasd_free_request       (ccw_req_t *, dasd_device_t *);
int  dasd_oper_handler       (int irq, devreg_t * devreg);
void dasd_schedule_bh        (dasd_device_t *);
void dasd_schedule_bh_timed  (unsigned long);
int  dasd_sleep_on_req       (ccw_req_t*);
int  dasd_set_normalized_cda (ccw1_t * cp, unsigned long address, 
                              ccw_req_t* request, 
                              dasd_device_t* device );
ccw_req_t *     dasd_default_erp_action     (ccw_req_t *);
ccw_req_t *     dasd_default_erp_postaction (ccw_req_t *);
inline void     dasd_chanq_deq              (dasd_chanq_t *, ccw_req_t *);
inline void     dasd_chanq_enq              (dasd_chanq_t *, ccw_req_t *);
inline void     dasd_chanq_enq_head         (dasd_chanq_t *, ccw_req_t *);
ccw_req_t *     dasd_alloc_request          (char *, int, int, dasd_device_t *);
dasd_device_t * dasd_device_from_kdev       (kdev_t kdev);

extern debug_info_t *dasd_debug_area;
extern int (*genhd_dasd_name) (char *, int, int, struct gendisk *);
extern int (*genhd_dasd_ioctl) (struct inode *inp, struct file *filp,
                                unsigned int no, unsigned long data);

#endif /* __KERNEL__ */

#endif				/* DASD_H */

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

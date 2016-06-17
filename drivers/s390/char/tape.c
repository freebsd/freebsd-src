
/***********************************************************************
 *  drivers/s390/char/tape.c
 *    tape device driver for S/390 and zSeries tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 ***********************************************************************
 */

#include "tapedefs.h"

#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <asm/types.h>
#include <asm/ccwcache.h>
#include <asm/idals.h>
#include <asm/ebcdic.h>
#include <linux/compatmac.h>
#ifdef MODULE
#include <linux/module.h>
#endif   
#include <asm/debug.h>
#ifdef CONFIG_S390_TAPE_DYNAMIC
#include <asm/s390dyn.h>
#endif
#include "tape.h"
#ifdef CONFIG_S390_TAPE_3490
#include "tape3490.h"
#endif
#ifdef CONFIG_S390_TAPE_3480
#include "tape3480.h"
#endif
#ifdef CONFIG_S390_TAPE_BLOCK
#include "tapeblock.h"
#endif
#ifdef CONFIG_S390_TAPE_CHAR
#include "tapechar.h"
#endif
#ifdef CONFIG_PROC_FS
#include <linux/vmalloc.h>
#endif
#define PRINTK_HEADER "T390:"


/* state handling routines */
inline void tapestate_set (tape_info_t * ti, int newstate);
inline int tapestate_get (tape_info_t * ti);
void tapestate_event (tape_info_t * ti, int event);

/* our globals */
tape_info_t *first_tape_info = NULL;
tape_discipline_t *first_discipline = NULL;
tape_frontend_t *first_frontend = NULL;
devreg_t* tape_devreg[128];
int devregct=0;

#ifdef TAPE_DEBUG
debug_info_t *tape_debug_area = NULL;
#endif

char* state_verbose[TS_SIZE]={
    "TS_UNUSED",  "TS_IDLE", "TS_DONE", "TS_FAILED",
    "TS_BLOCK_INIT",
    "TS_BSB_INIT",
    "TS_BSF_INIT",
    "TS_DSE_INIT",
    "TS_EGA_INIT",
    "TS_FSB_INIT",
    "TS_FSF_INIT",
    "TS_LDI_INIT",
    "TS_LBL_INIT",
    "TS_MSE_INIT",
    "TS_NOP_INIT",
    "TS_RBA_INIT",
    "TS_RBI_INIT",
    "TS_RBU_INIT",
    "TS_RBL_INIT",
    "TS_RDC_INIT",
    "TS_RFO_INIT",
    "TS_RSD_INIT",
    "TS_REW_INIT",
    "TS_REW_RELEASE_INIT",
    "TS_RUN_INIT",
    "TS_SEN_INIT",
    "TS_SID_INIT",
    "TS_SNP_INIT",
    "TS_SPG_INIT",
    "TS_SWI_INIT",
    "TS_SMR_INIT",
    "TS_SYN_INIT",
    "TS_TIO_INIT",
    "TS_UNA_INIT",
    "TS_WRI_INIT",
    "TS_WTM_INIT",
    "TS_NOT_OPER"};

char* event_verbose[TE_SIZE]= {
    "TE_START", "TE_DONE", "TE_FAILED", "TE_ERROR", "TE_OTHER"};

/* our root devfs handle */
#ifdef CONFIG_DEVFS_FS
devfs_handle_t tape_devfs_root_entry;

inline void
tape_mkdevfsroots (tape_info_t* ti) 
{
    char devno [5];
    sprintf (devno,"%04x",ti->devinfo.devno);
    ti->devfs_dir=devfs_mk_dir (tape_devfs_root_entry, devno, ti);
}

inline void
tape_rmdevfsroots (tape_info_t* ti)
{
    devfs_unregister (ti->devfs_dir);
}
#endif

#ifdef CONFIG_PROC_FS
/* our proc tapedevices entry */
static struct proc_dir_entry *tape_devices_entry;

typedef struct {
	char *data;
	int len;
} tempinfo_t;


static int
tape_devices_open (struct inode *inode, struct file *file)
{
    int size=80;
    tape_info_t* ti;
    tempinfo_t* tempinfo;
    char* data;
    int pos=0;
    tempinfo = kmalloc (sizeof(tempinfo_t),GFP_KERNEL);
    if (!tempinfo)
        return -ENOMEM;
    for (ti=first_tape_info;ti!=NULL;ti=ti->next)
        size+=80; // FIXME: Guess better!
    data=vmalloc(size);
    if (!data) {
        kfree (tempinfo);
        return -ENOMEM;
    }
    pos+=sprintf(data+pos,"TapeNo\tDevNo\tCuType\tCuModel\tDevType\tDevModel\tState\n");
    for (ti=first_tape_info;ti!=NULL;ti=ti->next) {
        pos+=sprintf(data+pos,"%d\t%04X\t%04X\t%02X\t%04X\t%02X\t\t%s\n",ti->rew_minor/2,
                     ti->devinfo.devno,ti->devinfo.sid_data.cu_type,
                     ti->devinfo.sid_data.cu_model,ti->devinfo.sid_data.dev_type,
                     ti->devinfo.sid_data.dev_model,((tapestate_get(ti) >= 0) &&
                                                       (tapestate_get(ti) < TS_SIZE)) ?
                     state_verbose[tapestate_get (ti)] : "TS UNKNOWN");
    }
    tempinfo->len=pos;
    tempinfo->data=data;
    file->private_data= (void*) tempinfo;
#ifdef MODULE
    MOD_INC_USE_COUNT;
#endif
    return 0;
}

static ssize_t
tape_devices_read (struct file *file, char *user_buf, size_t user_len, loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;

	if (*offset >= p_info->len) {
		return 0;	/* EOF */
	} else {
		len =  user_len<(p_info->len - *offset)?user_len:(p_info->len - *offset);
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;	/* number of bytes "read" */
	}
}

static int
tape_devices_release (struct inode *inode, struct file *file)
{
	int rc = 0;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;
	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		kfree (p_info);
	}
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
	return rc;
}

static struct file_operations tape_devices_file_ops =
{
	read:tape_devices_read,	/* read */
	open:tape_devices_open,	/* open */
	release:tape_devices_release,	/* close */
};

static struct inode_operations tape_devices_inode_ops =
{
#if !(LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
	default_file_ops:&tape_devices_file_ops		/* file ops */
#endif				/* LINUX_IS_24 */
};
#endif /* CONFIG_PROC_FS */

/* SECTION: Parameters for tape */
char *tape[256] = { NULL, };

#ifndef MODULE
static char tape_parm_string[1024] __initdata = { 0, };
static void
tape_split_parm_string (char *str)
{
	char *tmp = str;
	int count = 0;
	while (tmp != NULL && *tmp != '\0') {
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
		tape[count] = kmalloc (len * sizeof (char), GFP_ATOMIC);
		if (tape[count] == NULL) {
			printk (KERN_WARNING PRINTK_HEADER
				"can't store tape= parameter no %d\n",
				count + 1);
			break;
		}
		memset (tape[count], 0, len * sizeof (char));
		memcpy (tape[count], tmp, len * sizeof (char));
		count++;
		tmp = end;
	};
}

void __init
tape_parm_setup (char *str, int *ints)
{
	int len = strlen (tape_parm_string);
	if (len != 0) {
		strcat (tape_parm_string, ",");
	}
	strcat (tape_parm_string, str);
}

int __init
tape_parm_call_setup (char *str)
{
	int dummy;
	tape_parm_setup (str, &dummy);
	return 1;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,2,16))
__setup("tape=", tape_parm_call_setup);
#endif   /* kernel <2.2.19 */
#endif   /* not defined MODULE */

static inline int
tape_parm_strtoul (char *str, char **stra)
{
	char *temp = str;
	int val;
	if (*temp == '0') {
		temp++;		/* strip leading zero */
		if (*temp == 'x')
			temp++;	/* strip leading x */
	}
	val = simple_strtoul (temp, &temp, 16);	/* interpret anything as hex */
	*stra = temp;
	return val;
}

static inline devreg_t *
tape_create_devreg (int devno)
{
	devreg_t *devreg = kmalloc (sizeof (devreg_t), GFP_KERNEL);
	if (devreg != NULL) {
		memset (devreg, 0, sizeof (devreg_t));
		devreg->ci.devno = devno;
		devreg->flag = DEVREG_TYPE_DEVNO;
		devreg->oper_func = tape_oper_handler;
	}
	return devreg;
}

static inline void
tape_parm_parse (char **str)
{
	char *temp;
	int from, to,i,irq=0,rc,retries=0,tape_num=0;
        s390_dev_info_t dinfo;
        tape_info_t* ti,*tempti;
        tape_discipline_t* disc;
        long lockflags;
	if (*str==NULL) {
            /* no params present -> leave */
            return;
	}
	while (*str) {
		temp = *str;
		from = 0;
		to = 0;

                /* turn off autodetect mode, if any range is present */
                from = tape_parm_strtoul (temp, &temp);
                to = from;
                if (*temp == '-') {
                    temp++;
                    to = tape_parm_strtoul (temp, &temp);
                }
                for (i=from;i<=to;i++) {
                    retries=0;
                    // register for attch/detach of a devno
                    tape_devreg[devregct]=tape_create_devreg(i);
                    if (tape_devreg[devregct]==NULL) {
                        PRINT_WARN ("Could not create devreg for devno %04x, dyn. attach for this devno deactivated.\n",i);
                    } else {
                        s390_device_register (tape_devreg[devregct++]);
                    }
                    // we are activating a device if it is present
                    for (irq = get_irq_first(); irq!=-ENODEV; irq=get_irq_next(irq)) {
                        rc = get_dev_info_by_irq (irq, &dinfo);
                     
                        disc = first_discipline;
                        while ((dinfo.devno == i) && (disc != NULL) && (disc->cu_type != dinfo.sid_data.cu_type))
                            disc = (tape_discipline_t *) (disc->next);
                        if ((disc == NULL) || (rc == -ENODEV) || (i!=dinfo.devno)) {
                            continue;
                        }
#ifdef TAPE_DEBUG
                        debug_text_event (tape_debug_area,3,"det irq:  ");
                        debug_int_event (tape_debug_area,3,irq);
                        debug_text_event (tape_debug_area,3,"cu:       ");
                        debug_int_event (tape_debug_area,3,disc->cu_type);
#endif /* TAPE_DEBUG */
                        PRINT_INFO ("using devno %04x with discipline %04x on irq %d as tape device %d\n",dinfo.devno,dinfo.sid_data.cu_type,irq,tape_num/2);
                        /* Allocate tape structure  */
                        ti = kmalloc (sizeof (tape_info_t), GFP_ATOMIC);
                        if (ti == NULL) {
#ifdef TAPE_DEBUG
                            debug_text_exception (tape_debug_area,3,"ti:no mem ");
#endif /* TAPE_DEBUG */
                            PRINT_INFO ("tape: can't allocate memory for "
                                        "tape info structure\n");
                            continue;
                        }
                        memset(ti,0,sizeof(tape_info_t));
                        ti->discipline = disc;
                        disc->tape = ti;
                        rc = tape_setup (ti, irq, tape_num);
                        if (rc) {
#ifdef TAPE_DEBUG
                            debug_text_event (tape_debug_area,3,"tsetup err");
                            debug_int_exception (tape_debug_area,3,rc);
#endif /* TAPE_DEBUG */
                            kfree (ti);
                        } else {
                            s390irq_spin_lock_irqsave (irq, lockflags);
                            if (first_tape_info == NULL) {
                                first_tape_info = ti;
                            } else {
                                tempti = first_tape_info;
                                while (tempti->next != NULL)
                                    tempti = tempti->next;
                                tempti->next = ti;
                            }
                            s390irq_spin_unlock_irqrestore (irq, lockflags);
                        }
                    }
                    tape_num+=2;
                }
                str++;
        }
}


/* SECTION: Managing wrappers for ccwcache */

#define TAPE_EMERGENCY_REQUESTS 16

static ccw_req_t *tape_emergency_req[TAPE_EMERGENCY_REQUESTS] =
{NULL,};
static spinlock_t tape_emergency_req_lock = SPIN_LOCK_UNLOCKED;

static void
tape_init_emergency_req (void)
{
	int i;
	for (i = 0; i < TAPE_EMERGENCY_REQUESTS; i++) {
		tape_emergency_req[i] = (ccw_req_t *) get_free_page (GFP_KERNEL);
	}
}

#ifdef MODULE // We only cleanup the emergency requests on module unload.
static void
tape_cleanup_emergency_req (void)
{
	int i;
	for (i = 0; i < TAPE_EMERGENCY_REQUESTS; i++) {
		if (tape_emergency_req[i])
			free_page ((long) (tape_emergency_req[i]));
		else
			printk (KERN_WARNING PRINTK_HEADER "losing one page for 'in-use' emergency request\n");
	}
}
#endif

ccw_req_t *
tape_alloc_request (char *magic, int cplength, int datasize)
{
	ccw_req_t *rv = NULL;
	int i;
	if ((rv = ccw_alloc_request (magic, cplength, datasize)) != NULL) {
		return rv;
	}
	if (cplength * sizeof (ccw1_t) + datasize + sizeof (ccw_req_t) > PAGE_SIZE) {
		return NULL;
	}
	spin_lock (&tape_emergency_req_lock);
	for (i = 0; i < TAPE_EMERGENCY_REQUESTS; i++) {
		if (tape_emergency_req[i] != NULL) {
			rv = tape_emergency_req[i];
			tape_emergency_req[i] = NULL;
		}
	}
	spin_unlock (&tape_emergency_req_lock);
	if (rv) {
		memset (rv, 0, PAGE_SIZE);
		rv->cache = (kmem_cache_t *) (tape_emergency_req + i);
		strncpy ((char *) (&rv->magic), magic, 4);
		ASCEBC ((char *) (&rv->magic), 4);
		rv->cplength = cplength;
		rv->datasize = datasize;
		rv->data = (void *) ((long) rv + PAGE_SIZE - datasize);
		rv->cpaddr = (ccw1_t *) ((long) rv + sizeof (ccw_req_t));
	}
	return rv;
}

void
tape_free_request (ccw_req_t * request)
{
	if (request->cache >= (kmem_cache_t *) tape_emergency_req &&
	    request->cache <= (kmem_cache_t *) (tape_emergency_req + TAPE_EMERGENCY_REQUESTS)) {
		*((ccw_req_t **) (request->cache)) = request;
	} else {
		clear_normalized_cda ((ccw1_t *) (request->cpaddr));	// avoid memory leak caused by modeset_byte
		ccw_free_request (request);
	}
}

/*
 * Allocate a ccw request and reserve it for tape driver
 */
inline
 ccw_req_t *
tape_alloc_ccw_req (tape_info_t * ti, int cplength, int datasize)
{
	char tape_magic_id[] = "tape";
	ccw_req_t *cqr = NULL;

	if (!ti)
		return NULL;
	cqr = tape_alloc_request (tape_magic_id, cplength, datasize);

	if (!cqr) {
#ifdef TAPE_DEBUG
		PRINT_WARN ("empty CQR generated\n");
#endif
	}
	cqr->magic = TAPE_MAGIC;	/* sets an identifier for tape driver   */
	cqr->device = ti;	/* save pointer to tape info    */
	return cqr;
}

/*
 * Find the tape_info_t structure associated with irq
 */
static inline tape_info_t *
tapedev_find_info (int irq)
{
	tape_info_t *ti;

	ti = first_tape_info;
	if (ti != NULL)
		do {
			if (ti->devinfo.irq == irq)
				break;
		} while ((ti = (tape_info_t *) ti->next) != NULL);
	return ti;
}

#define QUEUE_THRESHOLD 5

/*
 * Tape interrupt routine, called from Ingo's I/O layer
 */
void
tape_irq (int irq, void *int_parm, struct pt_regs *regs)
{
	tape_info_t *ti = tapedev_find_info (irq);

	/* analyse devstat and fire event */
	if (ti->devstat.dstat & DEV_STAT_UNIT_CHECK) {
		tapestate_event (ti, TE_ERROR);
	} else if (ti->devstat.dstat & (DEV_STAT_DEV_END)) {
		tapestate_event (ti, TE_DONE);
	} else
		tapestate_event (ti, TE_OTHER);
}

int 
tape_oper_handler ( int irq, struct _devreg *dreg) {
    tape_info_t* ti=first_tape_info;
    tape_info_t* newtape;
    int rc,tape_num,retries=0,i;
    s390_dev_info_t dinfo;
    tape_discipline_t* disc;
#ifdef CONFIG_DEVFS_FS
    tape_frontend_t* frontend;
#endif
    long lockflags;
    while ((ti!=NULL) && (ti->devinfo.irq!=irq)) 
        ti=ti->next;
    if (ti!=NULL) {
        // irq is (still) used by tape. tell ingo to try again later
        PRINT_WARN ("Oper handler for irq %d called while irq still (internaly?) used.\n",irq);
        return -EAGAIN;
    }
    // irq is not used by tape
    rc = get_dev_info_by_irq (irq, &dinfo);
    if (rc == -ENODEV) {
        retries++;
        rc = get_dev_info_by_irq (irq, &dinfo);
        if (retries > 5) {
            PRINT_WARN ("No device information for new dev. could be retrieved.\n");
            return -ENODEV;
        }
    }
    disc = first_discipline;
    while ((disc != NULL) && (disc->cu_type != dinfo.sid_data.cu_type))
        disc = (tape_discipline_t *) (disc->next);
    if (disc == NULL)
        PRINT_WARN ("No matching discipline for cu_type %x found, ignoring device %04x.\n",dinfo.sid_data.cu_type,dinfo.devno);
    if (rc == -ENODEV) 
        PRINT_WARN ("No device information for new dev. could be retrieved.\n");
    if ((disc == NULL) || (rc == -ENODEV))
        return -ENODEV;
    
    /* Allocate tape structure  */
    ti = kmalloc (sizeof (tape_info_t), GFP_ATOMIC);
    if (ti == NULL) {
        PRINT_INFO ( "tape: can't allocate memory for "
                    "tape info structure\n");
        return -ENOBUFS;
    }
    memset(ti,0,sizeof(tape_info_t));
    ti->discipline = disc;
    disc->tape = ti;
    tape_num=0;
    if (*tape) {
        // we have static device ranges, so fingure out the tape_num of the attached tape
        for (i=0;i<devregct;i++)
            if (tape_devreg[i]->ci.devno==dinfo.devno) {
                tape_num=2*i;
                break;
            }
    } else {
        // we are running in autoprobe mode, find a free tape_num
        newtape=first_tape_info;
        while (newtape!=NULL) {
            if (newtape->rew_minor==tape_num) {
                // tape num in use. try next one
                tape_num+=2;
                newtape=first_tape_info;
            } else {
                // tape num not used by newtape. look at next tape info
                newtape=newtape->next;
            }
        }
    }
    rc = tape_setup (ti, irq, tape_num);
    if (rc) {
        kfree (ti);
        return -ENOBUFS;
    }
#ifdef CONFIG_DEVFS_FS
    for (frontend=first_frontend;frontend!=NULL;frontend=frontend->next) 
        frontend->mkdevfstree(ti);
#endif
    s390irq_spin_lock_irqsave (irq,lockflags);
    if (first_tape_info == NULL) {
        first_tape_info = ti;
    } else {
        newtape = first_tape_info;
        while (newtape->next != NULL)
            newtape = newtape->next;
        newtape->next = ti;
    }
    s390irq_spin_unlock_irqrestore (irq, lockflags);
    return 0;
}


static void
tape_noper_handler ( int irq, int status ) {
    tape_info_t *ti=first_tape_info;
    tape_info_t *lastti;
#ifdef CONFIG_DEVFS_FS
    tape_frontend_t *frontend;
#endif
    long lockflags;
    s390irq_spin_lock_irqsave(irq,lockflags);
    while (ti!=NULL && ti->devinfo.irq!=irq) ti=ti->next;
    if (ti==NULL) return;
    if (tapestate_get(ti)!=TS_UNUSED) {
        // device is in use!
        PRINT_WARN ("Tape #%d was detached while it was busy. Expect errors!",ti->blk_minor/2);
        tapestate_set(ti,TS_NOT_OPER);
        ti->rc=-ENODEV; 
	ti->wanna_wakeup=1;
	switch (tapestate_get(ti)) {
	case TS_REW_RELEASE_INIT:
	    tapestate_set(ti,TS_NOT_OPER);
	    wake_up (&ti->wq);
	    break;
#ifdef CONFIG_S390_TAPE_BLOCK
	case TS_BLOCK_INIT:
	    tapestate_set(ti,TS_NOT_OPER);
	    schedule_tapeblock_exec_IO(ti);
	    break;
#endif
	default:
	    tapestate_set(ti,TS_NOT_OPER);
	    wake_up_interruptible (&ti->wq);
	}
    } else {
        // device is unused!
        PRINT_WARN ("Tape #%d was detached.\n",ti->blk_minor/2);
        if (ti==first_tape_info) {
            first_tape_info=ti->next;
        } else {
            lastti=first_tape_info;
            while (lastti->next!=ti) lastti=lastti->next;
            lastti->next=ti->next;
        }
#ifdef CONFIG_DEVFS_FS
        for (frontend=first_frontend;frontend!=NULL;frontend=frontend->next)
            frontend->rmdevfstree(ti);
        tape_rmdevfsroots(ti);
#endif
        kfree(ti);
    }
    s390irq_spin_unlock_irqrestore(irq,lockflags);
    return;
}


void
tape_dump_sense (devstat_t * stat)
{
#ifdef TAPE_DEBUG
        int sl;
#endif
#if 0

	PRINT_WARN ("------------I/O resulted in unit check:-----------\n");
	for (sl = 0; sl < 4; sl++) {
		PRINT_WARN ("Sense:");
		for (sct = 0; sct < 8; sct++) {
			PRINT_WARN (" %2d:0x%02X", 8 * sl + sct,
				    stat->ii.sense.data[8 * sl + sct]);
		}
		PRINT_WARN ("\n");
	}
	PRINT_INFO ("Sense data: %02X%02X%02X%02X %02X%02X%02X%02X "
		    " %02X%02X%02X%02X %02X%02X%02X%02X \n",
		    stat->ii.sense.data[0], stat->ii.sense.data[1],
		    stat->ii.sense.data[2], stat->ii.sense.data[3],
		    stat->ii.sense.data[4], stat->ii.sense.data[5],
		    stat->ii.sense.data[6], stat->ii.sense.data[7],
		    stat->ii.sense.data[8], stat->ii.sense.data[9],
		    stat->ii.sense.data[10], stat->ii.sense.data[11],
		    stat->ii.sense.data[12], stat->ii.sense.data[13],
		    stat->ii.sense.data[14], stat->ii.sense.data[15]);
	PRINT_INFO ("Sense data: %02X%02X%02X%02X %02X%02X%02X%02X "
		    " %02X%02X%02X%02X %02X%02X%02X%02X \n",
		    stat->ii.sense.data[16], stat->ii.sense.data[17],
		    stat->ii.sense.data[18], stat->ii.sense.data[19],
		    stat->ii.sense.data[20], stat->ii.sense.data[21],
		    stat->ii.sense.data[22], stat->ii.sense.data[23],
		    stat->ii.sense.data[24], stat->ii.sense.data[25],
		    stat->ii.sense.data[26], stat->ii.sense.data[27],
		    stat->ii.sense.data[28], stat->ii.sense.data[29],
		    stat->ii.sense.data[30], stat->ii.sense.data[31]);
#endif
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"SENSE:");
        for (sl=0;sl<31;sl++) {
            debug_int_event (tape_debug_area,3,stat->ii.sense.data[sl]);
        }
        debug_int_exception (tape_debug_area,3,stat->ii.sense.data[31]);
#endif
}

/*
 * Setup tape_info_t structure of a tape device
 */
int
tape_setup (tape_info_t * ti, int irq, int minor)
{
	long lockflags;
	int rc = 0;

        if (minor>254) {
            PRINT_WARN ("Device id %d on irq %d will not be accessible since this driver is restricted to 128 devices.\n",minor/2,irq);
            return -EINVAL;
        }
	rc = get_dev_info_by_irq (irq, &(ti->devinfo));
	if (rc == -ENODEV) {	/* end of device list */
		return rc;
	}
	ti->rew_minor = minor;
	ti->nor_minor = minor + 1;
	ti->blk_minor = minor;
#ifdef CONFIG_DEVFS_FS
        tape_mkdevfsroots(ti);
#endif
	/* Register IRQ */
#ifdef CONFIG_S390_TAPE_DYNAMIC
        rc = s390_request_irq_special (irq, tape_irq, tape_noper_handler,0, "tape", &(ti->devstat));
#else
	rc = s390_request_irq (irq, tape_irq, 0, "tape", &(ti->devstat));
#endif
	s390irq_spin_lock_irqsave (irq, lockflags);
	ti->next = NULL;
	if (rc)
            PRINT_WARN ("Cannot register irq %d, rc=%d\n", irq, rc);
	init_waitqueue_head (&ti->wq);
	ti->kernbuf = ti->userbuf = ti->discdata = NULL;
	tapestate_set (ti, TS_UNUSED);
        ti->discdata=NULL;
	ti->discipline->setup_assist (ti);
        ti->wanna_wakeup=0;
	s390irq_spin_unlock_irqrestore (irq, lockflags);
	return rc;
}

/*
 *      tape_init will register the driver for each tape.
 */
int
tape_init (void)
{
	long lockflags;
	s390_dev_info_t dinfo;
	tape_discipline_t *disc;
	tape_info_t *ti = NULL, *tempti = NULL;
        char *opt_char,*opt_block,*opt_3490,*opt_3480;
	int irq = 0, rc, retries = 0, tape_num = 0;
        static int initialized=0;

        if (initialized) // Only init the devices once
            return 0;
        initialized=1;

#ifdef TAPE_DEBUG
        tape_debug_area = debug_register ( "tape", 3, 2, 10);
        debug_register_view(tape_debug_area,&debug_hex_ascii_view);
        debug_text_event (tape_debug_area,3,"begin init");
#endif /* TAPE_DEBUG */

        /* print banner */        
        PRINT_WARN ("IBM S/390 Tape Device Driver (v1.01).\n");
        PRINT_WARN ("(C) IBM Deutschland Entwicklung GmbH, 2000\n");
        opt_char=opt_block=opt_3480=opt_3490="not present";
#ifdef CONFIG_S390_TAPE_CHAR
        opt_char="built in";
#endif
#ifdef CONFIG_S390_TAPE_BLOCK
        opt_block="built in";
#endif
#ifdef CONFIG_S390_TAPE_3480
        opt_3480="built in";
#endif
#ifdef CONFIG_S390_TAPE_3490
        opt_3490="built in";
#endif
        /* print feature info */
        PRINT_WARN ("character device frontend   : %s\n",opt_char);
        PRINT_WARN ("block device frontend       : %s\n",opt_block);
        PRINT_WARN ("support for 3480 compatible : %s\n",opt_3480);
        PRINT_WARN ("support for 3490 compatible : %s\n",opt_3490);
        
#ifndef MODULE
        tape_split_parm_string(tape_parm_string);
#endif
        if (*tape) 
            PRINT_INFO ("Using ranges supplied in parameters, disabling autoprobe mode.\n");
        else
            PRINT_INFO ("No parameters supplied, enabling autoprobe mode for all supported devices.\n");
#ifdef CONFIG_S390_TAPE_3490
        if (*tape)
            first_discipline = tape3490_init (0); // no autoprobe for devices
        else
            first_discipline = tape3490_init (1); // do autoprobe since no parm specified
	first_discipline->next = NULL;
#endif

#ifdef CONFIG_S390_TAPE_3480
        if (first_discipline == NULL) {
            if (*tape)
                first_discipline = tape3480_init (0); // no autoprobe for devices
            else 
                first_discipline = tape3480_init (1); // do autoprobe since no parm specified
            first_discipline->next = NULL;
        } else {
            if (*tape)
                first_discipline->next = tape3480_init (0); // no autoprobe for devices
            else
                first_discipline->next = tape3480_init (1); // do autoprobe since no parm specified
            ((tape_discipline_t*) (first_discipline->next))->next=NULL;
        }
#endif
#ifdef CONFIG_DEVFS_FS
        tape_devfs_root_entry=devfs_mk_dir (NULL, "tape", NULL);
#endif CONFIG_DEVFS_FS

#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"dev detect");
#endif /* TAPE_DEBUG */
	/* Allocate the tape structures */
        if (*tape!=NULL) {
            // we have parameters, continue with parsing the parameters and set the devices online
            tape_parm_parse (tape);
        } else {
            // we are running in autodetect mode, search all devices for compatibles
            for (irq = get_irq_first(); irq!=-ENODEV; irq=get_irq_next(irq)) {
		rc = get_dev_info_by_irq (irq, &dinfo);
		disc = first_discipline;
		while ((disc != NULL) && (disc->cu_type != dinfo.sid_data.cu_type))
                    disc = (tape_discipline_t *) (disc->next);
		if ((disc == NULL) || (rc == -ENODEV))
                    continue;
#ifdef TAPE_DEBUG
                debug_text_event (tape_debug_area,3,"det irq:  ");
                debug_int_event (tape_debug_area,3,irq);
                debug_text_event (tape_debug_area,3,"cu:       ");
                debug_int_event (tape_debug_area,3,disc->cu_type);
#endif /* TAPE_DEBUG */
                PRINT_INFO ("using devno %04x with discipline %04x on irq %d as tape device %d\n",dinfo.devno,dinfo.sid_data.cu_type,irq,tape_num/2);
		/* Allocate tape structure  */
		ti = kmalloc (sizeof (tape_info_t), GFP_ATOMIC);
		if (ti == NULL) {
#ifdef TAPE_DEBUG
                    debug_text_exception (tape_debug_area,3,"ti:no mem ");
#endif /* TAPE_DEBUG */
                    PRINT_INFO ("tape: can't allocate memory for "
				    "tape info structure\n");
                    continue;
		}
		memset(ti,0,sizeof(tape_info_t));
		ti->discipline = disc;
		disc->tape = ti;
		rc = tape_setup (ti, irq, tape_num);
		if (rc) {
#ifdef TAPE_DEBUG
                    debug_text_event (tape_debug_area,3,"tsetup err");
                    debug_int_exception (tape_debug_area,3,rc);
#endif /* TAPE_DEBUG */
                    kfree (ti);
		} else {
                    s390irq_spin_lock_irqsave (irq, lockflags);
                    if (first_tape_info == NULL) {
                        first_tape_info = ti;
                    } else {
                        tempti = first_tape_info;
                        while (tempti->next != NULL)
                            tempti = tempti->next;
                        tempti->next = ti;
                    }
                    tape_num += 2;
                    s390irq_spin_unlock_irqrestore (irq, lockflags);
		}
            }
        }
            
	/* Allocate local buffer for the ccwcache       */
	tape_init_emergency_req ();
#ifdef CONFIG_PROC_FS
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
	tape_devices_entry = create_proc_entry ("tapedevices",
						S_IFREG | S_IRUGO | S_IWUSR,
						&proc_root);
	tape_devices_entry->proc_fops = &tape_devices_file_ops;
	tape_devices_entry->proc_iops = &tape_devices_inode_ops;
#else
	tape_devices_entry = (struct proc_dir_entry *) kmalloc 
            (sizeof (struct proc_dir_entry), GFP_ATOMIC);
        if (tape_devices_entry) {
            memset (tape_devices_entry, 0, sizeof (struct proc_dir_entry));
            tape_devices_entry->name = "tapedevices";
            tape_devices_entry->namelen = strlen ("tapedevices");
            tape_devices_entry->low_ino = 0;
            tape_devices_entry->mode = (S_IFREG | S_IRUGO | S_IWUSR);
            tape_devices_entry->nlink = 1;
            tape_devices_entry->uid = 0;
            tape_devices_entry->gid = 0;
            tape_devices_entry->size = 0;
            tape_devices_entry->get_info = NULL;
            tape_devices_entry->ops = &tape_devices_inode_ops;
            proc_register (&proc_root, tape_devices_entry);
	}
#endif
#endif /* CONFIG_PROC_FS */

	return 0;
}

#ifdef MODULE
MODULE_AUTHOR("(C) 2001 IBM Deutschland Entwicklung GmbH by Carsten Otte (cotte@de.ibm.com)");
MODULE_DESCRIPTION("Linux for S/390 channel attached tape device driver");
MODULE_PARM (tape, "1-" __MODULE_STRING (256) "s");

int
init_module (void)
{
#ifdef CONFIG_S390_TAPE_CHAR
	tapechar_init ();
#endif
#ifdef CONFIG_S390_TAPE_BLOCK
	tapeblock_init ();
#endif
        return 0;
}

void
cleanup_module (void)
{
        tape_info_t *ti ,*temp;
        tape_frontend_t* frontend, *tempfe;
        tape_discipline_t* disc ,*tempdi;
        int i;
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"cleaup mod");
#endif /* TAPE_DEBUG */

        if (*tape) {
            // we are running with parameters. we'll now deregister from our devno's
            for (i=0;i<devregct;i++) {
                s390_device_unregister(tape_devreg[devregct]);
            }
        }
	ti = first_tape_info;
	while (ti != NULL) {
		temp = ti;
		ti = ti->next;
                //cleanup a device 
#ifdef TAPE_DEBUG
                debug_text_event (tape_debug_area,6,"free irq:");
                debug_int_event (tape_debug_area,6,temp->devinfo.irq);
#endif /* TAPE_DEBUG */
		free_irq (temp->devinfo.irq, &(temp->devstat));
                if (temp->discdata) kfree (temp->discdata);
                if (temp->kernbuf) kfree (temp->kernbuf);
                if (temp->cqr) tape_free_request(temp->cqr);
#ifdef CONFIG_DEVFS_FS
                for (frontend=first_frontend;frontend!=NULL;frontend=frontend->next)
                    frontend->rmdevfstree(temp);
                tape_rmdevfsroots(temp);
#endif
		kfree (temp);
	}
#ifdef CONFIG_DEVFS_FS
        devfs_unregister (tape_devfs_root_entry);
#endif CONFIG_DEVFS_FS
#ifdef CONFIG_PROC_FS
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
	remove_proc_entry ("tapedevices", &proc_root);
#else
	proc_unregister (&proc_root, tape_devices_entry->low_ino);
	kfree (tape_devices_entry);
#endif				/* LINUX_IS_24 */
#endif 
#ifdef CONFIG_S390_TAPE_CHAR
	tapechar_uninit();
#endif
#ifdef CONFIG_S390_TAPE_BLOCK
        tapeblock_uninit();
#endif
        frontend=first_frontend;
	while (frontend != NULL) {
		tempfe = frontend;
		frontend = frontend->next;
		kfree (tempfe);
	}
        disc=first_discipline;
	while (disc != NULL) {
                if (*tape)
                    disc->shutdown(0);
                else
                    disc->shutdown(1);
		tempdi = disc;
		disc = disc->next;
		kfree (tempdi);
	}
	/* Deallocate the local buffer for the ccwcache         */
	tape_cleanup_emergency_req ();
#ifdef TAPE_DEBUG
        debug_unregister (tape_debug_area);
#endif /* TAPE_DEBUG */
}
#endif				/* MODULE */

inline void
tapestate_set (tape_info_t * ti, int newstate)
{
    if (ti->tape_state == TS_NOT_OPER) {
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"ts_set err");
        debug_text_exception (tape_debug_area,3,"dev n.oper");
#endif /* TAPE_DEBUG */
    } else {
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,4,"ts. dev:  ");
        debug_int_event (tape_debug_area,4,ti->blk_minor);
        debug_text_event (tape_debug_area,4,"old ts:   ");
        debug_text_event (tape_debug_area,4,(((tapestate_get (ti) < TS_SIZE) &&
                                             (tapestate_get (ti) >=0 )) ?
                                            state_verbose[tapestate_get (ti)] :
                                            "UNKNOWN TS"));
        debug_text_event (tape_debug_area,4,"new ts:   ");
        debug_text_event (tape_debug_area,4,(((newstate < TS_SIZE) &&
                                              (newstate >= 0)) ?
                                             state_verbose[newstate] :
                                             "UNKNOWN TS"));
#endif /* TAPE_DEBUG */
	ti->tape_state = newstate;
    }
}

inline int
tapestate_get (tape_info_t * ti)
{
	return (ti->tape_state);
}

void
tapestate_event (tape_info_t * ti, int event)
{
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"te! dev:  ");
        debug_int_event (tape_debug_area,6,ti->blk_minor);
        debug_text_event (tape_debug_area,6,"event:");
        debug_text_event (tape_debug_area,6,((event >=0) &&
                                            (event < TE_SIZE)) ?
                         event_verbose[event] : "TE UNKNOWN");
        debug_text_event (tape_debug_area,6,"state:");
        debug_text_event (tape_debug_area,6,((tapestate_get(ti) >= 0) &&
                                            (tapestate_get(ti) < TS_SIZE)) ?
                         state_verbose[tapestate_get (ti)] :
                         "TS UNKNOWN");
#endif /* TAPE_DEBUG */    
        if (event == TE_ERROR) { 
            ti->discipline->error_recovery(ti);
        } else {
            if ((event >= 0) &&
                (event < TE_SIZE) &&
                (tapestate_get (ti) >= 0) &&
                (tapestate_get (ti) < TS_SIZE) &&
                ((*(ti->discipline->event_table))[tapestate_get (ti)][event] != NULL))
		((*(ti->discipline->event_table))[tapestate_get (ti)][event]) (ti);
            else {
#ifdef TAPE_DEBUG
                debug_text_exception (tape_debug_area,3,"TE UNEXPEC");
#endif /* TAPE_DEBUG */
		ti->discipline->default_handler (ti);
            }
        }
}

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

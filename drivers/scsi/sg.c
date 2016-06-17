/*
 *  History:
 *  Started: Aug 9 by Lawrence Foard (entropy@world.std.com),
 *           to allow user process control of SCSI devices.
 *  Development Sponsored by Killy Corp. NY NY
 *
 * Original driver (sg.c):
 *        Copyright (C) 1992 Lawrence Foard
 * Version 2 and 3 extensions to driver:
 *        Copyright (C) 1998 - 2003 Douglas Gilbert
 *
 *  Modified  19-JAN-1998  Richard Gooch <rgooch@atnf.csiro.au>  Devfs support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 */
#include <linux/config.h>
#ifdef CONFIG_PROC_FS
 static char * sg_version_str = "Version: 3.1.25 (20030529)";
#endif
 static int sg_version_num = 30125; /* 2 digits for each component */
/*
 *  D. P. Gilbert (dgilbert@interlog.com, dougg@triode.net.au), notes:
 *      - scsi logging is available via SCSI_LOG_TIMEOUT macros. First
 *        the kernel/module needs to be built with CONFIG_SCSI_LOGGING
 *        (otherwise the macros compile to empty statements).
 *        Then before running the program to be debugged enter:
 *          # echo "scsi log timeout 7" > /proc/scsi/scsi
 *        This will send copious output to the console and the log which
 *        is usually /var/log/messages. To turn off debugging enter:
 *          # echo "scsi log timeout 0" > /proc/scsi/scsi
 *        The 'timeout' token was chosen because it is relatively unused.
 *        The token 'hlcomplete' should be used but that triggers too
 *        much output from the sd device driver. To dump the current
 *        state of the SCSI mid level data structures enter:
 *          # echo "scsi dump 1" > /proc/scsi/scsi
 *        To dump the state of sg's data structures use:
 *          # cat /proc/scsi/sg/debug
 *
 */
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/mtio.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
static int sg_proc_init(void);
static void sg_proc_cleanup(void);
#endif

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif /* LINUX_VERSION_CODE */

#define SG_ALLOW_DIO_DEF 0
#define SG_ALLOW_DIO_CODE	/* compile out be commenting this define */
#ifdef SG_ALLOW_DIO_CODE
#include <linux/iobuf.h>
#endif

#define SG_NEW_KIOVEC 0	/* use alloc_kiovec(), not alloc_kiovec_sz() */

int sg_big_buff = SG_DEF_RESERVED_SIZE;
/* N.B. This variable is readable and writeable via
   /proc/scsi/sg/def_reserved_size . Each time sg_open() is called a buffer
   of this size (or less if there is not enough memory) will be reserved
   for use by this file descriptor. [Deprecated usage: this variable is also
   readable via /proc/sys/kernel/sg-big-buff if the sg driver is built into
   the kernel (i.e. it is not a module).] */
static int def_reserved_size = -1;      /* picks up init parameter */
static int sg_allow_dio = SG_ALLOW_DIO_DEF;

#define SG_SECTOR_SZ 512
#define SG_SECTOR_MSK (SG_SECTOR_SZ - 1)

#define SG_LOW_POOL_THRESHHOLD 30
#define SG_MAX_POOL_SECTORS 320  /* Max. number of pool sectors to take */

static int sg_pool_secs_avail = SG_MAX_POOL_SECTORS;

#define SG_HEAP_PAGE 1  /* heap from kernel via get_free_pages() */
#define SG_HEAP_KMAL 2  /* heap from kernel via kmalloc() */
#define SG_HEAP_POOL 3  /* heap from scsi dma pool (mid-level) */
#define SG_USER_MEM 4   /* memory belongs to user space */

#define SG_DEV_ARR_LUMP 6 /* amount to over allocate sg_dev_arr by */


static int sg_init(void);
static int sg_attach(Scsi_Device *);
static void sg_finish(void);
static int sg_detect(Scsi_Device *);
static void sg_detach(Scsi_Device *);

static Scsi_Request * dummy_cmdp;	/* only used for sizeof */

static rwlock_t sg_dev_arr_lock = RW_LOCK_UNLOCKED;  /* Also used to lock
			file descriptor list for device */

static struct Scsi_Device_Template sg_template =
{
      tag:"sg",
      scsi_type:0xff,
      major:SCSI_GENERIC_MAJOR,
      detect:sg_detect,
      init:sg_init,
      finish:sg_finish,
      attach:sg_attach,
      detach:sg_detach
};


typedef struct sg_scatter_hold  /* holding area for scsi scatter gather info */
{
    unsigned short k_use_sg;    /* Count of kernel scatter-gather pieces */
    unsigned short sglist_len;  /* size of malloc'd scatter-gather list ++ */
    unsigned bufflen;           /* Size of (aggregate) data buffer */
    unsigned b_malloc_len;      /* actual len malloc'ed in buffer */
    void * buffer;              /* Data buffer or scatter list + mem_src_arr */
    struct kiobuf * kiobp;      /* for direct IO information */
    char mapped;                /* indicates kiobp has locked pages */
    char buffer_mem_src;        /* heap whereabouts of 'buffer' */
    unsigned char cmd_opcode;   /* first byte of command */
} Sg_scatter_hold;    /* 24 bytes long on i386 */

struct sg_device;               /* forward declarations */
struct sg_fd;

typedef struct sg_request  /* SG_MAX_QUEUE requests outstanding per file */
{
    Scsi_Request * my_cmdp;     /* != 0  when request with lower levels */
    struct sg_request * nextrp; /* NULL -> tail request (slist) */
    struct sg_fd * parentfp;    /* NULL -> not in use */
    Sg_scatter_hold data;       /* hold buffer, perhaps scatter list */
    sg_io_hdr_t header;         /* scsi command+info, see <scsi/sg.h> */
    unsigned char sense_b[sizeof(dummy_cmdp->sr_sense_buffer)];
    char res_used;              /* 1 -> using reserve buffer, 0 -> not ... */
    char orphan;                /* 1 -> drop on sight, 0 -> normal */
    char sg_io_owned;           /* 1 -> packet belongs to SG_IO */
    volatile char done;         /* 0->before bh, 1->before read, 2->read */
} Sg_request; /* 168 bytes long on i386 */

typedef struct sg_fd /* holds the state of a file descriptor */
{
    struct sg_fd * nextfp; /* NULL when last opened fd on this device */
    struct sg_device * parentdp;     /* owning device */
    wait_queue_head_t read_wait;     /* queue read until command done */
    rwlock_t rq_list_lock;	     /* protect access to list in req_arr */
    int timeout;                     /* defaults to SG_DEFAULT_TIMEOUT */
    Sg_scatter_hold reserve;  /* buffer held for this file descriptor */
    unsigned save_scat_len;   /* original length of trunc. scat. element */
    Sg_request * headrp;      /* head of request slist, NULL->empty */
    struct fasync_struct * async_qp; /* used by asynchronous notification */
    Sg_request req_arr[SG_MAX_QUEUE]; /* used as singly-linked list */
    char low_dma;       /* as in parent but possibly overridden to 1 */
    char force_packid;  /* 1 -> pack_id input to read(), 0 -> ignored */
    volatile char closed; /* 1 -> fd closed but request(s) outstanding */
    char fd_mem_src;    /* heap whereabouts of this Sg_fd object */
    char cmd_q;         /* 1 -> allow command queuing, 0 -> don't */
    char next_cmd_len;  /* 0 -> automatic (def), >0 -> use on next write() */
    char keep_orphan;   /* 0 -> drop orphan (def), 1 -> keep for read() */
    char mmap_called;   /* 0 -> mmap() never called on this fd */
} Sg_fd; /* 2760 bytes long on i386 */

typedef struct sg_device /* holds the state of each scsi generic device */
{
    Scsi_Device * device;
    wait_queue_head_t o_excl_wait;   /* queue open() when O_EXCL in use */
    int sg_tablesize;   /* adapter's max scatter-gather table size */
    Sg_fd * headfp;     /* first open fd belonging to this device */
    devfs_handle_t de;
    kdev_t i_rdev;      /* holds device major+minor number */
    volatile char detached;  /* 0->attached, 1->detached pending removal */
    volatile char exclude;   /* opened for exclusive access */
    char sgdebug;       /* 0->off, 1->sense, 9->dump dev, 10-> all devs */
} Sg_device; /* 36 bytes long on i386 */


static int sg_fasync(int fd, struct file * filp, int mode);
static void sg_cmd_done_bh(Scsi_Cmnd * SCpnt);
static int sg_start_req(Sg_request * srp);
static void sg_finish_rem_req(Sg_request * srp);
static int sg_build_indi(Sg_scatter_hold * schp, Sg_fd * sfp, int buff_size);
static int sg_build_sgat(Sg_scatter_hold * schp, const Sg_fd * sfp,
			 int tablesize);
static ssize_t sg_new_read(Sg_fd * sfp, char * buf, size_t count,
			   Sg_request * srp);
static ssize_t sg_new_write(Sg_fd * sfp, const char * buf, size_t count,
			int blocking, int read_only, Sg_request ** o_srp);
static int sg_common_write(Sg_fd * sfp, Sg_request * srp,
			   unsigned char * cmnd, int timeout, int blocking);
static int sg_u_iovec(sg_io_hdr_t * hp, int sg_num, int ind,
		      int wr_xf, int * countp, unsigned char ** up);
static int sg_write_xfer(Sg_request * srp);
static int sg_read_xfer(Sg_request * srp);
static void sg_read_oxfer(Sg_request * srp, char * outp, int num_read_xfer);
static void sg_remove_scat(Sg_scatter_hold * schp);
static char * sg_get_sgat_msa(Sg_scatter_hold * schp);
static void sg_build_reserve(Sg_fd * sfp, int req_size);
static void sg_link_reserve(Sg_fd * sfp, Sg_request * srp, int size);
static void sg_unlink_reserve(Sg_fd * sfp, Sg_request * srp);
static char * sg_malloc(const Sg_fd * sfp, int size, int * retSzp,
                        int * mem_srcp);
static void sg_free(char * buff, int size, int mem_src);
static char * sg_low_malloc(int rqSz, int lowDma, int mem_src,
                            int * retSzp);
static void sg_low_free(char * buff, int size, int mem_src);
static Sg_fd * sg_add_sfp(Sg_device * sdp, int dev);
static int sg_remove_sfp(Sg_device * sdp, Sg_fd * sfp);
static void __sg_remove_sfp(Sg_device * sdp, Sg_fd * sfp);
static Sg_request * sg_get_rq_mark(Sg_fd * sfp, int pack_id);
static Sg_request * sg_add_request(Sg_fd * sfp);
static int sg_remove_request(Sg_fd * sfp, Sg_request * srp);
static int sg_res_in_use(Sg_fd * sfp);
static int sg_ms_to_jif(unsigned int msecs);
static inline unsigned sg_jif_to_ms(int jifs);
static int sg_allow_access(unsigned char opcode, char dev_type);
static int sg_build_dir(Sg_request * srp, Sg_fd * sfp, int dxfer_len);
static void sg_unmap_and(Sg_scatter_hold * schp, int free_also);
static Sg_device * sg_get_dev(int dev);
static inline int sg_alloc_kiovec(int nr, struct kiobuf **bufp, int *szp);
static inline void sg_free_kiovec(int nr, struct kiobuf **bufp, int *szp);
#ifdef CONFIG_PROC_FS
static int sg_last_dev(void);
#endif

static Sg_device ** sg_dev_arr = NULL;

#define SZ_SG_HEADER sizeof(struct sg_header)
#define SZ_SG_IO_HDR sizeof(sg_io_hdr_t)
#define SZ_SG_IOVEC sizeof(sg_iovec_t)
#define SZ_SG_REQ_INFO sizeof(sg_req_info_t)


static int sg_open(struct inode * inode, struct file * filp)
{
    int dev = MINOR(inode->i_rdev);
    int flags = filp->f_flags;
    Sg_device * sdp;
    Sg_fd * sfp;
    int res;
    int retval = -EBUSY;

    SCSI_LOG_TIMEOUT(3, printk("sg_open: dev=%d, flags=0x%x\n", dev, flags));
    sdp = sg_get_dev(dev);
    if ((! sdp) || (! sdp->device))
        return -ENXIO;
    if (sdp->detached)
    	return -ENODEV;

     /* This driver's module count bumped by fops_get in <linux/fs.h> */
     /* Prevent the device driver from vanishing while we sleep */
     if (sdp->device->host->hostt->module)
        __MOD_INC_USE_COUNT(sdp->device->host->hostt->module);
    sdp->device->access_count++;

    if (! ((flags & O_NONBLOCK) ||
	   scsi_block_when_processing_errors(sdp->device))) {
        retval = -ENXIO;
	/* we are in error recovery for this device */
	goto error_out;
    }

    if (flags & O_EXCL) {
        if (O_RDONLY == (flags & O_ACCMODE))  {
            retval = -EPERM;   /* Can't lock it with read only access */
	    goto error_out;
	}
	if (sdp->headfp && (flags & O_NONBLOCK))
            goto error_out;
        res = 0; 
	__wait_event_interruptible(sdp->o_excl_wait,
	       ((sdp->headfp || sdp->exclude) ? 0 : (sdp->exclude = 1)),
                                   res);
        if (res) {
            retval = res; /* -ERESTARTSYS because signal hit process */
	    goto error_out;
        }
    }
    else if (sdp->exclude) { /* some other fd has an exclusive lock on dev */
        if (flags & O_NONBLOCK)
            goto error_out;
        res = 0; 
        __wait_event_interruptible(sdp->o_excl_wait, (! sdp->exclude), res);
        if (res) {
            retval = res; /* -ERESTARTSYS because signal hit process */
	    goto error_out;
        }
    }
    if (sdp->detached) {
    	retval = -ENODEV;
	goto error_out;
    }
    if (! sdp->headfp) { /* no existing opens on this device */
        sdp->sgdebug = 0;
        sdp->sg_tablesize = sdp->device->host->sg_tablesize;
    }
    if ((sfp = sg_add_sfp(sdp, dev)))
        filp->private_data = sfp;
    else {
        if (flags & O_EXCL) sdp->exclude = 0; /* undo if error */
        retval = -ENOMEM;
	goto error_out;
    }
    return 0;

error_out:
    sdp->device->access_count--;
    if ((! sdp->detached) && sdp->device->host->hostt->module)
        __MOD_DEC_USE_COUNT(sdp->device->host->hostt->module);
    return retval;
}

/* Following function was formerly called 'sg_close' */
static int sg_release(struct inode * inode, struct file * filp)
{
    Sg_device * sdp;
    Sg_fd * sfp;

    lock_kernel();
    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp))) {
	unlock_kernel();
        return -ENXIO;
    }
    SCSI_LOG_TIMEOUT(3, printk("sg_release: dev=%d\n", MINOR(sdp->i_rdev)));
    sg_fasync(-1, filp, 0);   /* remove filp from async notification list */
    if (0 == sg_remove_sfp(sdp, sfp)) { /* Returns 1 when sdp gone */
        if (! sdp->detached) {
            sdp->device->access_count--;
            if (sdp->device->host->hostt->module)
                __MOD_DEC_USE_COUNT(sdp->device->host->hostt->module);
        }
	sdp->exclude = 0;
	wake_up_interruptible(&sdp->o_excl_wait);
    }
    unlock_kernel();
    return 0;
}

static ssize_t sg_read(struct file * filp, char * buf,
                       size_t count, loff_t *ppos)
{
    int k, res;
    Sg_device * sdp;
    Sg_fd * sfp;
    Sg_request * srp;
    int req_pack_id = -1;
    struct sg_header old_hdr;
    sg_io_hdr_t new_hdr;
    sg_io_hdr_t * hp;

    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp)))
        return -ENXIO;
    SCSI_LOG_TIMEOUT(3, printk("sg_read: dev=%d, count=%d\n",
                               MINOR(sdp->i_rdev), (int)count));
    if (ppos != &filp->f_pos)
        ; /* FIXME: Hmm.  Seek to the right place, or fail?  */
    if ((k = verify_area(VERIFY_WRITE, buf, count)))
        return k;
    if (sfp->force_packid && (count >= SZ_SG_HEADER)) {
	__copy_from_user(&old_hdr, buf, SZ_SG_HEADER);
	if (old_hdr.reply_len < 0) {
	    if (count >= SZ_SG_IO_HDR) {
		__copy_from_user(&new_hdr, buf, SZ_SG_IO_HDR);
		req_pack_id = new_hdr.pack_id;
	    }
	}
	else
	    req_pack_id = old_hdr.pack_id;
    }
    srp = sg_get_rq_mark(sfp, req_pack_id);
    if (! srp) { /* now wait on packet to arrive */
	if (sdp->detached)
	    return -ENODEV;
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
	while (1) {
	    res = 0;  /* following is a macro that beats race condition */
	    __wait_event_interruptible(sfp->read_wait, (sdp->detached || 
		    (srp = sg_get_rq_mark(sfp, req_pack_id))), res);
	    if (sdp->detached)
		return -ENODEV;
	    if (0 == res)
		break;
	    return res; /* -ERESTARTSYS because signal hit process */
	}
    }
    if (srp->header.interface_id != '\0')
	return sg_new_read(sfp, buf, count, srp);

    hp = &srp->header;
    memset(&old_hdr, 0, SZ_SG_HEADER);
    old_hdr.reply_len = (int)hp->timeout;
    old_hdr.pack_len = old_hdr.reply_len;   /* very old, strange behaviour */
    old_hdr.pack_id = hp->pack_id;
    old_hdr.twelve_byte =
	    ((srp->data.cmd_opcode >= 0xc0) && (12 == hp->cmd_len)) ? 1 : 0;
    old_hdr.target_status = hp->masked_status;
    old_hdr.host_status = hp->host_status;
    old_hdr.driver_status = hp->driver_status;
    if ((CHECK_CONDITION & hp->masked_status) ||
	(DRIVER_SENSE & hp->driver_status))
	memcpy(old_hdr.sense_buffer, srp->sense_b,
	       sizeof(old_hdr.sense_buffer));
    switch (hp->host_status)
    { /* This setup of 'result' is for backward compatibility and is best
	 ignored by the user who should use target, host + driver status */
	case DID_OK:
	case DID_PASSTHROUGH:
	case DID_SOFT_ERROR:
	    old_hdr.result = 0;
	    break;
	case DID_NO_CONNECT:
	case DID_BUS_BUSY:
	case DID_TIME_OUT:
	    old_hdr.result = EBUSY;
	    break;
	case DID_BAD_TARGET:
	case DID_ABORT:
	case DID_PARITY:
	case DID_RESET:
	case DID_BAD_INTR:
	    old_hdr.result = EIO;
	    break;
	case DID_ERROR:
	    old_hdr.result =
	      (srp->sense_b[0] == 0 && hp->masked_status == GOOD) ? 0 : EIO;
	    break;
	default:
	    old_hdr.result = EIO;
	    break;
    }

    /* Now copy the result back to the user buffer.  */
    if (count >= SZ_SG_HEADER) {
	__copy_to_user(buf, &old_hdr, SZ_SG_HEADER);
        buf += SZ_SG_HEADER;
	if (count > old_hdr.reply_len)
	    count = old_hdr.reply_len;
	if (count > SZ_SG_HEADER)
	    sg_read_oxfer(srp, buf, count - SZ_SG_HEADER);
    }
    else
	count = (old_hdr.result == 0) ? 0 : -EIO;
    sg_finish_rem_req(srp);
    return count;
}

static ssize_t sg_new_read(Sg_fd * sfp, char * buf, size_t count,
			   Sg_request * srp)
{
    sg_io_hdr_t * hp = &srp->header;
    int err = 0;
    int len;

    if (count < SZ_SG_IO_HDR) {
	err = -EINVAL;
    	goto err_out;
    }
    hp->sb_len_wr = 0;
    if ((hp->mx_sb_len > 0) && hp->sbp) {
	if ((CHECK_CONDITION & hp->masked_status) ||
	    (DRIVER_SENSE & hp->driver_status)) {
	    int sb_len = sizeof(dummy_cmdp->sr_sense_buffer);
	    sb_len = (hp->mx_sb_len > sb_len) ? sb_len : hp->mx_sb_len;
	    len = 8 + (int)srp->sense_b[7]; /* Additional sense length field */
	    len = (len > sb_len) ? sb_len : len;
	    if ((err = verify_area(VERIFY_WRITE, hp->sbp, len)))
		goto err_out;
	    __copy_to_user(hp->sbp, srp->sense_b, len);
	    hp->sb_len_wr = len;
	}
    }
    if (hp->masked_status || hp->host_status || hp->driver_status)
	hp->info |= SG_INFO_CHECK;
    copy_to_user(buf, hp, SZ_SG_IO_HDR);
    err = sg_read_xfer(srp);
err_out:
    sg_finish_rem_req(srp);
    return (0 == err) ? count : err;
}


static ssize_t sg_write(struct file * filp, const char * buf,
                        size_t count, loff_t *ppos)
{
    int                   mxsize, cmd_size, k;
    int                   input_size, blocking;
    unsigned char         opcode;
    Sg_device           * sdp;
    Sg_fd               * sfp;
    Sg_request          * srp;
    struct sg_header      old_hdr;
    sg_io_hdr_t         * hp;
    unsigned char         cmnd[sizeof(dummy_cmdp->sr_cmnd)];

    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp)))
        return -ENXIO;
    SCSI_LOG_TIMEOUT(3, printk("sg_write: dev=%d, count=%d\n",
                               MINOR(sdp->i_rdev), (int)count));
    if (sdp->detached)
    	return -ENODEV;
    if (! ((filp->f_flags & O_NONBLOCK) ||
           scsi_block_when_processing_errors(sdp->device)))
        return -ENXIO;
    if (ppos != &filp->f_pos)
        ; /* FIXME: Hmm.  Seek to the right place, or fail?  */

    if ((k = verify_area(VERIFY_READ, buf, count)))
        return k;  /* protects following copy_from_user()s + get_user()s */
    if (count < SZ_SG_HEADER)
	return -EIO;
    __copy_from_user(&old_hdr, buf, SZ_SG_HEADER);
    blocking = !(filp->f_flags & O_NONBLOCK);
    if (old_hdr.reply_len < 0)
	return sg_new_write(sfp, buf, count, blocking, 0, NULL);
    if (count < (SZ_SG_HEADER + 6))
	return -EIO;   /* The minimum scsi command length is 6 bytes. */

    if (! (srp = sg_add_request(sfp))) {
	SCSI_LOG_TIMEOUT(1, printk("sg_write: queue full\n"));
	return -EDOM;
    }
    buf += SZ_SG_HEADER;
    __get_user(opcode, buf);
    if (sfp->next_cmd_len > 0) {
        if (sfp->next_cmd_len > MAX_COMMAND_SIZE) {
            SCSI_LOG_TIMEOUT(1, printk("sg_write: command length too long\n"));
            sfp->next_cmd_len = 0;
	    sg_remove_request(sfp, srp);
            return -EIO;
        }
        cmd_size = sfp->next_cmd_len;
        sfp->next_cmd_len = 0; /* reset so only this write() effected */
    }
    else {
        cmd_size = COMMAND_SIZE(opcode); /* based on SCSI command group */
	if ((opcode >= 0xc0) && old_hdr.twelve_byte)
            cmd_size = 12;
    }
    SCSI_LOG_TIMEOUT(4, printk("sg_write:   scsi opcode=0x%02x, cmd_size=%d\n",
                               (int)opcode, cmd_size));
/* Determine buffer size.  */
    input_size = count - cmd_size;
    mxsize = (input_size > old_hdr.reply_len) ? input_size :
						old_hdr.reply_len;
    mxsize -= SZ_SG_HEADER;
    input_size -= SZ_SG_HEADER;
    if (input_size < 0) {
        sg_remove_request(sfp, srp);
        return -EIO; /* User did not pass enough bytes for this command. */
    }
    hp = &srp->header;
    hp->interface_id = '\0'; /* indicator of old interface tunnelled */
    hp->cmd_len = (unsigned char)cmd_size;
    hp->iovec_count = 0;
    hp->mx_sb_len = 0;
    if (input_size > 0)
	hp->dxfer_direction = (old_hdr.reply_len > SZ_SG_HEADER) ?
			      SG_DXFER_TO_FROM_DEV : SG_DXFER_TO_DEV;
    else
	hp->dxfer_direction = (mxsize > 0) ? SG_DXFER_FROM_DEV :
					     SG_DXFER_NONE;
    hp->dxfer_len = mxsize;
    hp->dxferp = (unsigned char *)buf + cmd_size;
    hp->sbp = NULL;
    hp->timeout = old_hdr.reply_len;    /* structure abuse ... */
    hp->flags = input_size;             /* structure abuse ... */
    hp->pack_id = old_hdr.pack_id;
    hp->usr_ptr = NULL;
    __copy_from_user(cmnd, buf, cmd_size);
    k = sg_common_write(sfp, srp, cmnd, sfp->timeout, blocking);
    return (k < 0) ? k : count;
}

static ssize_t sg_new_write(Sg_fd * sfp, const char * buf, size_t count,
			    int blocking, int read_only, Sg_request ** o_srp)
{
    int                   k;
    Sg_request          * srp;
    sg_io_hdr_t         * hp;
    unsigned char         cmnd[sizeof(dummy_cmdp->sr_cmnd)];
    int                   timeout;

    if (count < SZ_SG_IO_HDR)
	return -EINVAL;
    if ((k = verify_area(VERIFY_READ, buf, count)))
	return k;  /* protects following copy_from_user()s + get_user()s */

    sfp->cmd_q = 1;  /* when sg_io_hdr seen, set command queuing on */
    if (! (srp = sg_add_request(sfp))) {
	SCSI_LOG_TIMEOUT(1, printk("sg_new_write: queue full\n"));
	return -EDOM;
    }
    hp = &srp->header;
    __copy_from_user(hp, buf, SZ_SG_IO_HDR);
    if (hp->interface_id != 'S') {
	sg_remove_request(sfp, srp);
	return -ENOSYS;
    }
    if (hp->flags & SG_FLAG_MMAP_IO) {
    	if (hp->dxfer_len > sfp->reserve.bufflen) {
	    sg_remove_request(sfp, srp);
	    return -ENOMEM;	/* MMAP_IO size must fit in reserve buffer */
	}
	if (hp->flags & SG_FLAG_DIRECT_IO) {
	    sg_remove_request(sfp, srp);
	    return -EINVAL;	/* either MMAP_IO or DIRECT_IO (not both) */
	}
	if (sg_res_in_use(sfp)) {
	    sg_remove_request(sfp, srp);
	    return -EBUSY;	/* reserve buffer already being used */
	}
    }
    timeout = sg_ms_to_jif(srp->header.timeout);
    if ((! hp->cmdp) || (hp->cmd_len < 6) || (hp->cmd_len > sizeof(cmnd))) {
	sg_remove_request(sfp, srp);
	return -EMSGSIZE;
    }
    if ((k = verify_area(VERIFY_READ, hp->cmdp, hp->cmd_len))) {
	sg_remove_request(sfp, srp);
	return k;  /* protects following copy_from_user()s + get_user()s */
    }
    __copy_from_user(cmnd, hp->cmdp, hp->cmd_len);
    if (read_only &&
	(! sg_allow_access(cmnd[0], sfp->parentdp->device->type))) {
	sg_remove_request(sfp, srp);
	return -EPERM;
    }
    k = sg_common_write(sfp, srp, cmnd, timeout, blocking);
    if (k < 0) return k;
    if (o_srp) *o_srp = srp;
    return count;
}

static int sg_common_write(Sg_fd * sfp, Sg_request * srp,
			   unsigned char * cmnd, int timeout, int blocking)
{
    int                   k;
    Scsi_Request        * SRpnt;
    Sg_device           * sdp = sfp->parentdp;
    sg_io_hdr_t         * hp = &srp->header;
    request_queue_t	* q;

    srp->data.cmd_opcode = cmnd[0];  /* hold opcode of command */
    hp->status = 0;
    hp->masked_status = 0;
    hp->msg_status = 0;
    hp->info = 0;
    hp->host_status = 0;
    hp->driver_status = 0;
    hp->resid = 0;
    SCSI_LOG_TIMEOUT(4,
	printk("sg_common_write:  scsi opcode=0x%02x, cmd_size=%d\n",
	       (int)cmnd[0], (int)hp->cmd_len));

    if ((k = sg_start_req(srp))) {
	SCSI_LOG_TIMEOUT(1, printk("sg_write: start_req err=%d\n", k));
	sg_finish_rem_req(srp);
        return k;    /* probably out of space --> ENOMEM */
    }
    if ((k = sg_write_xfer(srp))) {
	SCSI_LOG_TIMEOUT(1, printk("sg_write: write_xfer, bad address\n"));
	sg_finish_rem_req(srp);
	return k;
    }
    if (sdp->detached) {
    	sg_finish_rem_req(srp);
    	return -ENODEV;
    }
    SRpnt = scsi_allocate_request(sdp->device);
    if(SRpnt == NULL) {
    	SCSI_LOG_TIMEOUT(1, printk("sg_write: no mem\n"));
    	sg_finish_rem_req(srp);
    	return -ENOMEM;
    }

    srp->my_cmdp = SRpnt;
    q = &SRpnt->sr_device->request_queue;
    SRpnt->sr_request.rq_dev = sdp->i_rdev;
    SRpnt->sr_request.rq_status = RQ_ACTIVE;
    SRpnt->sr_sense_buffer[0] = 0;
    SRpnt->sr_cmd_len = hp->cmd_len;
    if (! (hp->flags & SG_FLAG_LUN_INHIBIT)) {
	if (sdp->device->scsi_level <= SCSI_2)
	    cmnd[1] = (cmnd[1] & 0x1f) | (sdp->device->lun << 5);
    }
    SRpnt->sr_use_sg = srp->data.k_use_sg;
    SRpnt->sr_sglist_len = srp->data.sglist_len;
    SRpnt->sr_bufflen = srp->data.bufflen;
    SRpnt->sr_underflow = 0;
    SRpnt->sr_buffer = srp->data.buffer;
    switch (hp->dxfer_direction) {
    case SG_DXFER_TO_FROM_DEV:
    case SG_DXFER_FROM_DEV:
	SRpnt->sr_data_direction = SCSI_DATA_READ; break;
    case SG_DXFER_TO_DEV:
	SRpnt->sr_data_direction = SCSI_DATA_WRITE; break;
    case SG_DXFER_UNKNOWN:
	SRpnt->sr_data_direction = SCSI_DATA_UNKNOWN; break;
    default:
	SRpnt->sr_data_direction = SCSI_DATA_NONE; break;
    }
    srp->data.k_use_sg = 0;
    srp->data.sglist_len = 0;
    srp->data.bufflen = 0;
    srp->data.buffer = NULL;
    hp->duration = jiffies;	/* unit jiffies now, millisecs after done */
/* Now send everything of to mid-level. The next time we hear about this
   packet is when sg_cmd_done_bh() is called (i.e. a callback). */
    scsi_do_req(SRpnt, (void *)cmnd,
		(void *)SRpnt->sr_buffer, hp->dxfer_len,
		sg_cmd_done_bh, timeout, SG_DEFAULT_RETRIES);
    /* dxfer_len overwrites SRpnt->sr_bufflen, hence need for b_malloc_len */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,1)
    generic_unplug_device(q);
#endif
    return 0;
}

static int sg_ioctl(struct inode * inode, struct file * filp,
                    unsigned int cmd_in, unsigned long arg)
{
    int result, val, read_only;
    Sg_device * sdp;
    Sg_fd * sfp;
    Sg_request * srp;
    unsigned long iflags;

    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp)))
        return -ENXIO;
    SCSI_LOG_TIMEOUT(3, printk("sg_ioctl: dev=%d, cmd=0x%x\n",
                               MINOR(sdp->i_rdev), (int)cmd_in));
    read_only = (O_RDWR != (filp->f_flags & O_ACCMODE));

    switch(cmd_in)
    {
    case SG_IO:
	{
	    int blocking = 1;   /* ignore O_NONBLOCK flag */

	    if (sdp->detached)
		return -ENODEV;
	    if(! scsi_block_when_processing_errors(sdp->device) )
		return -ENXIO;
	    result = verify_area(VERIFY_WRITE, (void *)arg, SZ_SG_IO_HDR);
	    if (result) return result;
	    result = sg_new_write(sfp, (const char *)arg, SZ_SG_IO_HDR,
				  blocking, read_only, &srp);
	    if (result < 0) return result;
	    srp->sg_io_owned = 1;
	    while (1) {
		result = 0;  /* following macro to beat race condition */
		__wait_event_interruptible(sfp->read_wait,
		       (sdp->detached || sfp->closed || srp->done), result);
		if (sdp->detached)
		    return -ENODEV;
		if (sfp->closed)
		    return 0;       /* request packet dropped already */
		if (0 == result)
		    break;
		srp->orphan = 1;
		return result; /* -ERESTARTSYS because signal hit process */
	    }
	    srp->done = 2;
	    result = sg_new_read(sfp, (char *)arg, SZ_SG_IO_HDR, srp);
	    return (result < 0) ? result : 0;
	}
    case SG_SET_TIMEOUT:
        result =  get_user(val, (int *)arg);
        if (result) return result;
        if (val < 0)
            return -EIO;
        sfp->timeout = val;
        return 0;
    case SG_GET_TIMEOUT:  /* N.B. User receives timeout as return value */
        return sfp->timeout; /* strange ..., for backward compatibility */
    case SG_SET_FORCE_LOW_DMA:
        result = get_user(val, (int *)arg);
        if (result) return result;
        if (val) {
            sfp->low_dma = 1;
            if ((0 == sfp->low_dma) && (0 == sg_res_in_use(sfp))) {
                val = (int)sfp->reserve.bufflen;
                sg_remove_scat(&sfp->reserve);
                sg_build_reserve(sfp, val);
            }
        }
        else {
	    if (sdp->detached)
		return -ENODEV;
            sfp->low_dma = sdp->device->host->unchecked_isa_dma;
	}
        return 0;
    case SG_GET_LOW_DMA:
        return put_user((int)sfp->low_dma, (int *)arg);
    case SG_GET_SCSI_ID:
	result = verify_area(VERIFY_WRITE, (void *)arg, sizeof(sg_scsi_id_t));
        if (result) return result;
        else {
	    sg_scsi_id_t * sg_idp = (sg_scsi_id_t *)arg;

	    if (sdp->detached)
		return -ENODEV;
            __put_user((int)sdp->device->host->host_no, &sg_idp->host_no);
            __put_user((int)sdp->device->channel, &sg_idp->channel);
            __put_user((int)sdp->device->id, &sg_idp->scsi_id);
            __put_user((int)sdp->device->lun, &sg_idp->lun);
            __put_user((int)sdp->device->type, &sg_idp->scsi_type);
	    __put_user((short)sdp->device->host->cmd_per_lun,
                       &sg_idp->h_cmd_per_lun);
	    __put_user((short)sdp->device->queue_depth,
                       &sg_idp->d_queue_depth);
	    __put_user(0, &sg_idp->unused[0]);
	    __put_user(0, &sg_idp->unused[1]);
            return 0;
        }
    case SG_SET_FORCE_PACK_ID:
        result = get_user(val, (int *)arg);
        if (result) return result;
        sfp->force_packid = val ? 1 : 0;
        return 0;
    case SG_GET_PACK_ID:
        result = verify_area(VERIFY_WRITE, (void *) arg, sizeof(int));
        if (result) return result;
	read_lock_irqsave(&sfp->rq_list_lock, iflags);
	for (srp = sfp->headrp; srp; srp = srp->nextrp) {
	    if ((1 == srp->done) && (! srp->sg_io_owned)) {
		read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
                __put_user(srp->header.pack_id, (int *)arg);
                return 0;
            }
        }
	read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
        __put_user(-1, (int *)arg);
        return 0;
    case SG_GET_NUM_WAITING:
	read_lock_irqsave(&sfp->rq_list_lock, iflags);
        for (val = 0, srp = sfp->headrp; srp; srp = srp->nextrp) {
	    if ((1 == srp->done) && (! srp->sg_io_owned))
                ++val;
        }
	read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
        return put_user(val, (int *)arg);
    case SG_GET_SG_TABLESIZE:
        return put_user(sdp->sg_tablesize, (int *)arg);
    case SG_SET_RESERVED_SIZE:
        result = get_user(val, (int *)arg);
        if (result) return result;
        if (val < 0)
            return -EINVAL;
        if (val != sfp->reserve.bufflen) {
            if (sg_res_in_use(sfp) || sfp->mmap_called)
                return -EBUSY;
            sg_remove_scat(&sfp->reserve);
            sg_build_reserve(sfp, val);
        }
        return 0;
    case SG_GET_RESERVED_SIZE:
        val = (int)sfp->reserve.bufflen;
        return put_user(val, (int *)arg);
    case SG_SET_COMMAND_Q:
        result = get_user(val, (int *)arg);
        if (result) return result;
        sfp->cmd_q = val ? 1 : 0;
        return 0;
    case SG_GET_COMMAND_Q:
        return put_user((int)sfp->cmd_q, (int *)arg);
    case SG_SET_KEEP_ORPHAN:
        result = get_user(val, (int *)arg);
        if (result) return result;
	sfp->keep_orphan = val;
        return 0;
    case SG_GET_KEEP_ORPHAN:
	return put_user((int)sfp->keep_orphan, (int *)arg);
    case SG_NEXT_CMD_LEN:
        result = get_user(val, (int *)arg);
        if (result) return result;
        sfp->next_cmd_len = (val > 0) ? val : 0;
        return 0;
    case SG_GET_VERSION_NUM:
        return put_user(sg_version_num, (int *)arg);
    case SG_GET_ACCESS_COUNT:
    	val = (sdp->device ? sdp->device->access_count : 0);
	return put_user(val, (int *)arg);
    case SG_GET_REQUEST_TABLE:
	result = verify_area(VERIFY_WRITE, (void *) arg,
			     SZ_SG_REQ_INFO * SG_MAX_QUEUE);
	if (result) return result;
	else {
	    sg_req_info_t rinfo[SG_MAX_QUEUE];
	    Sg_request * srp;
	    read_lock_irqsave(&sfp->rq_list_lock, iflags);
	    for (srp = sfp->headrp, val = 0; val < SG_MAX_QUEUE;
		 ++val, srp = srp ? srp->nextrp : srp) {
		memset(&rinfo[val], 0, SZ_SG_REQ_INFO);
		if (srp) {
		    rinfo[val].req_state = srp->done + 1;
		    rinfo[val].problem = srp->header.masked_status &
			srp->header.host_status & srp->header.driver_status;
		    rinfo[val].duration = srp->done ?
			    srp->header.duration :
			    sg_jif_to_ms(jiffies - srp->header.duration);
		    rinfo[val].orphan = srp->orphan;
		    rinfo[val].sg_io_owned = srp->sg_io_owned;
		    rinfo[val].pack_id = srp->header.pack_id;
		    rinfo[val].usr_ptr = srp->header.usr_ptr;
		}
	    }
	    read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
	    __copy_to_user((void *)arg, rinfo, SZ_SG_REQ_INFO * SG_MAX_QUEUE);
	    return 0;
	}
    case SG_EMULATED_HOST:
	if (sdp->detached)
	    return -ENODEV;
        return put_user(sdp->device->host->hostt->emulated, (int *)arg);
    case SG_SCSI_RESET:
	if (sdp->detached)
	    return -ENODEV;
        if (filp->f_flags & O_NONBLOCK) {
	    if (sdp->device->host->in_recovery)
		return -EBUSY;
	}
	else if (! scsi_block_when_processing_errors(sdp->device))
            return -EBUSY;
        result = get_user(val, (int *)arg);
        if (result) return result;
	if (SG_SCSI_RESET_NOTHING == val)
	    return 0;
#ifdef SCSI_TRY_RESET_DEVICE
	switch (val)
	{
	case SG_SCSI_RESET_DEVICE:
	    val = SCSI_TRY_RESET_DEVICE;
	    break;
	case SG_SCSI_RESET_BUS:
	    val = SCSI_TRY_RESET_BUS;
	    break;
	case SG_SCSI_RESET_HOST:
	    val = SCSI_TRY_RESET_HOST;
	    break;
	default:
	    return -EINVAL;
	}
	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
	    return -EACCES;
	return (scsi_reset_provider(sdp->device, val) == SUCCESS) ? 0 : -EIO;
#else
	SCSI_LOG_TIMEOUT(1, printk("sg_ioctl: SG_RESET_SCSI not supported\n"));
	result = -EINVAL;
#endif
    case SCSI_IOCTL_SEND_COMMAND:
	if (sdp->detached)
	    return -ENODEV;
	if (read_only) {
	    unsigned char opcode = WRITE_6;
	    Scsi_Ioctl_Command * siocp = (void *)arg;

	    copy_from_user(&opcode, siocp->data, 1);
	    if (! sg_allow_access(opcode, sdp->device->type))
		return -EPERM;
	}
        return scsi_ioctl_send_command(sdp->device, (void *)arg);
    case SG_SET_DEBUG:
        result = get_user(val, (int *)arg);
        if (result) return result;
        sdp->sgdebug = (char)val;
        return 0;
    case SCSI_IOCTL_GET_IDLUN:
    case SCSI_IOCTL_GET_BUS_NUMBER:
    case SCSI_IOCTL_PROBE_HOST:
    case SG_GET_TRANSFORM:
	if (sdp->detached)
	    return -ENODEV;
        return scsi_ioctl(sdp->device, cmd_in, (void *)arg);
    default:
	if (read_only)
            return -EPERM; /* don't know so take safe approach */
        return scsi_ioctl(sdp->device, cmd_in, (void *)arg);
    }
}

static unsigned int sg_poll(struct file * filp, poll_table * wait)
{
    unsigned int res = 0;
    Sg_device * sdp;
    Sg_fd * sfp;
    Sg_request * srp;
    int count = 0;
    unsigned long iflags;

    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp))
    	|| sfp->closed)
        return POLLERR;
    poll_wait(filp, &sfp->read_wait, wait);
    read_lock_irqsave(&sfp->rq_list_lock, iflags);
    for (srp = sfp->headrp; srp; srp = srp->nextrp) {   
    	/* if any read waiting, flag it */
	if ((0 == res) && (1 == srp->done) && (! srp->sg_io_owned))
            res = POLLIN | POLLRDNORM;
        ++count;
    }
    read_unlock_irqrestore(&sfp->rq_list_lock, iflags);

    if (sdp->detached)
	res |= POLLHUP;
    else if (! sfp->cmd_q) {
        if (0 == count)
            res |= POLLOUT | POLLWRNORM;
    }
    else if (count < SG_MAX_QUEUE)
        res |= POLLOUT | POLLWRNORM;
    SCSI_LOG_TIMEOUT(3, printk("sg_poll: dev=%d, res=0x%x\n",
                        MINOR(sdp->i_rdev), (int)res));
    return res;
}

static int sg_fasync(int fd, struct file * filp, int mode)
{
    int retval;
    Sg_device * sdp;
    Sg_fd * sfp;

    if ((! (sfp = (Sg_fd *)filp->private_data)) || (! (sdp = sfp->parentdp)))
        return -ENXIO;
    SCSI_LOG_TIMEOUT(3, printk("sg_fasync: dev=%d, mode=%d\n",
                               MINOR(sdp->i_rdev), mode));

    retval = fasync_helper(fd, filp, mode, &sfp->async_qp);
    return (retval < 0) ? retval : 0;
}

static void sg_rb_correct4mmap(Sg_scatter_hold * rsv_schp, int startFinish)
{
    void * page_ptr;
    struct page * page;
    int k, m;

    SCSI_LOG_TIMEOUT(3, printk("sg_rb_correct4mmap: startFinish=%d, "
			   "scatg=%d\n", startFinish, rsv_schp->k_use_sg)); 
    /* N.B. correction _not_ applied to base page of aech allocation */
    if (rsv_schp->k_use_sg) { /* reserve buffer is a scatter gather list */
        struct scatterlist * sclp = rsv_schp->buffer;

        for (k = 0; k < rsv_schp->k_use_sg; ++k, ++sclp) {
	    for (m = PAGE_SIZE; m < sclp->length; m += PAGE_SIZE) {
		page_ptr = (unsigned char *)sclp->address + m;
		page = virt_to_page(page_ptr);
		if (startFinish)
		    get_page(page);	/* increment page count */
		else {
		    if (page_count(page) > 0)
			put_page_testzero(page); /* decrement page count */
		}
	    }
        }
    }
    else { /* reserve buffer is just a single allocation */
	for (m = PAGE_SIZE; m < rsv_schp->bufflen; m += PAGE_SIZE) {
	    page_ptr = (unsigned char *)rsv_schp->buffer + m;
	    page = virt_to_page(page_ptr);
	    if (startFinish)
		get_page(page);	/* increment page count */
	    else {
		if (page_count(page) > 0)
		    put_page_testzero(page); /* decrement page count */
	    }
	}
    }
}

static struct page * sg_vma_nopage(struct vm_area_struct *vma, 
				   unsigned long addr, int unused)
{
    Sg_fd * sfp;
    struct page * page = NOPAGE_SIGBUS;
    void * page_ptr = NULL;
    unsigned long offset;
    Sg_scatter_hold * rsv_schp;

    if ((NULL == vma) || (! (sfp = (Sg_fd *)vma->vm_private_data)))
	return page;
    rsv_schp = &sfp->reserve;
    offset = addr - vma->vm_start;
    if (offset >= rsv_schp->bufflen)
	return page;
    SCSI_LOG_TIMEOUT(3, printk("sg_vma_nopage: offset=%lu, scatg=%d\n", 
			       offset, rsv_schp->k_use_sg));
    if (rsv_schp->k_use_sg) { /* reserve buffer is a scatter gather list */
        int k;
        unsigned long sa = vma->vm_start;
        unsigned long len;
        struct scatterlist * sclp = rsv_schp->buffer;

        for (k = 0; (k < rsv_schp->k_use_sg) && (sa < vma->vm_end);
             ++k, ++sclp) {
            len = vma->vm_end - sa;
            len = (len < sclp->length) ? len : sclp->length;
	    if (offset < len) {
		page_ptr = (unsigned char *)sclp->address + offset;
		page = virt_to_page(page_ptr);
		get_page(page);	/* increment page count */
		break;
	    }
            sa += len;
	    offset -= len;
        }
    }
    else { /* reserve buffer is just a single allocation */
        page_ptr = (unsigned char *)rsv_schp->buffer + offset;
	page = virt_to_page(page_ptr);
	get_page(page);	/* increment page count */
    }
    return page;
}

static struct vm_operations_struct sg_mmap_vm_ops = {
    nopage : sg_vma_nopage,
};

static int sg_mmap(struct file * filp, struct vm_area_struct *vma)
{
    Sg_fd * sfp;
    unsigned long req_sz = vma->vm_end - vma->vm_start;
    Sg_scatter_hold * rsv_schp;

    if ((! filp) || (! vma) || (! (sfp = (Sg_fd *)filp->private_data)))
        return -ENXIO;
    SCSI_LOG_TIMEOUT(3, printk("sg_mmap starting, vm_start=%p, len=%d\n", 
			       (void *)vma->vm_start, (int)req_sz));
    if (vma->vm_pgoff)
    	return -EINVAL;		/* want no offset */
    rsv_schp = &sfp->reserve;
    if (req_sz > rsv_schp->bufflen)
    	return -ENOMEM;		/* cannot map more than reserved buffer */

    if (rsv_schp->k_use_sg) { /* reserve buffer is a scatter gather list */
    	int k;
	unsigned long sa = vma->vm_start;
	unsigned long len;
	struct scatterlist * sclp = rsv_schp->buffer;

	for (k = 0; (k < rsv_schp->k_use_sg) && (sa < vma->vm_end); 
	     ++k, ++sclp) {
	    if ((unsigned long)sclp->address & (PAGE_SIZE - 1))
	    	return -EFAULT;     /* non page aligned memory ?? */
	    len = vma->vm_end - sa;
	    len = (len < sclp->length) ? len : sclp->length;
	    sa += len;
	}
    }
    else { /* reserve buffer is just a single allocation */
    	if ((unsigned long)rsv_schp->buffer & (PAGE_SIZE - 1))
	    return -EFAULT;	/* non page aligned memory ?? */
    }
    if (0 == sfp->mmap_called) {
    	sg_rb_correct4mmap(rsv_schp, 1);  /* do only once per fd lifetime */
	sfp->mmap_called = 1;
    }
    vma->vm_flags |= (VM_RESERVED | VM_IO);
    vma->vm_private_data = sfp;
    vma->vm_ops = &sg_mmap_vm_ops;
    return 0;
}

/* This function is a "bottom half" handler that is called by the
 * mid level when a command is completed (or has failed). */
static void sg_cmd_done_bh(Scsi_Cmnd * SCpnt)
{
    Scsi_Request * SRpnt = SCpnt->sc_request;
    int dev = MINOR(SRpnt->sr_request.rq_dev);
    Sg_device * sdp = NULL;
    Sg_fd * sfp;
    Sg_request * srp = NULL;

    read_lock(&sg_dev_arr_lock);
    if (sg_dev_arr && (dev >= 0)) {
	if (dev < sg_template.dev_max)
	    sdp = sg_dev_arr[dev];
    }
    if ((NULL == sdp) || sdp->detached) {
	read_unlock(&sg_dev_arr_lock);
	SCSI_LOG_TIMEOUT(1, printk("sg...bh: dev=%d gone\n", dev));
        scsi_release_request(SRpnt);
        SRpnt = NULL;
        return;
    }
    sfp = sdp->headfp;
    while (sfp) {
	read_lock(&sfp->rq_list_lock);
	for (srp = sfp->headrp; srp; srp = srp->nextrp) {
            if (SRpnt == srp->my_cmdp)
                break;
        }
	read_unlock(&sfp->rq_list_lock);
        if (srp)
            break;
        sfp = sfp->nextfp;
    }
    if (! srp) {
	read_unlock(&sg_dev_arr_lock);
	SCSI_LOG_TIMEOUT(1, printk("sg...bh: req missing, dev=%d\n", dev));
        scsi_release_request(SRpnt);
        SRpnt = NULL;
        return;
    }
    /* First transfer ownership of data buffers to sg_device object. */
    srp->data.k_use_sg = SRpnt->sr_use_sg;
    srp->data.sglist_len = SRpnt->sr_sglist_len;
    srp->data.bufflen = SRpnt->sr_bufflen;
    srp->data.buffer = SRpnt->sr_buffer;
    /* now clear out request structure */
    SRpnt->sr_use_sg = 0;
    SRpnt->sr_sglist_len = 0;
    SRpnt->sr_bufflen = 0;
    SRpnt->sr_buffer = NULL;
    SRpnt->sr_underflow = 0;
    SRpnt->sr_request.rq_dev = MKDEV(0, 0);  /* "sg" _disowns_ request blk */

    srp->my_cmdp = NULL;
    srp->done = 1;
    read_unlock(&sg_dev_arr_lock);

    SCSI_LOG_TIMEOUT(4, printk("sg...bh: dev=%d, pack_id=%d, res=0x%x\n",
		     dev, srp->header.pack_id, (int)SRpnt->sr_result));
    srp->header.resid = SCpnt->resid;
    /* sg_unmap_and(&srp->data, 0); */     /* unmap locked pages a.s.a.p. */
    /* N.B. unit of duration changes here from jiffies to millisecs */
    srp->header.duration = sg_jif_to_ms(jiffies - (int)srp->header.duration);
    if (0 != SRpnt->sr_result) {
	memcpy(srp->sense_b, SRpnt->sr_sense_buffer, sizeof(srp->sense_b));
	srp->header.status = 0xff & SRpnt->sr_result;
	srp->header.masked_status  = status_byte(SRpnt->sr_result);
	srp->header.msg_status  = msg_byte(SRpnt->sr_result);
	srp->header.host_status = host_byte(SRpnt->sr_result);
	srp->header.driver_status = driver_byte(SRpnt->sr_result);
	if ((sdp->sgdebug > 0) &&
	    ((CHECK_CONDITION == srp->header.masked_status) ||
	     (COMMAND_TERMINATED == srp->header.masked_status)))
	    print_req_sense("sg_cmd_done_bh", SRpnt);

	/* Following if statement is a patch supplied by Eric Youngdale */
	if (driver_byte(SRpnt->sr_result) != 0
	    && (SRpnt->sr_sense_buffer[0] & 0x7f) == 0x70
	    && (SRpnt->sr_sense_buffer[2] & 0xf) == UNIT_ATTENTION
	    && sdp->device->removable) {
	    /* Detected disc change. Set the bit - this may be used if */
	    /* there are filesystems using this device. */
	    sdp->device->changed = 1;
	}
    }
    /* Rely on write phase to clean out srp status values, so no "else" */

    scsi_release_request(SRpnt);
    SRpnt = NULL;
    if (sfp->closed) { /* whoops this fd already released, cleanup */
        SCSI_LOG_TIMEOUT(1,
	       printk("sg...bh: already closed, freeing ...\n"));
	sg_finish_rem_req(srp);
	srp = NULL;
	if (NULL == sfp->headrp) {
            SCSI_LOG_TIMEOUT(1,
		printk("sg...bh: already closed, final cleanup\n"));
            if (0 == sg_remove_sfp(sdp, sfp)) { /* device still present */
		sdp->device->access_count--;
		if (sdp->device->host->hostt->module)
		    __MOD_DEC_USE_COUNT(sdp->device->host->hostt->module);
	    }
	    if (sg_template.module)
		    __MOD_DEC_USE_COUNT(sg_template.module);
	    sfp = NULL;
        }
    }
    else if (srp && srp->orphan) {
	if (sfp->keep_orphan)
	    srp->sg_io_owned = 0;
	else {
	    sg_finish_rem_req(srp);
	    srp = NULL;
        }
    }
    if (sfp && srp) {
	/* Now wake up any sg_read() that is waiting for this packet. */
	wake_up_interruptible(&sfp->read_wait);
	kill_fasync(&sfp->async_qp, SIGPOLL, POLL_IN);
    }
}

static struct file_operations sg_fops = {
	owner:		THIS_MODULE,
	read:		sg_read,
	write:		sg_write,
	poll:		sg_poll,
	ioctl:		sg_ioctl,
	open:		sg_open,
	mmap:		sg_mmap,
	release:	sg_release,
	fasync:		sg_fasync,
};


static int sg_detect(Scsi_Device * scsidp)
{
    sg_template.dev_noticed++;
    return 1;
}

/* Driver initialization */
static int sg_init()
{
    static int sg_registered = 0;
    unsigned long iflags;

    if ((sg_template.dev_noticed == 0) || sg_dev_arr)
    	return 0;

    write_lock_irqsave(&sg_dev_arr_lock, iflags);
    if(!sg_registered) {
	if (devfs_register_chrdev(SCSI_GENERIC_MAJOR,"sg",&sg_fops))
        {
            printk(KERN_ERR "Unable to get major %d for generic SCSI device\n",
                   SCSI_GENERIC_MAJOR);
	    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
            sg_template.dev_noticed = 0;
            return 1;
        }
        sg_registered++;
    }

    SCSI_LOG_TIMEOUT(3, printk("sg_init\n"));
    sg_template.dev_max = sg_template.dev_noticed + SG_DEV_ARR_LUMP;
    sg_dev_arr = (Sg_device **)kmalloc(sg_template.dev_max * 
    					sizeof(Sg_device *), GFP_ATOMIC);
    if (NULL == sg_dev_arr) {
        printk(KERN_ERR "sg_init: no space for sg_dev_arr\n");
	write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
        sg_template.dev_noticed = 0;
        return 1;
    }
    memset(sg_dev_arr, 0, sg_template.dev_max * sizeof(Sg_device *));
    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
#ifdef CONFIG_PROC_FS
    sg_proc_init();
#endif  /* CONFIG_PROC_FS */
    return 0;
}

#ifndef MODULE
static int __init sg_def_reserved_size_setup(char *str)
{
    int tmp;

    if (get_option(&str, &tmp) == 1) {
	def_reserved_size = tmp;
	if (tmp >= 0)
	    sg_big_buff = tmp;
	return 1;
    } else {
	printk(KERN_WARNING "sg_def_reserved_size : usage "
	    "sg_def_reserved_size=n (n could be 65536, 131072 or 262144)\n");
	return 0;
    }
}

__setup("sg_def_reserved_size=", sg_def_reserved_size_setup);
#endif


static int sg_attach(Scsi_Device * scsidp)
{
    Sg_device * sdp;
    unsigned long iflags;
    int k;

    write_lock_irqsave(&sg_dev_arr_lock, iflags);
    if (sg_template.nr_dev >= sg_template.dev_max) { /* try to resize */
    	Sg_device ** tmp_da;
	int tmp_dev_max = sg_template.nr_dev + SG_DEV_ARR_LUMP;

	tmp_da = (Sg_device **)kmalloc(tmp_dev_max * 
    					sizeof(Sg_device *), GFP_ATOMIC);
	if (NULL == tmp_da) {
	    scsidp->attached--;
	    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
	    printk(KERN_ERR "sg_attach: device array cannot be resized\n");
	    return 1;
	}
	memset(tmp_da, 0, tmp_dev_max * sizeof(Sg_device *));
	memcpy(tmp_da, sg_dev_arr, sg_template.dev_max * sizeof(Sg_device *));
	kfree((char *)sg_dev_arr);
	sg_dev_arr = tmp_da;
	sg_template.dev_max = tmp_dev_max;
    }

    for(k = 0; k < sg_template.dev_max; k++)
        if(! sg_dev_arr[k]) break;
    if (k > MINORMASK) {
	scsidp->attached--;
	write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
	printk(KERN_WARNING "Unable to attach sg device <%d, %d, %d, %d>"
	       " type=%d, minor number exceed %d\n", scsidp->host->host_no, 
	       scsidp->channel, scsidp->id, scsidp->lun, scsidp->type,
	       MINORMASK);
	return 1;
    }
    if(k < sg_template.dev_max)
    	sdp = (Sg_device *)kmalloc(sizeof(Sg_device), GFP_ATOMIC);
    else
    	sdp = NULL;
    if (NULL == sdp) {
	scsidp->attached--;
	write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
	printk(KERN_ERR "sg_attach: Sg_device cannot be allocated\n");
	return 1;
    }

    SCSI_LOG_TIMEOUT(3, printk("sg_attach: dev=%d \n", k));
    sdp->device = scsidp;
    init_waitqueue_head(&sdp->o_excl_wait);
    sdp->headfp= NULL;
    sdp->exclude = 0;
    sdp->sgdebug = 0;
    sdp->detached = 0;
    sdp->sg_tablesize = scsidp->host ? scsidp->host->sg_tablesize : 0;
    sdp->i_rdev = MKDEV(SCSI_GENERIC_MAJOR, k);
    sdp->de = devfs_register (scsidp->de, "generic", DEVFS_FL_DEFAULT,
                             SCSI_GENERIC_MAJOR, k,
                             S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP,
                             &sg_fops, sdp);
    sg_template.nr_dev++;
    sg_dev_arr[k] = sdp;
    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    switch (scsidp->type) {
	case TYPE_DISK:
	case TYPE_MOD:
	case TYPE_ROM:
	case TYPE_WORM:
	case TYPE_TAPE: break;
	default:
	    printk(KERN_NOTICE "Attached scsi generic sg%d at scsi%d, channel"
	    	   " %d, id %d, lun %d,  type %d\n", k, scsidp->host->host_no, 
		   scsidp->channel, scsidp->id, scsidp->lun, scsidp->type);
    }
    return 0;
}

/* Called at 'finish' of init process, after all attaches */
static void sg_finish(void)
{ }

static void sg_detach(Scsi_Device * scsidp)
{
    Sg_device * sdp;
    unsigned long iflags;
    Sg_fd * sfp;
    Sg_fd * tsfp;
    Sg_request * srp;
    Sg_request * tsrp;
    int k, delay;

    if (NULL == sg_dev_arr)
    	return;
    delay = 0;
    write_lock_irqsave(&sg_dev_arr_lock, iflags);
    for (k = 0; k < sg_template.dev_max; k++) {
    	sdp = sg_dev_arr[k];
        if ((NULL == sdp) || (sdp->device != scsidp))
            continue;   /* dirty but lowers nesting */
        if (sdp->headfp) {
	    sdp->detached = 1;
	    for (sfp = sdp->headfp; sfp; sfp = tsfp) {
	    	tsfp = sfp->nextfp;
		for (srp = sfp->headrp; srp; srp = tsrp) {
		    tsrp = srp->nextrp;
		    if (sfp->closed || (0 == srp->done))
			sg_finish_rem_req(srp);
		}
		if (sfp->closed) {
		    sdp->device->access_count--;
		    if (sg_template.module)
			__MOD_DEC_USE_COUNT(sg_template.module);
		    if (sdp->device->host->hostt->module)
			__MOD_DEC_USE_COUNT(sdp->device->host->hostt->module);
		    __sg_remove_sfp(sdp, sfp);
		}
		else {
		    delay = 1;
		    wake_up_interruptible(&sfp->read_wait);
		    kill_fasync(&sfp->async_qp, SIGPOLL, POLL_HUP);
		}
            }
	    SCSI_LOG_TIMEOUT(3, printk("sg_detach: dev=%d, dirty\n", k));
	    devfs_unregister (sdp->de);
	    sdp->de = NULL;
	    if (NULL == sdp->headfp) {
		kfree((char *)sdp);
		sg_dev_arr[k] = NULL;
	    }
        }
        else { /* nothing active, simple case */
            SCSI_LOG_TIMEOUT(3, printk("sg_detach: dev=%d\n", k));
	    devfs_unregister (sdp->de);
	    kfree((char *)sdp);
	    sg_dev_arr[k] = NULL;
        }
        scsidp->attached--;
        sg_template.nr_dev--;
        sg_template.dev_noticed--;	/* from <dan@lectra.fr> */
        break;
    }
    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    if (delay)
	scsi_sleep(2);	/* dirty detach so delay device destruction */
}

MODULE_AUTHOR("Douglas Gilbert");
MODULE_DESCRIPTION("SCSI generic (sg) driver");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

MODULE_PARM(def_reserved_size, "i");
MODULE_PARM_DESC(def_reserved_size, "size of buffer reserved for each fd");

static int __init init_sg(void) {
    if (def_reserved_size >= 0)
	sg_big_buff = def_reserved_size;
    sg_template.module = THIS_MODULE;
    return scsi_register_module(MODULE_SCSI_DEV, &sg_template);
}

static void __exit exit_sg( void)
{
#ifdef CONFIG_PROC_FS
    sg_proc_cleanup();
#endif  /* CONFIG_PROC_FS */
    scsi_unregister_module(MODULE_SCSI_DEV, &sg_template);
    devfs_unregister_chrdev(SCSI_GENERIC_MAJOR, "sg");
    if(sg_dev_arr != NULL) {
	kfree((char *)sg_dev_arr);
        sg_dev_arr = NULL;
    }
    sg_template.dev_max = 0;
}


static int sg_start_req(Sg_request * srp)
{
    int res;
    Sg_fd * sfp = srp->parentfp;
    sg_io_hdr_t * hp = &srp->header;
    int dxfer_len = (int)hp->dxfer_len;
    int dxfer_dir = hp->dxfer_direction;
    Sg_scatter_hold * req_schp = &srp->data;
    Sg_scatter_hold * rsv_schp = &sfp->reserve;

    SCSI_LOG_TIMEOUT(4, printk("sg_start_req: dxfer_len=%d\n", dxfer_len));
    if ((dxfer_len <= 0) || (dxfer_dir == SG_DXFER_NONE))
    	return 0;
    if (sg_allow_dio && (hp->flags & SG_FLAG_DIRECT_IO) && 
	(dxfer_dir != SG_DXFER_UNKNOWN) && (0 == hp->iovec_count) &&
	(! sfp->parentdp->device->host->unchecked_isa_dma)) {
	res = sg_build_dir(srp, sfp, dxfer_len);
	if (res <= 0)   /* -ve -> error, 0 -> done, 1 -> try indirect */
	    return res;
    }
    if ((! sg_res_in_use(sfp)) && (dxfer_len <= rsv_schp->bufflen))
	sg_link_reserve(sfp, srp, dxfer_len);
    else {
	res = sg_build_indi(req_schp, sfp, dxfer_len);
        if (res) {
            sg_remove_scat(req_schp);
            return res;
        }
    }
    return 0;
}

static void sg_finish_rem_req(Sg_request * srp)
{
    Sg_fd * sfp = srp->parentfp;
    Sg_scatter_hold * req_schp = &srp->data;

    SCSI_LOG_TIMEOUT(4, printk("sg_finish_rem_req: res_used=%d\n",
			       (int)srp->res_used));
    sg_unmap_and(&srp->data, 1);
    if (srp->res_used)
        sg_unlink_reserve(sfp, srp);
    else
        sg_remove_scat(req_schp);
    sg_remove_request(sfp, srp);
}

static int sg_build_sgat(Sg_scatter_hold * schp, const Sg_fd * sfp,
			 int tablesize)
{
    int mem_src, ret_sz;
    int elem_sz = sizeof(struct scatterlist) + sizeof(char);
    /* scatter gather array, followed by mem_src_arr (array of chars) */
    int sg_bufflen = tablesize * elem_sz;
    int mx_sc_elems = tablesize;

    mem_src = SG_HEAP_KMAL;
    schp->buffer = sg_malloc(sfp, sg_bufflen, &ret_sz, &mem_src);
    if (! schp->buffer)
	return -ENOMEM;
    else if (ret_sz != sg_bufflen) {
	sg_bufflen = ret_sz;
	mx_sc_elems = sg_bufflen / elem_sz;
    }
    schp->buffer_mem_src = (char)mem_src;
    schp->sglist_len = sg_bufflen;
    memset(schp->buffer, 0, sg_bufflen);
    return mx_sc_elems; /* number of scat_gath elements allocated */
}

static void sg_unmap_and(Sg_scatter_hold * schp, int free_also)
{
#ifdef SG_ALLOW_DIO_CODE
    int nbhs = 0;

    if (schp && schp->kiobp) {
	if (schp->mapped) {
	    unmap_kiobuf(schp->kiobp);
	    schp->mapped = 0;
	}
	if (free_also) {
	    sg_free_kiovec(1, &schp->kiobp, &nbhs);
	    schp->kiobp = NULL;
	}
    }
#endif
}

static int sg_build_dir(Sg_request * srp, Sg_fd * sfp, int dxfer_len)
{
#ifdef SG_ALLOW_DIO_CODE
    int res, k, split, offset, num, mx_sc_elems, rem_sz;
    struct kiobuf * kp;
    char * mem_src_arr;
    struct scatterlist * sclp;
    unsigned long addr, prev_addr;
    sg_io_hdr_t * hp = &srp->header;
    Sg_scatter_hold * schp = &srp->data;
    int sg_tablesize = sfp->parentdp->sg_tablesize;
    int nbhs = 0;

    res = sg_alloc_kiovec(1, &schp->kiobp, &nbhs);
    if (0 != res) {
	SCSI_LOG_TIMEOUT(5, printk("sg_build_dir: sg_alloc_kiovec res=%d\n", 
			 res));
	return 1;
    }
    res = map_user_kiobuf((SG_DXFER_TO_DEV == hp->dxfer_direction) ? 1 : 0,
			  schp->kiobp, (unsigned long)hp->dxferp, dxfer_len);
    if (0 != res) {
	SCSI_LOG_TIMEOUT(5,
		printk("sg_build_dir: map_user_kiobuf res=%d\n", res));
	sg_unmap_and(schp, 1);
	return 1;
    }
    schp->mapped = 1;
    kp = schp->kiobp;
    prev_addr = (unsigned long) page_address(kp->maplist[0]);
    for (k = 1, split = 0; k < kp->nr_pages; ++k, prev_addr = addr) {
	addr = (unsigned long) page_address(kp->maplist[k]);
	if ((prev_addr + PAGE_SIZE) != addr) {
	    split = k;
	    break;
	}
    }
    if (! split) {
	schp->k_use_sg = 0;
	schp->buffer = page_address(kp->maplist[0]) + kp->offset;
	schp->bufflen = dxfer_len;
	schp->buffer_mem_src = SG_USER_MEM;
	schp->b_malloc_len = dxfer_len;
	hp->info |= SG_INFO_DIRECT_IO;
	return 0;
    }
    mx_sc_elems = sg_build_sgat(schp, sfp, sg_tablesize);
    if (mx_sc_elems <= 1) {
	sg_unmap_and(schp, 1);
	sg_remove_scat(schp);
	return 1;
    }
    mem_src_arr = schp->buffer + (mx_sc_elems * sizeof(struct scatterlist));
    for (k = 0, sclp = schp->buffer, rem_sz = dxfer_len;
	 (rem_sz > 0) && (k < mx_sc_elems);
	 ++k, ++sclp) {
	offset = (0 == k) ? kp->offset : 0;
	num = (rem_sz > (PAGE_SIZE - offset)) ? (PAGE_SIZE - offset) :
						rem_sz;
	sclp->address = page_address(kp->maplist[k]) + offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,13)
	sclp->page = NULL;
#endif
	sclp->length = num;
	mem_src_arr[k] = SG_USER_MEM;
	rem_sz -= num;
	SCSI_LOG_TIMEOUT(5,
	    printk("sg_build_dir: k=%d, a=0x%p, len=%d, ms=%d\n",
	    k, sclp->address, num, mem_src_arr[k]));
    }
    schp->k_use_sg = k;
    SCSI_LOG_TIMEOUT(5,
	printk("sg_build_dir: k_use_sg=%d, rem_sz=%d\n", k, rem_sz));
    schp->bufflen = dxfer_len;
    if (rem_sz > 0) {   /* must have failed */
	sg_unmap_and(schp, 1);
	sg_remove_scat(schp);
	return 1;   /* out of scatter gather elements, try indirect */
    }
    hp->info |= SG_INFO_DIRECT_IO;
    return 0;
#else
    return 1;
#endif /* SG_ALLOW_DIO_CODE */
}

static int sg_build_indi(Sg_scatter_hold * schp, Sg_fd * sfp, int buff_size)
{
    int ret_sz, mem_src;
    int blk_size = buff_size;
    char * p = NULL;

    if ((blk_size < 0) || (! sfp))
        return -EFAULT;
    if (0 == blk_size)
        ++blk_size;             /* don't know why */
/* round request up to next highest SG_SECTOR_SZ byte boundary */
    blk_size = (blk_size + SG_SECTOR_MSK) & (~SG_SECTOR_MSK);
    SCSI_LOG_TIMEOUT(4, printk("sg_build_indi: buff_size=%d, blk_size=%d\n",
                               buff_size, blk_size));
    if (blk_size <= SG_SCATTER_SZ) {
        mem_src = SG_HEAP_PAGE;
        p = sg_malloc(sfp, blk_size, &ret_sz, &mem_src);
        if (! p)
            return -ENOMEM;
        if (blk_size == ret_sz) { /* got it on the first attempt */
	    schp->k_use_sg = 0;
            schp->buffer = p;
            schp->bufflen = blk_size;
	    schp->buffer_mem_src = (char)mem_src;
            schp->b_malloc_len = blk_size;
            return 0;
        }
    }
    else {
        mem_src = SG_HEAP_PAGE;
        p = sg_malloc(sfp, SG_SCATTER_SZ, &ret_sz, &mem_src);
        if (! p)
            return -ENOMEM;
    }
/* Want some local declarations, so start new block ... */
    {   /* lets try and build a scatter gather list */
        struct scatterlist * sclp;
	int k, rem_sz, num;
	int mx_sc_elems;
        int sg_tablesize = sfp->parentdp->sg_tablesize;
        int first = 1;
	char * mem_src_arr;

        /* N.B. ret_sz and mem_src carried into this block ... */
	mx_sc_elems = sg_build_sgat(schp, sfp, sg_tablesize);
	if (mx_sc_elems < 0)
	    return mx_sc_elems; /* most likely -ENOMEM */
	mem_src_arr = schp->buffer +
		      (mx_sc_elems * sizeof(struct scatterlist));

	for (k = 0, sclp = schp->buffer, rem_sz = blk_size;
	     (rem_sz > 0) && (k < mx_sc_elems);
             ++k, rem_sz -= ret_sz, ++sclp) {
	    if (first)
                first = 0;
            else {
                num = (rem_sz > SG_SCATTER_SZ) ? SG_SCATTER_SZ : rem_sz;
                mem_src = SG_HEAP_PAGE;
                p = sg_malloc(sfp, num, &ret_sz, &mem_src);
                if (! p)
                    break;
            }
            sclp->address = p;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,13)
	    sclp->page = NULL;
#endif
            sclp->length = ret_sz;
	    mem_src_arr[k] = mem_src;

	    SCSI_LOG_TIMEOUT(5,
		printk("sg_build_build: k=%d, a=0x%p, len=%d, ms=%d\n",
                k, sclp->address, ret_sz, mem_src));
        } /* end of for loop */
	schp->k_use_sg = k;
	SCSI_LOG_TIMEOUT(5,
	    printk("sg_build_indi: k_use_sg=%d, rem_sz=%d\n", k, rem_sz));
        schp->bufflen = blk_size;
        if (rem_sz > 0)   /* must have failed */
            return -ENOMEM;
    }
    return 0;
}

static int sg_write_xfer(Sg_request * srp)
{
    sg_io_hdr_t * hp = &srp->header;
    Sg_scatter_hold * schp = &srp->data;
    int num_xfer = 0;
    int j, k, onum, usglen, ksglen, res, ok;
    int iovec_count = (int)hp->iovec_count;
    int dxfer_dir = hp->dxfer_direction;
    unsigned char * p;
    unsigned char * up;
    int new_interface = ('\0' == hp->interface_id) ? 0 : 1;

    if ((SG_DXFER_UNKNOWN == dxfer_dir) || (SG_DXFER_TO_DEV == dxfer_dir) ||
	(SG_DXFER_TO_FROM_DEV == dxfer_dir)) {
	num_xfer = (int)(new_interface ?  hp->dxfer_len : hp->flags);
	if (schp->bufflen < num_xfer)
	    num_xfer = schp->bufflen;
    }
    if ((num_xfer <= 0) || 
    	(new_interface && ((SG_FLAG_NO_DXFER | SG_FLAG_MMAP_IO) & hp->flags)))
	return 0;

    SCSI_LOG_TIMEOUT(4,
	 printk("sg_write_xfer: num_xfer=%d, iovec_count=%d, k_use_sg=%d\n",
		num_xfer, iovec_count, schp->k_use_sg));
    if (iovec_count) {
	onum = iovec_count;
	if ((k = verify_area(VERIFY_READ, hp->dxferp,
			     SZ_SG_IOVEC * onum)))
	    return k;
    }
    else
	onum = 1;

    if (0 == schp->k_use_sg) {  /* kernel has single buffer */
	if (SG_USER_MEM != schp->buffer_mem_src) { /* else nothing to do */

	    for (j = 0, p = schp->buffer; j < onum; ++j) {
		res = sg_u_iovec(hp, iovec_count, j, 1, &usglen, &up);
		if (res) return res;
		usglen = (num_xfer > usglen) ? usglen : num_xfer;
		__copy_from_user(p, up, usglen);
		p += usglen;
		num_xfer -= usglen;
		if (num_xfer <= 0)
		    return 0;
            }
	}
    }
    else {      /* kernel using scatter gather list */
	struct scatterlist * sclp = (struct scatterlist *)schp->buffer;
	char * mem_src_arr = sg_get_sgat_msa(schp);
	ksglen = (int)sclp->length;
	p = sclp->address;

	for (j = 0, k = 0; j < onum; ++j) {
	    res = sg_u_iovec(hp, iovec_count, j, 1, &usglen, &up);
	    if (res) return res;

	    for ( ; p; ++sclp, ksglen = (int)sclp->length, p = sclp->address) {
		ok = (SG_USER_MEM != mem_src_arr[k]);
		if (usglen <= 0)
		    break;
		if (ksglen > usglen) {
		    if (usglen >= num_xfer) {
			if (ok) __copy_from_user(p, up, num_xfer);
			return 0;
		    }
		    if (ok) __copy_from_user(p, up, usglen);
		    p += usglen;
		    ksglen -= usglen;
                    break;
		}
		else {
		    if (ksglen >= num_xfer) {
			if (ok) __copy_from_user(p, up, num_xfer);
			return 0;
		    }
		    if (ok) __copy_from_user(p, up, ksglen);
		    up += ksglen;
		    usglen -= ksglen;
		}
                ++k;
                if (k >= schp->k_use_sg)
                    return 0;
            }
        }
    }
    return 0;
}

static int sg_u_iovec(sg_io_hdr_t * hp, int sg_num, int ind,
		      int wr_xf, int * countp, unsigned char ** up)
{
    int num_xfer = (int)hp->dxfer_len;
    unsigned char * p;
    int count, k;
    sg_iovec_t u_iovec;

    if (0 == sg_num) {
	p = (unsigned char *)hp->dxferp;
	if (wr_xf && ('\0' == hp->interface_id))
	    count = (int)hp->flags; /* holds "old" input_size */
	else
	    count = num_xfer;
    }
    else {
	__copy_from_user(&u_iovec,
			 (unsigned char *)hp->dxferp + (ind * SZ_SG_IOVEC),
			 SZ_SG_IOVEC);
	p = (unsigned char *)u_iovec.iov_base;
	count = (int)u_iovec.iov_len;
    }
    if ((k = verify_area(wr_xf ? VERIFY_READ : VERIFY_WRITE, p, count)))
	return k;
    if (up) *up = p;
    if (countp) *countp = count;
    return 0;
}

static char * sg_get_sgat_msa(Sg_scatter_hold * schp)
{
    int elem_sz = sizeof(struct scatterlist) + sizeof(char);
    int mx_sc_elems = schp->sglist_len / elem_sz;
    return schp->buffer + (sizeof(struct scatterlist) * mx_sc_elems);
}

static void sg_remove_scat(Sg_scatter_hold * schp)
{
    SCSI_LOG_TIMEOUT(4, printk("sg_remove_scat: k_use_sg=%d\n",
			       schp->k_use_sg));
    if (schp->buffer && schp->sglist_len) {
        int k, mem_src;
        struct scatterlist * sclp = (struct scatterlist *)schp->buffer;
	char * mem_src_arr = sg_get_sgat_msa(schp);

	for (k = 0; (k < schp->k_use_sg) && sclp->address; ++k, ++sclp) {
	    mem_src = mem_src_arr[k];
	    SCSI_LOG_TIMEOUT(5,
		printk("sg_remove_scat: k=%d, a=0x%p, len=%d, ms=%d\n",
                       k, sclp->address, sclp->length, mem_src));
            sg_free(sclp->address, sclp->length, mem_src);
            sclp->address = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,13)
	    sclp->page = NULL;
#endif
            sclp->length = 0;
        }
	sg_free(schp->buffer, schp->sglist_len, schp->buffer_mem_src);
    }
    else if (schp->buffer)
	sg_free(schp->buffer, schp->b_malloc_len, schp->buffer_mem_src);
    memset(schp, 0, sizeof(*schp));
}

static int sg_read_xfer(Sg_request * srp)
{
    sg_io_hdr_t * hp = &srp->header;
    Sg_scatter_hold * schp = &srp->data;
    int num_xfer = 0;
    int j, k, onum, usglen, ksglen, res, ok;
    int iovec_count = (int)hp->iovec_count;
    int dxfer_dir = hp->dxfer_direction;
    unsigned char * p;
    unsigned char * up;
    int new_interface = ('\0' == hp->interface_id) ? 0 : 1;

    if ((SG_DXFER_UNKNOWN == dxfer_dir) || (SG_DXFER_FROM_DEV == dxfer_dir) ||
	(SG_DXFER_TO_FROM_DEV == dxfer_dir)) {
	num_xfer =  hp->dxfer_len;
	if (schp->bufflen < num_xfer)
	    num_xfer = schp->bufflen;
    }
    if ((num_xfer <= 0) || 
    	(new_interface && ((SG_FLAG_NO_DXFER | SG_FLAG_MMAP_IO) & hp->flags)))
	return 0;

    SCSI_LOG_TIMEOUT(4,
	 printk("sg_read_xfer: num_xfer=%d, iovec_count=%d, k_use_sg=%d\n",
		num_xfer, iovec_count, schp->k_use_sg));
    if (iovec_count) {
	onum = iovec_count;
	if ((k = verify_area(VERIFY_READ, hp->dxferp,
			     SZ_SG_IOVEC * onum)))
	    return k;
    }
    else
	onum = 1;

    if (0 == schp->k_use_sg) {  /* kernel has single buffer */
	if (SG_USER_MEM != schp->buffer_mem_src) { /* else nothing to do */

	    for (j = 0, p = schp->buffer; j < onum; ++j) {
		res = sg_u_iovec(hp, iovec_count, j, 0, &usglen, &up);
		if (res) return res;
		usglen = (num_xfer > usglen) ? usglen : num_xfer;
		__copy_to_user(up, p, usglen);
		p += usglen;
		num_xfer -= usglen;
		if (num_xfer <= 0)
		    return 0;
	    }
	}
    }
    else {      /* kernel using scatter gather list */
	struct scatterlist * sclp = (struct scatterlist *)schp->buffer;
	char * mem_src_arr = sg_get_sgat_msa(schp);
	ksglen = (int)sclp->length;
	p = sclp->address;

	for (j = 0, k = 0; j < onum; ++j) {
	    res = sg_u_iovec(hp, iovec_count, j, 0, &usglen, &up);
	    if (res) return res;

	    for ( ; p; ++sclp, ksglen = (int)sclp->length, p = sclp->address) {
		ok = (SG_USER_MEM != mem_src_arr[k]);
		if (usglen <= 0)
		    break;
		if (ksglen > usglen) {
		    if (usglen >= num_xfer) {
			if (ok) __copy_to_user(up, p, num_xfer);
			return 0;
		    }
		    if (ok) __copy_to_user(up, p, usglen);
		    p += usglen;
		    ksglen -= usglen;
		    break;
		}
		else {
		    if (ksglen >= num_xfer) {
			if (ok) __copy_to_user(up, p, num_xfer);
			return 0;
		    }
		    if (ok) __copy_to_user(up, p, ksglen);
		    up += ksglen;
		    usglen -= ksglen;
		}
                ++k;
                if (k >= schp->k_use_sg)
                    return 0;
	    }
	}
    }
    return 0;
}

static void sg_read_oxfer(Sg_request * srp, char * outp, int num_read_xfer)
{
    Sg_scatter_hold * schp = &srp->data;

    SCSI_LOG_TIMEOUT(4, printk("sg_read_oxfer: num_read_xfer=%d\n",
			       num_read_xfer));
    if ((! outp) || (num_read_xfer <= 0))
        return;
    if(schp->k_use_sg > 0) {
        int k, num;
        struct scatterlist * sclp = (struct scatterlist *)schp->buffer;

	for (k = 0; (k < schp->k_use_sg) && sclp->address; ++k, ++sclp) {
            num = (int)sclp->length;
            if (num > num_read_xfer) {
                __copy_to_user(outp, sclp->address, num_read_xfer);
                break;
            }
            else {
                __copy_to_user(outp, sclp->address, num);
                num_read_xfer -= num;
                if (num_read_xfer <= 0)
                    break;
                outp += num;
            }
        }
    }
    else
        __copy_to_user(outp, schp->buffer, num_read_xfer);
}

static void sg_build_reserve(Sg_fd * sfp, int req_size)
{
    Sg_scatter_hold * schp = &sfp->reserve;

    SCSI_LOG_TIMEOUT(4, printk("sg_build_reserve: req_size=%d\n", req_size));
    do {
        if (req_size < PAGE_SIZE)
            req_size = PAGE_SIZE;
	if (0 == sg_build_indi(schp, sfp, req_size))
            return;
        else
            sg_remove_scat(schp);
        req_size >>= 1; /* divide by 2 */
    } while (req_size >  (PAGE_SIZE / 2));
}

static void sg_link_reserve(Sg_fd * sfp, Sg_request * srp, int size)
{
    Sg_scatter_hold * req_schp = &srp->data;
    Sg_scatter_hold * rsv_schp = &sfp->reserve;

    srp->res_used = 1;
    SCSI_LOG_TIMEOUT(4, printk("sg_link_reserve: size=%d\n", size));
    size = (size + 1) & (~1);    /* round to even for aha1542 */
    if (rsv_schp->k_use_sg > 0) {
        int k, num;
        int rem = size;
        struct scatterlist * sclp = (struct scatterlist *)rsv_schp->buffer;

	for (k = 0; k < rsv_schp->k_use_sg; ++k, ++sclp) {
            num = (int)sclp->length;
            if (rem <= num) {
		if (0 == k) {
		    req_schp->k_use_sg = 0;
		    req_schp->buffer = sclp->address;
		}
		else {
    		    sfp->save_scat_len = num;
    		    sclp->length = (unsigned)rem;
    		    req_schp->k_use_sg = k + 1;
    		    req_schp->sglist_len = rsv_schp->sglist_len;
    		    req_schp->buffer = rsv_schp->buffer;
		}
		req_schp->bufflen = size;
		req_schp->buffer_mem_src = rsv_schp->buffer_mem_src;
		req_schp->b_malloc_len = rsv_schp->b_malloc_len;
		break;
            }
            else
                rem -= num;
        }
	if (k >= rsv_schp->k_use_sg)
	    SCSI_LOG_TIMEOUT(1, printk("sg_link_reserve: BAD size\n"));
    }
    else {
	req_schp->k_use_sg = 0;
        req_schp->bufflen = size;
        req_schp->buffer = rsv_schp->buffer;
	req_schp->buffer_mem_src = rsv_schp->buffer_mem_src;
        req_schp->b_malloc_len = rsv_schp->b_malloc_len;
    }
}

static void sg_unlink_reserve(Sg_fd * sfp, Sg_request * srp)
{
    Sg_scatter_hold * req_schp = &srp->data;
    Sg_scatter_hold * rsv_schp = &sfp->reserve;

    SCSI_LOG_TIMEOUT(4, printk("sg_unlink_reserve: req->k_use_sg=%d\n",
			       (int)req_schp->k_use_sg));
    if ((rsv_schp->k_use_sg > 0) && (req_schp->k_use_sg > 0)) {
        struct scatterlist * sclp = (struct scatterlist *)rsv_schp->buffer;

	if (sfp->save_scat_len > 0)
	    (sclp + (req_schp->k_use_sg - 1))->length =
                                        (unsigned)sfp->save_scat_len;
        else
            SCSI_LOG_TIMEOUT(1, printk(
			"sg_unlink_reserve: BAD save_scat_len\n"));
    }
    req_schp->k_use_sg = 0;
    req_schp->bufflen = 0;
    req_schp->buffer = NULL;
    req_schp->sglist_len = 0;
    sfp->save_scat_len = 0;
    srp->res_used = 0;
}

static Sg_request * sg_get_rq_mark(Sg_fd * sfp, int pack_id)
{
    Sg_request * resp;
    unsigned long iflags;

    write_lock_irqsave(&sfp->rq_list_lock, iflags);
    for (resp = sfp->headrp; resp; resp = resp->nextrp) { 
	/* look for requests that are ready + not SG_IO owned */
	if ((1 == resp->done) && (! resp->sg_io_owned) &&
            ((-1 == pack_id) || (resp->header.pack_id == pack_id))) {
	    resp->done = 2;   /* guard against other readers */
            break;
	}
    }
    write_unlock_irqrestore(&sfp->rq_list_lock, iflags);
    return resp;
}

#ifdef CONFIG_PROC_FS
static Sg_request * sg_get_nth_request(Sg_fd * sfp, int nth)
{
    Sg_request * resp;
    unsigned long iflags;
    int k;

    read_lock_irqsave(&sfp->rq_list_lock, iflags);
    for (k = 0, resp = sfp->headrp; resp && (k < nth); 
	 ++k, resp = resp->nextrp)
	;
    read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
    return resp;
}
#endif

/* always adds to end of list */
static Sg_request * sg_add_request(Sg_fd * sfp)
{
    int k;
    unsigned long iflags;
    Sg_request * resp;
    Sg_request * rp =  sfp->req_arr;

    write_lock_irqsave(&sfp->rq_list_lock, iflags);
    resp = sfp->headrp;
    if (! resp) {
	memset(rp, 0, sizeof(Sg_request));
	rp->parentfp = sfp;
	resp = rp;
	sfp->headrp = resp;
    }
    else {
        if (0 == sfp->cmd_q)
            resp = NULL;   /* command queuing disallowed */
        else {
            for (k = 0; k < SG_MAX_QUEUE; ++k, ++rp) {
                if (! rp->parentfp)
                    break;
            }
            if (k < SG_MAX_QUEUE) {
		memset(rp, 0, sizeof(Sg_request));
		rp->parentfp = sfp;
		while (resp->nextrp) 
		    resp = resp->nextrp;
		resp->nextrp = rp;
		resp = rp;
            }
            else
                resp = NULL;
        }
    }
    if (resp) {
        resp->nextrp = NULL;
	resp->header.duration = jiffies;
        resp->my_cmdp = NULL;
	resp->data.kiobp = NULL;
    }
    write_unlock_irqrestore(&sfp->rq_list_lock, iflags);
    return resp;
}

/* Return of 1 for found; 0 for not found */
static int sg_remove_request(Sg_fd * sfp, Sg_request * srp)
{
    Sg_request * prev_rp;
    Sg_request * rp;
    unsigned long iflags;
    int res = 0;

    if ((! sfp) || (! srp) || (! sfp->headrp))
        return res;
    write_lock_irqsave(&sfp->rq_list_lock, iflags);
    prev_rp = sfp->headrp;
    if (srp == prev_rp) {
        sfp->headrp = prev_rp->nextrp;
        prev_rp->parentfp = NULL;
        res = 1;
    }
    else {
	while ((rp = prev_rp->nextrp)) {
	    if (srp == rp) {
		prev_rp->nextrp = rp->nextrp;
		rp->parentfp = NULL;
		res = 1;
		break;
	    }
	    prev_rp = rp;
	}
    }
    write_unlock_irqrestore(&sfp->rq_list_lock, iflags);
    return res;
}

#ifdef CONFIG_PROC_FS
static Sg_fd * sg_get_nth_sfp(Sg_device * sdp, int nth)
{
    Sg_fd * resp;
    unsigned long iflags;
    int k;

    read_lock_irqsave(&sg_dev_arr_lock, iflags);
    for (k = 0, resp = sdp->headfp; resp && (k < nth); 
	 ++k, resp = resp->nextfp)
	;
    read_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    return resp;
}
#endif

static Sg_fd * sg_add_sfp(Sg_device * sdp, int dev)
{
    Sg_fd * sfp;
    unsigned long iflags;

    sfp = (Sg_fd *)sg_low_malloc(sizeof(Sg_fd), 0, SG_HEAP_KMAL, 0);
    if (! sfp)
        return NULL;
    memset(sfp, 0, sizeof(Sg_fd));
    sfp->fd_mem_src = SG_HEAP_KMAL;
    init_waitqueue_head(&sfp->read_wait);
    sfp->rq_list_lock = RW_LOCK_UNLOCKED;

    sfp->timeout = SG_DEFAULT_TIMEOUT;
    sfp->force_packid = SG_DEF_FORCE_PACK_ID;
    sfp->low_dma = (SG_DEF_FORCE_LOW_DMA == 0) ?
                   sdp->device->host->unchecked_isa_dma : 1;
    sfp->cmd_q = SG_DEF_COMMAND_Q;
    sfp->keep_orphan = SG_DEF_KEEP_ORPHAN;
    sfp->parentdp = sdp;
    write_lock_irqsave(&sg_dev_arr_lock, iflags);
    if (! sdp->headfp)
        sdp->headfp = sfp;
    else {    /* add to tail of existing list */
	Sg_fd * pfp = sdp->headfp;
	while (pfp->nextfp)
	    pfp = pfp->nextfp;
	pfp->nextfp = sfp;
    }
    write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    SCSI_LOG_TIMEOUT(3, printk("sg_add_sfp: sfp=0x%p, m_s=%d\n",
			       sfp, (int)sfp->fd_mem_src));
    sg_build_reserve(sfp, sg_big_buff);
    SCSI_LOG_TIMEOUT(3, printk("sg_add_sfp:   bufflen=%d, k_use_sg=%d\n",
			   sfp->reserve.bufflen, sfp->reserve.k_use_sg));
    return sfp;
}

static void __sg_remove_sfp(Sg_device * sdp, Sg_fd * sfp)
{
    Sg_fd * fp;
    Sg_fd * prev_fp;

    prev_fp =  sdp->headfp;
    if (sfp == prev_fp)
	sdp->headfp = prev_fp->nextfp;
    else {
	while ((fp = prev_fp->nextfp)) {
	    if (sfp == fp) {
		prev_fp->nextfp = fp->nextfp;
		break;
	    }
	    prev_fp = fp;
	}
    }
    if (sfp->reserve.bufflen > 0) {
    SCSI_LOG_TIMEOUT(6, printk("__sg_remove_sfp:    bufflen=%d, k_use_sg=%d\n",
	     (int)sfp->reserve.bufflen, (int)sfp->reserve.k_use_sg));
	if (sfp->mmap_called)
	    sg_rb_correct4mmap(&sfp->reserve, 0); /* undo correction */
	sg_remove_scat(&sfp->reserve);
    }
    sfp->parentdp = NULL;
    SCSI_LOG_TIMEOUT(6, printk("__sg_remove_sfp:    sfp=0x%p\n", sfp));
    sg_low_free((char *)sfp, sizeof(Sg_fd), sfp->fd_mem_src);
}

/* Returns 0 in normal case, 1 when detached and sdp object removed */
static int sg_remove_sfp(Sg_device * sdp, Sg_fd * sfp)
{
    Sg_request * srp;
    Sg_request * tsrp;
    int dirty = 0;
    int res = 0;

    for (srp = sfp->headrp; srp; srp = tsrp) {
	tsrp = srp->nextrp;
	if (srp->done)
	    sg_finish_rem_req(srp);
	else
	    ++dirty;
    }
    if (0 == dirty) {
	unsigned long iflags;

	write_lock_irqsave(&sg_dev_arr_lock, iflags);
	__sg_remove_sfp(sdp, sfp);
	if (sdp->detached && (NULL == sdp->headfp)) {
	    int k, maxd;

	    maxd = sg_template.dev_max;
	    for (k = 0; k < maxd; ++k) {
	    	if (sdp == sg_dev_arr[k])
		    break;
	    }
	    if (k < maxd)
		sg_dev_arr[k] = NULL;
	    kfree((char *)sdp);
	    res = 1;
	}
	write_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    }
    else {
        sfp->closed = 1; /* flag dirty state on this fd */
	sdp->device->access_count++;
	/* MOD_INC's to inhibit unloading sg and associated adapter driver */
	if (sg_template.module)
	    __MOD_INC_USE_COUNT(sg_template.module);
	if (sdp->device->host->hostt->module)
	    __MOD_INC_USE_COUNT(sdp->device->host->hostt->module);
        SCSI_LOG_TIMEOUT(1, printk(
          "sg_remove_sfp: worrisome, %d writes pending\n", dirty));
    }
    return res;
}

static int sg_res_in_use(Sg_fd * sfp)
{
    const Sg_request * srp;
    unsigned long iflags;

    read_lock_irqsave(&sfp->rq_list_lock, iflags);
    for (srp = sfp->headrp; srp; srp = srp->nextrp)
        if (srp->res_used) break;
    read_unlock_irqrestore(&sfp->rq_list_lock, iflags);
    return srp ? 1 : 0;
}

/* If retSzp==NULL want exact size or fail */
static char * sg_low_malloc(int rqSz, int lowDma, int mem_src, int * retSzp)
{
    char * resp = NULL;
    int page_mask = lowDma ? (GFP_ATOMIC | GFP_DMA) : GFP_ATOMIC;

    if (rqSz <= 0)
        return resp;
    if (SG_HEAP_KMAL == mem_src) {
        resp = kmalloc(rqSz, page_mask);
	if (resp) {
	    if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
	    	memset(resp, 0, rqSz);
	    if (retSzp) *retSzp = rqSz;
	}
        return resp;
    }
    if (SG_HEAP_POOL == mem_src) {
        int num_sect = rqSz / SG_SECTOR_SZ;

        if (0 != (rqSz & SG_SECTOR_MSK)) {
            if (! retSzp)
                return resp;
            ++num_sect;
            rqSz = num_sect * SG_SECTOR_SZ;
        }
        while (num_sect > 0) {
            if ((num_sect <= sg_pool_secs_avail) &&
                (scsi_dma_free_sectors > (SG_LOW_POOL_THRESHHOLD + num_sect))) {
                resp = scsi_malloc(rqSz);
                if (resp) {
		    if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			memset(resp, 0, rqSz);
                    if (retSzp) *retSzp = rqSz;
                    sg_pool_secs_avail -= num_sect;
                    return resp;
                }
            }
            if (! retSzp)
                return resp;
            num_sect /= 2;      /* try half as many */
            rqSz = num_sect * SG_SECTOR_SZ;
        }
    }
    else if (SG_HEAP_PAGE == mem_src) {
        int order, a_size;
        int resSz = rqSz;

        for (order = 0, a_size = PAGE_SIZE;
             a_size < rqSz; order++, a_size <<= 1)
            ;
        resp = (char *)__get_free_pages(page_mask, order);
        while ((! resp) && order && retSzp) {
            --order;
            a_size >>= 1;   /* divide by 2, until PAGE_SIZE */
            resp = (char *)__get_free_pages(page_mask, order); /* try half */
            resSz = a_size;
        }
	if (resp) {
	    if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
                memset(resp, 0, resSz);
	    if (retSzp) *retSzp = resSz;
	}
    }
    else
        printk(KERN_ERR "sg_low_malloc: bad mem_src=%d, rqSz=%df\n", 
	       mem_src, rqSz);
    return resp;
}

static char * sg_malloc(const Sg_fd * sfp, int size, int * retSzp,
                        int * mem_srcp)
{
    char * resp = NULL;

    if (retSzp) *retSzp = size;
    if (size <= 0)
        ;
    else {
        int low_dma = sfp->low_dma;
        int l_ms = -1;  /* invalid value */

	switch (*mem_srcp)
        {
        case SG_HEAP_PAGE:
            l_ms = (size < PAGE_SIZE) ? SG_HEAP_POOL : SG_HEAP_PAGE;
            resp = sg_low_malloc(size, low_dma, l_ms, 0);
            if (resp)
                break;
            resp = sg_low_malloc(size, low_dma, l_ms, &size);
            if (! resp) {
                l_ms = (SG_HEAP_POOL == l_ms) ? SG_HEAP_PAGE : SG_HEAP_POOL;
                resp = sg_low_malloc(size, low_dma, l_ms, &size);
                if (! resp) {
                    l_ms = SG_HEAP_KMAL;
                    resp = sg_low_malloc(size, low_dma, l_ms, &size);
                }
            }
            if (resp && retSzp) *retSzp = size;
            break;
        case SG_HEAP_KMAL:
            l_ms = SG_HEAP_KMAL; /* was SG_HEAP_PAGE */
            resp = sg_low_malloc(size, low_dma, l_ms, 0);
            if (resp)
                break;
            l_ms = SG_HEAP_POOL;
            resp = sg_low_malloc(size, low_dma, l_ms, &size);
            if (resp && retSzp) *retSzp = size;
            break;
        default:
            SCSI_LOG_TIMEOUT(1, printk("sg_malloc: bad ms=%d\n", *mem_srcp));
            break;
        }
        if (resp) *mem_srcp = l_ms;
    }
    SCSI_LOG_TIMEOUT(6, printk("sg_malloc: size=%d, ms=%d, ret=0x%p\n",
                               size, *mem_srcp, resp));
    return resp;
}

static inline int sg_alloc_kiovec(int nr, struct kiobuf **bufp, int *szp)
{
#if SG_NEW_KIOVEC
    return alloc_kiovec_sz(nr, bufp, szp);
#else
    return alloc_kiovec(nr, bufp);
#endif
}

static void sg_low_free(char * buff, int size, int mem_src)
{
    if (! buff) return;
    switch (mem_src) {
    case SG_HEAP_POOL:
	{
	    int num_sect = size / SG_SECTOR_SZ;

	    scsi_free(buff, size);
	    sg_pool_secs_avail += num_sect;
	}
	break;
    case SG_HEAP_KMAL:
	kfree(buff);    /* size not used */
	break;
    case SG_HEAP_PAGE:
	{
	    int order, a_size;
	    for (order = 0, a_size = PAGE_SIZE;
		 a_size < size; order++, a_size <<= 1)
		;
	    free_pages((unsigned long)buff, order);
	}
	break;
    case SG_USER_MEM:
	break; /* nothing to do */
    default:
	printk(KERN_ERR "sg_low_free: bad mem_src=%d, buff=0x%p, rqSz=%d\n",
               mem_src, buff, size);
	break;
    }
}

static void sg_free(char * buff, int size, int mem_src)
{
    SCSI_LOG_TIMEOUT(6,
        printk("sg_free: buff=0x%p, size=%d\n", buff, size));
    if ((! buff) || (size <= 0))
        ;
    else
        sg_low_free(buff, size, mem_src);
}

static inline void sg_free_kiovec(int nr, struct kiobuf **bufp, int *szp)
{
#if SG_NEW_KIOVEC
    free_kiovec_sz(nr, bufp, szp);
#else
    free_kiovec(nr, bufp);
#endif
}

static int sg_ms_to_jif(unsigned int msecs)
{
    if ((UINT_MAX / 2U) < msecs)
	return INT_MAX;      /* special case, set largest possible */
    else
	return ((int)msecs < (INT_MAX / 1000)) ? (((int)msecs * HZ) / 1000)
					       : (((int)msecs / 1000) * HZ);
}

static inline unsigned sg_jif_to_ms(int jifs)
{
    if (jifs <= 0)
	return 0U;
    else {
	unsigned int j = (unsigned int)jifs;
	return (j < (UINT_MAX / 1000)) ? ((j * 1000) / HZ) : ((j / HZ) * 1000);
    }
}

static unsigned char allow_ops[] = {TEST_UNIT_READY, REQUEST_SENSE,
INQUIRY, READ_CAPACITY, READ_BUFFER, READ_6, READ_10, READ_12,
MODE_SENSE, MODE_SENSE_10, LOG_SENSE};

static int sg_allow_access(unsigned char opcode, char dev_type)
{
    int k;

    if (TYPE_SCANNER == dev_type) /* TYPE_ROM maybe burner */
	return 1;
    for (k = 0; k < sizeof(allow_ops); ++k) {
	if (opcode == allow_ops[k])
	    return 1;
    }
    return 0;
}


#ifdef CONFIG_PROC_FS
static int sg_last_dev()
{
    int k;
    unsigned long iflags;

    read_lock_irqsave(&sg_dev_arr_lock, iflags);
    for (k = sg_template.dev_max - 1; k >= 0; --k)
	if (sg_dev_arr[k] && sg_dev_arr[k]->device) break;
    read_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    return k + 1;   /* origin 1 */
}
#endif

static Sg_device * sg_get_dev(int dev)
{
    Sg_device * sdp = NULL;
    unsigned long iflags;

    if (sg_dev_arr && (dev >= 0))
    {
	read_lock_irqsave(&sg_dev_arr_lock, iflags);
	if (dev < sg_template.dev_max)
	    sdp = sg_dev_arr[dev];
	read_unlock_irqrestore(&sg_dev_arr_lock, iflags);
    }
    return sdp;
}

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry * sg_proc_sgp = NULL;

static char sg_proc_sg_dirname[] = "sg";
static const char * sg_proc_leaf_names[] = {"allow_dio", "def_reserved_size",
		"debug", "devices", "device_hdr", "device_strs",
		"hosts", "host_hdr", "host_strs", "version"};

static int sg_proc_adio_read(char * buffer, char ** start, off_t offset,
			     int size, int * eof, void * data);
static int sg_proc_adio_info(char * buffer, int * len, off_t * begin,
			     off_t offset, int size);
static int sg_proc_adio_write(struct file * filp, const char * buffer,
			      unsigned long count, void * data);
static int sg_proc_dressz_read(char * buffer, char ** start, off_t offset,
			       int size, int * eof, void * data);
static int sg_proc_dressz_info(char * buffer, int * len, off_t * begin,
			       off_t offset, int size);
static int sg_proc_dressz_write(struct file * filp, const char * buffer,
				unsigned long count, void * data);
static int sg_proc_debug_read(char * buffer, char ** start, off_t offset,
			      int size, int * eof, void * data);
static int sg_proc_debug_info(char * buffer, int * len, off_t * begin,
			      off_t offset, int size);
static int sg_proc_dev_read(char * buffer, char ** start, off_t offset,
			    int size, int * eof, void * data);
static int sg_proc_dev_info(char * buffer, int * len, off_t * begin,
			    off_t offset, int size);
static int sg_proc_devhdr_read(char * buffer, char ** start, off_t offset,
			       int size, int * eof, void * data);
static int sg_proc_devhdr_info(char * buffer, int * len, off_t * begin,
			       off_t offset, int size);
static int sg_proc_devstrs_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data);
static int sg_proc_devstrs_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size);
static int sg_proc_host_read(char * buffer, char ** start, off_t offset,
			     int size, int * eof, void * data);
static int sg_proc_host_info(char * buffer, int * len, off_t * begin,
			     off_t offset, int size);
static int sg_proc_hosthdr_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data);
static int sg_proc_hosthdr_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size);
static int sg_proc_hoststrs_read(char * buffer, char ** start, off_t offset,
				 int size, int * eof, void * data);
static int sg_proc_hoststrs_info(char * buffer, int * len, off_t * begin,
				 off_t offset, int size);
static int sg_proc_version_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data);
static int sg_proc_version_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size);
static read_proc_t * sg_proc_leaf_reads[] = {
	     sg_proc_adio_read, sg_proc_dressz_read, sg_proc_debug_read,
	     sg_proc_dev_read, sg_proc_devhdr_read, sg_proc_devstrs_read,
	     sg_proc_host_read, sg_proc_hosthdr_read, sg_proc_hoststrs_read,
	     sg_proc_version_read};
static write_proc_t * sg_proc_leaf_writes[] = {
	     sg_proc_adio_write, sg_proc_dressz_write, 0, 0, 0, 0, 0, 0, 0, 0};

#define PRINT_PROC(fmt,args...)                                 \
    do {                                                        \
	*len += sprintf(buffer + *len, fmt, ##args);            \
	if (*begin + *len > offset + size)                      \
	    return 0;                                           \
	if (*begin + *len < offset) {                           \
	    *begin += *len;                                     \
	    *len = 0;                                           \
	}                                                       \
    } while(0)

#define SG_PROC_READ_FN(infofp)                                 \
    do {                                                        \
	int len = 0;                                            \
	off_t begin = 0;                                        \
	*eof = infofp(buffer, &len, &begin, offset, size);      \
	if (offset >= (begin + len))                            \
	    return 0;                                           \
	*start = buffer + offset - begin;			\
	return (size < (begin + len - offset)) ?                \
				size : begin + len - offset;    \
    } while(0)


static int sg_proc_init()
{
    int k, mask;
    int leaves = sizeof(sg_proc_leaf_names) / sizeof(sg_proc_leaf_names[0]);
    struct proc_dir_entry * pdep;

    if (! proc_scsi)
	return 1;
    sg_proc_sgp = create_proc_entry(sg_proc_sg_dirname,
				    S_IFDIR | S_IRUGO | S_IXUGO, proc_scsi);
    if (! sg_proc_sgp)
	return 1;
    for (k = 0; k < leaves; ++k) {
	mask = sg_proc_leaf_writes[k] ? S_IRUGO | S_IWUSR : S_IRUGO;
	pdep = create_proc_entry(sg_proc_leaf_names[k], mask, sg_proc_sgp);
	if (pdep) {
	    pdep->read_proc = sg_proc_leaf_reads[k];
	    if (sg_proc_leaf_writes[k])
		pdep->write_proc = sg_proc_leaf_writes[k];
	}
    }
    return 0;
}

static void sg_proc_cleanup()
{
    int k;
    int leaves = sizeof(sg_proc_leaf_names) / sizeof(sg_proc_leaf_names[0]);

    if ((! proc_scsi) || (! sg_proc_sgp))
	return;
    for (k = 0; k < leaves; ++k)
	remove_proc_entry(sg_proc_leaf_names[k], sg_proc_sgp);
    remove_proc_entry(sg_proc_sg_dirname, proc_scsi);
}

static int sg_proc_adio_read(char * buffer, char ** start, off_t offset,
			       int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_adio_info); }

static int sg_proc_adio_info(char * buffer, int * len, off_t * begin,
			     off_t offset, int size)
{
    PRINT_PROC("%d\n", sg_allow_dio);
    return 1;
}

static int sg_proc_adio_write(struct file * filp, const char * buffer,
			      unsigned long count, void * data)
{
    int num;
    char buff[11];

    if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
	return -EACCES;
    num = (count < 10) ? count : 10;
    copy_from_user(buff, buffer, num);
    buff[num] = '\0';
    sg_allow_dio = simple_strtoul(buff, 0, 10) ? 1 : 0;
    return count;
}

static int sg_proc_dressz_read(char * buffer, char ** start, off_t offset,
			       int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_dressz_info); }

static int sg_proc_dressz_info(char * buffer, int * len, off_t * begin,
			       off_t offset, int size)
{
    PRINT_PROC("%d\n", sg_big_buff);
    return 1;
}

static int sg_proc_dressz_write(struct file * filp, const char * buffer,
				unsigned long count, void * data)
{
    int num;
    unsigned long k = ULONG_MAX;
    char buff[11];

    if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
	return -EACCES;
    num = (count < 10) ? count : 10;
    copy_from_user(buff, buffer, num);
    buff[num] = '\0';
    k = simple_strtoul(buff, 0, 10);
    if (k <= 1048576) {
	sg_big_buff = k;
	return count;
    }
    return -ERANGE;
}

static int sg_proc_debug_read(char * buffer, char ** start, off_t offset,
			      int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_debug_info); }

static int sg_proc_debug_info(char * buffer, int * len, off_t * begin,
			      off_t offset, int size)
{
    Sg_device * sdp;
    const sg_io_hdr_t * hp;
    int j, max_dev, new_interface;

    if (NULL == sg_dev_arr) {
	PRINT_PROC("sg_dev_arr NULL, driver not initialized\n");
	return 1;
    }
    max_dev = sg_last_dev();
    PRINT_PROC("dev_max(currently)=%d max_active_device=%d (origin 1)\n",
	       sg_template.dev_max, max_dev);
    PRINT_PROC(" scsi_dma_free_sectors=%u sg_pool_secs_aval=%d "
	       "def_reserved_size=%d\n",
	       scsi_dma_free_sectors, sg_pool_secs_avail, sg_big_buff);
    for (j = 0; j < max_dev; ++j) {
	if ((sdp = sg_get_dev(j))) {
	    Sg_fd * fp;
	    Sg_request * srp;
	    struct scsi_device * scsidp;
	    int dev, k, m, blen, usg;
 
	    scsidp = sdp->device;
	    if (NULL == scsidp) {
		PRINT_PROC("device %d detached ??\n", j);
		continue;
	    }
	    dev = MINOR(sdp->i_rdev);

	    if (sg_get_nth_sfp(sdp, 0)) {
		PRINT_PROC(" >>> device=sg%d ", dev);
		if (sdp->detached)
		    PRINT_PROC("detached pending close ");
		else
		    PRINT_PROC("scsi%d chan=%d id=%d lun=%d   em=%d",
		       scsidp->host->host_no, scsidp->channel,
		       scsidp->id, scsidp->lun, scsidp->host->hostt->emulated);
		PRINT_PROC(" sg_tablesize=%d excl=%d\n", sdp->sg_tablesize, 
			   sdp->exclude);
	    }
	    for (k = 0; (fp = sg_get_nth_sfp(sdp, k)); ++k) {
		PRINT_PROC("   FD(%d): timeout=%dms bufflen=%d "
			   "(res)sgat=%d low_dma=%d\n", k + 1,
			   sg_jif_to_ms(fp->timeout), fp->reserve.bufflen,
			   (int)fp->reserve.k_use_sg, (int)fp->low_dma);
		PRINT_PROC("   cmd_q=%d f_packid=%d k_orphan=%d closed=%d\n",
			   (int)fp->cmd_q, (int)fp->force_packid,
			   (int)fp->keep_orphan, (int)fp->closed);
		for (m = 0; (srp = sg_get_nth_request(fp, m)); ++m) {
		    hp = &srp->header;
		    new_interface = (hp->interface_id == '\0') ? 0 : 1;
/* stop indenting so far ... */
	PRINT_PROC(srp->res_used ? ((new_interface && 
	    (SG_FLAG_MMAP_IO & hp->flags)) ? "     mmap>> " : "     rb>> ") :
	    ((SG_INFO_DIRECT_IO_MASK & hp->info) ? "     dio>> " : "     "));
	blen = srp->my_cmdp ? srp->my_cmdp->sr_bufflen : srp->data.bufflen;
	usg = srp->my_cmdp ? srp->my_cmdp->sr_use_sg : srp->data.k_use_sg;
	PRINT_PROC(srp->done ? ((1 == srp->done) ? "rcv:" : "fin:") 
			     : (srp->my_cmdp ? "act:" : "prior:"));
	PRINT_PROC(" id=%d blen=%d", srp->header.pack_id, blen);
	if (srp->done)
	    PRINT_PROC(" dur=%d", hp->duration);
	else
	    PRINT_PROC(" t_o/elap=%d/%d", new_interface ? hp->timeout :
			sg_jif_to_ms(fp->timeout),
		  sg_jif_to_ms(hp->duration ? (jiffies - hp->duration) : 0));
	PRINT_PROC("ms sgat=%d op=0x%02x\n", usg, (int)srp->data.cmd_opcode);
/* reset indenting */
		}
		if (0 == m)
		    PRINT_PROC("     No requests active\n");
	    }
	}
    }
    return 1;
}

static int sg_proc_dev_read(char * buffer, char ** start, off_t offset,
			    int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_dev_info); }

static int sg_proc_dev_info(char * buffer, int * len, off_t * begin,
			    off_t offset, int size)
{
    Sg_device * sdp;
    int j, max_dev;
    struct scsi_device * scsidp;

    max_dev = sg_last_dev();
    for (j = 0; j < max_dev; ++j) {
	sdp = sg_get_dev(j);
	if (sdp && (scsidp = sdp->device) && (! sdp->detached))
	    PRINT_PROC("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	       scsidp->host->host_no, scsidp->channel, scsidp->id,
	       scsidp->lun, (int)scsidp->type, (int)scsidp->access_count,
	       (int)scsidp->queue_depth, (int)scsidp->device_busy,
	       (int)scsidp->online);
	else
	    PRINT_PROC("-1\t-1\t-1\t-1\t-1\t-1\t-1\t-1\t-1\n");
    }
    return 1;
}

static int sg_proc_devhdr_read(char * buffer, char ** start, off_t offset,
			       int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_devhdr_info); }

static int sg_proc_devhdr_info(char * buffer, int * len, off_t * begin,
			       off_t offset, int size)
{
    PRINT_PROC("host\tchan\tid\tlun\ttype\topens\tqdepth\tbusy\tonline\n");
    return 1;
}

static int sg_proc_devstrs_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_devstrs_info); }

static int sg_proc_devstrs_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size)
{
    Sg_device * sdp;
    int j, max_dev;
    struct scsi_device * scsidp;

    max_dev = sg_last_dev();
    for (j = 0; j < max_dev; ++j) {
	sdp = sg_get_dev(j);
	if (sdp && (scsidp = sdp->device) && (! sdp->detached))
	    PRINT_PROC("%8.8s\t%16.16s\t%4.4s\n",
		       scsidp->vendor, scsidp->model, scsidp->rev);
	else
	    PRINT_PROC("<no active device>\n");
    }
    return 1;
}

static int sg_proc_host_read(char * buffer, char ** start, off_t offset,
			     int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_host_info); }

static int sg_proc_host_info(char * buffer, int * len, off_t * begin,
			     off_t offset, int size)
{
    struct Scsi_Host * shp;
    int k;

    for (k = 0, shp = scsi_hostlist; shp; shp = shp->next, ++k) {
    	for ( ; k < shp->host_no; ++k)
	    PRINT_PROC("-1\t-1\t-1\t-1\t-1\t-1\n");
	PRINT_PROC("%u\t%hu\t%hd\t%hu\t%d\t%d\n",
		   shp->unique_id, shp->host_busy, shp->cmd_per_lun,
		   shp->sg_tablesize, (int)shp->unchecked_isa_dma,
		   (int)shp->hostt->emulated);
    }
    return 1;
}

static int sg_proc_hosthdr_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_hosthdr_info); }

static int sg_proc_hosthdr_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size)
{
    PRINT_PROC("uid\tbusy\tcpl\tscatg\tisa\temul\n");
    return 1;
}

static int sg_proc_hoststrs_read(char * buffer, char ** start, off_t offset,
				 int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_hoststrs_info); }

#define SG_MAX_HOST_STR_LEN 256

static int sg_proc_hoststrs_info(char * buffer, int * len, off_t * begin,
				 off_t offset, int size)
{
    struct Scsi_Host * shp;
    int k;
    char buff[SG_MAX_HOST_STR_LEN];
    char * cp;

    for (k = 0, shp = scsi_hostlist; shp; shp = shp->next, ++k) {
    	for ( ; k < shp->host_no; ++k)
	    PRINT_PROC("<no active host>\n");
	strncpy(buff, shp->hostt->info ? shp->hostt->info(shp) :
		    (shp->hostt->name ? shp->hostt->name : "<no name>"),
		SG_MAX_HOST_STR_LEN);
	buff[SG_MAX_HOST_STR_LEN - 1] = '\0';
	for (cp = buff; *cp; ++cp) {
	    if ('\n' == *cp)
		*cp = ' '; /* suppress imbedded newlines */
	}
	PRINT_PROC("%s\n", buff);
    }
    return 1;
}

static int sg_proc_version_read(char * buffer, char ** start, off_t offset,
				int size, int * eof, void * data)
{ SG_PROC_READ_FN(sg_proc_version_info); }

static int sg_proc_version_info(char * buffer, int * len, off_t * begin,
				off_t offset, int size)
{
    PRINT_PROC("%d\t%s\n", sg_version_num, sg_version_str);
    return 1;
}
#endif  /* CONFIG_PROC_FS */


module_init(init_sg);
module_exit(exit_sg);

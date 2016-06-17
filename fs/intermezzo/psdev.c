/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *              An implementation of a loadable kernel mode driver providing
 *              multiple kernel/user space bidirectional communications links.
 *
 *              Author:         Alan Cox <alan@cymru.net>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              version 2 as published by the Free Software Foundation.
 *
 *              Adapted to become the Linux 2.0 Coda pseudo device
 *              Peter  Braam  <braam@maths.ox.ac.uk>
 *              Michael Callahan <mjc@emmy.smith.edu>
 *
 *              Changes for Linux 2.1
 *              Copyright (c) 1997 Carnegie-Mellon University
 *
 *              Redone again for InterMezzo
 *              Copyright (c) 1998 Peter J. Braam
 *              Copyright (c) 2000 Mountain View Data, Inc.
 *              Copyright (c) 2000 Tacitus Systems, Inc.
 *              Copyright (c) 2001 Cluster File Systems, Inc.
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/lp.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/poll.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>


#ifdef PRESTO_DEVEL
int  presto_print_entry = 1;
int  presto_debug = 4095;
#else
int  presto_print_entry = 0;
int  presto_debug = 0;
#endif

/* Like inode.c (presto_sym_iops), the initializer is just to prevent
   izo_channels from appearing as a COMMON symbol (and therefore
   interfering with other modules that use the same variable name). */
struct upc_channel izo_channels[MAX_CHANNEL] = {{0}};

int izo_psdev_get_free_channel(void)
{
        int i, result = -1;
        
        for (i = 0 ; i < MAX_CHANNEL ; i++ ) {
                if (list_empty(&(izo_channels[i].uc_cache_list))) { 
                    result = i;
                    break;
                }
        }
        return result;
}


int izo_psdev_setpid(int minor)
{
        struct upc_channel *channel; 
        if (minor < 0 || minor >= MAX_CHANNEL) { 
                return -EINVAL;
        }

        channel = &(izo_channels[minor]); 
        /*
         * This ioctl is performed by each Lento that starts up
         * and wants to do further communication with presto.
         */
        CDEBUG(D_PSDEV, "Setting current pid to %d channel %d\n", 
               current->pid, minor);
        channel->uc_pid = current->pid;
        spin_lock(&channel->uc_lock); 
        if ( !list_empty(&channel->uc_processing) ) {
                struct list_head *lh;
                struct upc_req *req;
                CERROR("WARNING: setpid & processing not empty!\n");
                lh = &channel->uc_processing;
                while ( (lh = lh->next) != &channel->uc_processing) {
                        req = list_entry(lh, struct upc_req, rq_chain);
                        /* freeing of req and data is done by the sleeper */
                        wake_up(&req->rq_sleep);
                }
        }
        if ( !list_empty(&channel->uc_processing) ) {
                CERROR("BAD: FAILDED TO CLEAN PROCESSING LIST!\n");
        }
        spin_unlock(&channel->uc_lock); 
        EXIT;
        return 0;
}

int izo_psdev_setchannel(struct file *file, int fd)
{

        struct file *psdev_file = fget(fd); 
        struct presto_cache *cache = presto_get_cache(file->f_dentry->d_inode);

        if (!psdev_file) { 
                CERROR("%s: no psdev_file!\n", __FUNCTION__);
                return -EINVAL;
        }

        if (!cache) { 
                CERROR("%s: no cache!\n", __FUNCTION__);
                fput(psdev_file); 
                return -EINVAL;
        } 

        if (psdev_file->private_data) { 
                CERROR("%s: channel already set!\n", __FUNCTION__);
                fput(psdev_file); 
                return -EINVAL;
        }

        psdev_file->private_data = cache->cache_psdev;
        fput(psdev_file); 
        EXIT; 
        return 0; 
}

inline int presto_lento_up(int minor) 
{
        return izo_channels[minor].uc_pid;
}

static unsigned int presto_psdev_poll(struct file *file, poll_table * wait)
 {
        struct upc_channel *channel = (struct upc_channel *)file->private_data;
        unsigned int mask = POLLOUT | POLLWRNORM;

        /* ENTRY; this will flood you */
        if ( ! channel ) { 
                CERROR("%s: bad psdev file\n", __FUNCTION__);
                return -EBADF;
        }

        poll_wait(file, &(channel->uc_waitq), wait);

        spin_lock(&channel->uc_lock);
        if (!list_empty(&channel->uc_pending)) {
                CDEBUG(D_PSDEV, "Non-empty pending list.\n");
                mask |= POLLIN | POLLRDNORM;
        }
        spin_unlock(&channel->uc_lock);

        /* EXIT; will flood you */
        return mask;
}

/*
 *      Receive a message written by Lento to the psdev
 */
static ssize_t presto_psdev_write(struct file *file, const char *buf,
                                  size_t count, loff_t *off)
{
        struct upc_channel *channel = (struct upc_channel *)file->private_data;
        struct upc_req *req = NULL;
        struct upc_req *tmp;
        struct list_head *lh;
        struct izo_upcall_resp hdr;
        int error;

        if ( ! channel ) { 
                CERROR("%s: bad psdev file\n", __FUNCTION__);
                return -EBADF;
        }

        /* Peek at the opcode, uniquefier */
        if ( count < sizeof(hdr) ) {
              CERROR("presto_psdev_write: Lento didn't write full hdr.\n");
                return -EINVAL;
        }

        error = copy_from_user(&hdr, buf, sizeof(hdr));
        if ( error )
                return -EFAULT;

        CDEBUG(D_PSDEV, "(process,opc,uniq)=(%d,%d,%d)\n",
               current->pid, hdr.opcode, hdr.unique);

        spin_lock(&channel->uc_lock); 
        /* Look for the message on the processing queue. */
        lh  = &channel->uc_processing;
        while ( (lh = lh->next) != &channel->uc_processing ) {
                tmp = list_entry(lh, struct upc_req , rq_chain);
                if (tmp->rq_unique == hdr.unique) {
                        req = tmp;
                        /* unlink here: keeps search length minimal */
                        list_del_init(&req->rq_chain);
                        CDEBUG(D_PSDEV,"Eureka opc %d uniq %d!\n",
                               hdr.opcode, hdr.unique);
                        break;
                }
        }
        spin_unlock(&channel->uc_lock); 
        if (!req) {
                CERROR("psdev_write: msg (%d, %d) not found\n",
                       hdr.opcode, hdr.unique);
                return(-ESRCH);
        }

        /* move data into response buffer. */
        if (req->rq_bufsize < count) {
                CERROR("psdev_write: too much cnt: %d, cnt: %d, "
                       "opc: %d, uniq: %d.\n",
                       req->rq_bufsize, count, hdr.opcode, hdr.unique);
                count = req->rq_bufsize; /* don't have more space! */
        }
        error = copy_from_user(req->rq_data, buf, count);
        if ( error )
                return -EFAULT;

        /* adjust outsize: good upcalls can be aware of this */
        req->rq_rep_size = count;
        req->rq_flags |= REQ_WRITE;

        wake_up(&req->rq_sleep);
        return(count);
}

/*
 *      Read a message from the kernel to Lento
 */
static ssize_t presto_psdev_read(struct file * file, char * buf,
                                 size_t count, loff_t *off)
{
        struct upc_channel *channel = (struct upc_channel *)file->private_data;
        struct upc_req *req;
        int result = count;

        if ( ! channel ) { 
                CERROR("%s: bad psdev file\n", __FUNCTION__);
                return -EBADF;
        }

        spin_lock(&channel->uc_lock); 
        if (list_empty(&(channel->uc_pending))) {
                CDEBUG(D_UPCALL, "Empty pending list in read, not good\n");
                spin_unlock(&channel->uc_lock); 
                return -EINVAL;
        }
        req = list_entry((channel->uc_pending.next), struct upc_req, rq_chain);
        list_del(&(req->rq_chain));
        if (! (req->rq_flags & REQ_ASYNC) ) {
                list_add(&(req->rq_chain), channel->uc_processing.prev);
        }
        spin_unlock(&channel->uc_lock); 

        req->rq_flags |= REQ_READ;

        /* Move the input args into userspace */
        CDEBUG(D_PSDEV, "\n");
        if (req->rq_bufsize <= count) {
                result = req->rq_bufsize;
        }

        if (count < req->rq_bufsize) {
                CERROR ("psdev_read: buffer too small, read %d of %d bytes\n",
                        count, req->rq_bufsize);
        }

        if ( copy_to_user(buf, req->rq_data, result) ) {
                BUG();
                return -EFAULT;
        }

        /* If request was asynchronous don't enqueue, but free */
        if (req->rq_flags & REQ_ASYNC) {
                CDEBUG(D_PSDEV, "psdev_read: async msg (%d, %d), result %d\n",
                       req->rq_opcode, req->rq_unique, result);
                PRESTO_FREE(req->rq_data, req->rq_bufsize);
                PRESTO_FREE(req, sizeof(*req));
                return result;
        }

        return result;
}


static int presto_psdev_open(struct inode * inode, struct file * file)
{
        ENTRY;

        file->private_data = NULL;  

        MOD_INC_USE_COUNT;

        CDEBUG(D_PSDEV, "Psdev_open: caller: %d, flags: %d\n", current->pid, file->f_flags);

        EXIT;
        return 0;
}



static int presto_psdev_release(struct inode * inode, struct file * file)
{
        struct upc_channel *channel = (struct upc_channel *)file->private_data;
        struct upc_req *req;
        struct list_head *lh;
        ENTRY;

        if ( ! channel ) { 
                CERROR("%s: bad psdev file\n", __FUNCTION__);
                return -EBADF;
        }

        MOD_DEC_USE_COUNT;
        CDEBUG(D_PSDEV, "Lento: pid %d\n", current->pid);
        channel->uc_pid = 0;

        /* Wake up clients so they can return. */
        CDEBUG(D_PSDEV, "Wake up clients sleeping for pending.\n");
        spin_lock(&channel->uc_lock); 
        lh = &channel->uc_pending;
        while ( (lh = lh->next) != &channel->uc_pending) {
                req = list_entry(lh, struct upc_req, rq_chain);

                /* Async requests stay around for a new lento */
                if (req->rq_flags & REQ_ASYNC) {
                        continue;
                }
                /* the sleeper will free the req and data */
                req->rq_flags |= REQ_DEAD; 
                wake_up(&req->rq_sleep);
        }

        CDEBUG(D_PSDEV, "Wake up clients sleeping for processing\n");
        lh = &channel->uc_processing;
        while ( (lh = lh->next) != &channel->uc_processing) {
                req = list_entry(lh, struct upc_req, rq_chain);
                /* freeing of req and data is done by the sleeper */
                req->rq_flags |= REQ_DEAD; 
                wake_up(&req->rq_sleep);
        }
        spin_unlock(&channel->uc_lock); 
        CDEBUG(D_PSDEV, "Done.\n");

        EXIT;
        return 0;
}

static struct file_operations presto_psdev_fops = {
        .read    = presto_psdev_read,
        .write   = presto_psdev_write,
        .poll    = presto_psdev_poll,
        .open    = presto_psdev_open,
        .release = presto_psdev_release
};

/* modules setup */
static struct miscdevice intermezzo_psdev = {
        INTERMEZZO_MINOR,
        "intermezzo",
        &presto_psdev_fops
};

int  presto_psdev_init(void)
{
        int i;
        int err; 

        if ( (err = misc_register(&intermezzo_psdev)) ) { 
                CERROR("%s: cannot register %d err %d\n", 
                       __FUNCTION__, INTERMEZZO_MINOR, err);
                return -EIO;
        }

        memset(&izo_channels, 0, sizeof(izo_channels));
        for ( i = 0 ; i < MAX_CHANNEL ; i++ ) {
                struct upc_channel *channel = &(izo_channels[i]);
                INIT_LIST_HEAD(&channel->uc_pending);
                INIT_LIST_HEAD(&channel->uc_processing);
                INIT_LIST_HEAD(&channel->uc_cache_list);
                init_waitqueue_head(&channel->uc_waitq);
                channel->uc_lock = SPIN_LOCK_UNLOCKED;
                channel->uc_hard = 0;
                channel->uc_no_filter = 0;
                channel->uc_no_journal = 0;
                channel->uc_no_upcall = 0;
                channel->uc_timeout = 30;
                channel->uc_errorval = 0;
                channel->uc_minor = i;
        }
        return 0;
}

void presto_psdev_cleanup(void)
{
        int i;

        misc_deregister(&intermezzo_psdev);

        for ( i = 0 ; i < MAX_CHANNEL ; i++ ) {
                struct upc_channel *channel = &(izo_channels[i]);
                struct list_head *lh;

                spin_lock(&channel->uc_lock); 
                if ( ! list_empty(&channel->uc_pending)) { 
                        CERROR("Weird, tell Peter: module cleanup and pending list not empty dev %d\n", i);
                }
                if ( ! list_empty(&channel->uc_processing)) { 
                        CERROR("Weird, tell Peter: module cleanup and processing list not empty dev %d\n", i);
                }
                if ( ! list_empty(&channel->uc_cache_list)) { 
                        CERROR("Weird, tell Peter: module cleanup and cache listnot empty dev %d\n", i);
                }
                lh = channel->uc_pending.next;
                while ( lh != &channel->uc_pending) {
                        struct upc_req *req;

                        req = list_entry(lh, struct upc_req, rq_chain);
                        lh = lh->next;
                        if ( req->rq_flags & REQ_ASYNC ) {
                                list_del(&(req->rq_chain));
                                CDEBUG(D_UPCALL, "free pending upcall type %d\n",
                                       req->rq_opcode);
                                PRESTO_FREE(req->rq_data, req->rq_bufsize);
                                PRESTO_FREE(req, sizeof(struct upc_req));
                        } else {
                                req->rq_flags |= REQ_DEAD; 
                                wake_up(&req->rq_sleep);
                        }
                }
                lh = &channel->uc_processing;
                while ( (lh = lh->next) != &channel->uc_processing ) {
                        struct upc_req *req;
                        req = list_entry(lh, struct upc_req, rq_chain);
                        list_del(&(req->rq_chain));
                        req->rq_flags |= REQ_DEAD; 
                        wake_up(&req->rq_sleep);
                }
                spin_unlock(&channel->uc_lock); 
        }
}

/*
 * lento_upcall and lento_downcall routines
 */
static inline unsigned long lento_waitfor_upcall
            (struct upc_channel *channel, struct upc_req *req, int minor)
{
        DECLARE_WAITQUEUE(wait, current);
        unsigned long posttime;

        req->rq_posttime = posttime = jiffies;

        add_wait_queue(&req->rq_sleep, &wait);
        for (;;) {
                if ( izo_channels[minor].uc_hard == 0 )
                        set_current_state(TASK_INTERRUPTIBLE);
                else
                        set_current_state(TASK_UNINTERRUPTIBLE);

                /* got a reply */
                if ( req->rq_flags & (REQ_WRITE | REQ_DEAD) )
                        break;

                /* these cases only apply when TASK_INTERRUPTIBLE */ 
                if ( !izo_channels[minor].uc_hard && signal_pending(current) ) {
                        /* if this process really wants to die, let it go */
                        if (sigismember(&(current->pending.signal), SIGKILL)||
                            sigismember(&(current->pending.signal), SIGINT) )
                                break;
                        /* signal is present: after timeout always return
                           really smart idea, probably useless ... */
                        if ( time_after(jiffies, req->rq_posttime +
                             izo_channels[minor].uc_timeout * HZ) )
                                break;
                }
                schedule();
        }

        spin_lock(&channel->uc_lock);
        list_del_init(&req->rq_chain); 
        spin_unlock(&channel->uc_lock);
        remove_wait_queue(&req->rq_sleep, &wait);
        set_current_state(TASK_RUNNING);

        CDEBUG(D_SPECIAL, "posttime: %ld, returned: %ld\n",
               posttime, jiffies-posttime);
        return  (jiffies - posttime);
}

/*
 * lento_upcall will return an error in the case of
 * failed communication with Lento _or_ will peek at Lento
 * reply and return Lento's error.
 *
 * As lento has 2 types of errors, normal errors (positive) and internal
 * errors (negative), normal errors are negated, while internal errors
 * are all mapped to -EINTR, while showing a nice warning message. (jh)
 *
 * lento_upcall will always free buffer, either directly, when an upcall
 * is read (in presto_psdev_read), when the filesystem is unmounted, or
 * when the module is unloaded.
 */
int izo_upc_upcall(int minor, int *size, struct izo_upcall_hdr *buffer, 
                   int async)
{
        unsigned long runtime;
        struct upc_channel *channel;
        struct izo_upcall_resp *out;
        struct upc_req *req;
        int error = 0;

        ENTRY;
        channel = &(izo_channels[minor]);

        if (channel->uc_no_upcall) {
                EXIT;
                goto exit_buf;
        }
        if (!channel->uc_pid && !async) {
                EXIT;
                error = -ENXIO;
                goto exit_buf;
        }

        /* Format the request message. */
        PRESTO_ALLOC(req, sizeof(struct upc_req));
        if ( !req ) {
                EXIT;
                error = -ENOMEM;
                goto exit_buf;
        }
        req->rq_data = (void *)buffer;
        req->rq_flags = 0;
        req->rq_bufsize = *size;
        req->rq_rep_size = 0;
        req->rq_opcode = buffer->u_opc;
        req->rq_unique = ++channel->uc_seq;
        init_waitqueue_head(&req->rq_sleep);

        /* Fill in the common input args. */
        buffer->u_uniq = req->rq_unique;
        buffer->u_async = async;

        spin_lock(&channel->uc_lock); 
        /* Append msg to pending queue and poke Lento. */
        list_add(&req->rq_chain, channel->uc_pending.prev);
        spin_unlock(&channel->uc_lock); 
        CDEBUG(D_UPCALL,
               "Proc %d waking Lento %d for(opc,uniq) =(%d,%d) msg at %p.\n",
               current->pid, channel->uc_pid, req->rq_opcode,
               req->rq_unique, req);
        wake_up_interruptible(&channel->uc_waitq);

        if ( async ) {
                /* req, rq_data are freed in presto_psdev_read for async */
                req->rq_flags = REQ_ASYNC;
                EXIT;
                return 0;
        }

        /* We can be interrupted while we wait for Lento to process
         * our request.  If the interrupt occurs before Lento has read
         * the request, we dequeue and return. If it occurs after the
         * read but before the reply, we dequeue, send a signal
         * message, and return. If it occurs after the reply we ignore
         * it. In no case do we want to restart the syscall.  If it
         * was interrupted by a lento shutdown (psdev_close), return
         * ENODEV.  */

        /* Go to sleep.  Wake up on signals only after the timeout. */
        runtime = lento_waitfor_upcall(channel, req, minor);

        CDEBUG(D_TIMING, "opc: %d time: %ld uniq: %d size: %d\n",
               req->rq_opcode, jiffies - req->rq_posttime,
               req->rq_unique, req->rq_rep_size);
        CDEBUG(D_UPCALL,
               "..process %d woken up by Lento for req at 0x%x, data at %x\n",
               current->pid, (int)req, (int)req->rq_data);

        if (channel->uc_pid) {      /* i.e. Lento is still alive */
          /* Op went through, interrupt or not we go on */
            if (req->rq_flags & REQ_WRITE) {
                    out = (struct izo_upcall_resp *)req->rq_data;
                    /* here we map positive Lento errors to kernel errors */
                    if ( out->result < 0 ) {
                            CERROR("Tell Peter: Lento returns negative error %d, for oc %d!\n",
                                   out->result, out->opcode);
                          out->result = EINVAL;
                    }
                    error = -out->result;
                    CDEBUG(D_UPCALL, "upcall: (u,o,r) (%d, %d, %d) out at %p\n",
                           out->unique, out->opcode, out->result, out);
                    *size = req->rq_rep_size;
                    EXIT;
                    goto exit_req;
            }
            /* Interrupted before lento read it. */
            if ( !(req->rq_flags & REQ_READ) && signal_pending(current)) {
                    CDEBUG(D_UPCALL,
                           "Interrupt before read: (op,un)=(%d,%d), flags %x\n",
                           req->rq_opcode, req->rq_unique, req->rq_flags);
                    /* perhaps the best way to convince the app to give up? */
                    error = -EINTR;
                    EXIT;
                    goto exit_req;
            }

            /* interrupted after Lento did its read, send signal */
            if ( (req->rq_flags & REQ_READ) && signal_pending(current) ) {
                    CDEBUG(D_UPCALL,"Interrupt after read: op = %d.%d, flags = %x\n",
                           req->rq_opcode, req->rq_unique, req->rq_flags);

                    error = -EINTR;
            } else {
                  CERROR("Lento: Strange interruption - tell Peter.\n");
                    error = -EINTR;
            }
        } else {        /* If lento died i.e. !UC_OPEN(channel) */
                CERROR("lento_upcall: Lento dead on (op,un) (%d.%d) flags %d\n",
                       req->rq_opcode, req->rq_unique, req->rq_flags);
                error = -ENODEV;
        }

exit_req:
        PRESTO_FREE(req, sizeof(struct upc_req));
exit_buf:
        return error;
}

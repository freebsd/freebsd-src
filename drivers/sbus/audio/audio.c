/* $Id: audio.c,v 1.62 2001/10/08 22:19:50 davem Exp $
 * drivers/sbus/audio/audio.c
 *
 * Copyright 1996 Thomas K. Dyas (tdyas@noc.rutgers.edu)
 * Copyright 1997,1998,1999 Derrick J. Brashear (shadow@dementia.org)
 * Copyright 1997 Brent Baccala (baccala@freesoft.org)
 * 
 * Mixer code adapted from code contributed by and
 * Copyright 1998 Michael Mraka (michael@fi.muni.cz)
 * and with fixes from Michael Shuey (shuey@ecn.purdue.edu)
 * The mixer code cheats; Sparc hardware doesn't generally allow independent
 * line control, and this fakes it badly.
 *
 * SNDCTL_DSP_SETFMT based on code contributed by
 * Ion Badulescu (ionut@moisil.cs.columbia.edu)
 *
 * This is the audio midlayer that sits between the VFS character
 * devices and the low-level audio hardware device drivers.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/tqueue.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/soundcard.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>

#include <asm/audioio.h>

#undef __AUDIO_DEBUG
#define __AUDIO_ERROR
#undef __AUDIO_TRACE
#undef __AUDIO_OSSDEBUG
#ifdef __AUDIO_DEBUG
#define dprintk(x) printk x
#else
#define dprintk(x)
#endif
#ifdef __AUDIO_OSSDEBUG
#define oprintk(x) printk x
#else
#define oprintk(x)
#endif
#ifdef __AUDIO_ERROR
#define eprintk(x) printk x
#else
#define eprintk(x)
#endif
#ifdef __AUDIO_TRACE
#define tprintk(x) printk x
#else
#define tprintk(x)
#endif

static short lis_get_elist_ent( strevent_t *list, pid_t pid );
static int lis_add_to_elist( strevent_t **list, pid_t pid, short events );
static int lis_del_from_elist( strevent_t **list, pid_t pid, short events );
static void lis_free_elist( strevent_t **list);
static void kill_procs( struct strevent *elist, int sig, short e);

static struct sparcaudio_driver *drivers[SPARCAUDIO_MAX_DEVICES];
static devfs_handle_t devfs_handle;
 

void sparcaudio_output_done(struct sparcaudio_driver * drv, int status)
{
        /* If !status, just restart current output.
         * If status & 1, a buffer is finished; make it available again.
         * If status & 2, a buffer was claimed for DMA and is still in use.
         *
         * The playing_count for non-DMA hardware should never be non-zero.
         * Value of status for non-DMA hardware should always be 1.
         */
        if (status & 1) {
                if (drv->playing_count) {
                        drv->playing_count--;
                } else {
                        drv->output_count--;
                        drv->output_size -= drv->output_sizes[drv->output_front];
                        if (drv->output_notify[drv->output_front] == 1) {
                                drv->output_eof++;
                                drv->output_notify[drv->output_front] = 0;
                                kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);
                        }
                        drv->output_front = (drv->output_front + 1) % 
                                drv->num_output_buffers;
                }
        }
    
        if (status & 2) {
                drv->output_count--;
                drv->playing_count++;
                drv->output_size -= drv->output_sizes[drv->output_front];
                if (drv->output_notify[drv->output_front] == 1) {
                        drv->output_eof++;
                        drv->output_notify[drv->output_front] = 0;
                        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);
                }
                drv->output_front = (drv->output_front + 1) % 
                        drv->num_output_buffers;
        }

        /* If we've played everything go inactive. */
        if ((drv->output_count < 1) && (drv->playing_count < 1)) 
                drv->output_active = 0;

        /* If we got back a buffer, see if anyone wants to write to it */
        if ((status & 1) || ((drv->output_count + drv->playing_count) 
                             < drv->num_output_buffers)) {
                wake_up_interruptible(&drv->output_write_wait);
        }

        /* If the output queue is empty, shut down the driver. */
        if ((drv->output_count < 1) && (drv->playing_count < 1)) {
                kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);

                /* Stop the lowlevel driver from outputing. */
                /* drv->ops->stop_output(drv); Should not be necessary  -- DJB 5/25/98 */
                drv->output_active = 0;
		  
                /* Wake up any waiting writers or syncers and return. */
                wake_up_interruptible(&drv->output_write_wait);
                wake_up_interruptible(&drv->output_drain_wait);
                return;
        }

        /* Start next block of output if we have it */
        if (drv->output_count > 0) {
                drv->ops->start_output(drv, drv->output_buffers[drv->output_front],
                                       drv->output_sizes[drv->output_front]);
                drv->output_active = 1;
        } else {
                drv->output_active = 0;
        }
}

void sparcaudio_input_done(struct sparcaudio_driver * drv, int status)
{
        /* Deal with the weird case here */
        if (drv->duplex == 2) {
                if (drv->input_count < drv->num_input_buffers)
                        drv->input_count++;
                drv->ops->start_input(drv, drv->input_buffers[drv->input_front],
                                      drv->input_buffer_size);
                wake_up_interruptible(&drv->input_read_wait);
                return;
        } 

        /* If status % 2, they filled a buffer for us. 
         * If status & 2, they took a buffer from us.
         */
        if ((status % 2) == 1) {
                drv->input_count++;
                drv->recording_count--;
                drv->input_size+=drv->input_buffer_size;
        }

        if (status > 1) {
                drv->recording_count++;
                drv->input_front = (drv->input_front + 1) % drv->num_input_buffers;
        }

        dprintk(("f%d r%d c%d u%d\n",
                 drv->input_front, drv->input_rear,
                 drv->input_count, drv->recording_count));

        /* If the input queue is full, shutdown the driver. */
        if ((drv->input_count + drv->recording_count) == drv->num_input_buffers) {
                kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);

                /* Stop the lowlevel driver from inputing. */
                drv->ops->stop_input(drv);
                drv->input_active = 0;
        } else {
                /* Otherwise, give the driver the next buffer. */
                drv->ops->start_input(drv, drv->input_buffers[drv->input_front],
                                      drv->input_buffer_size);
        }

        /* Wake up any tasks that are waiting. */
        wake_up_interruptible(&drv->input_read_wait);
}

/*
 *	VFS layer interface
 */

static unsigned int sparcaudio_poll(struct file *file, poll_table * wait)
{
        unsigned int mask = 0;
        struct inode *inode = file->f_dentry->d_inode;
        struct sparcaudio_driver *drv = drivers[(MINOR(inode->i_rdev) >>
                                                 SPARCAUDIO_DEVICE_SHIFT)];

        poll_wait(file, &drv->input_read_wait, wait);
        poll_wait(file, &drv->output_write_wait, wait);
        if (((!file->f_flags & O_NONBLOCK) && drv->input_count) ||
            (drv->input_size > drv->buffer_size)) {
                mask |= POLLIN | POLLRDNORM;
        }
        if ((drv->output_count + drv->playing_count) < (drv->num_output_buffers)) {
                mask |= POLLOUT | POLLWRNORM;
        }
        return mask;
}

static ssize_t sparcaudio_read(struct file * file, char *buf, 
                               size_t count, loff_t *ppos)
{
        struct inode *inode = file->f_dentry->d_inode;
        struct sparcaudio_driver *drv = drivers[(MINOR(inode->i_rdev) >>
                                                 SPARCAUDIO_DEVICE_SHIFT)];
        int bytes_to_copy, bytes_read = 0, err;

        if (! (file->f_mode & FMODE_READ))
                return -EINVAL;

        if ((file->f_flags & O_NONBLOCK) && (drv->input_size < count))
                return -EAGAIN;
    
        while (count > 0) {
                if (drv->input_count == 0) {
                        /* This *should* never happen. */
                        if (file->f_flags & O_NONBLOCK) {
                                printk("Warning: audio input leak!\n");
                                return -EAGAIN;
                        }
                        interruptible_sleep_on(&drv->input_read_wait);
                        if (signal_pending(current))
                                return -EINTR;
                }
  
                bytes_to_copy = drv->input_buffer_size - drv->input_offset;
                if (bytes_to_copy > count)
                        bytes_to_copy = count;

                err = verify_area(VERIFY_WRITE, buf, bytes_to_copy);
                if (err)
                        return err;

                copy_to_user(buf, drv->input_buffers[drv->input_rear]+drv->input_offset, 
                             bytes_to_copy);

                drv->input_offset += bytes_to_copy;
                drv->input_size -= bytes_to_copy;
                buf += bytes_to_copy;
                count -= bytes_to_copy;
                bytes_read += bytes_to_copy;

                if (drv->input_offset >= drv->input_buffer_size) {
                        drv->input_rear = (drv->input_rear + 1) % 
                                drv->num_input_buffers;
                        drv->input_count--;
                        drv->input_offset = 0;
                }

                /* If we're in "loop audio" mode, try waking up the other side
                 * in case they're waiting for us to eat a block. 
                 */
                if (drv->duplex == 2)
                        wake_up_interruptible(&drv->output_write_wait);
        }

        return bytes_read;
}

static void sparcaudio_sync_output(struct sparcaudio_driver * drv)
{
        unsigned long flags;

        /* If the low-level driver is not active, activate it. */
        save_and_cli(flags);
        if ((!drv->output_active) && (drv->output_count > 0)) {
                drv->ops->start_output(drv, 
                                       drv->output_buffers[drv->output_front],
                                       drv->output_sizes[drv->output_front]);
                drv->output_active = 1;
        }
        restore_flags(flags);
}

static ssize_t sparcaudio_write(struct file * file, const char *buf,
                                size_t count, loff_t *ppos)
{
        struct inode *inode = file->f_dentry->d_inode;
        struct sparcaudio_driver *drv = drivers[(MINOR(inode->i_rdev) >>
                                                 SPARCAUDIO_DEVICE_SHIFT)];
        int bytes_written = 0, bytes_to_copy, err;
  
        if (! (file->f_mode & FMODE_WRITE))
                return -EINVAL;

        /* A signal they want notification when this is processed. Too bad
         * sys_write doesn't tell us unless you patch it, in 2.0 kernels.
         */
        if (count == 0) {
#ifndef notdef
                drv->output_eof++;
                kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);
#else
                /* Nice code, but the world isn't ready yet... */
                drv->output_notify[drv->output_rear] = 1;
#endif
        }

        /* Loop until all output is written to device. */
        while (count > 0) {
                /* Check to make sure that an output buffer is available. */
                if (drv->num_output_buffers == (drv->output_count+drv->playing_count)) {
                        /* We need buffers, so... */
                        sparcaudio_sync_output(drv);
                        if (file->f_flags & O_NONBLOCK)
                                return -EAGAIN;

                        interruptible_sleep_on(&drv->output_write_wait);
                        if (signal_pending(current))
                                return bytes_written > 0 ? bytes_written : -EINTR;
                }

                /* No buffers were freed. Go back to sleep */
                if (drv->num_output_buffers == (drv->output_count+drv->playing_count)) 
                        continue;

                /* Deal with the weird case of a reader in the write area by trying to
                 * let them keep ahead of us... Go to sleep until they start servicing.
                 */
                if ((drv->duplex == 2) && (drv->flags & SDF_OPEN_READ) &&
                    (drv->output_rear == drv->input_rear) && (drv->input_count > 0)) {
                        if (file->f_flags & O_NONBLOCK)
                                return -EAGAIN;

                        interruptible_sleep_on(&drv->output_write_wait);
                        if (signal_pending(current))
                                return bytes_written > 0 ? bytes_written : -EINTR;
                }

                /* Determine how much we can copy in this iteration. */
                bytes_to_copy = count;
                if (bytes_to_copy > drv->output_buffer_size - drv->output_offset)
                        bytes_to_copy = drv->output_buffer_size - drv->output_offset;
    
                err = verify_area(VERIFY_READ, buf, bytes_to_copy);
                if (err)
                        return err;

                copy_from_user(drv->output_buffers[drv->output_rear]+drv->output_offset,
                               buf, bytes_to_copy);
    
                /* Update the queue pointers. */
                buf += bytes_to_copy;
                count -= bytes_to_copy;
                bytes_written += bytes_to_copy;

                /* A block can get orphaned in a flush and not cleaned up. */
                if (drv->output_offset)
                        drv->output_sizes[drv->output_rear] += bytes_to_copy;
                else
                        drv->output_sizes[drv->output_rear] = bytes_to_copy;

                drv->output_notify[drv->output_rear] = 0;

                if (drv->output_sizes[drv->output_rear] == drv->output_buffer_size) {
                        drv->output_rear = (drv->output_rear + 1) 
                                % drv->num_output_buffers;
                        drv->output_count++;
                        drv->output_offset = 0;
                } else {
                        drv->output_offset += bytes_to_copy;
                }

                drv->output_size += bytes_to_copy;
        }

        sparcaudio_sync_output(drv);
  
        /* Return the number of bytes written to the caller. */
        return bytes_written;
}

/* Add these in as new devices are supported. Belongs in audioio.h, actually */
#define MONO_DEVICES (SOUND_MASK_SPEAKER | SOUND_MASK_MIC)

static int sparcaudio_mixer_ioctl(struct inode * inode, struct file * file,
                                  unsigned int cmd, unsigned int *arg)
{
        struct sparcaudio_driver *drv = drivers[(MINOR(inode->i_rdev) >>
                                                 SPARCAUDIO_DEVICE_SHIFT)];
        unsigned long i = 0, j = 0, l = 0, m = 0;
        unsigned int k = 0;

        if (_SIOC_DIR(cmd) & _SIOC_WRITE)
                drv->mixer_modify_counter++;

        if(cmd == SOUND_MIXER_INFO) {
                audio_device_t tmp;
                mixer_info info;
                int retval = -EINVAL;

                if(drv->ops->sunaudio_getdev) {
                        drv->ops->sunaudio_getdev(drv, &tmp);
                        memset(&info, 0, sizeof(info));
                        strncpy(info.id, tmp.name, sizeof(info.id));
                        strncpy(info.name, "Sparc Audio", sizeof(info.name));
                        info.modify_counter = drv->mixer_modify_counter;

                        if(copy_to_user((char *)arg, &info, sizeof(info)))
                                retval = -EFAULT;
                        else
                                retval = 0;
                }
                return retval;
  }

        switch (cmd) {
        case SOUND_MIXER_WRITE_RECLEV:
                if (get_user(k, (int *)arg))
                        return -EFAULT;
        iretry:
                oprintk(("setting input volume (0x%x)", k));
                if (drv->ops->get_input_channels)
                        j = drv->ops->get_input_channels(drv);
                if (drv->ops->get_input_volume)
                        l = drv->ops->get_input_volume(drv);
                if (drv->ops->get_input_balance)
                        m = drv->ops->get_input_balance(drv);
                i = OSS_TO_GAIN(k);
                j = OSS_TO_BAL(k);
                oprintk((" for stereo to do %d (bal %d):", i, j));
                if (drv->ops->set_input_volume)
                        drv->ops->set_input_volume(drv, i);
                if (drv->ops->set_input_balance)
                        drv->ops->set_input_balance(drv, j);
        case SOUND_MIXER_READ_RECLEV:
                if (drv->ops->get_input_volume)
                        i = drv->ops->get_input_volume(drv);
                if (drv->ops->get_input_balance)
                        j = drv->ops->get_input_balance(drv);
                oprintk((" got (0x%x)\n", BAL_TO_OSS(i,j)));
                i = BAL_TO_OSS(i,j);
                /* Try to be reasonable about volume changes */
                if ((cmd == SOUND_MIXER_WRITE_RECLEV) && (i != k) && 
                    (i == BAL_TO_OSS(l,m))) {
                        k += (OSS_LEFT(k) > OSS_LEFT(i)) ? 256 : -256;
                        k += (OSS_RIGHT(k) > OSS_RIGHT(i)) ? 1 : -1;
                        oprintk((" try 0x%x\n", k));
                        goto iretry;
                }
                return put_user(i, (int *)arg);
        case SOUND_MIXER_WRITE_VOLUME:
                if (get_user(k, (int *)arg))
                        return -EFAULT;
                if (drv->ops->get_output_muted && drv->ops->set_output_muted) {
                        i = drv->ops->get_output_muted(drv);
                        if ((k == 0) || ((i == 0) && (OSS_LEFT(k) < 100)))
                                drv->ops->set_output_muted(drv, 1);
                        else
                                drv->ops->set_output_muted(drv, 0);
                }
        case SOUND_MIXER_READ_VOLUME:
                if (drv->ops->get_output_muted) 
                        i = drv->ops->get_output_muted(drv);
                k = 0x6464 * (1 - i);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_WRITE_PCM:
                if (get_user(k, (int *)arg))
                        return -EFAULT;
        oretry:
                oprintk(("setting output volume (0x%x)\n", k));
                if (drv->ops->get_output_channels)
                        j = drv->ops->get_output_channels(drv);
                if (drv->ops->get_output_volume)
                        l = drv->ops->get_output_volume(drv);
                if (drv->ops->get_output_balance)
                        m = drv->ops->get_output_balance(drv);
                oprintk((" started as (0x%x)\n", BAL_TO_OSS(l,m)));
                i = OSS_TO_GAIN(k);
                j = OSS_TO_BAL(k);
                oprintk((" for stereo to %d (bal %d)\n", i, j));
                if (drv->ops->set_output_volume)
                        drv->ops->set_output_volume(drv, i);
                if (drv->ops->set_output_balance)
                        drv->ops->set_output_balance(drv, j);
        case SOUND_MIXER_READ_PCM:
                if (drv->ops->get_output_volume)
                        i = drv->ops->get_output_volume(drv);
                if (drv->ops->get_output_balance)
                        j = drv->ops->get_output_balance(drv);
                oprintk((" got 0x%x\n", BAL_TO_OSS(i,j)));
                i = BAL_TO_OSS(i,j);

                /* Try to be reasonable about volume changes */
                if ((cmd == SOUND_MIXER_WRITE_PCM) && (i != k) && 
                    (i == BAL_TO_OSS(l,m))) {
                        k += (OSS_LEFT(k) > OSS_LEFT(i)) ? 256 : -256;
                        k += (OSS_RIGHT(k) > OSS_RIGHT(i)) ? 1 : -1;
                        oprintk((" try 0x%x\n", k));
                        goto oretry;
                }
                return put_user(i, (int *)arg);
        case SOUND_MIXER_READ_SPEAKER:
                k = OSS_PORT_AUDIO(drv, AUDIO_SPEAKER);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_MIC:
                k = OSS_IPORT_AUDIO(drv, AUDIO_MICROPHONE);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_CD:
                k = OSS_IPORT_AUDIO(drv, AUDIO_CD);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_LINE:
                k = OSS_IPORT_AUDIO(drv, AUDIO_LINE_IN);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_LINE1:
                k = OSS_PORT_AUDIO(drv, AUDIO_HEADPHONE);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_LINE2:
                k = OSS_PORT_AUDIO(drv, AUDIO_LINE_OUT);
                return put_user(k, (int *)arg);

        case SOUND_MIXER_WRITE_MIC:
        case SOUND_MIXER_WRITE_CD:
        case SOUND_MIXER_WRITE_LINE:
        case SOUND_MIXER_WRITE_LINE1:
        case SOUND_MIXER_WRITE_LINE2:
        case SOUND_MIXER_WRITE_SPEAKER:
                if (get_user(k, (int *)arg))
                        return -EFAULT;
                OSS_TWIDDLE_IPORT(drv, cmd, SOUND_MIXER_WRITE_LINE, AUDIO_LINE_IN, k);
                OSS_TWIDDLE_IPORT(drv, cmd, SOUND_MIXER_WRITE_MIC, AUDIO_MICROPHONE, k);
                OSS_TWIDDLE_IPORT(drv, cmd, SOUND_MIXER_WRITE_CD, AUDIO_CD, k);

                OSS_TWIDDLE_PORT(drv, cmd, SOUND_MIXER_WRITE_SPEAKER, AUDIO_SPEAKER, k);
                OSS_TWIDDLE_PORT(drv, cmd, SOUND_MIXER_WRITE_LINE1, AUDIO_HEADPHONE, k);
                OSS_TWIDDLE_PORT(drv, cmd, SOUND_MIXER_WRITE_LINE2, AUDIO_LINE_OUT, k);
                return put_user(k, (int *)arg);
        case SOUND_MIXER_READ_RECSRC: 
                if (drv->ops->get_input_port)
                        i = drv->ops->get_input_port(drv);

                /* only one should ever be selected */
                if (i & AUDIO_CD) j = SOUND_MASK_CD;
                if (i & AUDIO_LINE_IN) j = SOUND_MASK_LINE;
                if (i & AUDIO_MICROPHONE) j = SOUND_MASK_MIC;
    
                return put_user(j, (int *)arg);
  case SOUND_MIXER_WRITE_RECSRC: 
          if (!drv->ops->set_input_port)
                  return -EINVAL;
          if (get_user(k, (int *)arg))
                  return -EFAULT;

          /* only one should ever be selected */
          if (k & SOUND_MASK_CD) j = AUDIO_CD;
          if (k & SOUND_MASK_LINE) j = AUDIO_LINE_IN;
          if (k & SOUND_MASK_MIC) j = AUDIO_MICROPHONE;
          oprintk(("setting inport to %d\n", j));
          i = drv->ops->set_input_port(drv, j);
    
          return put_user(i, (int *)arg);
        case SOUND_MIXER_READ_RECMASK: 
                if (drv->ops->get_input_ports)
                        i = drv->ops->get_input_ports(drv);
                /* what do we support? */
                if (i & AUDIO_MICROPHONE) j |= SOUND_MASK_MIC;
                if (i & AUDIO_LINE_IN) j |= SOUND_MASK_LINE;
                if (i & AUDIO_CD) j |= SOUND_MASK_CD;
    
                return put_user(j, (int *)arg);
        case SOUND_MIXER_READ_CAPS: /* mixer capabilities */
                i = SOUND_CAP_EXCL_INPUT;
                return put_user(i, (int *)arg);

        case SOUND_MIXER_READ_DEVMASK: /* all supported devices */
                if (drv->ops->get_input_ports)
                        i = drv->ops->get_input_ports(drv);
                /* what do we support? */
                if (i & AUDIO_MICROPHONE) j |= SOUND_MASK_MIC;
                if (i & AUDIO_LINE_IN) j |= SOUND_MASK_LINE;
                if (i & AUDIO_CD) j |= SOUND_MASK_CD;
    
                if (drv->ops->get_output_ports)
                        i = drv->ops->get_output_ports(drv);
                if (i & AUDIO_SPEAKER) j |= SOUND_MASK_SPEAKER;
                if (i & AUDIO_HEADPHONE) j |= SOUND_MASK_LINE1;
                if (i & AUDIO_LINE_OUT) j |= SOUND_MASK_LINE2;

                j |= SOUND_MASK_VOLUME;

        case SOUND_MIXER_READ_STEREODEVS: /* what supports stereo */
                j |= SOUND_MASK_PCM|SOUND_MASK_RECLEV;

                if (cmd == SOUND_MIXER_READ_STEREODEVS)
                        j &= ~(MONO_DEVICES);
                return put_user(j, (int *)arg);
        default:
                return -EINVAL;
        };
}

/* AUDIO_SETINFO uses these to set values if possible. */
static __inline__ int 
__sparcaudio_if_set_do(struct sparcaudio_driver *drv, 
		       int (*set_function)(struct sparcaudio_driver *, int), 
		       int (*get_function)(struct sparcaudio_driver *), 
		       unsigned int value)
{
        if (set_function && Modify(value))
                return (int) set_function(drv, value);
        else if (get_function)
                return (int) get_function(drv);
        else 
                return 0;
}

static __inline__ int 
__sparcaudio_if_setc_do(struct sparcaudio_driver *drv, 
			int (*set_function)(struct sparcaudio_driver *, int), 
			int (*get_function)(struct sparcaudio_driver *), 
			unsigned char value)
{
        if (set_function && Modifyc(value))
                return (char) set_function(drv, (int)value);
        else if (get_function)
                return (char) get_function(drv);
        else 
                return 0;
}

/* I_FLUSH, I_{G,S}ETSIG, I_NREAD provided for SunOS compatibility
 *
 * I must admit I'm quite ashamed of the state of the ioctl handling,
 * but I do have several optimizations which I'm planning. -- DJB
 */
static int sparcaudio_ioctl(struct inode * inode, struct file * file,
			    unsigned int cmd, unsigned long arg)
{
	int retval = 0, i, j, k;
	int minor = MINOR(inode->i_rdev);
	struct audio_info ainfo;
	audio_buf_info binfo;
	count_info cinfo;
	struct sparcaudio_driver *drv = 
                drivers[(minor >> SPARCAUDIO_DEVICE_SHIFT)];

	switch (minor & 0xf) {
	case SPARCAUDIO_MIXER_MINOR:
                return sparcaudio_mixer_ioctl(inode, file, cmd, (unsigned int *)arg);
	case SPARCAUDIO_DSP16_MINOR:
        case SPARCAUDIO_DSP_MINOR:
	case SPARCAUDIO_AUDIO_MINOR:
	case SPARCAUDIO_AUDIOCTL_MINOR:
                /* According to the OSS prog int, you can mixer ioctl /dev/dsp */
                if (_IOC_TYPE(cmd) == 'M')
                        return sparcaudio_mixer_ioctl(inode, 
                                                      file, cmd, (unsigned int *)arg);
                switch (cmd) {
                case I_GETSIG:
                case I_GETSIG_SOLARIS:
                        j = (int) lis_get_elist_ent(drv->sd_siglist,current->pid);
                        put_user(j, (int *)arg);
                        retval = drv->input_count;
                        break;

                case I_SETSIG:
                case I_SETSIG_SOLARIS:
                        if ((minor & 0xf) == SPARCAUDIO_AUDIOCTL_MINOR) {
                                if (!arg) {
                                        if (lis_del_from_elist(&(drv->sd_siglist),
                                                               current->pid,S_ALL)) {
                                                retval = -EINVAL;
                                        } else if (!drv->sd_siglist) {
                                                drv->sd_sigflags=0;
                                        }
                                } else if (lis_add_to_elist(&(drv->sd_siglist),
                                                            current->pid,
                                                            (short)arg)) {
                                        retval = -EAGAIN;
                                } else {
                                        ((drv->sd_sigflags) |= (arg));
                                }
                        }
                        break;
                case I_NREAD:
                case I_NREAD_SOLARIS:
                        /* According to the Solaris man page, this copies out
                         * the size of the first streams buffer and returns 
                         * the number of streams messages on the read queue as
                         * as its retval. (streamio(7I)) This should work.
                         */
                        j = (drv->input_count > 0) ? drv->input_buffer_size : 0;
                        put_user(j, (int *)arg);
                        retval = drv->input_count;
                        break;

                        /* A poor substitute until we do true resizable buffers. */
                case SNDCTL_DSP_GETISPACE:
                        binfo.fragstotal = drv->num_input_buffers;
                        binfo.fragments = drv->num_input_buffers - 
                                (drv->input_count + drv->recording_count);
                        binfo.fragsize = drv->input_buffer_size;
                        binfo.bytes = binfo.fragments*binfo.fragsize;
	    
                        retval = verify_area(VERIFY_WRITE, (int *)arg, sizeof(binfo));
                        if (retval)
                                break;
                        copy_to_user(&((char *)arg)[0], (char *)&binfo, sizeof(binfo));
                        break;
                case SNDCTL_DSP_GETOSPACE:
                        binfo.fragstotal = drv->num_output_buffers;
                        binfo.fragments = drv->num_output_buffers - 
                                (drv->output_count + drv->playing_count + 
                                 (drv->output_offset ? 1 : 0));
                        binfo.fragsize = drv->output_buffer_size;
                        binfo.bytes = binfo.fragments*binfo.fragsize + 
                                (drv->output_buffer_size - drv->output_offset);
	    
                        retval = verify_area(VERIFY_WRITE, (int *)arg, sizeof(binfo));
                        if (retval)
                                break;
                        copy_to_user(&((char *)arg)[0], (char *)&binfo, sizeof(binfo));
                        break;
                case SNDCTL_DSP_GETIPTR:
                case SNDCTL_DSP_GETOPTR:
                        /* int bytes (number of bytes read/written since last)
                         * int blocks (number of frags read/wrote since last call)
                         * int ptr (current position of dma in buffer)
                         */
                        retval = 0;
                        cinfo.bytes = 0;
                        cinfo.ptr = 0;
                        cinfo.blocks = 0;
                        cinfo.bytes += cinfo.ptr;
	    
                        retval = verify_area(VERIFY_WRITE, (int *)arg, sizeof(cinfo));
                        if (retval)
                                break;
                        copy_to_user(&((char *)arg)[0], (char *)&cinfo, sizeof(cinfo));
                        break;
                case SNDCTL_DSP_SETFRAGMENT:
                        /* XXX Small hack to get ESD/Enlightenment to work.  --DaveM */
                        retval = 0;
                        break;

                case SNDCTL_DSP_SUBDIVIDE:
                        /* I don't understand what I need to do yet. */
                        retval = -EINVAL;
                        break;
                case SNDCTL_DSP_SETTRIGGER:
                        /* This may not be 100% correct */
                        if ((arg & PCM_ENABLE_INPUT) && drv->ops->get_input_pause &&
                            drv->ops->set_input_pause) {
                                if (drv->ops->get_input_pause(drv))
                                        drv->ops->set_input_pause(drv, 0);
                        } else {
                                if (!drv->ops->get_input_pause(drv))
                                        drv->ops->set_input_pause(drv, 1);
                        }
                        if ((arg & PCM_ENABLE_OUTPUT) && drv->ops->get_output_pause &&
                            drv->ops->set_output_pause) {
                                if (drv->ops->get_output_pause(drv))
                                        drv->ops->set_output_pause(drv, 0);
                        } else {
                                if (!drv->ops->get_output_pause(drv))
                                        drv->ops->set_output_pause(drv, 1);
                        }
                        break;
                case SNDCTL_DSP_GETTRIGGER:
                        j = 0;
                        if (drv->ops->get_input_pause) {
                                if (drv->ops->get_input_pause(drv))
                                        j = PCM_ENABLE_INPUT;
                        }
                        if (drv->ops->get_output_pause) {
                                if (drv->ops->get_output_pause(drv))
                                        j |= PCM_ENABLE_OUTPUT;
                        }
                        put_user(j, (int *)arg);
                        break;
                case SNDCTL_DSP_GETBLKSIZE:
                        j = drv->input_buffer_size;
                        put_user(j, (int *)arg);
                        break;
                case SNDCTL_DSP_SPEED:
                        if ((!drv->ops->set_output_rate) && 
                            (!drv->ops->set_input_rate)) {
                                retval = -EINVAL;
                                break;
                        }
                        get_user(i, (int *)arg)
                        tprintk(("setting speed to %d\n", i));
                        drv->ops->set_input_rate(drv, i);
                        drv->ops->set_output_rate(drv, i);
                        j = drv->ops->get_output_rate(drv);
                        put_user(j, (int *)arg);
                        break;
                case SNDCTL_DSP_GETCAPS:
                        /* All Sparc audio hardware is full duplex.
                         * 4231 supports DMA pointer reading, 7930 is byte at a time.
                         * Pause functionality emulates trigger
                         */
                        j = DSP_CAP_DUPLEX | DSP_CAP_TRIGGER | DSP_CAP_REALTIME;
                        put_user(j, (int *)arg);
                        break;
                case SNDCTL_DSP_GETFMTS:
                        if (drv->ops->get_formats) {
                                j = drv->ops->get_formats(drv);
                                put_user(j, (int *)arg);
                        } else {
                                retval = -EINVAL;
                        }
                        break;
                case SNDCTL_DSP_SETFMT:
                        /* need to decode into encoding, precision */
                        get_user(i, (int *)arg);
	    
                        /* handle special case here */
                        if (i == AFMT_QUERY) {
                                j = drv->ops->get_output_encoding(drv);
                                k = drv->ops->get_output_precision(drv);
                                if (j == AUDIO_ENCODING_DVI) {
                                        i = AFMT_IMA_ADPCM;
                                } else if (k == 8) {
                                        switch (j) {
                                        case AUDIO_ENCODING_ULAW:
                                                i = AFMT_MU_LAW;
                                                break;
                                        case AUDIO_ENCODING_ALAW:
                                                i = AFMT_A_LAW;
                                                break;
                                        case AUDIO_ENCODING_LINEAR8:
                                                i = AFMT_U8;
                                                break;
                                        };
                                } else if (k == 16) {
                                        switch (j) {
                                        case AUDIO_ENCODING_LINEAR:
                                                i = AFMT_S16_BE;
                                                break;
                                        case AUDIO_ENCODING_LINEARLE:
                                                i = AFMT_S16_LE;
                                                break;
                                        };
                                } 
                                put_user(i, (int *)arg);
                                break;
                        }

                        /* Without these there's no point in trying */
                        if (!drv->ops->set_input_precision ||
                            !drv->ops->set_input_encoding ||
                            !drv->ops->set_output_precision ||
                            !drv->ops->set_output_encoding) {
                                eprintk(("missing set routines: failed\n"));
                                retval = -EINVAL;
                                break;
                        }

                        if (drv->ops->get_formats) {
                                if (!(drv->ops->get_formats(drv) & i)) {
                                        dprintk(("format not supported\n"));
                                        return -EINVAL;
                                }
                        }
                        switch (i) {
                        case AFMT_S16_LE:
                                ainfo.record.precision = ainfo.play.precision = 16;
                                ainfo.record.encoding = ainfo.play.encoding =
                                        AUDIO_ENCODING_LINEARLE;
                                break;
                        case AFMT_S16_BE:
                                ainfo.record.precision = ainfo.play.precision = 16;
                                ainfo.record.encoding = ainfo.play.encoding =
                                        AUDIO_ENCODING_LINEAR;
                                break;
                        case AFMT_MU_LAW:
                                ainfo.record.precision = ainfo.play.precision = 8;
                                ainfo.record.encoding = ainfo.play.encoding =
                                        AUDIO_ENCODING_ULAW;
                                break;
                        case AFMT_A_LAW:
                                ainfo.record.precision = ainfo.play.precision = 8;
                                ainfo.record.encoding = ainfo.play.encoding =
                                        AUDIO_ENCODING_ALAW;
                                break;
                        case AFMT_U8:
                                ainfo.record.precision = ainfo.play.precision = 8;
                                ainfo.record.encoding = ainfo.play.encoding =
                                        AUDIO_ENCODING_LINEAR8;
                                break;
                        };
                        tprintk(("setting fmt to enc %d pr %d\n",
                                 ainfo.play.encoding,
                                 ainfo.play.precision));
                        if ((drv->ops->set_input_precision(drv,
                                                           ainfo.record.precision) 
                             < 0) ||
                            (drv->ops->set_output_precision(drv,
                                                            ainfo.play.precision)  
                             < 0) ||
                            (drv->ops->set_input_encoding(drv,
                                                          ainfo.record.encoding)
                             < 0) ||
                            (drv->ops->set_output_encoding(drv,
                                                           ainfo.play.encoding)
                             < 0)) {
                                dprintk(("setting format: failed\n"));
                                return -EINVAL;
                        }
                        put_user(i, (int *)arg);
                        break;
                case SNDCTL_DSP_CHANNELS:
                        if ((!drv->ops->set_output_channels) && 
                            (!drv->ops->set_input_channels)) {
                                retval = -EINVAL;
                                break;
                        }
                        get_user(i, (int *)arg);
                        drv->ops->set_input_channels(drv, i);
                        drv->ops->set_output_channels(drv, i);
                        i = drv->ops->get_output_channels(drv);
                        put_user(i, (int *)arg);
                        break;
                case SNDCTL_DSP_STEREO:
                        if ((!drv->ops->set_output_channels) && 
                            (!drv->ops->set_input_channels)) {
                                retval = -EINVAL;
                                break;
                        }
                        get_user(i, (int *)arg);
                        drv->ops->set_input_channels(drv, (i + 1));
                        drv->ops->set_output_channels(drv, (i + 1));
                        i = ((drv->ops->get_output_channels(drv)) - 1);
                        put_user(i, (int *)arg);
                        break;
                case SNDCTL_DSP_POST:
                case SNDCTL_DSP_SYNC:
                case AUDIO_DRAIN:
                        /* Deal with weirdness so we can fill buffers */
                        if (drv->output_offset) {
                                drv->output_offset = 0;
                                drv->output_rear = (drv->output_rear + 1)
                                        % drv->num_output_buffers;
                                drv->output_count++;
                        }
                        if (drv->output_count > 0) {
                                sparcaudio_sync_output(drv);
                                /* Only pause for DRAIN/SYNC, not POST */
                                if (cmd != SNDCTL_DSP_POST) {
                                        interruptible_sleep_on(&drv->output_drain_wait);
                                        retval = (signal_pending(current)) ? -EINTR : 0;
                                }
                        }
                        break;
                case I_FLUSH:
                case I_FLUSH_SOLARIS:
                        if (((unsigned int)arg == FLUSHW) || 
                            ((unsigned int)arg == FLUSHRW)) {
                                if (file->f_mode & FMODE_WRITE) {
                                        sparcaudio_sync_output(drv);
                                        if (drv->output_active) {
                                                wake_up_interruptible(&drv->output_write_wait);
                                                drv->ops->stop_output(drv);
                                        }
                                        drv->output_offset = 0;
                                        drv->output_active = 0;
                                        drv->output_front = 0;
                                        drv->output_rear = 0;
                                        drv->output_count = 0;
                                        drv->output_size = 0;
                                        drv->playing_count = 0;
                                        drv->output_eof = 0;
                                }
                        }
                        if (((unsigned int)arg == FLUSHR) || 
                            ((unsigned int)arg == FLUSHRW)) {
                                if (drv->input_active && (file->f_mode & FMODE_READ)) {
                                        wake_up_interruptible(&drv->input_read_wait);
                                        drv->ops->stop_input(drv);
                                        drv->input_active = 0;
                                        drv->input_front = 0;
                                        drv->input_rear = 0;
                                        drv->input_count = 0;
                                        drv->input_size = 0;
                                        drv->input_offset = 0;
                                        drv->recording_count = 0;
                                }
                                if ((file->f_mode & FMODE_READ) && 
                                    (drv->flags & SDF_OPEN_READ)) {
                                        if (drv->duplex == 2)
                                                drv->input_count = drv->output_count;
                                        drv->ops->start_input(drv, 
                                                              drv->input_buffers[drv->input_front],
                                                              drv->input_buffer_size);
                                        drv->input_active = 1;
                                }
                        }
                        if (((unsigned int)arg == FLUSHW) || 
                            ((unsigned int)arg == FLUSHRW)) {
                                if ((file->f_mode & FMODE_WRITE) && 
                                    !(drv->flags & SDF_OPEN_WRITE)) {
                                        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);
                                        sparcaudio_sync_output(drv);
                                }
                        }
                        break;
                case SNDCTL_DSP_RESET:
                case AUDIO_FLUSH:
                        if (drv->output_active && (file->f_mode & FMODE_WRITE)) {
                                wake_up_interruptible(&drv->output_write_wait);
                                drv->ops->stop_output(drv);
                                drv->output_active = 0;
                                drv->output_front = 0;
                                drv->output_rear = 0;
                                drv->output_count = 0;
                                drv->output_size = 0;
                                drv->playing_count = 0;
                                drv->output_offset = 0;
                                drv->output_eof = 0;
                        }
                        if (drv->input_active && (file->f_mode & FMODE_READ)) {
                                wake_up_interruptible(&drv->input_read_wait);
                                drv->ops->stop_input(drv);
                                drv->input_active = 0;
                                drv->input_front = 0;
                                drv->input_rear = 0;
                                drv->input_count = 0;
                                drv->input_size = 0;
                                drv->input_offset = 0;
                                drv->recording_count = 0;
                        }
                        if ((file->f_mode & FMODE_READ) && 
                            !(drv->flags & SDF_OPEN_READ)) {
                                drv->ops->start_input(drv, 
                                                      drv->input_buffers[drv->input_front],
                                                      drv->input_buffer_size);
                                drv->input_active = 1;
                        }
                        if ((file->f_mode & FMODE_WRITE) && 
                            !(drv->flags & SDF_OPEN_WRITE)) {
                                sparcaudio_sync_output(drv);
                        }
                        break;
                case AUDIO_GETDEV:
                        if (drv->ops->sunaudio_getdev) {
                                audio_device_t tmp;
	      
                                retval = verify_area(VERIFY_WRITE, (void *)arg, 
                                                     sizeof(audio_device_t));
                                if (!retval)
                                        drv->ops->sunaudio_getdev(drv, &tmp);
                                copy_to_user((audio_device_t *)arg, &tmp, sizeof(tmp));
                        } else {
                                retval = -EINVAL;
                        }
                        break;
                case AUDIO_GETDEV_SUNOS:
                        if (drv->ops->sunaudio_getdev_sunos) {
                                int tmp = drv->ops->sunaudio_getdev_sunos(drv);

                                retval = verify_area(VERIFY_WRITE, (void *)arg, sizeof(int));
                                if (!retval)
                                        copy_to_user((int *)arg, &tmp, sizeof(tmp));
                        } else {
                                retval = -EINVAL;
                        }
                        break;
                case AUDIO_GETINFO:
                        AUDIO_INITINFO(&ainfo);

                        if (drv->ops->get_input_rate)
                                ainfo.record.sample_rate =
                                        drv->ops->get_input_rate(drv);
                        else
                                ainfo.record.sample_rate = (8000);
                        if (drv->ops->get_input_channels)
                                ainfo.record.channels =
                                        drv->ops->get_input_channels(drv);
                        else
                                ainfo.record.channels = (1);
                        if (drv->ops->get_input_precision)
                                ainfo.record.precision =
                                        drv->ops->get_input_precision(drv);
                        else
                                ainfo.record.precision = (8);
                        if (drv->ops->get_input_encoding)
                                ainfo.record.encoding =
                                        drv->ops->get_input_encoding(drv);
                        else
                                ainfo.record.encoding = (AUDIO_ENCODING_ULAW);
                        if (drv->ops->get_input_volume)
                                ainfo.record.gain =
                                        drv->ops->get_input_volume(drv);
                        else
                                ainfo.record.gain = (0);
                        if (drv->ops->get_input_port)
                                ainfo.record.port =
                                        drv->ops->get_input_port(drv);
                        else
                                ainfo.record.port = (0);
                        if (drv->ops->get_input_ports)
                                ainfo.record.avail_ports = 
                                        drv->ops->get_input_ports(drv);
                        else
                                ainfo.record.avail_ports = (0);

                        /* To make e.g. vat happy, we let them think they control this */
                        ainfo.record.buffer_size = drv->buffer_size;
                        if (drv->ops->get_input_samples)
                                ainfo.record.samples = drv->ops->get_input_samples(drv);
                        else
                                ainfo.record.samples = 0;

                        /* This is undefined in the record context in Solaris */
                        ainfo.record.eof = 0;
                        if (drv->ops->get_input_pause)
                                ainfo.record.pause =
                                        drv->ops->get_input_pause(drv);
                        else
                                ainfo.record.pause = 0;
                        if (drv->ops->get_input_error)
                                ainfo.record.error = 
                                        (unsigned char) drv->ops->get_input_error(drv);
                        else
                                ainfo.record.error = 0;
                        ainfo.record.waiting = 0;
                        if (drv->ops->get_input_balance)
                                ainfo.record.balance =
                                        (unsigned char) drv->ops->get_input_balance(drv);
                        else
                                ainfo.record.balance = (unsigned char)(AUDIO_MID_BALANCE);
                        ainfo.record.minordev = 4 + (minor << SPARCAUDIO_DEVICE_SHIFT);
                        ainfo.record.open = (drv->flags & SDF_OPEN_READ);
                        ainfo.record.active = 0;

                        if (drv->ops->get_output_rate)
                                ainfo.play.sample_rate =
                                        drv->ops->get_output_rate(drv);
                        else
                                ainfo.play.sample_rate = (8000);
                        if (drv->ops->get_output_channels)
                                ainfo.play.channels =
                                        drv->ops->get_output_channels(drv);
                        else
                                ainfo.play.channels = (1);
                        if (drv->ops->get_output_precision)
                                ainfo.play.precision =
                                        drv->ops->get_output_precision(drv);
                        else
                                ainfo.play.precision = (8);
                        if (drv->ops->get_output_encoding)
                                ainfo.play.encoding =
                                        drv->ops->get_output_encoding(drv);
                        else
                                ainfo.play.encoding = (AUDIO_ENCODING_ULAW);
                        if (drv->ops->get_output_volume)
                                ainfo.play.gain =
                                        drv->ops->get_output_volume(drv);
                        else
                                ainfo.play.gain = (0);
                        if (drv->ops->get_output_port)
                                ainfo.play.port =
                                        drv->ops->get_output_port(drv);
                        else
                                ainfo.play.port = (0);
                        if (drv->ops->get_output_ports)
                                ainfo.play.avail_ports = 
                                        drv->ops->get_output_ports(drv);
                        else
                                ainfo.play.avail_ports = (0);

                        /* This is not defined in the play context in Solaris */
                        ainfo.play.buffer_size = 0;
                        if (drv->ops->get_output_samples)
                                ainfo.play.samples = drv->ops->get_output_samples(drv);
                        else
                                ainfo.play.samples = 0;
                        ainfo.play.eof = drv->output_eof;
                        if (drv->ops->get_output_pause)
                                ainfo.play.pause =
                                        drv->ops->get_output_pause(drv);
                        else
                                ainfo.play.pause = 0;
                        if (drv->ops->get_output_error)
                                ainfo.play.error =
                                        (unsigned char)drv->ops->get_output_error(drv);
                        else
                                ainfo.play.error = 0;
                        ainfo.play.waiting = waitqueue_active(&drv->open_wait);
                        if (drv->ops->get_output_balance)
                                ainfo.play.balance =
                                        (unsigned char)drv->ops->get_output_balance(drv);
                        else
                                ainfo.play.balance = (unsigned char)(AUDIO_MID_BALANCE);
                        ainfo.play.minordev = 4 + (minor << SPARCAUDIO_DEVICE_SHIFT);
                        ainfo.play.open = (drv->flags & SDF_OPEN_WRITE);
                        ainfo.play.active = drv->output_active;
	    
                        if (drv->ops->get_monitor_volume)
                                ainfo.monitor_gain =
                                        drv->ops->get_monitor_volume(drv);
                        else
                                ainfo.monitor_gain = (0);

                        if (drv->ops->get_output_muted)
                                ainfo.output_muted = 
                                        (unsigned char)drv->ops->get_output_muted(drv);
                        else
                                ainfo.output_muted = (unsigned char)(0);

                        retval = verify_area(VERIFY_WRITE, (void *)arg,
                                             sizeof(struct audio_info));
                        if (retval < 0)
                                break;

                        copy_to_user((struct audio_info *)arg, &ainfo, sizeof(ainfo));
                        break;
                case AUDIO_SETINFO:
                {
                        audio_info_t curinfo, newinfo;
	      
                        if (verify_area(VERIFY_READ, (audio_info_t *)arg, 
                                        sizeof(audio_info_t))) {
                                dprintk(("verify_area failed\n"));
                                return -EINVAL;
                        }
                        copy_from_user(&ainfo, (audio_info_t *)arg, sizeof(audio_info_t));

                        /* Without these there's no point in trying */
                        if (!drv->ops->get_input_precision ||
                            !drv->ops->get_input_channels ||
                            !drv->ops->get_input_rate ||
                            !drv->ops->get_input_encoding ||
                            !drv->ops->get_output_precision ||
                            !drv->ops->get_output_channels ||
                            !drv->ops->get_output_rate ||
                            !drv->ops->get_output_encoding) {
                                eprintk(("missing get routines: failed\n"));
                                retval = -EINVAL;
                                break;
                        }

                        /* Do bounds checking for things which always apply.
                         * Follow with enforcement of basic tenets of certain
                         * encodings. Everything over and above generic is
                         * enforced by the driver, which can assume that
                         * Martian cases are taken care of here.
                         */
                        if (Modify(ainfo.play.gain) && 
                            ((ainfo.play.gain > AUDIO_MAX_GAIN) || 
                             (ainfo.play.gain < AUDIO_MIN_GAIN))) {
                                /* Need to differentiate this from e.g. the above error */
                                eprintk(("play gain bounds: failed %d\n", ainfo.play.gain));
                                retval = -EINVAL;
                                break;
                        }
                        if (Modify(ainfo.record.gain) &&
                            ((ainfo.record.gain > AUDIO_MAX_GAIN) ||
                             (ainfo.record.gain < AUDIO_MIN_GAIN))) {
                                eprintk(("rec gain bounds: failed %d\n", ainfo.record.gain));
                                retval = -EINVAL;
                                break;
                        }
                        if (Modify(ainfo.monitor_gain) &&
                            ((ainfo.monitor_gain > AUDIO_MAX_GAIN) ||
                             (ainfo.monitor_gain < AUDIO_MIN_GAIN))) {
                                eprintk(("monitor gain bounds: failed\n"));
                                retval = -EINVAL;
                                break;
                        }

                        /* Don't need to check less than zero on these */
                        if (Modifyc(ainfo.play.balance) &&
                            (ainfo.play.balance > AUDIO_RIGHT_BALANCE)) {
                                eprintk(("play balance bounds: %d failed\n", 
                                         (int)ainfo.play.balance));
                                retval = -EINVAL;
                                break;
                        }
                        if (Modifyc(ainfo.record.balance) &&
                            (ainfo.record.balance > AUDIO_RIGHT_BALANCE)) {
                                eprintk(("rec balance bounds: failed\n"));
                                retval = -EINVAL;
                                break;
                        }
	      
                        /* If any of these changed, record them all, then make
                         * changes atomically. If something fails, back it all out.
                         */
                        if (Modify(ainfo.record.precision) || 
                            Modify(ainfo.record.sample_rate) ||
                            Modify(ainfo.record.channels) ||
                            Modify(ainfo.record.encoding) || 
                            Modify(ainfo.play.precision) || 
                            Modify(ainfo.play.sample_rate) ||
                            Modify(ainfo.play.channels) ||
                            Modify(ainfo.play.encoding)) {
                                /* If they're trying to change something we
                                 * have no routine for, they lose.
                                 */
                                if ((!drv->ops->set_input_encoding && 
                                     Modify(ainfo.record.encoding)) ||
                                    (!drv->ops->set_input_rate && 
                                     Modify(ainfo.record.sample_rate)) ||
                                    (!drv->ops->set_input_precision && 
                                     Modify(ainfo.record.precision)) ||
                                    (!drv->ops->set_input_channels && 
                                     Modify(ainfo.record.channels))) {
                                        eprintk(("rec set no routines: failed\n"));
                                        retval = -EINVAL;
                                        break;
                                }		  
		  
                                curinfo.record.encoding = 
                                        drv->ops->get_input_encoding(drv);
                                curinfo.record.sample_rate = 
                                        drv->ops->get_input_rate(drv);	   
                                curinfo.record.precision = 
                                        drv->ops->get_input_precision(drv);	   
                                curinfo.record.channels = 
                                        drv->ops->get_input_channels(drv);	   
                                newinfo.record.encoding =
                                        Modify(ainfo.record.encoding) ? 
                                        ainfo.record.encoding :
                                        curinfo.record.encoding;
                                newinfo.record.sample_rate =
                                        Modify(ainfo.record.sample_rate) ?
                                        ainfo.record.sample_rate :
                                        curinfo.record.sample_rate;
                                newinfo.record.precision =
                                        Modify(ainfo.record.precision) ? 
                                        ainfo.record.precision :
                                        curinfo.record.precision;
                                newinfo.record.channels =
                                        Modify(ainfo.record.channels) ? 
                                        ainfo.record.channels :
                                        curinfo.record.channels;
		    
                                switch (newinfo.record.encoding) {
                                case AUDIO_ENCODING_ALAW:
                                case AUDIO_ENCODING_ULAW:
                                        if (newinfo.record.precision != 8) {
                                                eprintk(("rec law precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.record.channels != 1) {
                                                eprintk(("rec law channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        break;
                                case AUDIO_ENCODING_LINEAR:
                                case AUDIO_ENCODING_LINEARLE:
                                        if (newinfo.record.precision != 16) {
                                                eprintk(("rec lin precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.record.channels != 1 &&
                                            newinfo.record.channels != 2) {
                                                eprintk(("rec lin channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        break;
                                case AUDIO_ENCODING_LINEAR8:
                                        if (newinfo.record.precision != 8) {
                                                eprintk(("rec lin8 precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.record.channels != 1 && 
                                            newinfo.record.channels != 2) {
                                                eprintk(("rec lin8 channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                };
		  
                                if (retval < 0)
                                        break;
		  
                                /* If they're trying to change something we
                                 * have no routine for, they lose.
                                 */
                                if ((!drv->ops->set_output_encoding && 
                                     Modify(ainfo.play.encoding)) ||
                                    (!drv->ops->set_output_rate && 
                                     Modify(ainfo.play.sample_rate)) ||
                                    (!drv->ops->set_output_precision && 
                                     Modify(ainfo.play.precision)) ||
                                    (!drv->ops->set_output_channels && 
                                     Modify(ainfo.play.channels))) {
                                        eprintk(("play set no routine: failed\n"));
                                        retval = -EINVAL;
                                        break;
                                }		  
		  
                                curinfo.play.encoding = 
                                        drv->ops->get_output_encoding(drv);
                                curinfo.play.sample_rate = 
                                        drv->ops->get_output_rate(drv);	   
                                curinfo.play.precision = 
                                        drv->ops->get_output_precision(drv);	   
                                curinfo.play.channels = 
                                        drv->ops->get_output_channels(drv);	   
                                newinfo.play.encoding =
                                        Modify(ainfo.play.encoding) ? 
                                        ainfo.play.encoding :
                                                curinfo.play.encoding;
                                newinfo.play.sample_rate =
                                        Modify(ainfo.play.sample_rate) ? 
                                        ainfo.play.sample_rate :
                                                curinfo.play.sample_rate;
                                newinfo.play.precision =
                                        Modify(ainfo.play.precision) ? 
                                        ainfo.play.precision :
                                                curinfo.play.precision;
                                newinfo.play.channels =
                                        Modify(ainfo.play.channels) ? 
                                        ainfo.play.channels :
                                                curinfo.play.channels;
		  
                                switch (newinfo.play.encoding) {
                                case AUDIO_ENCODING_ALAW:
                                case AUDIO_ENCODING_ULAW:
                                        if (newinfo.play.precision != 8) {
                                                eprintk(("play law precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.play.channels != 1) {
                                                eprintk(("play law channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        break;
                                case AUDIO_ENCODING_LINEAR:
                                case AUDIO_ENCODING_LINEARLE:
                                        if (newinfo.play.precision != 16) {
                                                eprintk(("play lin precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.play.channels != 1 && 
                                            newinfo.play.channels != 2) {
                                                eprintk(("play lin channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        break;
                                case AUDIO_ENCODING_LINEAR8:
                                        if (newinfo.play.precision != 8) {
                                                eprintk(("play lin8 precision bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                        if (newinfo.play.channels != 1 && 
                                            newinfo.play.channels != 2) {
                                                eprintk(("play lin8 channel bounds: "
                                                         "failed\n"));
                                                retval = -EINVAL;
                                                break;
                                        }
                                };
		  
                                if (retval < 0)
                                        break;
		  
                                /* If we got this far, we're at least sane with
                                 * respect to generics. Try the changes.
                                 */
                                if ((drv->ops->set_input_channels &&
                                     (drv->ops->set_input_channels(drv, 
                                                                   newinfo.record.channels)
                                      < 0)) ||
                                    (drv->ops->set_output_channels &&
                                     (drv->ops->set_output_channels(drv, 
                                                                    newinfo.play.channels)
                                      < 0)) ||
                                    (drv->ops->set_input_rate &&
                                     (drv->ops->set_input_rate(drv, 
                                                               newinfo.record.sample_rate) 
                                      < 0)) ||
                                    (drv->ops->set_output_rate &&
                                     (drv->ops->set_output_rate(drv, 
                                                                newinfo.play.sample_rate) 
                                      < 0)) ||
                                    (drv->ops->set_input_precision &&
                                     (drv->ops->set_input_precision(drv, 
                                                                    newinfo.record.precision)
                                      < 0)) ||
                                    (drv->ops->set_output_precision &&
                                     (drv->ops->set_output_precision(drv, 
                                                                     newinfo.play.precision)
                                      < 0)) ||
                                    (drv->ops->set_input_encoding &&
                                     (drv->ops->set_input_encoding(drv, 
                                                                   newinfo.record.encoding)
                                      < 0)) ||
                                    (drv->ops->set_output_encoding &&
                                     (drv->ops->set_output_encoding(drv, 
                                                                    newinfo.play.encoding)
                                      < 0))) 
                                {
                                        dprintk(("setting format: failed\n"));
                                        /* Pray we can set it all back. If not, uh... */
                                        if (drv->ops->set_input_channels)
                                                drv->ops->set_input_channels(drv, 
						     curinfo.record.channels);
                                        if (drv->ops->set_output_channels)
                                                drv->ops->set_output_channels(drv, 
                                                                              curinfo.play.channels);
                                        if (drv->ops->set_input_rate)
                                                drv->ops->set_input_rate(drv, 
                                                                         curinfo.record.sample_rate);
                                        if (drv->ops->set_output_rate)
                                                drv->ops->set_output_rate(drv, 
                                                                          curinfo.play.sample_rate);
                                        if (drv->ops->set_input_precision)
                                                drv->ops->set_input_precision(drv, 
                                                                              curinfo.record.precision);
                                        if (drv->ops->set_output_precision)
                                                drv->ops->set_output_precision(drv, 
                                                                               curinfo.play.precision);
                                        if (drv->ops->set_input_encoding)
                                                drv->ops->set_input_encoding(drv, 
                                                                             curinfo.record.encoding);
                                        if (drv->ops->set_output_encoding)
                                                drv->ops->set_output_encoding(drv, 
                                                                              curinfo.play.encoding);
                                        retval = -EINVAL;
                                        break;
                                }
                        }
                        
                        if (retval < 0)
                                break;
                        
                        newinfo.record.balance =
                                __sparcaudio_if_setc_do(drv, 
                                                        drv->ops->set_input_balance, 
                                                        drv->ops->get_input_balance,
                                                        ainfo.record.balance);
                        newinfo.play.balance =
                                __sparcaudio_if_setc_do(drv, 
                                                        drv->ops->set_output_balance, 
                                                        drv->ops->get_output_balance,
                                                        ainfo.play.balance);
                        newinfo.record.error =
                                __sparcaudio_if_setc_do(drv, 
                                                        drv->ops->set_input_error, 
                                                        drv->ops->get_input_error,
                                                        ainfo.record.error);
                        newinfo.play.error =
                                __sparcaudio_if_setc_do(drv, 
                                                        drv->ops->set_output_error, 
                                                        drv->ops->get_output_error,
                                                        ainfo.play.error);
                        newinfo.output_muted =
                                __sparcaudio_if_setc_do(drv, 
                                                        drv->ops->set_output_muted, 
                                                        drv->ops->get_output_muted,
                                                        ainfo.output_muted);
                        newinfo.record.gain =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_input_volume, 
                                                       drv->ops->get_input_volume,
                                                       ainfo.record.gain);
                        newinfo.play.gain =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_output_volume, 
                                                       drv->ops->get_output_volume,
                                                       ainfo.play.gain);
                        newinfo.record.port =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_input_port, 
                                                       drv->ops->get_input_port,
                                                       ainfo.record.port);
                        newinfo.play.port =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_output_port, 
                                                       drv->ops->get_output_port,
                                                       ainfo.play.port);
                        newinfo.record.samples =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_input_samples, 
                                                       drv->ops->get_input_samples,
                                                       ainfo.record.samples);
                        newinfo.play.samples =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_output_samples, 
                                                       drv->ops->get_output_samples,
                                                       ainfo.play.samples);
                        newinfo.monitor_gain =
                                __sparcaudio_if_set_do(drv, 
                                                       drv->ops->set_monitor_volume, 
                                                       drv->ops->get_monitor_volume,
                                                       ainfo.monitor_gain);

                        if (Modify(ainfo.record.buffer_size)) {
                                /* Should sanity check this */
                                newinfo.record.buffer_size = ainfo.record.buffer_size;
                                drv->buffer_size = ainfo.record.buffer_size;
                        } else {
                                newinfo.record.buffer_size = drv->buffer_size;
                        }

                        if (Modify(ainfo.play.eof)) {
                                ainfo.play.eof = newinfo.play.eof;
                                newinfo.play.eof = drv->output_eof;
                                drv->output_eof = ainfo.play.eof;
                        } else {
                                newinfo.play.eof = drv->output_eof;
                        }		

                        if (drv->flags & SDF_OPEN_READ) {
                                newinfo.record.pause =
                                        __sparcaudio_if_setc_do(drv, 
                                                                drv->ops->set_input_pause, 
                                                                drv->ops->get_input_pause,
                                                                ainfo.record.pause);
                        } else if (drv->ops->get_input_pause) {
                                newinfo.record.pause = drv->ops->get_input_pause(drv);
                        } else {
                                newinfo.record.pause = 0;
                        }

                        if (drv->flags & SDF_OPEN_WRITE) {
                                newinfo.play.pause =
                                        __sparcaudio_if_setc_do(drv, 
                                                                drv->ops->set_output_pause, 
                                                                drv->ops->get_output_pause,
                                                                ainfo.play.pause);
                        } else if (drv->ops->get_output_pause) {
                                newinfo.play.pause = drv->ops->get_output_pause(drv);
                        } else {
                                newinfo.play.pause = 0;
                        }
	      
                        retval = verify_area(VERIFY_WRITE, (void *)arg,
                                             sizeof(struct audio_info));

                        /* Even if we fail, if we made changes let's try notification */
                        if (!retval) 
                                copy_to_user((struct audio_info *)arg, &newinfo, 
                                             sizeof(newinfo));
	    
#ifdef REAL_AUDIO_SIGNALS
                        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);
#endif
                        break;
                }
	  
                default:
                        if (drv->ops->ioctl)
                                retval = drv->ops->ioctl(inode,file,cmd,arg,drv);
                        else
                                retval = -EINVAL;
                };
                break;
	case SPARCAUDIO_STATUS_MINOR:
                eprintk(("status minor not yet implemented\n"));
                retval = -EINVAL;
	default:
                eprintk(("unknown minor device number\n"));
                retval = -EINVAL;
	}
	
	return retval;
}

static struct file_operations sparcaudioctl_fops = {
	owner:		THIS_MODULE,
	poll:		sparcaudio_poll,
	ioctl:		sparcaudio_ioctl,
};

static int sparcaudio_open(struct inode * inode, struct file * file)
{
        int minor = MINOR(inode->i_rdev);
	struct sparcaudio_driver *drv = 
                drivers[(minor >> SPARCAUDIO_DEVICE_SHIFT)];
	int err;

	/* A low-level audio driver must exist. */
	if (!drv)
		return -ENODEV;

#ifdef S_ZERO_WR
        /* This is how 2.0 ended up dealing with 0 len writes */
        inode->i_flags |= S_ZERO_WR;
#endif

	switch (minor & 0xf) {
	case SPARCAUDIO_AUDIOCTL_MINOR:
                file->f_op = &sparcaudioctl_fops;
                break;
	case SPARCAUDIO_DSP16_MINOR:
	case SPARCAUDIO_DSP_MINOR:
	case SPARCAUDIO_AUDIO_MINOR:
                /* If the driver is busy, then wait to get through. */
        retry_open:
        	if (file->f_mode & FMODE_READ && drv->flags & SDF_OPEN_READ) {
                        if (file->f_flags & O_NONBLOCK)
                                return -EBUSY;

                        /* If something is now waiting, signal control device */
                        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);

                        interruptible_sleep_on(&drv->open_wait);
                        if (signal_pending(current))
                                return -EINTR;
                        goto retry_open;
                }
                if (file->f_mode & FMODE_WRITE && drv->flags & SDF_OPEN_WRITE) {
                        if (file->f_flags & O_NONBLOCK)
                                return -EBUSY;
	    
                        /* If something is now waiting, signal control device */
                        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);

                        interruptible_sleep_on(&drv->open_wait);
                        if (signal_pending(current))
                                return -EINTR;
                        goto retry_open;
                }

                /* Allow the low-level driver to initialize itself. */
                if (drv->ops->open) {
                        err = drv->ops->open(inode,file,drv);
                        if (err < 0)
                                return err;
                }

                /* Mark the driver as locked for read and/or write. */
                if (file->f_mode & FMODE_READ) {
                        drv->input_offset = 0;
                        drv->input_front = 0;
                        drv->input_rear = 0;
                        drv->input_count = 0;
                        drv->input_size = 0;
                        drv->recording_count = 0;

                        /* Clear pause */
                        if (drv->ops->set_input_pause)
                                drv->ops->set_input_pause(drv, 0); 
                        drv->ops->start_input(drv, drv->input_buffers[drv->input_front],
                                              drv->input_buffer_size);
                        drv->input_active = 1;
                        drv->flags |= SDF_OPEN_READ;
                }

                if (file->f_mode & FMODE_WRITE) {
                        drv->output_offset = 0;
                        drv->output_eof = 0;
                        drv->playing_count = 0;
                        drv->output_size = 0;
                        drv->output_front = 0;
                        drv->output_rear = 0;
                        drv->output_count = 0;
                        drv->output_active = 0;

                        /* Clear pause */
                        if (drv->ops->set_output_pause)
                                drv->ops->set_output_pause(drv, 0); 
                        drv->flags |= SDF_OPEN_WRITE;
                }  

                break;
	case SPARCAUDIO_MIXER_MINOR:     
                file->f_op = &sparcaudioctl_fops;
                break;

	default:
                return -ENXIO;
	};

        /* From the dbri driver:
         * SunOS 5.5.1 audio(7I) man page says:
         * "Upon the initial open() of the audio device, the driver
         *  will reset the data format of the device to the default
         *  state of 8-bit, 8KHz, mono u-law data."
         *
         * Of course, we only do this for /dev/audio, and assume
         * OSS semantics on /dev/dsp
         */

	if ((minor & 0xf) == SPARCAUDIO_AUDIO_MINOR) {
                if (file->f_mode & FMODE_WRITE) {
                        if (drv->ops->set_output_channels)
                                drv->ops->set_output_channels(drv, 1);
                        if (drv->ops->set_output_encoding)
                                drv->ops->set_output_encoding(drv, AUDIO_ENCODING_ULAW);
                        if (drv->ops->set_output_rate)
                                drv->ops->set_output_rate(drv, 8000);
                }          

                if (file->f_mode & FMODE_READ) {
                        if (drv->ops->set_input_channels)
                                drv->ops->set_input_channels(drv, 1);
                        if (drv->ops->set_input_encoding)
                                drv->ops->set_input_encoding(drv, AUDIO_ENCODING_ULAW);
                        if (drv->ops->set_input_rate)
                                drv->ops->set_input_rate(drv, 8000);
                }          
        }

	/* Success! */
	return 0;
}

static int sparcaudio_release(struct inode * inode, struct file * file)
{
        struct sparcaudio_driver *drv = drivers[(MINOR(inode->i_rdev) >>
                                                 SPARCAUDIO_DEVICE_SHIFT)];

	lock_kernel();
        if (file->f_mode & FMODE_READ) {
                /* Stop input */
                drv->ops->stop_input(drv);
                drv->input_active = 0;
        }

        if (file->f_mode & FMODE_WRITE) {
                /* Anything in the queue? */
                if (drv->output_offset) {
                        drv->output_offset = 0;
                        drv->output_rear = (drv->output_rear + 1)
                                % drv->num_output_buffers;
                        drv->output_count++;
                }
                sparcaudio_sync_output(drv);

                /* Wait for any output still in the queue to be played. */
                if ((drv->output_count > 0) || (drv->playing_count > 0))
                        interruptible_sleep_on(&drv->output_drain_wait);

                /* Force any output to be stopped. */
                drv->ops->stop_output(drv);
                drv->output_active = 0;
                drv->playing_count = 0;
                drv->output_eof = 0;
        }

        /* Let the low-level driver do any release processing. */
        if (drv->ops->release)
                drv->ops->release(inode,file,drv);

        if (file->f_mode & FMODE_READ)
                drv->flags &= ~(SDF_OPEN_READ);

        if (file->f_mode & FMODE_WRITE) 
                drv->flags &= ~(SDF_OPEN_WRITE);

        /* Status changed. Signal control device */
        kill_procs(drv->sd_siglist,SIGPOLL,S_MSG);

        wake_up_interruptible(&drv->open_wait);
	unlock_kernel();

        return 0;
}

static struct file_operations sparcaudio_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		sparcaudio_read,
	write:		sparcaudio_write,
	poll:		sparcaudio_poll,
	ioctl:		sparcaudio_ioctl,
	open:		sparcaudio_open,
	release:	sparcaudio_release,
};

static struct {
	unsigned short minor;
	char *name;
	umode_t mode;
} dev_list[] = {
	{ SPARCAUDIO_MIXER_MINOR, "mixer", S_IWUSR | S_IRUGO },
	{ SPARCAUDIO_DSP_MINOR, "dsp", S_IWUGO | S_IRUSR | S_IRGRP },
	{ SPARCAUDIO_AUDIO_MINOR, "audio", S_IWUGO | S_IRUSR | S_IRGRP },
	{ SPARCAUDIO_DSP16_MINOR, "dspW", S_IWUGO | S_IRUSR | S_IRGRP },
	{ SPARCAUDIO_STATUS_MINOR, "status", S_IRUGO },
	{ SPARCAUDIO_AUDIOCTL_MINOR, "audioctl", S_IRUGO }
};

static void sparcaudio_mkname (char *buf, char *name, int dev)
{
        if (dev)
                sprintf (buf, "%s%d", name, dev);
        else
                sprintf (buf, "%s", name);
}

int register_sparcaudio_driver(struct sparcaudio_driver *drv, int duplex)
{
	int i, dev;
	unsigned short minor;
	char name_buf[32];

	/* If we've used up SPARCAUDIO_MAX_DEVICES, fail */
	for (dev = 0; dev < SPARCAUDIO_MAX_DEVICES; dev++) {
                if (drivers[dev] == NULL)
                        break;
	}

	if (drivers[dev])
                return -EIO;

	/* Ensure that the driver has a proper operations structure. */
	if (!drv->ops || !drv->ops->start_output || !drv->ops->stop_output ||
	    !drv->ops->start_input || !drv->ops->stop_input)
		return -EINVAL;

        /* Register ourselves with devfs */
	for (i=0; i < sizeof (dev_list) / sizeof (*dev_list); i++) {
		sparcaudio_mkname (name_buf, dev_list[i].name, dev);
		minor = (dev << SPARCAUDIO_DEVICE_SHIFT) | dev_list[i].minor;
		devfs_register (devfs_handle, name_buf, DEVFS_FL_NONE,
				SOUND_MAJOR, minor, S_IFCHR | dev_list[i].mode,
				&sparcaudio_fops, NULL);
	}

        /* Setup the circular queues of output and input buffers
         *
         * Each buffer is a single page, but output buffers might
         * be partially filled (by a write with count < output_buffer_size),
         * so each output buffer also has a paired output size.
         *
         * Input buffers, on the other hand, always fill completely,
         * so we don't need input counts - each contains input_buffer_size
         * bytes of audio data.
         *
         * TODO: Make number of input/output buffers tunable parameters
         */

        init_waitqueue_head(&drv->open_wait);
        init_waitqueue_head(&drv->output_write_wait);
        init_waitqueue_head(&drv->output_drain_wait);
        init_waitqueue_head(&drv->input_read_wait);

        drv->num_output_buffers = 8;
	drv->output_buffer_size = (4096 * 2);
	drv->playing_count = 0;
	drv->output_offset = 0;
	drv->output_eof = 0;
        drv->output_front = 0;
        drv->output_rear = 0;
        drv->output_count = 0;
        drv->output_active = 0;
        drv->output_buffers = kmalloc(drv->num_output_buffers * 
				      sizeof(__u8 *), GFP_KERNEL);
        drv->output_sizes = kmalloc(drv->num_output_buffers * 
				    sizeof(size_t), GFP_KERNEL);
        drv->output_notify = kmalloc(drv->num_output_buffers * 
				    sizeof(char), GFP_KERNEL);
        if (!drv->output_buffers || !drv->output_sizes || !drv->output_notify)
                goto kmalloc_failed1;

	drv->output_buffer = kmalloc((drv->output_buffer_size * 
                                      drv->num_output_buffers),
                                     GFP_KERNEL);
	if (!drv->output_buffer)
                goto kmalloc_failed2;

        /* Allocate the pages for each output buffer. */
        for (i = 0; i < drv->num_output_buffers; i++) {
	        drv->output_buffers[i] = (void *)(drv->output_buffer + 
                                                  (i * drv->output_buffer_size));
		drv->output_sizes[i] = 0;
		drv->output_notify[i] = 0;
        }

        /* Setup the circular queue of input buffers. */
        drv->num_input_buffers = 8;
	drv->input_buffer_size = (4096 * 2);
	drv->recording_count = 0;
        drv->input_front = 0;
        drv->input_rear = 0;
        drv->input_count = 0;
	drv->input_offset = 0;
        drv->input_size = 0;
        drv->input_active = 0;
        drv->input_buffers = kmalloc(drv->num_input_buffers * sizeof(__u8 *),
				     GFP_KERNEL);
        drv->input_sizes = kmalloc(drv->num_input_buffers * 
                                   sizeof(size_t), GFP_KERNEL);
        if (!drv->input_buffers || !drv->input_sizes)
                goto kmalloc_failed3;

        /* Allocate the pages for each input buffer. */
	if (duplex == 1) {
                drv->input_buffer = kmalloc((drv->input_buffer_size * 
                                             drv->num_input_buffers), 
                                            GFP_DMA);
                if (!drv->input_buffer)
                        goto kmalloc_failed4;

                for (i = 0; i < drv->num_input_buffers; i++)
                        drv->input_buffers[i] = (void *)(drv->input_buffer + 
                                                         (i * drv->input_buffer_size));
	} else {
                if (duplex == 2) {
                        drv->input_buffer = drv->output_buffer;
                        drv->input_buffer_size = drv->output_buffer_size;
                        drv->num_input_buffers = drv->num_output_buffers;
                        for (i = 0; i < drv->num_input_buffers; i++) 
                                drv->input_buffers[i] = drv->output_buffers[i];
                } else {
                        for (i = 0; i < drv->num_input_buffers; i++) 
                                drv->input_buffers[i] = NULL;
                }
	}

	/* Take note of our duplexity */
	drv->duplex = duplex;

	/* Ensure that the driver is marked as not being open. */
	drv->flags = 0;

	MOD_INC_USE_COUNT;

	/* Take driver slot, note which we took */
	drv->index = dev;
	drivers[dev] = drv;

	return 0;

kmalloc_failed4:
	kfree(drv->input_buffer);

kmalloc_failed3:
        if (drv->input_sizes)
                kfree(drv->input_sizes);
        if (drv->input_buffers)
                kfree(drv->input_buffers);
        i = drv->num_output_buffers;

kmalloc_failed2:
	kfree(drv->output_buffer);

kmalloc_failed1:
        if (drv->output_buffers)
                kfree(drv->output_buffers);
        if (drv->output_sizes)
                kfree(drv->output_sizes);
        if (drv->output_notify)
                kfree(drv->output_notify);

        return -ENOMEM;
}

int unregister_sparcaudio_driver(struct sparcaudio_driver *drv, int duplex)
{
	devfs_handle_t de;
	int i;
	char name_buf[32];

	/* Figure out which driver is unregistering */
	if (drivers[drv->index] != drv)
		return -EIO;

	/* Deallocate the queue of output buffers. */
	kfree(drv->output_buffer);
	kfree(drv->output_buffers);
	kfree(drv->output_sizes);
	kfree(drv->output_notify);

	/* Deallocate the queue of input buffers. */
	if (duplex == 1) {
                kfree(drv->input_buffer);
                kfree(drv->input_sizes);
	}
	kfree(drv->input_buffers);

	if (&(drv->sd_siglist) != NULL)
                lis_free_elist( &(drv->sd_siglist) );

	/* Unregister ourselves with devfs */
	for (i=0; i < sizeof (dev_list) / sizeof (*dev_list); i++) {
		sparcaudio_mkname (name_buf, dev_list[i].name, drv->index);
		de = devfs_find_handle (devfs_handle, name_buf, 0, 0,
					DEVFS_SPECIAL_CHR, 0);
		devfs_unregister (de);
	}

	MOD_DEC_USE_COUNT;

	/* Null the appropriate driver */
	drivers[drv->index] = NULL;

	return 0;
}

EXPORT_SYMBOL(register_sparcaudio_driver);
EXPORT_SYMBOL(unregister_sparcaudio_driver);
EXPORT_SYMBOL(sparcaudio_output_done);
EXPORT_SYMBOL(sparcaudio_input_done);

static int __init sparcaudio_init(void)
{
	/* Register our character device driver with the VFS. */
	if (devfs_register_chrdev(SOUND_MAJOR, "sparcaudio", &sparcaudio_fops))
		return -EIO;

	devfs_handle = devfs_mk_dir (NULL, "sound", NULL);
	return 0;
}

static void __exit sparcaudio_exit(void)
{
	devfs_unregister_chrdev(SOUND_MAJOR, "sparcaudio");
	devfs_unregister (devfs_handle);
}

module_init(sparcaudio_init);
module_exit(sparcaudio_exit);
MODULE_LICENSE("GPL");

/*
 * Code from Linux Streams, Copyright 1995 by
 * Graham Wheeler, Francisco J. Ballesteros, Denis Froschauer
 * and available under GPL 
 */

static int
lis_add_to_elist( strevent_t **list, pid_t pid, short events )
{
        strevent_t *ev = NULL;

        if (*list != NULL) {
                for (ev = (*list)->se_next;
                     ev != *list && ev->se_pid < pid;
                     ev = ev->se_next)
                        ;
        }

        if (ev == NULL || ev == *list) {             /* no slot for pid in list */
                ev = (strevent_t *) kmalloc(sizeof(strevent_t), GFP_KERNEL);
                if (ev == NULL)
                        return(-ENOMEM);

                if (!*list) {                   /* create dummy head node */
                        strevent_t *hd;

                        hd = (strevent_t *) kmalloc(sizeof(strevent_t), GFP_KERNEL);
                        if (hd == NULL) {
                                kfree(ev);
                                return(-ENOMEM);
                        }
                        (*list = hd)->se_pid = 0;
                        hd->se_next = hd->se_prev = hd;         /* empty list */
                }

                /* link node last in the list */
                ev->se_prev = (*list)->se_prev;
                (*list)->se_prev->se_next = ev;
                ((*list)->se_prev = ev)->se_next = *list;

                ev->se_pid = pid;
                ev->se_evs = 0;
        } else if (ev->se_pid != pid) {  /* link node in the middle of the list */
                strevent_t *new;

                new = (strevent_t *) kmalloc(sizeof(strevent_t), GFP_KERNEL);
                if (new == NULL)
                        return -ENOMEM;

                new->se_prev = ev->se_prev;
                new->se_next = ev;
                ev->se_prev->se_next = new;
                ev->se_prev = new;
                ev = new;                              /* use new element */
                ev->se_pid = pid;
                ev->se_evs = 0;
        }

        ev->se_evs |= events;
        return 0;
}

static int
lis_del_from_elist( strevent_t **list, pid_t pid, short events )
{
        strevent_t *ev = NULL;     

        if (*list != NULL) {
                for (ev = (*list)->se_next;
                     ev != *list && ev->se_pid < pid;
                     ev = ev->se_next)
                        ;
        }

        if (ev == NULL || ev == *list || ev->se_pid != pid)
                return 1;

        if ((ev->se_evs &= ~events) == 0) {        /* unlink */
                if (ev->se_next)                        /* should always be true */
                        ev->se_next->se_prev = ev->se_prev;
                if (ev->se_prev)                        /* should always be true */
                        ev->se_prev->se_next = ev->se_next;
                kfree(ev);
        }
        return 0;
}

static void
lis_free_elist( strevent_t **list )
{
        strevent_t  *ev;     
        strevent_t  *nxt;

        for (ev = *list; ev != NULL; ) {
                nxt = ev->se_next;
                kfree(ev);
                ev = nxt;
                if (ev == *list)
                        break;                /* all done */
        }

        *list = NULL;
}

static short
lis_get_elist_ent( strevent_t *list, pid_t pid )
{
        strevent_t *ev = NULL;

        if (list == NULL)
                return 0;

        for(ev = list->se_next ; ev != list && ev->se_pid < pid; ev = ev->se_next)
                ;
        if (ev != list && ev->se_pid == pid)
                return ev->se_evs;
        else
                return 0;
}

static void 
kill_procs( struct strevent *elist, int sig, short e)
{
        strevent_t *ev;
        int res;

        if (elist) {
                for(ev = elist->se_next ; ev != elist; ev = ev->se_next)
                        if ((ev->se_evs & e) != 0) {
                                res = kill_proc(ev->se_pid, SIGPOLL, 1);

                                if (res < 0) {
                                        if (res == -3) {
                                                lis_del_from_elist(&elist,
                                                                   ev->se_pid,
                                                                   S_ALL);
                                                continue;
                                        }
                                        dprintk(("kill_proc: errno %d\n",res));
                                }
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

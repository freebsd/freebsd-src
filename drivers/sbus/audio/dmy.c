/* $Id: dmy.c,v 1.10 2001/10/08 22:19:50 davem Exp $
 * drivers/sbus/audio/dummy.c
 *
 * Copyright 1998 Derrick J Brashear (shadow@andrew.cmu.edu)
 *
 * This is a dummy lowlevel driver. Consider it a distant cousin of 
 * /proc/audio; It pretends to be a piece of audio hardware, and writes
 * to a file instead. (or will shortly)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/soundcard.h>
#include <linux/delay.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/sbus.h>

#include <asm/audioio.h>
#include "dummy.h"

#define MAX_DRIVERS 1
static struct sparcaudio_driver drivers[MAX_DRIVERS];
static int num_drivers;

static int dummy_play_gain(struct sparcaudio_driver *drv, int value, 
                            unsigned char balance);
static int dummy_record_gain(struct sparcaudio_driver *drv, int value, 
                            unsigned char balance);
static int dummy_output_muted(struct sparcaudio_driver *drv, int value);
static int dummy_attach(struct sparcaudio_driver *drv) __init;

static int
dummy_set_output_encoding(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (value != 0) {
                dummy_chip->perchip_info.play.encoding = value;
                return 0;
        }
        return -EINVAL;
}

static int
dummy_set_input_encoding(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (value != 0) {
                dummy_chip->perchip_info.record.encoding = value;
                return 0;
        }
        return -EINVAL;
}

static int dummy_get_output_encoding(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.encoding;
}

static int dummy_get_input_encoding(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.encoding;
}

static int
dummy_set_output_rate(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (value != 0) {
                dummy_chip->perchip_info.play.sample_rate = value;
                return 0;
        }
        return -EINVAL;
}

static int
dummy_set_input_rate(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (value != 0) {
                dummy_chip->perchip_info.record.sample_rate = value;
                return 0;
        }
        return -EINVAL;
}

static int dummy_get_output_rate(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.sample_rate;
}

static int dummy_get_input_rate(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.sample_rate;
}

/* Generically we support 4 channels. This does 2 */
static int
dummy_set_output_channels(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        switch (value) {
        case 1:
        case 2:
                break;
        default:
                return -(EINVAL);
        };

        dummy_chip->perchip_info.play.channels = value;
        return 0;
}

/* Generically we support 4 channels. This does 2 */
static int
dummy_set_input_channels(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        switch (value) {
        case 1:
        case 2:
                break;
        default:
                return -(EINVAL);
        };

        dummy_chip->perchip_info.record.channels = value;
        return 0;
}

static int dummy_get_input_channels(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.channels;
}

static int dummy_get_output_channels(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.channels;
}

static int dummy_get_output_precision(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.precision;
}

static int dummy_get_input_precision(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.precision;
}

static int dummy_set_output_precision(struct sparcaudio_driver *drv, int val)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private; 

        dummy_chip->perchip_info.play.precision = val;
        return dummy_chip->perchip_info.play.precision;
}

static int dummy_set_input_precision(struct sparcaudio_driver *drv, int val)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private; 

        dummy_chip->perchip_info.record.precision = val;
        return dummy_chip->perchip_info.record.precision;
}

/* Set output mute */
static int dummy_output_muted(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (!value) 
                dummy_chip->perchip_info.output_muted = 0;
        else
                dummy_chip->perchip_info.output_muted = 1;

        return 0;
}

static int dummy_get_output_muted(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.output_muted;
}

static int dummy_get_formats(struct sparcaudio_driver *drv)
{
        return (AFMT_MU_LAW | AFMT_A_LAW |
                AFMT_U8 | AFMT_IMA_ADPCM | 
                AFMT_S16_LE | AFMT_S16_BE);
}

static int dummy_get_output_ports(struct sparcaudio_driver *drv)
{
        return (AUDIO_LINE_OUT | AUDIO_SPEAKER | AUDIO_HEADPHONE);
}

static int dummy_get_input_ports(struct sparcaudio_driver *drv)
{
        return (AUDIO_ANALOG_LOOPBACK);
}

/* Set chip "output" port */
static int dummy_set_output_port(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.play.port = value;
        return value;
}

static int dummy_set_input_port(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.record.port = value;
        return value;
}

static int dummy_get_output_port(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.port;
}

static int dummy_get_input_port(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.port;
}

static int dummy_get_output_error(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.play.error;
}

static int dummy_get_input_error(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.record.error;
}

static int dummy_get_output_samples(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.samples;
}

static int dummy_get_output_pause(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.play.pause;
}

static int dummy_set_output_volume(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_play_gain(drv, value, dummy_chip->perchip_info.play.balance);
        return 0;
}

static int dummy_get_output_volume(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.play.gain;
}

static int dummy_set_output_balance(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.play.balance = value;
        dummy_play_gain(drv, dummy_chip->perchip_info.play.gain, 
                        dummy_chip->perchip_info.play.balance);
        return 0;
}

static int dummy_get_output_balance(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.play.balance;
}

/* Set chip play gain */
static int dummy_play_gain(struct sparcaudio_driver *drv,
                           int value, unsigned char balance)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;
        int tmp = 0, r, l, r_adj, l_adj;

        r = l = value;
        if (balance < AUDIO_MID_BALANCE) {
                r = (int) (value -
                           ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT));

                if (r < 0)
                        r = 0;
        } else if (balance > AUDIO_MID_BALANCE) {
                l = (int) (value -
                           ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT));

                if (l < 0)
                        l = 0;
        }
        (l == 0) ? (l_adj = DUMMY_MAX_DEV_ATEN) : (l_adj = DUMMY_MAX_ATEN - 
                                                   (l * (DUMMY_MAX_ATEN + 1) / 
                                                    (AUDIO_MAX_GAIN + 1)));
        (r == 0) ? (r_adj = DUMMY_MAX_DEV_ATEN) : (r_adj = DUMMY_MAX_ATEN -
                                                   (r * (DUMMY_MAX_ATEN + 1) /
                                                    (AUDIO_MAX_GAIN + 1)));
        if ((value == 0) || (value == AUDIO_MAX_GAIN)) {
                tmp = value;
        } else {
                if (value == l) {
                        tmp = ((DUMMY_MAX_ATEN - l_adj) * (AUDIO_MAX_GAIN + 1) / 
                               (DUMMY_MAX_ATEN + 1));
                } else if (value == r) {
                        tmp = ((DUMMY_MAX_ATEN - r_adj) * (AUDIO_MAX_GAIN + 1) / 
                               (DUMMY_MAX_ATEN + 1));
                }
        }
        dummy_chip->perchip_info.play.gain = tmp;
        return 0;
}

static int dummy_get_input_samples(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.samples;
}

static int dummy_get_input_pause(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.record.pause;
}

static int dummy_set_monitor_volume(struct sparcaudio_driver *drv, int value)
{
        return 0;
}

static int dummy_get_monitor_volume(struct sparcaudio_driver *drv)
{
        return 0;
}

static int dummy_set_input_volume(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_record_gain(drv, value, dummy_chip->perchip_info.record.balance);
        return 0;
}

static int dummy_get_input_volume(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return dummy_chip->perchip_info.record.gain;
}

static int dummy_set_input_balance(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.record.balance = value;
        dummy_record_gain(drv, dummy_chip->perchip_info.record.gain, 
                          dummy_chip->perchip_info.play.balance);
        return 0;
}

static int dummy_get_input_balance(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        return (int) dummy_chip->perchip_info.record.balance;
}

static int dummy_record_gain(struct sparcaudio_driver *drv,
                             int value, unsigned char balance)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;
        int tmp = 0, r, l, r_adj, l_adj;

        r = l = value;
        if (balance < AUDIO_MID_BALANCE) {
                r = (int) (value -
                           ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT));

                if (r < 0)
                        r = 0;
        } else if (balance > AUDIO_MID_BALANCE) {
                l = (int) (value -
                           ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT));

                if (l < 0)
                        l = 0;
        }
        (l == 0) ? (l_adj = DUMMY_MAX_DEV_ATEN) : (l_adj = DUMMY_MAX_ATEN - 
                                                   (l * (DUMMY_MAX_ATEN + 1) / 
                                                    (AUDIO_MAX_GAIN + 1)));
        (r == 0) ? (r_adj = DUMMY_MAX_DEV_ATEN) : (r_adj = DUMMY_MAX_ATEN -
                                                   (r * (DUMMY_MAX_ATEN + 1) /
                                                    (AUDIO_MAX_GAIN + 1)));
        if ((value == 0) || (value == AUDIO_MAX_GAIN)) {
                tmp = value;
        } else {
                if (value == l) {
                        tmp = ((DUMMY_MAX_ATEN - l_adj) * (AUDIO_MAX_GAIN + 1) / 
                               (DUMMY_MAX_ATEN + 1));
                } else if (value == r) {
                        tmp = ((DUMMY_MAX_ATEN - r_adj) * (AUDIO_MAX_GAIN + 1) / 
                               (DUMMY_MAX_ATEN + 1));
                }
        }
        dummy_chip->perchip_info.record.gain = tmp;
        return 0;
}

/* Reset the audio chip to a sane state. */
static void dummy_chip_reset(struct sparcaudio_driver *drv)
{
        dummy_set_output_encoding(drv, AUDIO_ENCODING_ULAW);
        dummy_set_output_rate(drv, DUMMY_RATE);
        dummy_set_output_channels(drv, DUMMY_CHANNELS);
        dummy_set_output_precision(drv, DUMMY_PRECISION);
        dummy_set_output_balance(drv, AUDIO_MID_BALANCE);
        dummy_set_output_volume(drv, DUMMY_DEFAULT_PLAYGAIN);
        dummy_set_output_port(drv, AUDIO_SPEAKER);
        dummy_output_muted(drv, 0);
        dummy_set_input_encoding(drv, AUDIO_ENCODING_ULAW);
        dummy_set_input_rate(drv, DUMMY_RATE);
        dummy_set_input_channels(drv, DUMMY_CHANNELS);
        dummy_set_input_precision(drv, DUMMY_PRECISION);
        dummy_set_input_balance(drv, AUDIO_MID_BALANCE);
        dummy_set_input_volume(drv, DUMMY_DEFAULT_PLAYGAIN);
        dummy_set_input_port(drv, AUDIO_SPEAKER);
}

static int dummy_open(struct inode * inode, struct file * file, struct sparcaudio_driver *drv)
{	
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        /* Set the default audio parameters if not already in use. */
        if (file->f_mode & FMODE_WRITE) {
                if (!(drv->flags & SDF_OPEN_WRITE) && 
                    (dummy_chip->perchip_info.play.active == 0)) {
                        dummy_chip->perchip_info.play.open = 1;
                        dummy_chip->perchip_info.play.samples =
                                dummy_chip->perchip_info.play.error = 0;
                }
        }

        if (file->f_mode & FMODE_READ) {
                if (!(drv->flags & SDF_OPEN_READ) && 
                    (dummy_chip->perchip_info.record.active == 0)) {
                        dummy_chip->perchip_info.record.open = 1;
                        dummy_chip->perchip_info.record.samples =
                                dummy_chip->perchip_info.record.error = 0;
                }
        }

        MOD_INC_USE_COUNT;

        return 0;
}

static void dummy_release(struct inode * inode, struct file * file, struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (file->f_mode & FMODE_WRITE) {
                dummy_chip->perchip_info.play.active =
                        dummy_chip->perchip_info.play.open = 0;
        }

        if (file->f_mode & FMODE_READ) {
                dummy_chip->perchip_info.record.active =
                        dummy_chip->perchip_info.record.open = 0;
        }

        MOD_DEC_USE_COUNT;
}

static void dummy_output_done_task(void * arg)
{
        struct sparcaudio_driver *drv = (struct sparcaudio_driver *) arg;
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        sparcaudio_output_done(drv, 1);
        if (dummy_chip->perchip_info.record.active)
                sparcaudio_input_done(drv, 1);
}

static void dummy_start_output(struct sparcaudio_driver *drv, __u8 * buffer,
                               unsigned long count)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        if (dummy_chip->perchip_info.play.pause || !count) 
                return;

        dummy_chip->perchip_info.play.active = 1;

        /* fake an "interrupt" to deal with this block */
        INIT_LIST_HEAD(&dummy_chip->tqueue.list);
        dummy_chip->tqueue.sync = 0;
        dummy_chip->tqueue.routine = dummy_output_done_task;
        dummy_chip->tqueue.data = drv;

        queue_task(&dummy_chip->tqueue, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
}

static void dummy_start_input(struct sparcaudio_driver *drv, __u8 * buffer,
                              unsigned long count)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.record.active = 1;
}

static void dummy_stop_output(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.play.active = 0;
}

static void dummy_stop_input(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.record.active = 0;
}

static int dummy_set_output_pause(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.play.pause = value;

        if (!value)
                sparcaudio_output_done(drv, 0);

        return value;
}

static int dummy_set_input_pause(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;

        dummy_chip->perchip_info.record.pause = value;

        /* This should probably cause play pause. */

        return value;
}

static int dummy_set_input_error(struct sparcaudio_driver *drv, int value)
{
        return 0;
}

static int dummy_set_output_error(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;
        int i;

        i = dummy_chip->perchip_info.play.error;
        dummy_chip->perchip_info.play.error = value;
        return i;
}

static int dummy_set_output_samples(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;
        int i;

        i = dummy_chip->perchip_info.play.samples;
        dummy_chip->perchip_info.play.samples = value;
        return i;
}

static int dummy_set_input_samples(struct sparcaudio_driver *drv, int value)
{
        struct dummy_chip *dummy_chip = (struct dummy_chip *) drv->private;
        int i;

        i = dummy_chip->perchip_info.play.samples;
        dummy_chip->perchip_info.record.samples = value;
        return i;
}

/* In order to fake things which care out, play we're a 4231 */
static void dummy_audio_getdev(struct sparcaudio_driver *drv,
                               audio_device_t * audinfo)
{
        strncpy(audinfo->name, "SUNW,cs4231", sizeof(audinfo->name) - 1);
        strncpy(audinfo->version, "a", sizeof(audinfo->version) - 1);
        strncpy(audinfo->config, "onboard1", sizeof(audinfo->config) - 1);
}


static int dummy_audio_getdev_sunos(struct sparcaudio_driver *drv)
{
        return 5; 
}

static struct sparcaudio_operations dummy_ops = {
	dummy_open,
	dummy_release,
	NULL,
	dummy_start_output,
	dummy_stop_output,
	dummy_start_input,
	dummy_stop_input,
	dummy_audio_getdev,
        dummy_set_output_volume,
        dummy_get_output_volume,
        dummy_set_input_volume,
        dummy_get_input_volume,
        dummy_set_monitor_volume,
        dummy_get_monitor_volume,
	dummy_set_output_balance,
	dummy_get_output_balance,
	dummy_set_input_balance,
	dummy_get_input_balance,
        dummy_set_output_channels,
        dummy_get_output_channels,
        dummy_set_input_channels,
        dummy_get_input_channels,
        dummy_set_output_precision,
        dummy_get_output_precision,
        dummy_set_input_precision,
        dummy_get_input_precision,
        dummy_set_output_port,
        dummy_get_output_port,
        dummy_set_input_port,
        dummy_get_input_port,
        dummy_set_output_encoding,
        dummy_get_output_encoding,
        dummy_set_input_encoding,
        dummy_get_input_encoding,
        dummy_set_output_rate,
        dummy_get_output_rate,
        dummy_set_input_rate,
        dummy_get_input_rate,
	dummy_audio_getdev_sunos,
	dummy_get_output_ports,
	dummy_get_input_ports,
	dummy_output_muted,
	dummy_get_output_muted,
	dummy_set_output_pause,
	dummy_get_output_pause,
        dummy_set_input_pause,
	dummy_get_input_pause,
	dummy_set_output_samples,
	dummy_get_output_samples,
        dummy_set_input_samples,
	dummy_get_input_samples,
	dummy_set_output_error,
	dummy_get_output_error,
        dummy_set_input_error,
	dummy_get_input_error,
        dummy_get_formats,
};

/* Attach to an dummy chip given its PROM node. */
static int __init dummy_attach(struct sparcaudio_driver *drv)
{
        struct dummy_chip *dummy_chip;
        int err;

        /* Allocate our private information structure. */
        drv->private = kmalloc(sizeof(struct dummy_chip), GFP_KERNEL);
        if (drv->private == NULL)
                return -ENOMEM;

        /* Point at the information structure and initialize it. */
        drv->ops = &dummy_ops;
        dummy_chip = (struct dummy_chip *) drv->private;

        /* Reset parameters. */
        dummy_chip_reset(drv);

        /* Register ourselves with the midlevel audio driver. */
        err = register_sparcaudio_driver(drv, 2);

        if (err < 0) {
                printk(KERN_ERR "dummy: unable to register\n");
                kfree(drv->private);
                return -EIO;
        }

        dummy_chip->perchip_info.play.active = 
                dummy_chip->perchip_info.play.pause = 0;

        dummy_chip->perchip_info.play.avail_ports = (AUDIO_HEADPHONE |
                                                     AUDIO_SPEAKER |
                                                     AUDIO_LINE_OUT);

        /* Announce the hardware to the user. */
        printk(KERN_INFO "audio%d: dummy at 0x0 irq 0\n", drv->index);
  
        /* Success! */
        return 0;
}

/* Detach from an dummy chip given the device structure. */
static void __exit dummy_detach(struct sparcaudio_driver *drv)
{
        unregister_sparcaudio_driver(drv, 2);
        kfree(drv->private);
}

/* Probe for the dummy chip and then attach the driver. */
static int __init dummy_init(void)
{
	num_drivers = 0;
      
	/* Add support here for specifying multiple dummies to attach at once. */
	if (dummy_attach(&drivers[num_drivers]) == 0)
		num_drivers++;
  
	/* Only return success if we found some dummy chips. */
	return (num_drivers > 0) ? 0 : -EIO;
}

static void __exit dummy_exit(void)
{
        int i;

        for (i = 0; i < num_drivers; i++) {
                dummy_detach(&drivers[i]);
                num_drivers--;
        }
}

module_init(dummy_init);
module_exit(dummy_exit);
MODULE_LICENSE("GPL");

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

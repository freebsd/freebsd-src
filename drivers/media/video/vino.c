/*
 * Driver for the VINO (Video In No Out) system found in SGI Indys.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * Copyright (C) 2003 Ladislav Michl <ladis@linux-mips.org>
 */

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/wrapper.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/videodev.h>
#include <linux/video_decoder.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-sgi.h>

#include <asm/paccess.h>
#include <asm/io.h>
#include <asm/sgi/ip22.h>
#include <asm/sgi/hpc3.h>
#include <asm/sgi/mc.h>

#include "vino.h"

/* debugging? */
#if 1
#define DEBUG(x...)	printk(x);
#else
#define DEBUG(x...)
#endif

/* VINO video size */
#define VINO_PAL_WIDTH		768
#define VINO_PAL_HEIGHT		576
#define VINO_NTSC_WIDTH		646
#define VINO_NTSC_HEIGHT	486

/* set this to some sensible values. note: VINO_MIN_WIDTH has to be 8*x */
#define VINO_MIN_WIDTH		32
#define VINO_MIN_HEIGHT		32

/* channel selection */
#define VINO_INPUT_COMP		0
#define VINO_INPUT_SVIDEO	1
#define VINO_INPUT_CAMERA	2
#define VINO_INPUT_CHANNELS	3

#define PAGE_RATIO	(PAGE_SIZE / VINO_PAGE_SIZE)

/* VINO ASIC registers */
struct sgi_vino *vino;

static const char *vinostr = "VINO IndyCam/TV";
static int threshold_a = 512;
static int threshold_b = 512;

struct vino_device {
	struct video_device vdev;
#define VINO_CHAN_A	1
#define VINO_CHAN_B	2
	int chan;
	int alpha;
	/* clipping... */
	unsigned int left, right, top, bottom;
	/* decimation used */
	unsigned int decimation;
	/* palette used, picture hue, etc */
	struct video_picture picture;
	/* VINO_INPUT_COMP, VINO_INPUT_SVIDEO or VINO_INPUT_CAMERA */
	unsigned int input;
	/* bytes per line */
	unsigned int line_size;
	/* descriptor table (virtual addresses) */
	unsigned long *desc;
	/* # of allocated pages */
	int page_count;
	/* descriptor table (dma addresses) */
	struct {
		dma_addr_t *cpu;
		dma_addr_t dma;
	} dma_desc;
	/* add some more space to let VINO trigger End Of Field interrupt
	 * before reaching end of buffer */
#define VINO_FBUFSIZE	(VINO_PAL_WIDTH * VINO_PAL_HEIGHT * 4 + 2 * PAGE_SIZE)
	unsigned int frame_size;
#define VINO_BUF_UNUSED		0
#define VINO_BUF_GRABBING	1
#define VINO_BUF_DONE		2
	int buffer_state;

	wait_queue_head_t dma_wait;
	spinlock_t state_lock;
	struct semaphore sem;

	/* Make sure we only have one user at the time */
	int users;
};

struct vino_client {
	struct i2c_client *driver;
	int owner;
};

struct vino_video {
	struct vino_device chA;
	struct vino_device chB;

	struct vino_client decoder;
	struct vino_client camera;
	spinlock_t vino_lock;
	spinlock_t input_lock;

	/* Loaded into VINO descriptors to clear End Of Descriptors table
	 * interupt condition */
	unsigned long dummy_desc;
	struct {
		dma_addr_t *cpu;
		dma_addr_t dma;
	} dummy_dma;
};

static struct vino_video *Vino;

/* --- */

unsigned i2c_vino_getctrl(void *data)
{
	return vino->i2c_control;
}

void i2c_vino_setctrl(void *data, unsigned val)
{
	vino->i2c_control = val;
}

unsigned i2c_vino_rdata(void *data)
{
	return vino->i2c_data;
}

void i2c_vino_wdata(void *data, unsigned val)
{
	vino->i2c_data = val;
}

static struct i2c_algo_sgi_data i2c_sgi_vino_data =
{
	.getctrl = &i2c_vino_getctrl,
	.setctrl = &i2c_vino_setctrl,
	.rdata   = &i2c_vino_rdata,
	.wdata   = &i2c_vino_wdata,
	.xfer_timeout = 200,
	.ack_timeout  = 1000,
};

/*
 * There are two possible clients on VINO I2C bus, so we limit usage only
 * to them.
 */
static int i2c_vino_client_reg(struct i2c_client *client)
{
	int res = 0;

	spin_lock(&Vino->input_lock);
	switch (client->driver->id) {
	case I2C_DRIVERID_SAA7191:
		if (Vino->decoder.driver)
			res = -EBUSY;
		else
			Vino->decoder.driver = client;
		break;
	case I2C_DRIVERID_INDYCAM:
		if (Vino->camera.driver)
			res = -EBUSY;
		else
			Vino->camera.driver = client;
		break;
	default:
		res = -ENODEV;
	}
	spin_unlock(&Vino->input_lock);

	return res;
}

static int i2c_vino_client_unreg(struct i2c_client *client)
{
	int res = 0;
	
	spin_lock(&Vino->input_lock);
	if (client == Vino->decoder.driver) {
		if (Vino->decoder.owner)
			res = -EBUSY;
		else
			Vino->decoder.driver = NULL;
	} else if (client == Vino->camera.driver) {
		if (Vino->camera.owner)
			res = -EBUSY;
		else
			Vino->camera.driver = NULL;
	}
	spin_unlock(&Vino->input_lock);

	return res;
}

static struct i2c_adapter vino_i2c_adapter =
{
	.name			= "VINO I2C bus",
	.id			= I2C_HW_SGI_VINO,
	.algo_data		= &i2c_sgi_vino_data,
	.client_register	= &i2c_vino_client_reg,
	.client_unregister	= &i2c_vino_client_unreg,
};

static int vino_i2c_add_bus(void)
{
	return i2c_sgi_add_bus(&vino_i2c_adapter);
}

static int vino_i2c_del_bus(void)
{
	return i2c_sgi_del_bus(&vino_i2c_adapter);
}

static int i2c_camera_command(unsigned int cmd, void *arg)
{
	return Vino->camera.driver->driver->command(Vino->camera.driver,
						    cmd, arg);
}

static int i2c_decoder_command(unsigned int cmd, void *arg)
{
	return Vino->decoder.driver->driver->command(Vino->decoder.driver,
						     cmd, arg);
}

/* --- */

static int bytes_per_pixel(struct vino_device *v)
{
	switch (v->picture.palette) {
	case VIDEO_PALETTE_GREY:
		return 1;
	case VIDEO_PALETTE_YUV422:
		return 2;
	default: /* VIDEO_PALETTE_RGB32 */
		return 4;
	}
}

static int get_capture_norm(struct vino_device *v)
{
	if (v->input == VINO_INPUT_CAMERA)
		return VIDEO_MODE_NTSC;
	else {
		/* TODO */
		return VIDEO_MODE_NTSC;
	}
}

/*
 * Set clipping. Try new values to fit, if they don't return -EINVAL
 */
static int set_clipping(struct vino_device *v, int x, int y, int w, int h,
			int d)
{
	int maxwidth, maxheight, lsize;

	if (d < 1)
		d = 1;
	if (d > 8)
		d = 8;
	if (w / d < VINO_MIN_WIDTH || h / d < VINO_MIN_HEIGHT)
		return -EINVAL;
	if (get_capture_norm(v) == VIDEO_MODE_NTSC) {
		maxwidth = VINO_NTSC_WIDTH;
		maxheight = VINO_NTSC_HEIGHT;
	} else {
		maxwidth = VINO_PAL_WIDTH;
		maxheight = VINO_PAL_HEIGHT;
	}
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	y &= ~1;	/* odd/even fields */
	if (x + w > maxwidth) {
		w = maxwidth - x;
		if (w / d < VINO_MIN_WIDTH)
			x = maxwidth - VINO_MIN_WIDTH * d;
	}
	if (y + h > maxheight) {
		h = maxheight - y;
		if (h / d < VINO_MIN_HEIGHT)
			y = maxheight - VINO_MIN_HEIGHT * d;
	}
	/* line size must be multiple of 8 bytes */
	lsize = (bytes_per_pixel(v) * w / d) & ~7;
	w = lsize * d / bytes_per_pixel(v);
	v->left = x;
	v->top = y;
	v->right = x + w;
	v->bottom = y + h;
	v->decimation = d;
	v->line_size = lsize;
	DEBUG("VINO: clipping %d, %d, %d, %d / %d - %d\n", v->left, v->top,
	      v->right, v->bottom, v->decimation, v->line_size);
	return 0;
}

static int set_scaling(struct vino_device *v, int w, int h)
{
	int maxwidth, maxheight, lsize, d;

	if (w < VINO_MIN_WIDTH || h < VINO_MIN_HEIGHT)
		return -EINVAL;
	if (get_capture_norm(v) == VIDEO_MODE_NTSC) {
		maxwidth = VINO_NTSC_WIDTH;
		maxheight = VINO_NTSC_HEIGHT;
	} else {
		maxwidth = VINO_PAL_WIDTH;
		maxheight = VINO_PAL_HEIGHT;
	}
	if (w > maxwidth)
		w = maxwidth;
	if (h > maxheight)
		h = maxheight;
	d = max(maxwidth / w, maxheight / h);
	if (d > 8)
		d = 8;
	/* line size must be multiple of 8 bytes */
	lsize = (bytes_per_pixel(v) * w) & ~7;
	w = lsize * d / bytes_per_pixel(v);
	h *= d;
	if (v->left + w > maxwidth)
		v->left = maxwidth - w;
	if (v->top + h > maxheight)
		v->top = (maxheight - h) & ~1;	/* odd/even fields */
	/* FIXME: -1 bug... Verify clipping with video signal generator */
	v->right = v->left + w;
	v->bottom = v->top + h;
	v->decimation = d;
	v->line_size = lsize;
	DEBUG("VINO: scaling %d, %d, %d, %d / %d - %d\n", v->left, v->top,
	      v->right, v->bottom, v->decimation, v->line_size);

	return 0;
}

/* 
 * Prepare vino for DMA transfer... (execute only with vino_lock locked)
 */
static int dma_setup(struct vino_device *v)
{
	u32 ctrl, intr;
	struct sgi_vino_channel *ch;

	ch = (v->chan == VINO_CHAN_A) ? &vino->a : &vino->b;
	ch->page_index = 0;
	ch->line_count = 0;
	/* let VINO know where to transfer data */
	ch->start_desc_tbl = v->dma_desc.dma;
	ch->next_4_desc = v->dma_desc.dma;
	/* give vino time to fetch the first four descriptors, 5 usec
	 * should be more than enough time */
	udelay(5);
	/* VINO line size register is set 8 bytes less than actual */
	ch->line_size = v->line_size - 8;
	/* set the alpha register */
	ch->alpha = v->alpha;
	/* set cliping registers */
	ch->clip_start = VINO_CLIP_ODD(v->top) | VINO_CLIP_EVEN(v->top+1) |
			 VINO_CLIP_X(v->left);
	ch->clip_end = VINO_CLIP_ODD(v->bottom) | VINO_CLIP_EVEN(v->bottom+1) |
		       VINO_CLIP_X(v->right);
	/* FIXME: end-of-field bug workaround
		       VINO_CLIP_X(VINO_PAL_WIDTH);
	 */
	/* init the frame rate and norm (full frame rate only for now...) */
	ch->frame_rate = VINO_FRAMERT_RT(0x1fff) |
			 (get_capture_norm(v) == VIDEO_MODE_PAL ?
			  VINO_FRAMERT_PAL : 0);
	ctrl = vino->control;
	intr = vino->intr_status;
	if (v->chan == VINO_CHAN_A) {
		/* All interrupt conditions for this channel was cleared
		 * so clear the interrupt status register and enable
		 * interrupts */
		intr &=	~VINO_INTSTAT_A;
		ctrl |= VINO_CTRL_A_INT;
		/* enable synchronization */
		ctrl |= VINO_CTRL_A_SYNC_ENBL;
		/* enable frame assembly */
		ctrl |= VINO_CTRL_A_INTERLEAVE_ENBL;
		/* set decimation used */
		if (v->decimation < 2)
			ctrl &= ~VINO_CTRL_A_DEC_ENBL;
		else {
			ctrl |= VINO_CTRL_A_DEC_ENBL;
			ctrl &= ~VINO_CTRL_A_DEC_SCALE_MASK;
			ctrl |= (v->decimation - 1) <<
				VINO_CTRL_A_DEC_SCALE_SHIFT;
		}
		/* select input interface */
		if (v->input == VINO_INPUT_CAMERA)
			ctrl |= VINO_CTRL_A_SELECT;
		else
			ctrl &= ~VINO_CTRL_A_SELECT;
		/* palette */
		ctrl &= ~(VINO_CTRL_A_LUMA_ONLY | VINO_CTRL_A_RGB |
			  VINO_CTRL_A_DITHER);
	} else {
		intr &= ~VINO_INTSTAT_B;
		ctrl |= VINO_CTRL_B_INT;
		ctrl |= VINO_CTRL_B_SYNC_ENBL;
		ctrl |= VINO_CTRL_B_INTERLEAVE_ENBL;
		if (v->decimation < 2)
			ctrl &= ~VINO_CTRL_B_DEC_ENBL;
		else {
			ctrl |= VINO_CTRL_B_DEC_ENBL;
			ctrl &= ~VINO_CTRL_B_DEC_SCALE_MASK;
			ctrl |= (v->decimation - 1) <<
				VINO_CTRL_B_DEC_SCALE_SHIFT;
		}
		if (v->input == VINO_INPUT_CAMERA)
			ctrl |= VINO_CTRL_B_SELECT;
		else
			ctrl &= ~VINO_CTRL_B_SELECT;
		ctrl &= ~(VINO_CTRL_B_LUMA_ONLY | VINO_CTRL_B_RGB |
			  VINO_CTRL_B_DITHER);
	}
	/* set palette */
	switch (v->picture.palette) {
		case VIDEO_PALETTE_GREY:
			ctrl |= (v->chan == VINO_CHAN_A) ? 
				VINO_CTRL_A_LUMA_ONLY : VINO_CTRL_B_LUMA_ONLY;
			break;
		case VIDEO_PALETTE_RGB32:
			ctrl |= (v->chan == VINO_CHAN_A) ?
				VINO_CTRL_A_RGB : VINO_CTRL_B_RGB;
			break;
#if 0
		/* FIXME: this is NOT in v4l API :-( */
		case VIDEO_PALETTE_RGB332:
			ctrl |= (v->chan == VINO_CHAN_A) ?
				VINO_CTRL_A_RGB | VINO_CTRL_A_DITHER : 
				VINO_CTRL_B_RGB | VINO_CTRL_B_DITHER;
			break;
#endif
	}
	vino->control = ctrl;
	vino->intr_status = intr;

	return 0;
}

/* (execute only with vino_lock locked) */
static void dma_stop(struct vino_device *v)
{
	u32 ctrl = vino->control;
	ctrl &= (v->chan == VINO_CHAN_A) ?
		~VINO_CTRL_A_DMA_ENBL : ~VINO_CTRL_B_DMA_ENBL;
	vino->control = ctrl;
}

/* (execute only with vino_lock locked) */
static void dma_go(struct vino_device *v)
{
	u32 ctrl = vino->control;
	ctrl |= (v->chan == VINO_CHAN_A) ?
		VINO_CTRL_A_DMA_ENBL : VINO_CTRL_B_DMA_ENBL;
	vino->control = ctrl;
}

/*
 * Load dummy page to descriptor registers. This prevents generating of
 * spurious interrupts. (execute only with vino_lock locked)
 */
static void clear_eod(struct vino_device *v)
{
	struct sgi_vino_channel *ch;

	DEBUG("VINO: chnl %c clear EOD\n", (v->chan == VINO_CHAN_A) ? 'A':'B');
	ch = (v->chan == VINO_CHAN_A) ? &vino->a : &vino->b;
	ch->page_index = 0;
	ch->line_count = 0;
	ch->start_desc_tbl = Vino->dummy_dma.dma;
	ch->next_4_desc = Vino->dummy_dma.dma;
	udelay(5);
}

static void field_done(struct vino_device *v)
{
	spin_lock(&v->state_lock);
	if (v->buffer_state == VINO_BUF_GRABBING)
		v->buffer_state = VINO_BUF_DONE;
	spin_unlock(&v->state_lock);
	wake_up(&v->dma_wait);
}

static void vino_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 intr, ctrl;

	spin_lock(&Vino->vino_lock);
	ctrl = vino->control;
	intr = vino->intr_status;
	DEBUG("VINO: intr status %04x\n", intr);
	if (intr & (VINO_INTSTAT_A_FIFO | VINO_INTSTAT_A_EOD)) {
		ctrl &= ~VINO_CTRL_A_DMA_ENBL;
		vino->control = ctrl;
		clear_eod(&Vino->chA);
	}
	if (intr & (VINO_INTSTAT_B_FIFO | VINO_INTSTAT_B_EOD)) {
		ctrl &= ~VINO_CTRL_B_DMA_ENBL;
		vino->control = ctrl;
		clear_eod(&Vino->chB);
	}
	vino->intr_status = ~intr;
	spin_unlock(&Vino->vino_lock);
	/* FIXME: For now we are assuming that interrupt means that frame is
	 * done. That's not true, but we can live with such brokeness for
	 * a while ;-) */
	field_done(&Vino->chA);
}

static int vino_grab(struct vino_device *v, int frame)
{
	int err = 0;

	spin_lock_irq(&v->state_lock);
	if (v->buffer_state == VINO_BUF_GRABBING)
		err = -EBUSY;
	v->buffer_state = VINO_BUF_GRABBING;
	spin_unlock_irq(&v->state_lock);

	if (err)
		return err;

	spin_lock_irq(&Vino->vino_lock);
	dma_setup(v);
	dma_go(v);
	spin_unlock_irq(&Vino->vino_lock);

	return 0;
}

static int vino_waitfor(struct vino_device *v, int frame)
{
	wait_queue_t wait;
	int i, err = 0;

	if (frame != 0)
		return -EINVAL;

	spin_lock_irq(&v->state_lock);
	switch (v->buffer_state) {
	case VINO_BUF_GRABBING:
		init_waitqueue_entry(&wait, current);
		/* add ourselves into wait queue */
		add_wait_queue(&v->dma_wait, &wait);
		/* and set current state */
		set_current_state(TASK_INTERRUPTIBLE);
		/* before releasing spinlock */
		spin_unlock_irq(&v->state_lock);
		/* to ensure that schedule_timeout will return imediately
		 * if VINO interrupt was triggred meanwhile */
		schedule_timeout(HZ / 10);
		if (signal_pending(current))
			err = -EINTR;
		spin_lock_irq(&v->state_lock);
		remove_wait_queue(&v->dma_wait, &wait);
		/* don't rely on schedule_timeout return value and check what
		 * really happened */
		if (!err && v->buffer_state == VINO_BUF_GRABBING)
			err = -EIO;
		/* fall through */
	case VINO_BUF_DONE:
		for (i = 0; i < v->page_count; i++)
			pci_dma_sync_single(NULL, v->dma_desc.cpu[PAGE_RATIO*i],
					    PAGE_SIZE, PCI_DMA_FROMDEVICE);
		v->buffer_state = VINO_BUF_UNUSED;
		break;
	default:
		err = -EINVAL;
	}
	spin_unlock_irq(&v->state_lock);

	if (err && err != -EINVAL) {
		DEBUG("VINO: waiting for frame failed\n");
		spin_lock_irq(&Vino->vino_lock);
		dma_stop(v);
		clear_eod(v);
		spin_unlock_irq(&Vino->vino_lock);
	}

	return err;
}

static int alloc_buffer(struct vino_device *v, int size)
{
	int count, i, j, err;

	err = i = 0;
	count = (size / PAGE_SIZE + 4) & ~3;
	v->desc = (unsigned long *) kmalloc(count * sizeof(unsigned long),
					    GFP_KERNEL);
	if (!v->desc)
		return -ENOMEM;

	v->dma_desc.cpu = pci_alloc_consistent(NULL, PAGE_RATIO * (count+4) *
					       sizeof(dma_addr_t),
					       &v->dma_desc.dma);
	if (!v->dma_desc.cpu) {
		err = -ENOMEM;
		goto out_free_desc;
	}
	while (i < count) {
		dma_addr_t dma;

		v->desc[i] = get_zeroed_page(GFP_KERNEL | GFP_DMA);
		if (!v->desc[i])
			break;
		dma = pci_map_single(NULL, (void *)v->desc[i], PAGE_SIZE,
				     PCI_DMA_FROMDEVICE);
		for (j = 0; j < PAGE_RATIO; j++)
			v->dma_desc.cpu[PAGE_RATIO * i + j ] = 
				dma + VINO_PAGE_SIZE * j;
		mem_map_reserve(virt_to_page(v->desc[i]));
		i++;
	}
	v->dma_desc.cpu[PAGE_RATIO * count] = VINO_DESC_STOP;
	if (i-- < count) {
		while (i >= 0) {
			mem_map_unreserve(virt_to_page(v->desc[i]));
			pci_unmap_single(NULL, v->dma_desc.cpu[PAGE_RATIO * i],
					 PAGE_SIZE, PCI_DMA_FROMDEVICE);
			free_page(v->desc[i]);
			i--;
		}
		pci_free_consistent(NULL,
				    PAGE_RATIO * (count+4) * sizeof(dma_addr_t),
				    (void *)v->dma_desc.cpu, v->dma_desc.dma);
		err = -ENOBUFS;
		goto out_free_desc;
	}
	v->page_count = count;
	return 0;

out_free_desc:
	kfree(v->desc);
	return err;
}

static void free_buffer(struct vino_device *v)
{
	int i;

	for (i = 0; i < v->page_count; i++) {
		mem_map_unreserve(virt_to_page(v->desc[i]));
		pci_unmap_single(NULL, v->dma_desc.cpu[PAGE_RATIO * i],
				 PAGE_SIZE, PCI_DMA_FROMDEVICE);
		free_page(v->desc[i]);
	}
	pci_free_consistent(NULL,
			    PAGE_RATIO * (v->page_count+4) * sizeof(dma_addr_t),
			    (void *)v->dma_desc.cpu, v->dma_desc.dma);
	kfree(v->desc);
}

static int vino_open(struct inode *inode, struct file *file)
{
	struct video_device *dev = video_devdata(file);
	struct vino_device *v = dev->priv;
	int err = 0;

	down(&v->sem);
	if (v->users) {
		err =  -EBUSY;
		goto out;
	}
	/* Check for input device (IndyCam, saa7191) availability.
	 * Both DMA channels can run from the same source, but only
	 * source owner is allowed to change its parameters */
	spin_lock(&Vino->input_lock);
	if (Vino->camera.driver) {
		v->input = VINO_INPUT_CAMERA;
		if (!Vino->camera.owner)
			Vino->camera.owner = v->chan;
	}
	if (Vino->decoder.driver && Vino->camera.owner != v->chan) {
		/* There are two inputs (Composite and SVideo) but only
		 * one output available to VINO DMA engine */
		if (!Vino->decoder.owner) {
			Vino->decoder.owner = v->chan;
			v->input = VINO_INPUT_COMP;
			i2c_decoder_command(DECODER_SET_INPUT, &v->input);
		} else
			v->input = (v->chan == VINO_CHAN_A) ?
				   Vino->chB.input : Vino->chA.input;
	}
	if (v->input == -1)
		err = -ENODEV;
	spin_unlock(&Vino->input_lock);

	if (err)
		goto out;
	if (alloc_buffer(v, VINO_FBUFSIZE)) {
		err = -ENOBUFS;
		goto out;
	}
	v->users++;
out:
	up(&v->sem);
	return err;
}

static int vino_close(struct inode *inode, struct file *file)
{
	struct video_device *dev = video_devdata(file);
	struct vino_device *v = dev->priv;

	down(&v->sem);
	v->users--;
	if (!v->users) {
		struct vino_device *w = (v->chan == VINO_CHAN_A) ?
					&Vino->chB : &Vino->chA;
		/* Eventually make other channel owner of input device */
		spin_lock(&Vino->input_lock);
		if (Vino->camera.owner == v->chan)
			Vino->camera.owner = (w->input == VINO_INPUT_CAMERA) ?
					     w->chan : 0;
		else if (Vino->decoder.owner == v->chan)
			Vino->decoder.owner = (w->input == VINO_INPUT_COMP ||
					       w->input == VINO_INPUT_SVIDEO) ?
					      w->chan : 0;
		v->input = -1;
		spin_unlock(&Vino->input_lock);

		vino_waitfor(v, 0);
		free_buffer(v);
	}
	up(&v->sem);
	return 0;
}

static int vino_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_device *dev = video_devdata(file);
	struct vino_device *v = dev->priv;
	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	int i, err = 0;

	if (down_interruptible(&v->sem))
		return -EINTR;
	if (size > v->page_count * PAGE_SIZE) {
		err = -EINVAL;
		goto out;
	}
	for (i = 0; i < v->page_count; i++) {
		unsigned long page = virt_to_phys((void *)v->desc[i]);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_READONLY)) {
			err = -EAGAIN;
			goto out;
		}
		start += PAGE_SIZE;
		if (size <= PAGE_SIZE) break;
		size -= PAGE_SIZE;
	}
out:
	up(&v->sem);
	return err;
	
}

static int vino_do_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, void *arg)
{
	struct video_device *dev = video_devdata(file);
	struct vino_device *v = dev->priv;

	switch (cmd) {
	case VIDIOCGCAP: {
		struct video_capability *cap = arg;

		strcpy(cap->name, vinostr);
		cap->type = VID_TYPE_CAPTURE | VID_TYPE_SUBCAPTURE;
		cap->channels = VINO_INPUT_CHANNELS;
		cap->audios = 0;
		cap->maxwidth = VINO_PAL_WIDTH;
		cap->maxheight = VINO_PAL_HEIGHT;
		cap->minwidth = VINO_MIN_WIDTH;
		cap->minheight = VINO_MIN_HEIGHT;
		break;
	}
	case VIDIOCGCHAN: {
		struct video_channel *ch = arg;

		ch->flags = 0;
		ch->tuners = 0;
		switch (ch->channel) {
		case VINO_INPUT_COMP:
			ch->norm = VIDEO_MODE_PAL | VIDEO_MODE_NTSC;
			ch->type = VIDEO_TYPE_TV;
			strcpy(ch->name, "Composite");
			break;
		case VINO_INPUT_SVIDEO:
			ch->norm = VIDEO_MODE_PAL | VIDEO_MODE_NTSC;
			ch->type = VIDEO_TYPE_TV;
			strcpy(ch->name, "S-Video");
			break;
		case VINO_INPUT_CAMERA:
			ch->norm = VIDEO_MODE_NTSC;
			ch->type = VIDEO_TYPE_CAMERA;
			strcpy(ch->name, "IndyCam");
			break;
		default:
			return -EINVAL;
		}
		break;
	}
	case VIDIOCSCHAN: {
		struct video_channel *ch = arg;
		int err = 0;
		struct vino_device *w = (v->chan == VINO_CHAN_A) ?
					&Vino->chB : &Vino->chA;

		spin_lock(&Vino->input_lock);
		switch (ch->channel) {
		case VINO_INPUT_COMP:
		case VINO_INPUT_SVIDEO:
			if (!Vino->decoder.driver) {
				err = -ENODEV;
				break;
			}
			if (!Vino->decoder.owner)
				Vino->decoder.owner = v->chan;
			if (Vino->decoder.owner == v->chan)
				i2c_decoder_command(DECODER_SET_INPUT,
						    &ch->channel);
			else
				if (ch->channel != w->input) {
					err = -EBUSY;
					break;
				}
			if (Vino->camera.owner == v->chan)
				Vino->camera.owner =
					(w->input == VINO_INPUT_CAMERA) ?
					w->chan : 0;
			break;
		case VINO_INPUT_CAMERA:
			if (!Vino->camera.driver) {
				err = -ENODEV;
				break;
			}
			if (!Vino->camera.owner)
				Vino->camera.owner = v->chan;
			if (Vino->decoder.owner == v->chan)
				Vino->decoder.owner =
					(w->input == VINO_INPUT_COMP ||
					 w->input == VINO_INPUT_SVIDEO) ?
					w->chan : 0;
			break;
		default:
			err = -EINVAL;
		}
		if (!err)
			v->input = ch->channel;
		spin_unlock(&Vino->input_lock);

		return err;
	}
	case VIDIOCGPICT: {
		struct video_picture *pic = arg;

		memcpy(pic, &v->picture, sizeof(struct video_picture));
		break;
	}
	case VIDIOCSPICT: {
		struct video_picture *pic = arg;

		switch (pic->palette) {
		case VIDEO_PALETTE_GREY:
			pic->depth = 8;
			break;
		case VIDEO_PALETTE_YUV422:
			pic->depth = 16;
			break;
		case VIDEO_PALETTE_RGB32:
			pic->depth = 24;
			break;
		default:
			return -EINVAL;
		}
		if (v->picture.palette != pic->palette) {
			v->picture.palette = pic->palette;
			v->picture.depth = pic->depth;
			/* TODO: we need to change line size */
		}
		DEBUG("XXX %d, %d\n", v->input, Vino->camera.owner);
		spin_lock(&Vino->input_lock);
		if (v->input == VINO_INPUT_CAMERA) {
			if (Vino->camera.owner == v->chan) {
				spin_unlock(&Vino->input_lock);
				memcpy(&v->picture, pic,
					sizeof(struct video_picture));
				i2c_camera_command(DECODER_SET_PICTURE, pic);
				goto out_unlocked;
			}
		} else {
			if (Vino->decoder.owner == v->chan) {
				spin_unlock(&Vino->input_lock);
				memcpy(&v->picture, pic,
					sizeof(struct video_picture));
				i2c_decoder_command(DECODER_SET_PICTURE, pic);
				goto out_unlocked;
			}
		}
		spin_unlock(&Vino->input_lock);
out_unlocked:
		break;
	}
	/* get cropping */
	case VIDIOCGCAPTURE: {
		struct video_capture *capture = arg;

		capture->x = v->left;
		capture->y = v->top;
		capture->width = v->right - v->left;
		capture->height = v->bottom - v->top;
		capture->decimation = v->decimation;
		capture->flags = 0;
		break;
	}
	/* set cropping */
	case VIDIOCSCAPTURE: {
		struct video_capture *capture = arg; 
		
		return set_clipping(v, capture->x, capture->y, capture->width,
				    capture->height, capture->decimation);
	}
	/* get scaling */
	case VIDIOCGWIN: {
		struct video_window *win = arg;

		memset(win, 0, sizeof(*win));
		win->width = (v->right - v->left) / v->decimation;
		win->height = (v->bottom - v->top) / v->decimation;
		break;
	}
	/* set scaling */
	case VIDIOCSWIN: {
		struct video_window *win = arg;

		if (win->x || win->y || win->clipcount || win->clips)
			return -EINVAL;
		return set_scaling(v, win->width, win->height);
	}
	case VIDIOCGMBUF: {
		struct video_mbuf *buf = arg;

		buf->frames = 1;
		buf->offsets[0] = 0;
		buf->size = v->page_count * PAGE_SIZE;
		break;
	}
	case VIDIOCMCAPTURE: {
		struct video_mmap *mmap = arg; 

		if (mmap->width != v->right - v->left ||
		    mmap->height != v->bottom - v->top ||
		    mmap->format != v->picture.palette ||
		    mmap->frame != 0)
			return -EINVAL;

		return vino_grab(v, mmap->frame);
	}
	case VIDIOCSYNC:
		return vino_waitfor(v, *((int*)arg));
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static int vino_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	struct video_device *dev = video_devdata(file);
	struct vino_device *v = dev->priv;
	int err;

	if (down_interruptible(&v->sem))
		return -EINTR;
	err = video_usercopy(inode, file, cmd, arg, vino_do_ioctl);
	up(&v->sem);
	return err;
}

static struct file_operations vino_fops = {
	.owner		= THIS_MODULE,
	.open		= vino_open,
	.release	= vino_close,
	.ioctl		= vino_ioctl,
	.mmap		= vino_mmap,
	.llseek		= no_llseek,
};

static const struct video_device vino_template = {
	.owner		= THIS_MODULE,
	.type		= VID_TYPE_CAPTURE | VID_TYPE_SUBCAPTURE,
	.hardware	= VID_HARDWARE_VINO,
	.name		= "VINO",
	.fops		= &vino_fops,
	.minor		= -1,
};

static void init_channel_data(struct vino_device *v, int channel)
{
	init_waitqueue_head(&v->dma_wait);
	init_MUTEX(&v->sem);
	spin_lock_init(&v->state_lock);
	memcpy(&v->vdev, &vino_template, sizeof(vino_template));
	v->vdev.priv = v;
	v->chan = channel;
	v->input = -1;
	v->picture.palette = VIDEO_PALETTE_GREY;
	v->picture.depth = 8;
	v->buffer_state = VINO_BUF_UNUSED;
	v->users = 0;
	set_clipping(v, 0, 0, VINO_NTSC_WIDTH, VINO_NTSC_HEIGHT, 1);
}

static int __init vino_init(void)
{
	unsigned long rev;
	dma_addr_t dma;
	int i, ret = 0;
	
	/* VINO is Indy specific beast */
	if (ip22_is_fullhouse())
		return -ENODEV;

	/*
	 * VINO is in the EISA address space, so the sysid register will tell
	 * us if the EISA_PRESENT pin on MC has been pulled low.
	 * 
	 * If EISA_PRESENT is not set we definitely don't have a VINO equiped
	 * system.
	 */
	if (!(sgimc->systemid & SGIMC_SYSID_EPRESENT)) {
		printk(KERN_ERR "VINO not found\n");
		return -ENODEV;
	}

	vino = (struct sgi_vino *)ioremap(VINO_BASE, sizeof(struct sgi_vino));
	if (!vino)
		return -EIO;

	/* Okay, once we know that VINO is present we'll read its revision
	 * safe way. One never knows... */
	if (get_dbe(rev, &(vino->rev_id))) {
		printk(KERN_ERR "VINO: failed to read revision register\n");
		ret = -ENODEV;
		goto out_unmap;
	}
	if (VINO_ID_VALUE(rev) != VINO_CHIP_ID) {
		printk(KERN_ERR "VINO is not VINO (Rev/ID: 0x%04lx)\n", rev);
		ret = -ENODEV;
		goto out_unmap;
	}
	printk(KERN_INFO "VINO Rev: 0x%02lx\n", VINO_REV_NUM(rev));

	Vino = (struct vino_video *)
		kmalloc(sizeof(struct vino_video), GFP_KERNEL);
	if (!Vino) {
		ret = -ENOMEM;
		goto out_unmap;
	}
	memset(Vino, 0, sizeof(struct vino_video));

	Vino->dummy_desc = get_zeroed_page(GFP_KERNEL | GFP_DMA);
	if (!Vino->dummy_desc) {
		ret = -ENOMEM;
		goto out_free_vino;
	}
	Vino->dummy_dma.cpu = pci_alloc_consistent(NULL, 4 * sizeof(dma_addr_t),
						   &Vino->dummy_dma.dma);
	if (!Vino->dummy_dma.cpu) {
		ret = -ENOMEM;
		goto out_free_dummy_desc;
	}
	dma = pci_map_single(NULL, (void *)Vino->dummy_desc, PAGE_SIZE,
			     PCI_DMA_FROMDEVICE);
	for (i = 0; i < 4; i++)
		Vino->dummy_dma.cpu[i] = dma;

	vino->control = 0;
	/* prevent VINO from throwing spurious interrupts */
	vino->a.next_4_desc = Vino->dummy_dma.dma;
	vino->b.next_4_desc = Vino->dummy_dma.dma;
	udelay(5);
	vino->intr_status = 0;
        /* set threshold level */
        vino->a.fifo_thres = threshold_a;
	vino->b.fifo_thres = threshold_b;

	spin_lock_init(&Vino->vino_lock);
	spin_lock_init(&Vino->input_lock);
	init_channel_data(&Vino->chA, VINO_CHAN_A);
	init_channel_data(&Vino->chB, VINO_CHAN_B);

	if (request_irq(SGI_VINO_IRQ, vino_interrupt, 0, vinostr, NULL)) {
		printk(KERN_ERR "VINO: request irq%02d failed\n",
		       SGI_VINO_IRQ);
		ret = -EAGAIN;
		goto out_unmap_dummy_desc;
	}

	ret = vino_i2c_add_bus();
	if (ret) {
		printk(KERN_ERR "VINO: I2C bus registration failed\n");
		goto out_free_irq;
	}

	if (video_register_device(&Vino->chA.vdev, VFL_TYPE_GRABBER, -1) < 0) {
		printk("%s, chnl %d: device registration failed.\n",
			Vino->chA.vdev.name, Vino->chA.chan);
		ret = -EINVAL;
		goto out_i2c_del_bus;
	}
	if (video_register_device(&Vino->chB.vdev, VFL_TYPE_GRABBER, -1) < 0) {
		printk("%s, chnl %d: device registration failed.\n",
			Vino->chB.vdev.name, Vino->chB.chan);
		ret = -EINVAL;
		goto out_unregister_vdev;
	}

#if defined(CONFIG_KMOD) && defined (MODULE)
	request_module("saa7191");
	request_module("indycam");
#endif
	return 0;

out_unregister_vdev:
	video_unregister_device(&Vino->chA.vdev);
out_i2c_del_bus:
	vino_i2c_del_bus();
out_free_irq:
	free_irq(SGI_VINO_IRQ, NULL);
out_unmap_dummy_desc:
	pci_unmap_single(NULL, Vino->dummy_dma.dma, PAGE_SIZE,
			 PCI_DMA_FROMDEVICE);
	pci_free_consistent(NULL, 4 * sizeof(dma_addr_t),
			    (void *)Vino->dummy_dma.cpu, Vino->dummy_dma.dma);
out_free_dummy_desc:
	free_page(Vino->dummy_desc);
out_free_vino:
	kfree(Vino);
out_unmap:
	iounmap(vino);

	return ret;
}

static void __exit vino_exit(void)
{
	video_unregister_device(&Vino->chA.vdev);
	video_unregister_device(&Vino->chB.vdev);
	vino_i2c_del_bus();
	free_irq(SGI_VINO_IRQ, NULL);
	pci_unmap_single(NULL, Vino->dummy_dma.dma, PAGE_SIZE,
			 PCI_DMA_FROMDEVICE);
	pci_free_consistent(NULL, 4 * sizeof(dma_addr_t),
			    (void *)Vino->dummy_dma.cpu, Vino->dummy_dma.dma);
	free_page(Vino->dummy_desc);
	kfree(Vino);
	iounmap(vino);
}

module_init(vino_init);
module_exit(vino_exit);

MODULE_AUTHOR("Ladislav Michl <ladis@linux-mips.org>");
MODULE_DESCRIPTION("Video4Linux driver for SGI Indy VINO (IndyCam)");
MODULE_LICENSE("GPL");

/* i810_dma.c -- DMA support for the i810 -*- linux-c -*-
 * Created: Mon Dec 13 01:50:01 1999 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 *	    Jeff Hartmann <jhartmann@valinux.com>
 *          Keith Whitwell <keithw@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "i810_drv.h"
#include <linux/interrupt.h>	/* For task queue support */
#include <linux/pagemap.h>

/* in case we don't have a 2.3.99-pre6 kernel or later: */
#ifndef VM_DONTCOPY
#define VM_DONTCOPY 0
#endif

#define I810_BUF_FREE		2
#define I810_BUF_CLIENT		1
#define I810_BUF_HARDWARE      	0

#define I810_BUF_UNMAPPED 0
#define I810_BUF_MAPPED   1

#define I810_REG(reg)		2
#define I810_BASE(reg)		((unsigned long) \
				dev->maplist[I810_REG(reg)]->handle)
#define I810_ADDR(reg)		(I810_BASE(reg) + reg)
#define I810_DEREF(reg)		*(__volatile__ int *)I810_ADDR(reg)
#define I810_READ(reg)		I810_DEREF(reg)
#define I810_WRITE(reg,val) 	do { I810_DEREF(reg) = val; } while (0)
#define I810_DEREF16(reg)	*(__volatile__ u16 *)I810_ADDR(reg)
#define I810_READ16(reg)	I810_DEREF16(reg)
#define I810_WRITE16(reg,val)	do { I810_DEREF16(reg) = val; } while (0)

#define RING_LOCALS	unsigned int outring, ringmask; volatile char *virt;

#define BEGIN_LP_RING(n) do {				\
	if (I810_VERBOSE)				\
		DRM_DEBUG("BEGIN_LP_RING(%d) in %s\n",	\
			  n, __FUNCTION__);		\
	if (dev_priv->ring.space < n*4) 		\
		i810_wait_ring(dev, n*4);		\
	dev_priv->ring.space -= n*4;			\
	outring = dev_priv->ring.tail;			\
	ringmask = dev_priv->ring.tail_mask;		\
	virt = dev_priv->ring.virtual_start;		\
} while (0)

#define ADVANCE_LP_RING() do {					\
	if (I810_VERBOSE) DRM_DEBUG("ADVANCE_LP_RING\n");	\
	dev_priv->ring.tail = outring;				\
	I810_WRITE(LP_RING + RING_TAIL, outring);		\
} while(0)

#define OUT_RING(n) do {						\
	if (I810_VERBOSE) DRM_DEBUG("   OUT_RING %x\n", (int)(n));	\
	*(volatile unsigned int *)(virt + outring) = n;			\
	outring += 4;							\
	outring &= ringmask;						\
} while (0)

static inline void i810_print_status_page(drm_device_t *dev)
{
   	drm_device_dma_t *dma = dev->dma;
      	drm_i810_private_t *dev_priv = dev->dev_private;
	u32 *temp = (u32 *)dev_priv->hw_status_page;
   	int i;

   	DRM_DEBUG(  "hw_status: Interrupt Status : %x\n", temp[0]);
   	DRM_DEBUG(  "hw_status: LpRing Head ptr : %x\n", temp[1]);
   	DRM_DEBUG(  "hw_status: IRing Head ptr : %x\n", temp[2]);
      	DRM_DEBUG(  "hw_status: Reserved : %x\n", temp[3]);
   	DRM_DEBUG(  "hw_status: Driver Counter : %d\n", temp[5]);
   	for(i = 6; i < dma->buf_count + 6; i++) {
	   	DRM_DEBUG( "buffer status idx : %d used: %d\n", i - 6, temp[i]);
	}
}

static drm_buf_t *i810_freelist_get(drm_device_t *dev)
{
   	drm_device_dma_t *dma = dev->dma;
	int		 i;
   	int 		 used;
   
	/* Linear search might not be the best solution */

   	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
		/* In use is already a pointer */
	   	used = cmpxchg(buf_priv->in_use, I810_BUF_FREE, 
			       I810_BUF_CLIENT);
	   	if(used == I810_BUF_FREE) {
			return buf;
		}
	}
   	return NULL;
}

/* This should only be called if the buffer is not sent to the hardware
 * yet, the hardware updates in use for us once its on the ring buffer.
 */

static int i810_freelist_put(drm_device_t *dev, drm_buf_t *buf)
{
   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
   	int used;
   
   	/* In use is already a pointer */
   	used = cmpxchg(buf_priv->in_use, I810_BUF_CLIENT, I810_BUF_FREE);
   	if(used != I810_BUF_CLIENT) {
	   	DRM_ERROR("Freeing buffer thats not in use : %d\n", buf->idx);
	   	return -EINVAL;
	}
   
   	return 0;
}

static struct file_operations i810_buffer_fops = {
	open:	 i810_open,
	flush:	 drm_flush,
	release: i810_release,
	ioctl:	 i810_ioctl,
	mmap:	 i810_mmap_buffers,
	read:	 drm_read,
	fasync:	 drm_fasync,
      	poll:	 drm_poll,
};

int i810_mmap_buffers(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t	    *priv	  = filp->private_data;
	drm_device_t	    *dev;
	drm_i810_private_t  *dev_priv;
	drm_buf_t           *buf;
	drm_i810_buf_priv_t *buf_priv;

	lock_kernel();
	dev	 = priv->dev;
	dev_priv = dev->dev_private;
	buf      = dev_priv->mmap_buffer;
	buf_priv = buf->dev_private;
   
	vma->vm_flags |= (VM_IO | VM_DONTCOPY);
	vma->vm_file = filp;
   
   	buf_priv->currently_mapped = I810_BUF_MAPPED;
	unlock_kernel();

	if (remap_page_range(vma->vm_start,
			     VM_OFFSET(vma),
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot)) return -EAGAIN;
	return 0;
}

static int i810_map_buffer(drm_buf_t *buf, struct file *filp)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_device_t	  *dev	  = priv->dev;
	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	struct file_operations *old_fops;
	int retcode = 0;

	if(buf_priv->currently_mapped == I810_BUF_MAPPED) return -EINVAL;

	if(VM_DONTCOPY != 0) {
		down_write(&current->mm->mmap_sem);
		old_fops = filp->f_op;
		filp->f_op = &i810_buffer_fops;
		dev_priv->mmap_buffer = buf;
		buf_priv->virtual = (void *)do_mmap(filp, 0, buf->total, 
						    PROT_READ|PROT_WRITE,
						    MAP_SHARED, 
						    buf->bus_address);
		dev_priv->mmap_buffer = NULL;
   		filp->f_op = old_fops;
		if ((unsigned long)buf_priv->virtual > -1024UL) {
			/* Real error */
			DRM_DEBUG("mmap error\n");
			retcode = (signed int)buf_priv->virtual;
			buf_priv->virtual = 0;
		}
   		up_write(&current->mm->mmap_sem);
	} else {
		buf_priv->virtual = buf_priv->kernel_virtual;
   		buf_priv->currently_mapped = I810_BUF_MAPPED;
	}
	return retcode;
}

static int i810_unmap_buffer(drm_buf_t *buf)
{
	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	int retcode = 0;

	if(VM_DONTCOPY != 0) {
		if(buf_priv->currently_mapped != I810_BUF_MAPPED) 
			return -EINVAL;
		down_write(&current->mm->mmap_sem);
        	retcode = do_munmap(current->mm,
				    (unsigned long)buf_priv->virtual, 
				    (size_t) buf->total);
   		up_write(&current->mm->mmap_sem);
	}
   	buf_priv->currently_mapped = I810_BUF_UNMAPPED;
   	buf_priv->virtual = 0;

	return retcode;
}

static int i810_dma_get_buffer(drm_device_t *dev, drm_i810_dma_t *d, 
			       struct file *filp)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_buf_t	  *buf;
	drm_i810_buf_priv_t *buf_priv;
	int retcode = 0;

	buf = i810_freelist_get(dev);
	if (!buf) {
		retcode = -ENOMEM;
	   	DRM_DEBUG("retcode=%d\n", retcode);
		return retcode;
	}
   
	retcode = i810_map_buffer(buf, filp);
	if(retcode) {
		i810_freelist_put(dev, buf);
	   	DRM_DEBUG("mapbuf failed, retcode %d\n", retcode);
		return retcode;
	}
	buf->pid     = priv->pid;
	buf_priv = buf->dev_private;	
	d->granted = 1;
   	d->request_idx = buf->idx;
   	d->request_size = buf->total;
   	d->virtual = buf_priv->virtual;

	return retcode;
}

static unsigned long i810_alloc_page(drm_device_t *dev)
{
	unsigned long address;
   
	address = __get_free_page(GFP_KERNEL);
	if(address == 0UL) 
		return 0;

	get_page(virt_to_page(address));
	LockPage(virt_to_page(address));
   
	return address;
}

static void i810_free_page(drm_device_t *dev, unsigned long page)
{
	struct page * p = virt_to_page(page);
	if(page == 0UL) 
		return;
	
	put_page(p);
	UnlockPage(p);
	free_page(page);
	return;
}

static int i810_dma_cleanup(drm_device_t *dev)
{
	drm_device_dma_t *dma = dev->dma;

	if(dev->dev_private) {
		int i;
	   	drm_i810_private_t *dev_priv = 
	     		(drm_i810_private_t *) dev->dev_private;
	   
	   	if(dev_priv->ring.virtual_start) {
		   	drm_ioremapfree((void *) dev_priv->ring.virtual_start,
					dev_priv->ring.Size, dev);
		}
	   	if(dev_priv->hw_status_page != 0UL) {
		   	i810_free_page(dev, dev_priv->hw_status_page);
		   	/* Need to rewrite hardware status page */
		   	I810_WRITE(0x02080, 0x1ffff000);
		}
	   	drm_free(dev->dev_private, sizeof(drm_i810_private_t), 
			 DRM_MEM_DRIVER);
	   	dev->dev_private = NULL;

		for (i = 0; i < dma->buf_count; i++) {
			drm_buf_t *buf = dma->buflist[ i ];
			drm_i810_buf_priv_t *buf_priv = buf->dev_private;
			drm_ioremapfree(buf_priv->kernel_virtual, buf->total, dev);
		}
	}
   	return 0;
}

static int i810_wait_ring(drm_device_t *dev, int n)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
   	drm_i810_ring_buffer_t *ring = &(dev_priv->ring);
   	int iters = 0;
   	unsigned long end;
	unsigned int last_head = I810_READ(LP_RING + RING_HEAD) & HEAD_ADDR;

	end = jiffies + (HZ*3);
   	while (ring->space < n) {
	   	int i;
	
	   	ring->head = I810_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
	   	ring->space = ring->head - (ring->tail+8);
		if (ring->space < 0) ring->space += ring->Size;
	   
		if (ring->head != last_head)
		   end = jiffies + (HZ*3);
	  
	   	iters++;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("space: %d wanted %d\n", ring->space, n);
		   	DRM_ERROR("lockup\n");
		   	goto out_wait_ring;
		}

	   	for (i = 0 ; i < 2000 ; i++) ;
	}

out_wait_ring:   
   	return iters;
}

static void i810_kernel_lost_context(drm_device_t *dev)
{
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	drm_i810_ring_buffer_t *ring = &(dev_priv->ring);
      
   	ring->head = I810_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
     	ring->tail = I810_READ(LP_RING + RING_TAIL);
     	ring->space = ring->head - (ring->tail+8);
     	if (ring->space < 0) ring->space += ring->Size;
}

static int i810_freelist_init(drm_device_t *dev)
{
      	drm_device_dma_t *dma = dev->dma;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
   	int my_idx = 24;
   	u32 *hw_status = (u32 *)(dev_priv->hw_status_page + my_idx);
   	int i;
   
   	if(dma->buf_count > 1019) {
	   	/* Not enough space in the status page for the freelist */
	   	return -EINVAL;
	}

   	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	   
	   	buf_priv->in_use = hw_status++;
	   	buf_priv->my_use_idx = my_idx;
	   	my_idx += 4;

	   	*buf_priv->in_use = I810_BUF_FREE;

		buf_priv->kernel_virtual = drm_ioremap(buf->bus_address, 
						       buf->total, dev);
	}
	return 0;
}

static int i810_dma_initialize(drm_device_t *dev, 
			       drm_i810_private_t *dev_priv,
			       drm_i810_init_t *init)
{
	drm_map_t *sarea_map;

   	dev->dev_private = (void *) dev_priv;
   	memset(dev_priv, 0, sizeof(drm_i810_private_t));

   	if (init->ring_map_idx >= dev->map_count ||
	    init->buffer_map_idx >= dev->map_count) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("ring_map or buffer_map are invalid\n");
	   	return -EINVAL;
	}
   
   	dev_priv->ring_map_idx = init->ring_map_idx;
   	dev_priv->buffer_map_idx = init->buffer_map_idx;
	sarea_map = dev->maplist[0];
	dev_priv->sarea_priv = (drm_i810_sarea_t *) 
		((u8 *)sarea_map->handle + 
		 init->sarea_priv_offset);

   	atomic_set(&dev_priv->flush_done, 0);
	init_waitqueue_head(&dev_priv->flush_queue);
   	
   	dev_priv->ring.Start = init->ring_start;
   	dev_priv->ring.End = init->ring_end;
   	dev_priv->ring.Size = init->ring_size;

   	dev_priv->ring.virtual_start = drm_ioremap(dev->agp->base + 
						   init->ring_start, 
						   init->ring_size, dev);

   	dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;
   
   	if (dev_priv->ring.virtual_start == NULL) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
	   	return -ENOMEM;
	}

	dev_priv->w = init->w;
	dev_priv->h = init->h;
	dev_priv->pitch = init->pitch;
	dev_priv->back_offset = init->back_offset;
	dev_priv->depth_offset = init->depth_offset;

	dev_priv->front_di1 = init->front_offset | init->pitch_bits;
	dev_priv->back_di1 = init->back_offset | init->pitch_bits;
	dev_priv->zi1 = init->depth_offset | init->pitch_bits;
	
   
   	/* Program Hardware Status Page */
   	dev_priv->hw_status_page = i810_alloc_page(dev);
   	memset((void *) dev_priv->hw_status_page, 0, PAGE_SIZE);
   	if(dev_priv->hw_status_page == 0UL) {
		i810_dma_cleanup(dev);
		DRM_ERROR("Can not allocate hardware status page\n");
		return -ENOMEM;
	}
   	DRM_DEBUG("hw status page @ %lx\n", dev_priv->hw_status_page);
   
   	I810_WRITE(0x02080, virt_to_bus((void *)dev_priv->hw_status_page));
   	DRM_DEBUG("Enabled hardware status page\n");
   
   	/* Now we need to init our freelist */
   	if(i810_freelist_init(dev) != 0) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("Not enough space in the status page for"
			  " the freelist\n");
	   	return -ENOMEM;
	}
   	return 0;
}

int i810_dma_init(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
   	drm_file_t *priv = filp->private_data;
   	drm_device_t *dev = priv->dev;
   	drm_i810_private_t *dev_priv;
   	drm_i810_init_t init;
   	int retcode = 0;
	
  	if (copy_from_user(&init, (drm_i810_init_t *)arg, sizeof(init)))
		return -EFAULT;
	
   	switch(init.func) {
	 	case I810_INIT_DMA:
	   		dev_priv = drm_alloc(sizeof(drm_i810_private_t), 
					     DRM_MEM_DRIVER);
	   		if(dev_priv == NULL) return -ENOMEM;
	   		retcode = i810_dma_initialize(dev, dev_priv, &init);
	   	break;
	 	case I810_CLEANUP_DMA:
	   		retcode = i810_dma_cleanup(dev);
	   	break;
	 	default:
	   		retcode = -EINVAL;
	   	break;
	}
   
   	return retcode;
}



/* Most efficient way to verify state for the i810 is as it is
 * emitted.  Non-conformant state is silently dropped.
 *
 * Use 'volatile' & local var tmp to force the emitted values to be
 * identical to the verified ones.
 */
static void i810EmitContextVerified( drm_device_t *dev, 
				     volatile unsigned int *code ) 
{	
   	drm_i810_private_t *dev_priv = dev->dev_private;
	int i, j = 0;
	unsigned int tmp;
	RING_LOCALS;

	BEGIN_LP_RING( I810_CTX_SETUP_SIZE );

	OUT_RING( GFX_OP_COLOR_FACTOR );
	OUT_RING( code[I810_CTXREG_CF1] );

	OUT_RING( GFX_OP_STIPPLE );
	OUT_RING( code[I810_CTXREG_ST1] );

	for ( i = 4 ; i < I810_CTX_SETUP_SIZE ; i++ ) {
		tmp = code[i];

		if ((tmp & (7<<29)) == (3<<29) &&
		    (tmp & (0x1f<<24)) < (0x1d<<24)) 
		{
			OUT_RING( tmp ); 
			j++;
		} 
	}

	if (j & 1) 
		OUT_RING( 0 ); 

	ADVANCE_LP_RING();
}

static void i810EmitTexVerified( drm_device_t *dev, 
				 volatile unsigned int *code ) 
{	
   	drm_i810_private_t *dev_priv = dev->dev_private;
	int i, j = 0;
	unsigned int tmp;
	RING_LOCALS;

	BEGIN_LP_RING( I810_TEX_SETUP_SIZE );

	OUT_RING( GFX_OP_MAP_INFO );
	OUT_RING( code[I810_TEXREG_MI1] );
	OUT_RING( code[I810_TEXREG_MI2] );
	OUT_RING( code[I810_TEXREG_MI3] );

	for ( i = 4 ; i < I810_TEX_SETUP_SIZE ; i++ ) {
		tmp = code[i];

		if ((tmp & (7<<29)) == (3<<29) &&
		    (tmp & (0x1f<<24)) < (0x1d<<24)) 
		{
			OUT_RING( tmp ); 
			j++;
		}
	} 
		
	if (j & 1) 
		OUT_RING( 0 ); 

	ADVANCE_LP_RING();
}


/* Need to do some additional checking when setting the dest buffer.
 */
static void i810EmitDestVerified( drm_device_t *dev, 
				  volatile unsigned int *code ) 
{	
   	drm_i810_private_t *dev_priv = dev->dev_private;
	unsigned int tmp;
	RING_LOCALS;

	BEGIN_LP_RING( I810_DEST_SETUP_SIZE + 2 );

	tmp = code[I810_DESTREG_DI1];
	if (tmp == dev_priv->front_di1 || tmp == dev_priv->back_di1) {
		OUT_RING( CMD_OP_DESTBUFFER_INFO );
		OUT_RING( tmp );
	} else
	   DRM_DEBUG("bad di1 %x (allow %x or %x)\n",
		     tmp, dev_priv->front_di1, dev_priv->back_di1);

	/* invarient:
	 */
	OUT_RING( CMD_OP_Z_BUFFER_INFO );
	OUT_RING( dev_priv->zi1 );

	OUT_RING( GFX_OP_DESTBUFFER_VARS );
	OUT_RING( code[I810_DESTREG_DV1] );

	OUT_RING( GFX_OP_DRAWRECT_INFO );
	OUT_RING( code[I810_DESTREG_DR1] );
	OUT_RING( code[I810_DESTREG_DR2] );
	OUT_RING( code[I810_DESTREG_DR3] );
	OUT_RING( code[I810_DESTREG_DR4] );
	OUT_RING( 0 );

	ADVANCE_LP_RING();
}



static void i810EmitState( drm_device_t *dev )
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
      	drm_i810_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty;

	if (dirty & I810_UPLOAD_BUFFERS) {
		i810EmitDestVerified( dev, sarea_priv->BufferState );
		sarea_priv->dirty &= ~I810_UPLOAD_BUFFERS;
	}

	if (dirty & I810_UPLOAD_CTX) {
		i810EmitContextVerified( dev, sarea_priv->ContextState );
		sarea_priv->dirty &= ~I810_UPLOAD_CTX;
	}

	if (dirty & I810_UPLOAD_TEX0) {
		i810EmitTexVerified( dev, sarea_priv->TexState[0] );
		sarea_priv->dirty &= ~I810_UPLOAD_TEX0;
	}

	if (dirty & I810_UPLOAD_TEX1) {
		i810EmitTexVerified( dev, sarea_priv->TexState[1] );
		sarea_priv->dirty &= ~I810_UPLOAD_TEX1;
	}
}



/* need to verify 
 */
static void i810_dma_dispatch_clear( drm_device_t *dev, int flags, 
				     unsigned int clear_color,
				     unsigned int clear_zval )
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
      	drm_i810_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int pitch = dev_priv->pitch;
	int cpp = 2;
	int i;
	RING_LOCALS;

  	i810_kernel_lost_context(dev);

      	if (nbox > I810_NR_SAREA_CLIPRECTS)
     		nbox = I810_NR_SAREA_CLIPRECTS;

	for (i = 0 ; i < nbox ; i++, pbox++) {
		unsigned int x = pbox->x1;
		unsigned int y = pbox->y1;
		unsigned int width = (pbox->x2 - x) * cpp;
		unsigned int height = pbox->y2 - y;
		unsigned int start = y * pitch + x * cpp;

		if (pbox->x1 > pbox->x2 ||
		    pbox->y1 > pbox->y2 ||
		    pbox->x2 > dev_priv->w ||
		    pbox->y2 > dev_priv->h)
			continue;

	   	if ( flags & I810_FRONT ) {	    
		   	DRM_DEBUG("clear front\n");
			BEGIN_LP_RING( 6 );	    
			OUT_RING( BR00_BITBLT_CLIENT | 
				  BR00_OP_COLOR_BLT | 0x3 );
			OUT_RING( BR13_SOLID_PATTERN | (0xF0 << 16) | pitch );
			OUT_RING( (height << 16) | width );
			OUT_RING( start );
			OUT_RING( clear_color );
			OUT_RING( 0 );
			ADVANCE_LP_RING();
		}

		if ( flags & I810_BACK ) {
			DRM_DEBUG("clear back\n");
			BEGIN_LP_RING( 6 );	    
			OUT_RING( BR00_BITBLT_CLIENT | 
				  BR00_OP_COLOR_BLT | 0x3 );
			OUT_RING( BR13_SOLID_PATTERN | (0xF0 << 16) | pitch );
			OUT_RING( (height << 16) | width );
			OUT_RING( dev_priv->back_offset + start );
			OUT_RING( clear_color );
			OUT_RING( 0 );
			ADVANCE_LP_RING();
		}

		if ( flags & I810_DEPTH ) {
			DRM_DEBUG("clear depth\n");
			BEGIN_LP_RING( 6 );	    
			OUT_RING( BR00_BITBLT_CLIENT | 
				  BR00_OP_COLOR_BLT | 0x3 );
			OUT_RING( BR13_SOLID_PATTERN | (0xF0 << 16) | pitch );
			OUT_RING( (height << 16) | width );
			OUT_RING( dev_priv->depth_offset + start );
			OUT_RING( clear_zval );
			OUT_RING( 0 );
			ADVANCE_LP_RING();
		}
	}
}

static void i810_dma_dispatch_swap( drm_device_t *dev )
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
      	drm_i810_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int pitch = dev_priv->pitch;
	int cpp = 2;
	int ofs = dev_priv->back_offset;
	int i;
	RING_LOCALS;

	DRM_DEBUG("swapbuffers\n");

  	i810_kernel_lost_context(dev);

      	if (nbox > I810_NR_SAREA_CLIPRECTS)
     		nbox = I810_NR_SAREA_CLIPRECTS;

	for (i = 0 ; i < nbox; i++, pbox++) 
	{
		unsigned int w = pbox->x2 - pbox->x1;
		unsigned int h = pbox->y2 - pbox->y1;
		unsigned int dst = pbox->x1*cpp + pbox->y1*pitch;
		unsigned int start = ofs + dst;

		if (pbox->x1 > pbox->x2 ||
		    pbox->y1 > pbox->y2 ||
		    pbox->x2 > dev_priv->w ||
		    pbox->y2 > dev_priv->h)
			continue;
 
	   	DRM_DEBUG("dispatch swap %d,%d-%d,%d!\n",
			  pbox[i].x1, pbox[i].y1,
			  pbox[i].x2, pbox[i].y2);

		BEGIN_LP_RING( 6 );
		OUT_RING( BR00_BITBLT_CLIENT | BR00_OP_SRC_COPY_BLT | 0x4 );
		OUT_RING( pitch | (0xCC << 16));
		OUT_RING( (h << 16) | (w * cpp));
		OUT_RING( dst );
		OUT_RING( pitch );	
		OUT_RING( start );
		ADVANCE_LP_RING();
	}
}


static void i810_dma_dispatch_vertex(drm_device_t *dev, 
				     drm_buf_t *buf,
				     int discard,
				     int used)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
   	drm_i810_sarea_t *sarea_priv = dev_priv->sarea_priv;
   	drm_clip_rect_t *box = sarea_priv->boxes;
   	int nbox = sarea_priv->nbox;
	unsigned long address = (unsigned long)buf->bus_address;
	unsigned long start = address - dev->agp->base;     
	int i = 0, u;
   	RING_LOCALS;

   	i810_kernel_lost_context(dev);

   	if (nbox > I810_NR_SAREA_CLIPRECTS) 
		nbox = I810_NR_SAREA_CLIPRECTS;

	if (discard) {
		u = cmpxchg(buf_priv->in_use, I810_BUF_CLIENT, 
			    I810_BUF_HARDWARE);
		if(u != I810_BUF_CLIENT) {
			DRM_DEBUG("xxxx 2\n");
		}
	}

	if (used > 4*1024) 
		used = 0;

	if (sarea_priv->dirty)
	   i810EmitState( dev );

  	DRM_DEBUG("dispatch vertex addr 0x%lx, used 0x%x nbox %d\n", 
		  address, used, nbox);

   	dev_priv->counter++;
   	DRM_DEBUG(  "dispatch counter : %ld\n", dev_priv->counter);
   	DRM_DEBUG(  "i810_dma_dispatch\n");
   	DRM_DEBUG(  "start : %lx\n", start);
	DRM_DEBUG(  "used : %d\n", used);
   	DRM_DEBUG(  "start + used - 4 : %ld\n", start + used - 4);

	if (buf_priv->currently_mapped == I810_BUF_MAPPED) {
		*(u32 *)buf_priv->virtual = (GFX_OP_PRIMITIVE |
					     sarea_priv->vertex_prim |
					     ((used/4)-2));
		
		if (used & 4) {
			*(u32 *)((u32)buf_priv->virtual + used) = 0;
			used += 4;
		}

		i810_unmap_buffer(buf);
	}
		   
	if (used) {
		do {
			if (i < nbox) {
				BEGIN_LP_RING(4);
				OUT_RING( GFX_OP_SCISSOR | SC_UPDATE_SCISSOR | 
					  SC_ENABLE );
				OUT_RING( GFX_OP_SCISSOR_INFO );
				OUT_RING( box[i].x1 | (box[i].y1<<16) );
				OUT_RING( (box[i].x2-1) | ((box[i].y2-1)<<16) );
				ADVANCE_LP_RING();
			}
			
			BEGIN_LP_RING(4);
			OUT_RING( CMD_OP_BATCH_BUFFER );
			OUT_RING( start | BB1_PROTECTED );
			OUT_RING( start + used - 4 );
			OUT_RING( 0 );
			ADVANCE_LP_RING();
			
		} while (++i < nbox);
	}

	BEGIN_LP_RING(10);
	OUT_RING( CMD_STORE_DWORD_IDX );
	OUT_RING( 20 );
	OUT_RING( dev_priv->counter );
	OUT_RING( 0 );

	if (discard) {
		OUT_RING( CMD_STORE_DWORD_IDX );
		OUT_RING( buf_priv->my_use_idx );
		OUT_RING( I810_BUF_FREE );
		OUT_RING( 0 );
	}

      	OUT_RING( CMD_REPORT_HEAD );
	OUT_RING( 0 );
   	ADVANCE_LP_RING();
}


/* Interrupts are only for flushing */
static void i810_dma_service(int irq, void *device, struct pt_regs *regs)
{
	drm_device_t	 *dev = (drm_device_t *)device;
   	u16 temp;
   
	atomic_inc(&dev->total_irq);
      	temp = I810_READ16(I810REG_INT_IDENTITY_R);
   	temp = temp & ~(0x6000);
   	if(temp != 0) I810_WRITE16(I810REG_INT_IDENTITY_R, 
				   temp); /* Clear all interrupts */
	else
	   return;
 
   	queue_task(&dev->tq, &tq_immediate);
   	mark_bh(IMMEDIATE_BH);
}

static void i810_dma_task_queue(void *device)
{
	drm_device_t *dev = (drm_device_t *) device;
      	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;

   	atomic_set(&dev_priv->flush_done, 1);
   	wake_up_interruptible(&dev_priv->flush_queue);
}

int i810_irq_install(drm_device_t *dev, int irq)
{
	int retcode;
	u16 temp;
   
	if (!irq)     return -EINVAL;
	
	down(&dev->struct_sem);
	if (dev->irq) {
		up(&dev->struct_sem);
		return -EBUSY;
	}
	dev->irq = irq;
	up(&dev->struct_sem);
	
   	DRM_DEBUG(  "Interrupt Install : %d\n", irq);
	DRM_DEBUG("%d\n", irq);

	dev->context_flag     = 0;
	dev->interrupt_flag   = 0;
	dev->dma_flag	      = 0;
	
	dev->dma->next_buffer = NULL;
	dev->dma->next_queue  = NULL;
	dev->dma->this_buffer = NULL;

	INIT_LIST_HEAD(&dev->tq.list);
	dev->tq.sync	      = 0;
	dev->tq.routine	      = i810_dma_task_queue;
	dev->tq.data	      = dev;

				/* Before installing handler */
   	temp = I810_READ16(I810REG_HWSTAM);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_HWSTAM, temp);
   	
      	temp = I810_READ16(I810REG_INT_MASK_R);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_INT_MASK_R, temp); /* Unmask interrupts */
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
      	I810_WRITE16(I810REG_INT_ENABLE_R, temp); /* Disable all interrupts */

				/* Install handler */
	if ((retcode = request_irq(dev->irq,
				   i810_dma_service,
				   SA_SHIRQ,
				   dev->devname,
				   dev))) {
		down(&dev->struct_sem);
		dev->irq = 0;
		up(&dev->struct_sem);
		return retcode;
	}
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
   	temp = temp | 0x0003;
   	I810_WRITE16(I810REG_INT_ENABLE_R, 
		     temp); /* Enable bp & user interrupts */
	return 0;
}

int i810_irq_uninstall(drm_device_t *dev)
{
	int irq;
   	u16 temp;


/*  	return 0; */

	down(&dev->struct_sem);
	irq	 = dev->irq;
	dev->irq = 0;
	up(&dev->struct_sem);
	
	if (!irq) return -EINVAL;

   	DRM_DEBUG(  "Interrupt UnInstall: %d\n", irq);	
	DRM_DEBUG("%d\n", irq);
   
   	temp = I810_READ16(I810REG_INT_IDENTITY_R);
   	temp = temp & ~(0x6000);
   	if(temp != 0) I810_WRITE16(I810REG_INT_IDENTITY_R, 
				   temp); /* Clear all interrupts */
   
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_INT_ENABLE_R, 
		     temp);                     /* Disable all interrupts */

   	free_irq(irq, dev);

	return 0;
}

int i810_control(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_control_t	ctl;
	int		retcode;
   
   	DRM_DEBUG(  "i810_control\n");

	if (copy_from_user(&ctl, (drm_control_t *)arg, sizeof(ctl)))
		return -EFAULT;
	
	switch (ctl.func) {
	case DRM_INST_HANDLER:
		if ((retcode = i810_irq_install(dev, ctl.irq)))
			return retcode;
		break;
	case DRM_UNINST_HANDLER:
		if ((retcode = i810_irq_uninstall(dev)))
			return retcode;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline void i810_dma_emit_flush(drm_device_t *dev)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
   	RING_LOCALS;

   	i810_kernel_lost_context(dev);

   	BEGIN_LP_RING(2);
      	OUT_RING( CMD_REPORT_HEAD );
      	OUT_RING( GFX_OP_USER_INTERRUPT );
      	ADVANCE_LP_RING();

/*  	i810_wait_ring( dev, dev_priv->ring.Size - 8 ); */
/*     	atomic_set(&dev_priv->flush_done, 1); */
/*     	wake_up_interruptible(&dev_priv->flush_queue); */
}

static inline void i810_dma_quiescent_emit(drm_device_t *dev)
{
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	RING_LOCALS;

  	i810_kernel_lost_context(dev);

   	BEGIN_LP_RING(4);
   	OUT_RING( INST_PARSER_CLIENT | INST_OP_FLUSH | INST_FLUSH_MAP_CACHE );
   	OUT_RING( CMD_REPORT_HEAD );
      	OUT_RING( 0 );
      	OUT_RING( GFX_OP_USER_INTERRUPT );
   	ADVANCE_LP_RING();

/*  	i810_wait_ring( dev, dev_priv->ring.Size - 8 ); */
/*     	atomic_set(&dev_priv->flush_done, 1); */
/*     	wake_up_interruptible(&dev_priv->flush_queue); */
}

static void i810_dma_quiescent(drm_device_t *dev)
{
      	DECLARE_WAITQUEUE(entry, current);
  	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
	unsigned long end;      

   	if(dev_priv == NULL) {
	   	return;
	}
      	atomic_set(&dev_priv->flush_done, 0);
   	add_wait_queue(&dev_priv->flush_queue, &entry);
   	end = jiffies + (HZ*3);
   
   	for (;;) {
		current->state = TASK_INTERRUPTIBLE;
	      	i810_dma_quiescent_emit(dev);
	   	if (atomic_read(&dev_priv->flush_done) == 1) break;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("lockup\n");
		   	break;
		}	   
	      	schedule_timeout(HZ*3);
	      	if (signal_pending(current)) {
		   	break;
		}
	}
   
   	current->state = TASK_RUNNING;
   	remove_wait_queue(&dev_priv->flush_queue, &entry);
   
   	return;
}

static int i810_flush_queue(drm_device_t *dev)
{
   	DECLARE_WAITQUEUE(entry, current);
  	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	unsigned long end;
   	int i, ret = 0;      

   	if(dev_priv == NULL) {
	   	return 0;
	}
      	atomic_set(&dev_priv->flush_done, 0);
   	add_wait_queue(&dev_priv->flush_queue, &entry);
   	end = jiffies + (HZ*3);
   	for (;;) {
		current->state = TASK_INTERRUPTIBLE;
	      	i810_dma_emit_flush(dev);
	   	if (atomic_read(&dev_priv->flush_done) == 1) break;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("lockup\n");
		   	break;
		}	   
	      	schedule_timeout(HZ*3);
	      	if (signal_pending(current)) {
		   	ret = -EINTR; /* Can't restart */
		   	break;
		}
	}
   
   	current->state = TASK_RUNNING;
   	remove_wait_queue(&dev_priv->flush_queue, &entry);


   	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	   
		int used = cmpxchg(buf_priv->in_use, I810_BUF_HARDWARE, 
				   I810_BUF_FREE);

		if (used == I810_BUF_HARDWARE)
			DRM_DEBUG("reclaimed from HARDWARE\n");
		if (used == I810_BUF_CLIENT)
			DRM_DEBUG("still on client HARDWARE\n");
	}

   	return ret;
}

/* Must be called with the lock held */
void i810_reclaim_buffers(drm_device_t *dev, pid_t pid)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
      	if (!dev->dev_private) return;
	if (!dma->buflist) return;

        i810_flush_queue(dev);

	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	   
		if (buf->pid == pid && buf_priv) {
			int used = cmpxchg(buf_priv->in_use, I810_BUF_CLIENT, 
					   I810_BUF_FREE);

			if (used == I810_BUF_CLIENT)
				DRM_DEBUG("reclaimed from client\n");
		   	if(buf_priv->currently_mapped == I810_BUF_MAPPED)
		     		buf_priv->currently_mapped = I810_BUF_UNMAPPED;
		}
	}
}

int i810_lock(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_device_t	  *dev	  = priv->dev;

	DECLARE_WAITQUEUE(entry, current);
	int		  ret	= 0;
	drm_lock_t	  lock;

	if (copy_from_user(&lock, (drm_lock_t *)arg, sizeof(lock)))
		return -EFAULT;

	if (lock.context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  current->pid, lock.context);
		return -EINVAL;
	}
   
   	DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		  lock.context, current->pid, dev->lock.hw_lock->lock,
		  lock.flags);

	if (lock.context < 0) {
		return -EINVAL;
	}
	/* Only one queue:
	 */

	if (!ret) {
		add_wait_queue(&dev->lock.lock_queue, &entry);
		for (;;) {
			current->state = TASK_INTERRUPTIBLE;
			if (!dev->lock.hw_lock) {
				/* Device has been unregistered */
				ret = -EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock.hw_lock->lock,
					  lock.context)) {
				dev->lock.pid	    = current->pid;
				dev->lock.lock_time = jiffies;
				atomic_inc(&dev->total_locks);
				break;	/* Got lock */
			}
			
				/* Contention */
			atomic_inc(&dev->total_sleeps);
		   	DRM_DEBUG("Calling lock schedule\n");
			schedule();
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&dev->lock.lock_queue, &entry);
	}
	
	if (!ret) {
		sigemptyset(&dev->sigmask);
		sigaddset(&dev->sigmask, SIGSTOP);
		sigaddset(&dev->sigmask, SIGTSTP);
		sigaddset(&dev->sigmask, SIGTTIN);
		sigaddset(&dev->sigmask, SIGTTOU);
		dev->sigdata.context = lock.context;
		dev->sigdata.lock    = dev->lock.hw_lock;
		block_all_signals(drm_notifier, &dev->sigdata, &dev->sigmask);

		if (lock.flags & _DRM_LOCK_QUIESCENT) {
		   DRM_DEBUG("_DRM_LOCK_QUIESCENT\n");
		   DRM_DEBUG("fred\n");
		   i810_dma_quiescent(dev);
		}
	}
	DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");
	return ret;
}

int i810_flush_ioctl(struct inode *inode, struct file *filp, 
		     unsigned int cmd, unsigned long arg)
{
   	drm_file_t	  *priv	  = filp->private_data;
   	drm_device_t	  *dev	  = priv->dev;
   
   	DRM_DEBUG("i810_flush_ioctl\n");
   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_flush_ioctl called without lock held\n");
		return -EINVAL;
	}

   	i810_flush_queue(dev);
   	return 0;
}


int i810_dma_vertex(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
      	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 
	drm_i810_vertex_t vertex;

	if (copy_from_user(&vertex, (drm_i810_vertex_t *)arg, sizeof(vertex)))
		return -EFAULT;

   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma_vertex called without lock held\n");
		return -EINVAL;
	}

	DRM_DEBUG("i810 dma vertex, idx %d used %d discard %d\n",
		  vertex.idx, vertex.used, vertex.discard);

	if(vertex.idx < 0 || vertex.idx > dma->buf_count) return -EINVAL;

	i810_dma_dispatch_vertex( dev, 
				  dma->buflist[ vertex.idx ], 
				  vertex.discard, vertex.used );

   	atomic_add(vertex.used, &dma->total_bytes);
	atomic_inc(&dma->total_dmas);
	sarea_priv->last_enqueue = dev_priv->counter-1;
   	sarea_priv->last_dispatch = (int) hw_status[5];
   
	return 0;
}



int i810_clear_bufs(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_i810_clear_t clear;

   	if (copy_from_user(&clear, (drm_i810_clear_t *)arg, sizeof(clear)))
		return -EFAULT;
   
   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_clear_bufs called without lock held\n");
		return -EINVAL;
	}

	i810_dma_dispatch_clear( dev, clear.flags, 
				 clear.clear_color, 
				 clear.clear_depth );
   	return 0;
}

int i810_swap_bufs(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
   
	DRM_DEBUG("i810_swap_bufs\n");

   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_swap_buf called without lock held\n");
		return -EINVAL;
	}

	i810_dma_dispatch_swap( dev );
   	return 0;
}

int i810_getage(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
   	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
      	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 

      	sarea_priv->last_dispatch = (int) hw_status[5];
	return 0;
}

int i810_getbuf(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
	int		  retcode   = 0;
	drm_i810_dma_t	  d;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
   	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 

	DRM_DEBUG("getbuf\n");
   	if (copy_from_user(&d, (drm_i810_dma_t *)arg, sizeof(d)))
		return -EFAULT;
   
	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma called without lock held\n");
		return -EINVAL;
	}
	
	d.granted = 0;

	retcode = i810_dma_get_buffer(dev, &d, filp);

	DRM_DEBUG("i810_dma: %d returning %d, granted = %d\n",
		  current->pid, retcode, d.granted);

	if (copy_to_user((drm_dma_t *)arg, &d, sizeof(d)))
		return -EFAULT;
   	sarea_priv->last_dispatch = (int) hw_status[5];

	return retcode;
}

int i810_copybuf(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
	drm_i810_copy_t	  d;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
   	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 
	drm_buf_t *buf;
	drm_i810_buf_priv_t *buf_priv;
	drm_device_dma_t *dma = dev->dma;

	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma called without lock held\n");
		return -EINVAL;
	}
   
   	if (copy_from_user(&d, (drm_i810_copy_t *)arg, sizeof(d)))
		return -EFAULT;

	if(d.idx < 0 || d.idx > dma->buf_count) return -EINVAL;
	buf = dma->buflist[ d.idx ];
   	buf_priv = buf->dev_private;
	if (buf_priv->currently_mapped != I810_BUF_MAPPED) return -EPERM;

	/* Stopping end users copying their data to the entire kernel
	   is good.. */
	if (d.used < 0 || d.used > buf->total)
		return -EINVAL;
		
   	if (copy_from_user(buf_priv->virtual, d.address, d.used))
		return -EFAULT;

   	sarea_priv->last_dispatch = (int) hw_status[5];

	return 0;
}

int i810_docopy(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	if(VM_DONTCOPY == 0) return 1;
	return 0;
}

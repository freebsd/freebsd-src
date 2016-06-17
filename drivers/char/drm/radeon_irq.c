/* radeon_irq.c -- IRQ handling for radeon -*- linux-c -*-
 *
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 * 
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
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
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 *    Michel Dänzer <michel@daenzer.net>
 */

#include "radeon.h"
#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "drm_os_linux.h"

/* Interrupts - Used for device synchronization and flushing in the
 * following circumstances:
 *
 * - Exclusive FB access with hw idle:
 *    - Wait for GUI Idle (?) interrupt, then do normal flush.
 *
 * - Frame throttling, NV_fence:
 *    - Drop marker irq's into command stream ahead of time.
 *    - Wait on irq's with lock *not held*
 *    - Check each for termination condition
 *
 * - Internally in cp_getbuffer, etc:
 *    - as above, but wait with lock held???
 *
 * NOTE: These functions are misleadingly named -- the irq's aren't
 * tied to dma at all, this is just a hangover from dri prehistory.
 */

void DRM(dma_service)(int irq, void *arg, struct pt_regs *reg)
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
   	u32 stat;

	stat = RADEON_READ(RADEON_GEN_INT_STATUS) 
		& (RADEON_SW_INT_TEST | RADEON_CRTC_VBLANK_STAT);
	if (!stat)
		return;

	/* SW interrupt */
	if (stat & RADEON_SW_INT_TEST) {
		wake_up_interruptible( &dev_priv->swi_queue );
	}

	/* VBLANK interrupt */
	if (stat & RADEON_CRTC_VBLANK_STAT) {
		atomic_inc(&dev->vbl_received);
		wake_up_interruptible(&dev->vbl_queue);
		DRM(vbl_send_signals)(dev);
	}

	/* Acknowledge all the bits in GEN_INT_STATUS -- seem to get
	 * more than we asked for...
	 */
	RADEON_WRITE(RADEON_GEN_INT_STATUS, stat);
}

static __inline__ void radeon_acknowledge_irqs(drm_radeon_private_t *dev_priv)
{
	u32 tmp = RADEON_READ( RADEON_GEN_INT_STATUS )
		& (RADEON_SW_INT_TEST_ACK | RADEON_CRTC_VBLANK_STAT);
	if (tmp)
		RADEON_WRITE( RADEON_GEN_INT_STATUS, tmp );
}

int radeon_emit_irq(drm_device_t *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	unsigned int ret;
	RING_LOCALS;

	atomic_inc(&dev_priv->swi_emitted);
	ret = atomic_read(&dev_priv->swi_emitted);

	BEGIN_RING( 4 );
	OUT_RING_REG( RADEON_LAST_SWI_REG, ret );
	OUT_RING_REG( RADEON_GEN_INT_STATUS, RADEON_SW_INT_FIRE );
	ADVANCE_RING(); 
 	COMMIT_RING();

	return ret;
}


int radeon_wait_irq(drm_device_t *dev, int swi_nr)
{
  	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
	int ret = 0;

 	if (RADEON_READ( RADEON_LAST_SWI_REG ) >= swi_nr)  
 		return 0; 

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	/* This is a hack to work around mysterious freezes on certain
	 * systems:
	 */ 
	radeon_acknowledge_irqs( dev_priv );

	DRM_WAIT_ON( ret, dev_priv->swi_queue, 3 * HZ, 
		     RADEON_READ( RADEON_LAST_SWI_REG ) >= swi_nr );

	return ret;
}

int radeon_emit_and_wait_irq(drm_device_t *dev)
{
	return radeon_wait_irq( dev, radeon_emit_irq(dev) );
}


int DRM(vblank_wait)(drm_device_t *dev, unsigned int *sequence)
{
  	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
	unsigned int cur_vblank;
	int ret = 0;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	radeon_acknowledge_irqs( dev_priv );

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using vertical blanks... 
	 */
	DRM_WAIT_ON( ret, dev->vbl_queue, 3*HZ, 
		     ( ( ( cur_vblank = atomic_read(&dev->vbl_received ) )
			 - *sequence ) <= (1<<23) ) );

	*sequence = cur_vblank;

	return ret;
}


/* Needs the lock as it touches the ring.
 */
int radeon_irq_emit(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long data)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_irq_emit_t emit;
	int result;

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	DRM_COPY_FROM_USER_IOCTL( emit, (drm_radeon_irq_emit_t *)data,
				  sizeof(emit) );

	result = radeon_emit_irq( dev );

	if ( copy_to_user( emit.irq_seq, &result, sizeof(int) ) ) {
		DRM_ERROR( "copy_to_user\n" );
		return -EFAULT;
	}

	return 0;
}


/* Doesn't need the hardware lock.
 */
int radeon_irq_wait(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long data)
{
        drm_file_t      *priv   = filp->private_data;
	drm_device_t    *dev    = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_irq_wait_t irqwait;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	DRM_COPY_FROM_USER_IOCTL( irqwait, (drm_radeon_irq_wait_t *)data,
				  sizeof(irqwait) );

	return radeon_wait_irq( dev, irqwait.irq_seq );
}


/* drm_dma.h hooks
*/
void DRM(driver_irq_preinstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;

 	/* Disable *all* interrupts */
      	RADEON_WRITE( RADEON_GEN_INT_CNTL, 0 );

	/* Clear bits if they're already high */
	radeon_acknowledge_irqs( dev_priv );
}

void DRM(driver_irq_postinstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;

   	atomic_set(&dev_priv->swi_emitted, 0);
	init_waitqueue_head( &dev_priv->swi_queue );

	/* Turn on SW and VBL ints */
   	RADEON_WRITE( RADEON_GEN_INT_CNTL,
		      RADEON_CRTC_VBLANK_MASK |	
		      RADEON_SW_INT_ENABLE );
}

void DRM(driver_irq_uninstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;
	if ( dev_priv ) {
		/* Disable *all* interrupts */
		RADEON_WRITE( RADEON_GEN_INT_CNTL, 0 );
	}
}

/* gamma_drv.h -- Private header for 3dlabs GMX 2000 driver -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
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
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 * 
 */

#ifndef _GAMMA_DRV_H_
#define _GAMMA_DRV_H_

				/* gamma_drv.c */
extern int  gamma_version(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  gamma_open(struct inode *inode, struct file *filp);
extern int  gamma_release(struct inode *inode, struct file *filp);
extern int  gamma_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  gamma_lock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  gamma_unlock(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);

				/* gamma_dma.c */
extern int  gamma_dma_schedule(drm_device_t *dev, int locked);
extern int  gamma_dma(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);
extern int  gamma_irq_install(drm_device_t *dev, int irq);
extern int  gamma_irq_uninstall(drm_device_t *dev);
extern int  gamma_control(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  gamma_find_devices(void);
extern int  gamma_found(void);

#endif

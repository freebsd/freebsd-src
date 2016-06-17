/* i810_drv.h -- Private header for the Matrox g200/g400 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:50:01 1999 by jhartmann@precisioninsight.com
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
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 * 	    Jeff Hartmann <jhartmann@valinux.com>
 *
 */

#ifndef _I810_DRV_H_
#define _I810_DRV_H_

typedef struct drm_i810_buf_priv {
   	u32 *in_use;
   	int my_use_idx;
	int currently_mapped;
	void *virtual;
	void *kernel_virtual;
	int map_count;
   	struct vm_area_struct *vma;
} drm_i810_buf_priv_t;

typedef struct _drm_i810_ring_buffer{
	int tail_mask;
	unsigned long Start;
	unsigned long End;
	unsigned long Size;
	u8 *virtual_start;
	int head;
	int tail;
	int space;
} drm_i810_ring_buffer_t;

typedef struct drm_i810_private {
   	int ring_map_idx;
   	int buffer_map_idx;

   	drm_i810_ring_buffer_t ring;
	drm_i810_sarea_t *sarea_priv;

      	unsigned long hw_status_page;
   	unsigned long counter;

   	atomic_t flush_done;
   	wait_queue_head_t flush_queue;	/* Processes waiting until flush    */
	drm_buf_t *mmap_buffer;

	
	u32 front_di1, back_di1, zi1;
	
	int back_offset;
	int depth_offset;
	int w, h;
	int pitch;
} drm_i810_private_t;

				/* i810_drv.c */
extern int  i810_version(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_open(struct inode *inode, struct file *filp);
extern int  i810_release(struct inode *inode, struct file *filp);
extern int  i810_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  i810_unlock(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);

				/* i810_dma.c */
extern int  i810_dma_schedule(drm_device_t *dev, int locked);
extern int  i810_getbuf(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  i810_irq_install(drm_device_t *dev, int irq);
extern int  i810_irq_uninstall(drm_device_t *dev);
extern int  i810_control(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_lock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_dma_init(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_flush_ioctl(struct inode *inode, struct file *filp,
			     unsigned int cmd, unsigned long arg);
extern void i810_reclaim_buffers(drm_device_t *dev, pid_t pid);
extern int  i810_getage(struct inode *inode, struct file *filp, unsigned int cmd,
			unsigned long arg);
extern int i810_mmap_buffers(struct file *filp, struct vm_area_struct *vma);
extern int i810_copybuf(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg);
extern int i810_docopy(struct inode *inode, struct file *filp, 
		       unsigned int cmd, unsigned long arg);

				/* i810_bufs.c */
extern int  i810_addbufs(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg);
extern int  i810_infobufs(struct inode *inode, struct file *filp, 
			 unsigned int cmd, unsigned long arg);
extern int  i810_markbufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  i810_freebufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  i810_addmap(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);

				/* i810_context.c */
extern int  i810_resctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_addctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_modctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_getctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_switchctx(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_newctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  i810_rmctx(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);

extern int  i810_context_switch(drm_device_t *dev, int old, int new);
extern int  i810_context_switch_complete(drm_device_t *dev, int new);

#define I810_VERBOSE 0


int i810_dma_vertex(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg);

int i810_swap_bufs(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg);

int i810_clear_bufs(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg);

#define GFX_OP_USER_INTERRUPT 		((0<<29)|(2<<23))
#define GFX_OP_BREAKPOINT_INTERRUPT	((0<<29)|(1<<23))
#define CMD_REPORT_HEAD			(7<<23)
#define CMD_STORE_DWORD_IDX		((0x21<<23) | 0x1)
#define CMD_OP_BATCH_BUFFER  ((0x0<<29)|(0x30<<23)|0x1)

#define INST_PARSER_CLIENT   0x00000000
#define INST_OP_FLUSH        0x02000000
#define INST_FLUSH_MAP_CACHE 0x00000001


#define BB1_START_ADDR_MASK   (~0x7)
#define BB1_PROTECTED         (1<<0)
#define BB1_UNPROTECTED       (0<<0)
#define BB2_END_ADDR_MASK     (~0x7)

#define I810REG_HWSTAM		0x02098
#define I810REG_INT_IDENTITY_R	0x020a4
#define I810REG_INT_MASK_R 	0x020a8
#define I810REG_INT_ENABLE_R	0x020a0

#define LP_RING     		0x2030
#define HP_RING     		0x2040
#define RING_TAIL      		0x00
#define TAIL_ADDR		0x000FFFF8
#define RING_HEAD      		0x04
#define HEAD_WRAP_COUNT     	0xFFE00000
#define HEAD_WRAP_ONE       	0x00200000
#define HEAD_ADDR           	0x001FFFFC
#define RING_START     		0x08
#define START_ADDR          	0x00FFFFF8
#define RING_LEN       		0x0C
#define RING_NR_PAGES       	0x000FF000 
#define RING_REPORT_MASK    	0x00000006
#define RING_REPORT_64K     	0x00000002
#define RING_REPORT_128K    	0x00000004
#define RING_NO_REPORT      	0x00000000
#define RING_VALID_MASK     	0x00000001
#define RING_VALID          	0x00000001
#define RING_INVALID        	0x00000000

#define GFX_OP_SCISSOR         ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define SC_UPDATE_SCISSOR       (0x1<<1)
#define SC_ENABLE_MASK          (0x1<<0)
#define SC_ENABLE               (0x1<<0)

#define GFX_OP_SCISSOR_INFO    ((0x3<<29)|(0x1d<<24)|(0x81<<16)|(0x1))
#define SCI_YMIN_MASK      (0xffff<<16)
#define SCI_XMIN_MASK      (0xffff<<0)
#define SCI_YMAX_MASK      (0xffff<<16)
#define SCI_XMAX_MASK      (0xffff<<0)

#define GFX_OP_COLOR_FACTOR      ((0x3<<29)|(0x1d<<24)|(0x1<<16)|0x0)
#define GFX_OP_STIPPLE           ((0x3<<29)|(0x1d<<24)|(0x83<<16))
#define GFX_OP_MAP_INFO          ((0x3<<29)|(0x1d<<24)|0x2)
#define GFX_OP_DESTBUFFER_VARS   ((0x3<<29)|(0x1d<<24)|(0x85<<16)|0x0)
#define GFX_OP_DRAWRECT_INFO     ((0x3<<29)|(0x1d<<24)|(0x80<<16)|(0x3))
#define GFX_OP_PRIMITIVE         ((0x3<<29)|(0x1f<<24))

#define CMD_OP_Z_BUFFER_INFO     ((0x0<<29)|(0x16<<23))
#define CMD_OP_DESTBUFFER_INFO   ((0x0<<29)|(0x15<<23))

#define BR00_BITBLT_CLIENT   0x40000000
#define BR00_OP_COLOR_BLT    0x10000000
#define BR00_OP_SRC_COPY_BLT 0x10C00000
#define BR13_SOLID_PATTERN   0x80000000



#endif


/* drm.h -- Header for Direct Rendering Manager -*- linux-c -*-
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
 * Acknowledgements:
 * Dec 1999, Richard Henderson <rth@twiddle.net>, move to generic cmpxchg.
 *
 */

#ifndef _DRM_H_
#define _DRM_H_

#include <linux/config.h>
#if defined(__linux__)
#include <asm/ioctl.h>		/* For _IO* macros */
#define DRM_IOCTL_NR(n)	     _IOC_NR(n)
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#define DRM_IOCTL_NR(n)	     ((n) & 0xff)
#endif

#define DRM_PROC_DEVICES "/proc/devices"
#define DRM_PROC_MISC	 "/proc/misc"
#define DRM_PROC_DRM	 "/proc/drm"
#define DRM_DEV_DRM	 "/dev/drm"
#define DRM_DEV_MODE	 (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	 0
#define DRM_DEV_GID	 0


#define DRM_NAME	"drm"	  /* Name in kernel, /dev, and /proc	    */
#define DRM_MIN_ORDER	5	  /* At least 2^5 bytes = 32 bytes	    */
#define DRM_MAX_ORDER	22	  /* Up to 2^22 bytes = 4MB		    */
#define DRM_RAM_PERCENT 10	  /* How much system ram can we lock?	    */

#define _DRM_LOCK_HELD	0x80000000 /* Hardware lock is held		    */
#define _DRM_LOCK_CONT	0x40000000 /* Hardware lock is contended	    */
#define _DRM_LOCK_IS_HELD(lock)	   ((lock) & _DRM_LOCK_HELD)
#define _DRM_LOCK_IS_CONT(lock)	   ((lock) & _DRM_LOCK_CONT)
#define _DRM_LOCKING_CONTEXT(lock) ((lock) & ~(_DRM_LOCK_HELD|_DRM_LOCK_CONT))

typedef unsigned long drm_handle_t;
typedef unsigned int  drm_context_t;
typedef unsigned int  drm_drawable_t;
typedef unsigned int  drm_magic_t;

/* Warning: If you change this structure, make sure you change
 * XF86DRIClipRectRec in the server as well */

typedef struct drm_clip_rect {
           unsigned short x1;
           unsigned short y1;
           unsigned short x2;
           unsigned short y2;
} drm_clip_rect_t;

/* Seperate include files for the i810/mga/r128 specific structures */
#include "mga_drm.h"
#include "i810_drm.h"
#include "r128_drm.h"
#include "radeon_drm.h"
#ifdef CONFIG_DRM40_SIS
#include "sis_drm.h"
#endif

typedef struct drm_version {
	int    version_major;	  /* Major version			    */
	int    version_minor;	  /* Minor version			    */
	int    version_patchlevel;/* Patch level			    */
	size_t name_len;	  /* Length of name buffer		    */
	char   *name;		  /* Name of driver			    */
	size_t date_len;	  /* Length of date buffer		    */
	char   *date;		  /* User-space buffer to hold date	    */
	size_t desc_len;	  /* Length of desc buffer		    */
	char   *desc;		  /* User-space buffer to hold desc	    */
} drm_version_t;

typedef struct drm_unique {
	size_t unique_len;	  /* Length of unique			    */
	char   *unique;		  /* Unique name for driver instantiation   */
} drm_unique_t;

typedef struct drm_list {
	int		 count;	  /* Length of user-space structures	    */
	drm_version_t	 *version;
} drm_list_t;

typedef struct drm_block {
	int		 unused;
} drm_block_t;

typedef struct drm_control {
	enum {
		DRM_ADD_COMMAND,
		DRM_RM_COMMAND,
		DRM_INST_HANDLER,
		DRM_UNINST_HANDLER
	}		 func;
	int		 irq;
} drm_control_t;

typedef enum drm_map_type {
	_DRM_FRAME_BUFFER = 0,	  /* WC (no caching), no core dump	    */
	_DRM_REGISTERS	  = 1,	  /* no caching, no core dump		    */
	_DRM_SHM	  = 2,	  /* shared, cached			    */
	_DRM_AGP          = 3	  /* AGP/GART                               */
} drm_map_type_t;

typedef enum drm_map_flags {
	_DRM_RESTRICTED	     = 0x01, /* Cannot be mapped to user-virtual    */
	_DRM_READ_ONLY	     = 0x02,
	_DRM_LOCKED	     = 0x04, /* shared, cached, locked		    */
	_DRM_KERNEL	     = 0x08, /* kernel requires access		    */
	_DRM_WRITE_COMBINING = 0x10, /* use write-combining if available    */
	_DRM_CONTAINS_LOCK   = 0x20  /* SHM page that contains lock	    */
} drm_map_flags_t;

typedef struct drm_map {
	unsigned long	offset;	 /* Requested physical address (0 for SAREA)*/
	unsigned long	size;	 /* Requested physical size (bytes)	    */
	drm_map_type_t	type;	 /* Type of memory to map		    */
	drm_map_flags_t flags;	 /* Flags				    */
	void		*handle; /* User-space: "Handle" to pass to mmap    */
				 /* Kernel-space: kernel-virtual address    */
	int		mtrr;	 /* MTRR slot used			    */
				 /* Private data			    */
} drm_map_t;

typedef enum drm_lock_flags {
	_DRM_LOCK_READY	     = 0x01, /* Wait until hardware is ready for DMA */
	_DRM_LOCK_QUIESCENT  = 0x02, /* Wait until hardware quiescent	     */
	_DRM_LOCK_FLUSH	     = 0x04, /* Flush this context's DMA queue first */
	_DRM_LOCK_FLUSH_ALL  = 0x08, /* Flush all DMA queues first	     */
				/* These *HALT* flags aren't supported yet
				   -- they will be used to support the
				   full-screen DGA-like mode. */
	_DRM_HALT_ALL_QUEUES = 0x10, /* Halt all current and future queues   */
	_DRM_HALT_CUR_QUEUES = 0x20  /* Halt all current queues		     */
} drm_lock_flags_t;

typedef struct drm_lock {
	int		 context;
	drm_lock_flags_t flags;
} drm_lock_t;

typedef enum drm_dma_flags {	      /* These values *MUST* match xf86drm.h */
				      /* Flags for DMA buffer dispatch	     */
	_DRM_DMA_BLOCK	      = 0x01, /* Block until buffer dispatched.
					 Note, the buffer may not yet have
					 been processed by the hardware --
					 getting a hardware lock with the
					 hardware quiescent will ensure
					 that the buffer has been
					 processed.			     */
	_DRM_DMA_WHILE_LOCKED = 0x02, /* Dispatch while lock held	     */
	_DRM_DMA_PRIORITY     = 0x04, /* High priority dispatch		     */

				      /* Flags for DMA buffer request	     */
	_DRM_DMA_WAIT	      = 0x10, /* Wait for free buffers		     */
	_DRM_DMA_SMALLER_OK   = 0x20, /* Smaller-than-requested buffers ok   */
	_DRM_DMA_LARGER_OK    = 0x40  /* Larger-than-requested buffers ok    */
} drm_dma_flags_t;

typedef struct drm_buf_desc {
	int	      count;	 /* Number of buffers of this size	     */
	int	      size;	 /* Size in bytes			     */
	int	      low_mark;	 /* Low water mark			     */
	int	      high_mark; /* High water mark			     */
	enum {
		_DRM_PAGE_ALIGN = 0x01, /* Align on page boundaries for DMA  */
		_DRM_AGP_BUFFER = 0x02  /* Buffer is in agp space            */
	}	      flags;
	unsigned long agp_start; /* Start address of where the agp buffers
				  * are in the agp aperture */
} drm_buf_desc_t;

typedef struct drm_buf_info {
	int	       count;	/* Entries in list			     */
	drm_buf_desc_t *list;
} drm_buf_info_t;

typedef struct drm_buf_free {
	int	       count;
	int	       *list;
} drm_buf_free_t;

typedef struct drm_buf_pub {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	void		  *address;    /* Address of buffer		     */
} drm_buf_pub_t;

typedef struct drm_buf_map {
	int	      count;	/* Length of buflist			    */
	void	      *virtual;	/* Mmaped area in user-virtual		    */
	drm_buf_pub_t *list;	/* Buffer information			    */
} drm_buf_map_t;

typedef struct drm_dma {
				/* Indices here refer to the offset into
				   buflist in drm_buf_get_t.  */
	int		context;	  /* Context handle		    */
	int		send_count;	  /* Number of buffers to send	    */
	int		*send_indices;	  /* List of handles to buffers	    */
	int		*send_sizes;	  /* Lengths of data to send	    */
	drm_dma_flags_t flags;		  /* Flags			    */
	int		request_count;	  /* Number of buffers requested    */
	int		request_size;	  /* Desired size for buffers	    */
	int		*request_indices; /* Buffer information		    */
	int		*request_sizes;
	int		granted_count;	  /* Number of buffers granted	    */
} drm_dma_t;

typedef enum {
	_DRM_CONTEXT_PRESERVED = 0x01,
	_DRM_CONTEXT_2DONLY    = 0x02
} drm_ctx_flags_t;

typedef struct drm_ctx {
	drm_context_t	handle;
	drm_ctx_flags_t flags;
} drm_ctx_t;

typedef struct drm_ctx_res {
	int		count;
	drm_ctx_t	*contexts;
} drm_ctx_res_t;

typedef struct drm_draw {
	drm_drawable_t	handle;
} drm_draw_t;

typedef struct drm_auth {
	drm_magic_t	magic;
} drm_auth_t;

typedef struct drm_irq_busid {
	int irq;
	int busnum;
	int devnum;
	int funcnum;
} drm_irq_busid_t;

typedef struct drm_agp_mode {
	unsigned long mode;
} drm_agp_mode_t;

				/* For drm_agp_alloc -- allocated a buffer */
typedef struct drm_agp_buffer {
	unsigned long size;	/* In bytes -- will round to page boundary */
	unsigned long handle;	/* Used for BIND/UNBIND ioctls */
	unsigned long type;     /* Type of memory to allocate  */
        unsigned long physical; /* Physical used by i810       */
} drm_agp_buffer_t;

				/* For drm_agp_bind */
typedef struct drm_agp_binding {
	unsigned long handle;   /* From drm_agp_buffer */
	unsigned long offset;	/* In bytes -- will round to page boundary */
} drm_agp_binding_t;

typedef struct drm_agp_info {
	int            agp_version_major;
	int            agp_version_minor;
	unsigned long  mode;
	unsigned long  aperture_base;  /* physical address */
	unsigned long  aperture_size;  /* bytes */
	unsigned long  memory_allowed; /* bytes */
	unsigned long  memory_used;

				/* PCI information */
	unsigned short id_vendor;
	unsigned short id_device;
} drm_agp_info_t;

#define DRM_IOCTL_BASE			'd'
#define DRM_IO(nr)			_IO(DRM_IOCTL_BASE,nr)
#define DRM_IOR(nr,size)		_IOR(DRM_IOCTL_BASE,nr,size)
#define DRM_IOW(nr,size)		_IOW(DRM_IOCTL_BASE,nr,size)
#define DRM_IOWR(nr,size)		_IOWR(DRM_IOCTL_BASE,nr,size)


#define DRM_IOCTL_VERSION		DRM_IOWR(0x00, drm_version_t)
#define DRM_IOCTL_GET_UNIQUE		DRM_IOWR(0x01, drm_unique_t)
#define DRM_IOCTL_GET_MAGIC		DRM_IOR( 0x02, drm_auth_t)
#define DRM_IOCTL_IRQ_BUSID		DRM_IOWR(0x03, drm_irq_busid_t)

#define DRM_IOCTL_SET_UNIQUE		DRM_IOW( 0x10, drm_unique_t)
#define DRM_IOCTL_AUTH_MAGIC		DRM_IOW( 0x11, drm_auth_t)
#define DRM_IOCTL_BLOCK			DRM_IOWR(0x12, drm_block_t)
#define DRM_IOCTL_UNBLOCK		DRM_IOWR(0x13, drm_block_t)
#define DRM_IOCTL_CONTROL		DRM_IOW( 0x14, drm_control_t)
#define DRM_IOCTL_ADD_MAP		DRM_IOWR(0x15, drm_map_t)
#define DRM_IOCTL_ADD_BUFS		DRM_IOWR(0x16, drm_buf_desc_t)
#define DRM_IOCTL_MARK_BUFS		DRM_IOW( 0x17, drm_buf_desc_t)
#define DRM_IOCTL_INFO_BUFS		DRM_IOWR(0x18, drm_buf_info_t)
#define DRM_IOCTL_MAP_BUFS		DRM_IOWR(0x19, drm_buf_map_t)
#define DRM_IOCTL_FREE_BUFS		DRM_IOW( 0x1a, drm_buf_free_t)

#define DRM_IOCTL_ADD_CTX		DRM_IOWR(0x20, drm_ctx_t)
#define DRM_IOCTL_RM_CTX		DRM_IOWR(0x21, drm_ctx_t)
#define DRM_IOCTL_MOD_CTX		DRM_IOW( 0x22, drm_ctx_t)
#define DRM_IOCTL_GET_CTX		DRM_IOWR(0x23, drm_ctx_t)
#define DRM_IOCTL_SWITCH_CTX		DRM_IOW( 0x24, drm_ctx_t)
#define DRM_IOCTL_NEW_CTX		DRM_IOW( 0x25, drm_ctx_t)
#define DRM_IOCTL_RES_CTX		DRM_IOWR(0x26, drm_ctx_res_t)
#define DRM_IOCTL_ADD_DRAW		DRM_IOWR(0x27, drm_draw_t)
#define DRM_IOCTL_RM_DRAW		DRM_IOWR(0x28, drm_draw_t)
#define DRM_IOCTL_DMA			DRM_IOWR(0x29, drm_dma_t)
#define DRM_IOCTL_LOCK			DRM_IOW( 0x2a, drm_lock_t)
#define DRM_IOCTL_UNLOCK		DRM_IOW( 0x2b, drm_lock_t)
#define DRM_IOCTL_FINISH		DRM_IOW( 0x2c, drm_lock_t)

#define DRM_IOCTL_AGP_ACQUIRE		DRM_IO(  0x30)
#define DRM_IOCTL_AGP_RELEASE		DRM_IO(  0x31)
#define DRM_IOCTL_AGP_ENABLE		DRM_IOW( 0x32, drm_agp_mode_t)
#define DRM_IOCTL_AGP_INFO		DRM_IOR( 0x33, drm_agp_info_t)
#define DRM_IOCTL_AGP_ALLOC		DRM_IOWR(0x34, drm_agp_buffer_t)
#define DRM_IOCTL_AGP_FREE		DRM_IOW( 0x35, drm_agp_buffer_t)
#define DRM_IOCTL_AGP_BIND		DRM_IOW( 0x36, drm_agp_binding_t)
#define DRM_IOCTL_AGP_UNBIND		DRM_IOW( 0x37, drm_agp_binding_t)

/* Mga specific ioctls */
#define DRM_IOCTL_MGA_INIT		DRM_IOW( 0x40, drm_mga_init_t)
#define DRM_IOCTL_MGA_SWAP		DRM_IOW( 0x41, drm_mga_swap_t)
#define DRM_IOCTL_MGA_CLEAR		DRM_IOW( 0x42, drm_mga_clear_t)
#define DRM_IOCTL_MGA_ILOAD		DRM_IOW( 0x43, drm_mga_iload_t)
#define DRM_IOCTL_MGA_VERTEX		DRM_IOW( 0x44, drm_mga_vertex_t)
#define DRM_IOCTL_MGA_FLUSH		DRM_IOW( 0x45, drm_lock_t )
#define DRM_IOCTL_MGA_INDICES		DRM_IOW( 0x46, drm_mga_indices_t)
#define DRM_IOCTL_MGA_BLIT		DRM_IOW( 0x47, drm_mga_blit_t)

/* I810 specific ioctls */
#define DRM_IOCTL_I810_INIT		DRM_IOW( 0x40, drm_i810_init_t)
#define DRM_IOCTL_I810_VERTEX		DRM_IOW( 0x41, drm_i810_vertex_t)
#define DRM_IOCTL_I810_CLEAR		DRM_IOW( 0x42, drm_i810_clear_t)
#define DRM_IOCTL_I810_FLUSH		DRM_IO(  0x43)
#define DRM_IOCTL_I810_GETAGE		DRM_IO(  0x44)
#define DRM_IOCTL_I810_GETBUF		DRM_IOWR(0x45, drm_i810_dma_t)
#define DRM_IOCTL_I810_SWAP		DRM_IO(  0x46)
#define DRM_IOCTL_I810_COPY		DRM_IOW( 0x47, drm_i810_copy_t)
#define DRM_IOCTL_I810_DOCOPY		DRM_IO(  0x48)

/* Rage 128 specific ioctls */
#define DRM_IOCTL_R128_INIT		DRM_IOW( 0x40, drm_r128_init_t)
#define DRM_IOCTL_R128_CCE_START	DRM_IO(  0x41)
#define DRM_IOCTL_R128_CCE_STOP		DRM_IOW( 0x42, drm_r128_cce_stop_t)
#define DRM_IOCTL_R128_CCE_RESET	DRM_IO(  0x43)
#define DRM_IOCTL_R128_CCE_IDLE		DRM_IO(  0x44)
#define DRM_IOCTL_R128_RESET		DRM_IO(  0x46)
#define DRM_IOCTL_R128_SWAP		DRM_IO(  0x47)
#define DRM_IOCTL_R128_CLEAR		DRM_IOW( 0x48, drm_r128_clear_t)
#define DRM_IOCTL_R128_VERTEX		DRM_IOW( 0x49, drm_r128_vertex_t)
#define DRM_IOCTL_R128_INDICES		DRM_IOW( 0x4a, drm_r128_indices_t)
#define DRM_IOCTL_R128_BLIT		DRM_IOW( 0x4b, drm_r128_blit_t)
#define DRM_IOCTL_R128_DEPTH		DRM_IOW( 0x4c, drm_r128_depth_t)
#define DRM_IOCTL_R128_STIPPLE		DRM_IOW( 0x4d, drm_r128_stipple_t)
#define DRM_IOCTL_R128_PACKET		DRM_IOWR(0x4e, drm_r128_packet_t)

/* Radeon specific ioctls */
#define DRM_IOCTL_RADEON_CP_INIT	DRM_IOW( 0x40, drm_radeon_init_t)
#define DRM_IOCTL_RADEON_CP_START	DRM_IO(  0x41)
#define DRM_IOCTL_RADEON_CP_STOP	DRM_IOW( 0x42, drm_radeon_cp_stop_t)
#define DRM_IOCTL_RADEON_CP_RESET	DRM_IO(  0x43)
#define DRM_IOCTL_RADEON_CP_IDLE	DRM_IO(  0x44)
#define DRM_IOCTL_RADEON_RESET		DRM_IO(  0x45)
#define DRM_IOCTL_RADEON_FULLSCREEN	DRM_IOW( 0x46, drm_radeon_fullscreen_t)
#define DRM_IOCTL_RADEON_SWAP		DRM_IO(  0x47)
#define DRM_IOCTL_RADEON_CLEAR		DRM_IOW( 0x48, drm_radeon_clear_t)
#define DRM_IOCTL_RADEON_VERTEX		DRM_IOW( 0x49, drm_radeon_vertex_t)
#define DRM_IOCTL_RADEON_INDICES	DRM_IOW( 0x4a, drm_radeon_indices_t)
#define DRM_IOCTL_RADEON_BLIT		DRM_IOW( 0x4b, drm_radeon_blit_t)
#define DRM_IOCTL_RADEON_STIPPLE	DRM_IOW( 0x4c, drm_radeon_stipple_t)
#define DRM_IOCTL_RADEON_INDIRECT	DRM_IOWR(0x4d, drm_radeon_indirect_t)

#ifdef CONFIG_DRM40_SIS
/* SiS specific ioctls */
#define SIS_IOCTL_FB_ALLOC		DRM_IOWR(0x44, drm_sis_mem_t)
#define SIS_IOCTL_FB_FREE		DRM_IOW( 0x45, drm_sis_mem_t)
#define SIS_IOCTL_AGP_INIT		DRM_IOWR(0x53, drm_sis_agp_t)
#define SIS_IOCTL_AGP_ALLOC		DRM_IOWR(0x54, drm_sis_mem_t)
#define SIS_IOCTL_AGP_FREE		DRM_IOW( 0x55, drm_sis_mem_t)
#define SIS_IOCTL_FLIP			DRM_IOW( 0x48, drm_sis_flip_t)
#define SIS_IOCTL_FLIP_INIT		DRM_IO(  0x49)
#define SIS_IOCTL_FLIP_FINAL		DRM_IO(  0x50)
#endif

#endif

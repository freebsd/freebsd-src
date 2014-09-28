/**
 * \file drm.h
 * Header for the Direct Rendering Manager
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 *
 * \par Acknowledgments:
 * Dec 1999, Richard Henderson <rth@twiddle.net>, move to generic \c cmpxchg.
 */

/*-
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/**
 * \mainpage
 *
 * The Direct Rendering Manager (DRM) is a device-independent kernel-level
 * device driver that provides support for the XFree86 Direct Rendering
 * Infrastructure (DRI).
 *
 * The DRM supports the Direct Rendering Infrastructure (DRI) in four major
 * ways:
 *     -# The DRM provides synchronized access to the graphics hardware via
 *        the use of an optimized two-tiered lock.
 *     -# The DRM enforces the DRI security policy for access to the graphics
 *        hardware by only allowing authenticated X11 clients access to
 *        restricted regions of memory.
 *     -# The DRM provides a generic DMA engine, complete with multiple
 *        queues and the ability to detect the need for an OpenGL context
 *        switch.
 *     -# The DRM is extensible via the use of small device-specific modules
 *        that rely extensively on the API exported by the DRM module.
 *
 */

#ifndef _DRM_H_
#define _DRM_H_

#ifndef __user
#define __user
#endif
#ifndef __iomem
#define __iomem
#endif

#ifdef __GNUC__
# define DEPRECATED  __attribute__ ((deprecated))
#else
# define DEPRECATED
#endif

#if defined(__linux__)
#include <asm/ioctl.h>		/* For _IO* macros */
#define DRM_IOCTL_NR(n)		_IOC_NR(n)
#define DRM_IOC_VOID		_IOC_NONE
#define DRM_IOC_READ		_IOC_READ
#define DRM_IOC_WRITE		_IOC_WRITE
#define DRM_IOC_READWRITE	_IOC_READ|_IOC_WRITE
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/ioccom.h>
#define DRM_IOCTL_NR(n)		((n) & 0xff)
#define DRM_IOC_VOID		IOC_VOID
#define DRM_IOC_READ		IOC_OUT
#define DRM_IOC_WRITE		IOC_IN
#define DRM_IOC_READWRITE	IOC_INOUT
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)
#endif

#ifdef __OpenBSD__
#define DRM_MAJOR       81
#endif
#if defined(__linux__) || defined(__NetBSD__)
#define DRM_MAJOR       226
#endif
#define DRM_MAX_MINOR   15

#define DRM_NAME	"drm"	  /**< Name in kernel, /dev, and /proc */
#define DRM_MIN_ORDER	5	  /**< At least 2^5 bytes = 32 bytes */
#define DRM_MAX_ORDER	22	  /**< Up to 2^22 bytes = 4MB */
#define DRM_RAM_PERCENT 10	  /**< How much system ram can we lock? */

#define _DRM_LOCK_HELD	0x80000000U /**< Hardware lock is held */
#define _DRM_LOCK_CONT	0x40000000U /**< Hardware lock is contended */
#define _DRM_LOCK_IS_HELD(lock)	   ((lock) & _DRM_LOCK_HELD)
#define _DRM_LOCK_IS_CONT(lock)	   ((lock) & _DRM_LOCK_CONT)
#define _DRM_LOCKING_CONTEXT(lock) ((lock) & ~(_DRM_LOCK_HELD|_DRM_LOCK_CONT))

#if defined(__linux__)
typedef unsigned int drm_handle_t;
#else
#include <sys/types.h>
typedef unsigned long drm_handle_t;	/**< To mapped regions */
#endif
typedef unsigned int drm_context_t;	/**< GLXContext handle */
typedef unsigned int drm_drawable_t;
typedef unsigned int drm_magic_t;	/**< Magic for authentication */

/**
 * Cliprect.
 *
 * \warning If you change this structure, make sure you change
 * XF86DRIClipRectRec in the server as well
 *
 * \note KW: Actually it's illegal to change either for
 * backwards-compatibility reasons.
 */
struct drm_clip_rect {
	unsigned short x1;
	unsigned short y1;
	unsigned short x2;
	unsigned short y2;
};

/**
 * Texture region,
 */
struct drm_tex_region {
	unsigned char next;
	unsigned char prev;
	unsigned char in_use;
	unsigned char padding;
	unsigned int age;
};

/**
 * Hardware lock.
 *
 * The lock structure is a simple cache-line aligned integer.  To avoid
 * processor bus contention on a multiprocessor system, there should not be any
 * other data stored in the same cache line.
 */
struct drm_hw_lock {
	__volatile__ unsigned int lock;		/**< lock variable */
	char padding[60];			/**< Pad to cache line */
};

/* This is beyond ugly, and only works on GCC.  However, it allows me to use
 * drm.h in places (i.e., in the X-server) where I can't use size_t.  The real
 * fix is to use uint32_t instead of size_t, but that fix will break existing
 * LP64 (i.e., PowerPC64, SPARC64, Alpha, etc.) systems.  That *will*
 * eventually happen, though.  I chose 'unsigned long' to be the fallback type
 * because that works on all the platforms I know about.  Hopefully, the
 * real fix will happen before that bites us.
 */

#ifdef __SIZE_TYPE__
# define DRM_SIZE_T __SIZE_TYPE__
#else
# warning "__SIZE_TYPE__ not defined.  Assuming sizeof(size_t) == sizeof(unsigned long)!"
# define DRM_SIZE_T unsigned long
#endif

/**
 * DRM_IOCTL_VERSION ioctl argument type.
 *
 * \sa drmGetVersion().
 */
struct drm_version {
	int version_major;	  /**< Major version */
	int version_minor;	  /**< Minor version */
	int version_patchlevel;	  /**< Patch level */
	DRM_SIZE_T name_len;	  /**< Length of name buffer */
	char __user *name;		  /**< Name of driver */
	DRM_SIZE_T date_len;	  /**< Length of date buffer */
	char __user *date;		  /**< User-space buffer to hold date */
	DRM_SIZE_T desc_len;	  /**< Length of desc buffer */
	char __user *desc;		  /**< User-space buffer to hold desc */
};

/**
 * DRM_IOCTL_GET_UNIQUE ioctl argument type.
 *
 * \sa drmGetBusid() and drmSetBusId().
 */
struct drm_unique {
	DRM_SIZE_T unique_len;	  /**< Length of unique */
	char __user *unique;		  /**< Unique name for driver instantiation */
};

#undef DRM_SIZE_T

struct drm_list {
	int count;		  /**< Length of user-space structures */
	struct drm_version __user *version;
};

struct drm_block {
	int unused;
};

/**
 * DRM_IOCTL_CONTROL ioctl argument type.
 *
 * \sa drmCtlInstHandler() and drmCtlUninstHandler().
 */
struct drm_control {
	enum {
		DRM_ADD_COMMAND,
		DRM_RM_COMMAND,
		DRM_INST_HANDLER,
		DRM_UNINST_HANDLER
	} func;
	int irq;
};

/**
 * Type of memory to map.
 */
enum drm_map_type {
	_DRM_FRAME_BUFFER = 0,	  /**< WC (no caching), no core dump */
	_DRM_REGISTERS = 1,	  /**< no caching, no core dump */
	_DRM_SHM = 2,		  /**< shared, cached */
	_DRM_AGP = 3,		  /**< AGP/GART */
	_DRM_SCATTER_GATHER = 4,  /**< Scatter/gather memory for PCI DMA */
	_DRM_CONSISTENT = 5,	  /**< Consistent memory for PCI DMA */
	_DRM_TTM = 6
};

/**
 * Memory mapping flags.
 */
enum drm_map_flags {
	_DRM_RESTRICTED = 0x01,	     /**< Cannot be mapped to user-virtual */
	_DRM_READ_ONLY = 0x02,
	_DRM_LOCKED = 0x04,	     /**< shared, cached, locked */
	_DRM_KERNEL = 0x08,	     /**< kernel requires access */
	_DRM_WRITE_COMBINING = 0x10, /**< use write-combining if available */
	_DRM_CONTAINS_LOCK = 0x20,   /**< SHM page that contains lock */
	_DRM_REMOVABLE = 0x40,	     /**< Removable mapping */
	_DRM_DRIVER = 0x80	     /**< Managed by driver */
};

struct drm_ctx_priv_map {
	unsigned int ctx_id;	 /**< Context requesting private mapping */
	void *handle;		 /**< Handle of map */
};

/**
 * DRM_IOCTL_GET_MAP, DRM_IOCTL_ADD_MAP and DRM_IOCTL_RM_MAP ioctls
 * argument type.
 *
 * \sa drmAddMap().
 */
struct drm_map {
	unsigned long offset;	 /**< Requested physical address (0 for SAREA)*/
	unsigned long size;	 /**< Requested physical size (bytes) */
	enum drm_map_type type;	 /**< Type of memory to map */
	enum drm_map_flags flags;	 /**< Flags */
	void *handle;		 /**< User-space: "Handle" to pass to mmap() */
				 /**< Kernel-space: kernel-virtual address */
	int mtrr;		 /**< MTRR slot used */
	/*   Private data */
};

/**
 * DRM_IOCTL_GET_CLIENT ioctl argument type.
 */
struct drm_client {
	int idx;		/**< Which client desired? */
	int auth;		/**< Is client authenticated? */
	unsigned long pid;	/**< Process ID */
	unsigned long uid;	/**< User ID */
	unsigned long magic;	/**< Magic */
	unsigned long iocs;	/**< Ioctl count */
};

enum drm_stat_type {
	_DRM_STAT_LOCK,
	_DRM_STAT_OPENS,
	_DRM_STAT_CLOSES,
	_DRM_STAT_IOCTLS,
	_DRM_STAT_LOCKS,
	_DRM_STAT_UNLOCKS,
	_DRM_STAT_VALUE,	/**< Generic value */
	_DRM_STAT_BYTE,		/**< Generic byte counter (1024bytes/K) */
	_DRM_STAT_COUNT,	/**< Generic non-byte counter (1000/k) */

	_DRM_STAT_IRQ,		/**< IRQ */
	_DRM_STAT_PRIMARY,	/**< Primary DMA bytes */
	_DRM_STAT_SECONDARY,	/**< Secondary DMA bytes */
	_DRM_STAT_DMA,		/**< DMA */
	_DRM_STAT_SPECIAL,	/**< Special DMA (e.g., priority or polled) */
	_DRM_STAT_MISSED	/**< Missed DMA opportunity */
	    /* Add to the *END* of the list */
};

/**
 * DRM_IOCTL_GET_STATS ioctl argument type.
 */
struct drm_stats {
	unsigned long count;
	struct {
		unsigned long value;
		enum drm_stat_type type;
	} data[15];
};

/**
 * Hardware locking flags.
 */
enum drm_lock_flags {
	_DRM_LOCK_READY = 0x01,	     /**< Wait until hardware is ready for DMA */
	_DRM_LOCK_QUIESCENT = 0x02,  /**< Wait until hardware quiescent */
	_DRM_LOCK_FLUSH = 0x04,	     /**< Flush this context's DMA queue first */
	_DRM_LOCK_FLUSH_ALL = 0x08,  /**< Flush all DMA queues first */
	/* These *HALT* flags aren't supported yet
	   -- they will be used to support the
	   full-screen DGA-like mode. */
	_DRM_HALT_ALL_QUEUES = 0x10, /**< Halt all current and future queues */
	_DRM_HALT_CUR_QUEUES = 0x20  /**< Halt all current queues */
};

/**
 * DRM_IOCTL_LOCK, DRM_IOCTL_UNLOCK and DRM_IOCTL_FINISH ioctl argument type.
 *
 * \sa drmGetLock() and drmUnlock().
 */
struct drm_lock {
	int context;
	enum drm_lock_flags flags;
};

/**
 * DMA flags
 *
 * \warning
 * These values \e must match xf86drm.h.
 *
 * \sa drm_dma.
 */
enum drm_dma_flags {
	/* Flags for DMA buffer dispatch */
	_DRM_DMA_BLOCK = 0x01,	      /**<
				       * Block until buffer dispatched.
				       *
				       * \note The buffer may not yet have
				       * been processed by the hardware --
				       * getting a hardware lock with the
				       * hardware quiescent will ensure
				       * that the buffer has been
				       * processed.
				       */
	_DRM_DMA_WHILE_LOCKED = 0x02, /**< Dispatch while lock held */
	_DRM_DMA_PRIORITY = 0x04,     /**< High priority dispatch */

	/* Flags for DMA buffer request */
	_DRM_DMA_WAIT = 0x10,	      /**< Wait for free buffers */
	_DRM_DMA_SMALLER_OK = 0x20,   /**< Smaller-than-requested buffers OK */
	_DRM_DMA_LARGER_OK = 0x40     /**< Larger-than-requested buffers OK */
};

/**
 * DRM_IOCTL_ADD_BUFS and DRM_IOCTL_MARK_BUFS ioctl argument type.
 *
 * \sa drmAddBufs().
 */
struct drm_buf_desc {
	int count;		 /**< Number of buffers of this size */
	int size;		 /**< Size in bytes */
	int low_mark;		 /**< Low water mark */
	int high_mark;		 /**< High water mark */
	enum {
		_DRM_PAGE_ALIGN = 0x01,	/**< Align on page boundaries for DMA */
		_DRM_AGP_BUFFER = 0x02,	/**< Buffer is in AGP space */
		_DRM_SG_BUFFER  = 0x04,	/**< Scatter/gather memory buffer */
		_DRM_FB_BUFFER  = 0x08, /**< Buffer is in frame buffer */
		_DRM_PCI_BUFFER_RO = 0x10 /**< Map PCI DMA buffer read-only */
	} flags;
	unsigned long agp_start; /**<
				  * Start address of where the AGP buffers are
				  * in the AGP aperture
				  */
};

/**
 * DRM_IOCTL_INFO_BUFS ioctl argument type.
 */
struct drm_buf_info {
	int count;		  /**< Number of buffers described in list */
	struct drm_buf_desc __user *list; /**< List of buffer descriptions */
};

/**
 * DRM_IOCTL_FREE_BUFS ioctl argument type.
 */
struct drm_buf_free {
	int count;
	int __user *list;
};

/**
 * Buffer information
 *
 * \sa drm_buf_map.
 */
struct drm_buf_pub {
	int idx;		       /**< Index into the master buffer list */
	int total;		       /**< Buffer size */
	int used;		       /**< Amount of buffer in use (for DMA) */
	void __user *address;	       /**< Address of buffer */
};

/**
 * DRM_IOCTL_MAP_BUFS ioctl argument type.
 */
struct drm_buf_map {
	int count;		/**< Length of the buffer list */
#if defined(__cplusplus)
	void __user *c_virtual;
#else
	void __user *virtual;		/**< Mmap'd area in user-virtual */
#endif
	struct drm_buf_pub __user *list;	/**< Buffer information */
};

/**
 * DRM_IOCTL_DMA ioctl argument type.
 *
 * Indices here refer to the offset into the buffer list in drm_buf_get.
 *
 * \sa drmDMA().
 */
struct drm_dma {
	int context;			  /**< Context handle */
	int send_count;			  /**< Number of buffers to send */
	int __user *send_indices;	  /**< List of handles to buffers */
	int __user *send_sizes;		  /**< Lengths of data to send */
	enum drm_dma_flags flags;	  /**< Flags */
	int request_count;		  /**< Number of buffers requested */
	int request_size;		  /**< Desired size for buffers */
	int __user *request_indices;	 /**< Buffer information */
	int __user *request_sizes;
	int granted_count;		  /**< Number of buffers granted */
};

enum drm_ctx_flags {
	_DRM_CONTEXT_PRESERVED = 0x01,
	_DRM_CONTEXT_2DONLY = 0x02
};

/**
 * DRM_IOCTL_ADD_CTX ioctl argument type.
 *
 * \sa drmCreateContext() and drmDestroyContext().
 */
struct drm_ctx {
	drm_context_t handle;
	enum drm_ctx_flags flags;
};

/**
 * DRM_IOCTL_RES_CTX ioctl argument type.
 */
struct drm_ctx_res {
	int count;
	struct drm_ctx __user *contexts;
};

/**
 * DRM_IOCTL_ADD_DRAW and DRM_IOCTL_RM_DRAW ioctl argument type.
 */
struct drm_draw {
	drm_drawable_t handle;
};

/**
 * DRM_IOCTL_UPDATE_DRAW ioctl argument type.
 */
typedef enum {
	DRM_DRAWABLE_CLIPRECTS,
} drm_drawable_info_type_t;

struct drm_update_draw {
	drm_drawable_t handle;
	unsigned int type;
	unsigned int num;
	unsigned long long data;
};

/**
 * DRM_IOCTL_GET_MAGIC and DRM_IOCTL_AUTH_MAGIC ioctl argument type.
 */
struct drm_auth {
	drm_magic_t magic;
};

/**
 * DRM_IOCTL_IRQ_BUSID ioctl argument type.
 *
 * \sa drmGetInterruptFromBusID().
 */
struct drm_irq_busid {
	int irq;	/**< IRQ number */
	int busnum;	/**< bus number */
	int devnum;	/**< device number */
	int funcnum;	/**< function number */
};

enum drm_vblank_seq_type {
	_DRM_VBLANK_ABSOLUTE = 0x0,	/**< Wait for specific vblank sequence number */
	_DRM_VBLANK_RELATIVE = 0x1,	/**< Wait for given number of vblanks */
	_DRM_VBLANK_FLIP = 0x8000000,	/**< Scheduled buffer swap should flip */
	_DRM_VBLANK_NEXTONMISS = 0x10000000,	/**< If missed, wait for next vblank */
	_DRM_VBLANK_SECONDARY = 0x20000000,	/**< Secondary display controller */
	_DRM_VBLANK_SIGNAL = 0x40000000	/**< Send signal instead of blocking */
};

#define _DRM_VBLANK_TYPES_MASK (_DRM_VBLANK_ABSOLUTE | _DRM_VBLANK_RELATIVE)
#define _DRM_VBLANK_FLAGS_MASK (_DRM_VBLANK_SIGNAL | _DRM_VBLANK_SECONDARY | \
				_DRM_VBLANK_NEXTONMISS)

struct drm_wait_vblank_request {
	enum drm_vblank_seq_type type;
	unsigned int sequence;
	unsigned long signal;
};

struct drm_wait_vblank_reply {
	enum drm_vblank_seq_type type;
	unsigned int sequence;
	long tval_sec;
	long tval_usec;
};

/**
 * DRM_IOCTL_WAIT_VBLANK ioctl argument type.
 *
 * \sa drmWaitVBlank().
 */
union drm_wait_vblank {
	struct drm_wait_vblank_request request;
	struct drm_wait_vblank_reply reply;
};


#define _DRM_PRE_MODESET 1
#define _DRM_POST_MODESET 2

/**
 * DRM_IOCTL_MODESET_CTL ioctl argument type
 *
 * \sa drmModesetCtl().
 */
struct drm_modeset_ctl {
	uint32_t crtc;
	uint32_t cmd;
};

/**
 * DRM_IOCTL_AGP_ENABLE ioctl argument type.
 *
 * \sa drmAgpEnable().
 */
struct drm_agp_mode {
	unsigned long mode;	/**< AGP mode */
};

/**
 * DRM_IOCTL_AGP_ALLOC and DRM_IOCTL_AGP_FREE ioctls argument type.
 *
 * \sa drmAgpAlloc() and drmAgpFree().
 */
struct drm_agp_buffer {
	unsigned long size;	/**< In bytes -- will round to page boundary */
	unsigned long handle;	/**< Used for binding / unbinding */
	unsigned long type;	/**< Type of memory to allocate */
	unsigned long physical;	/**< Physical used by i810 */
};

/**
 * DRM_IOCTL_AGP_BIND and DRM_IOCTL_AGP_UNBIND ioctls argument type.
 *
 * \sa drmAgpBind() and drmAgpUnbind().
 */
struct drm_agp_binding {
	unsigned long handle;	/**< From drm_agp_buffer */
	unsigned long offset;	/**< In bytes -- will round to page boundary */
};

/**
 * DRM_IOCTL_AGP_INFO ioctl argument type.
 *
 * \sa drmAgpVersionMajor(), drmAgpVersionMinor(), drmAgpGetMode(),
 * drmAgpBase(), drmAgpSize(), drmAgpMemoryUsed(), drmAgpMemoryAvail(),
 * drmAgpVendorId() and drmAgpDeviceId().
 */
struct drm_agp_info {
	int agp_version_major;
	int agp_version_minor;
	unsigned long mode;
	unsigned long aperture_base;   /**< physical address */
	unsigned long aperture_size;   /**< bytes */
	unsigned long memory_allowed;  /**< bytes */
	unsigned long memory_used;

	/** \name PCI information */
	/*@{ */
	unsigned short id_vendor;
	unsigned short id_device;
	/*@} */
};

/**
 * DRM_IOCTL_SG_ALLOC ioctl argument type.
 */
struct drm_scatter_gather {
	unsigned long size;	/**< In bytes -- will round to page boundary */
	unsigned long handle;	/**< Used for mapping / unmapping */
};

/**
 * DRM_IOCTL_SET_VERSION ioctl argument type.
 */
struct drm_set_version {
	int drm_di_major;
	int drm_di_minor;
	int drm_dd_major;
	int drm_dd_minor;
};


#define DRM_FENCE_FLAG_EMIT                0x00000001
#define DRM_FENCE_FLAG_SHAREABLE           0x00000002
/**
 * On hardware with no interrupt events for operation completion,
 * indicates that the kernel should sleep while waiting for any blocking
 * operation to complete rather than spinning.
 *
 * Has no effect otherwise.
 */
#define DRM_FENCE_FLAG_WAIT_LAZY           0x00000004
#define DRM_FENCE_FLAG_NO_USER             0x00000010

/* Reserved for driver use */
#define DRM_FENCE_MASK_DRIVER              0xFF000000

#define DRM_FENCE_TYPE_EXE                 0x00000001

struct drm_fence_arg {
	unsigned int handle;
	unsigned int fence_class;
	unsigned int type;
	unsigned int flags;
	unsigned int signaled;
	unsigned int error;
	unsigned int sequence;
	unsigned int pad64;
	uint64_t expand_pad[2]; /*Future expansion */
};

/* Buffer permissions, referring to how the GPU uses the buffers.
 * these translate to fence types used for the buffers.
 * Typically a texture buffer is read, A destination buffer is write and
 *  a command (batch-) buffer is exe. Can be or-ed together.
 */

#define DRM_BO_FLAG_READ        (1ULL << 0)
#define DRM_BO_FLAG_WRITE       (1ULL << 1)
#define DRM_BO_FLAG_EXE         (1ULL << 2)

/*
 * All of the bits related to access mode
 */
#define DRM_BO_MASK_ACCESS	(DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE | DRM_BO_FLAG_EXE)
/*
 * Status flags. Can be read to determine the actual state of a buffer.
 * Can also be set in the buffer mask before validation.
 */

/*
 * Mask: Never evict this buffer. Not even with force. This type of buffer is only
 * available to root and must be manually removed before buffer manager shutdown
 * or lock.
 * Flags: Acknowledge
 */
#define DRM_BO_FLAG_NO_EVICT    (1ULL << 4)

/*
 * Mask: Require that the buffer is placed in mappable memory when validated.
 *       If not set the buffer may or may not be in mappable memory when validated.
 * Flags: If set, the buffer is in mappable memory.
 */
#define DRM_BO_FLAG_MAPPABLE    (1ULL << 5)

/* Mask: The buffer should be shareable with other processes.
 * Flags: The buffer is shareable with other processes.
 */
#define DRM_BO_FLAG_SHAREABLE   (1ULL << 6)

/* Mask: If set, place the buffer in cache-coherent memory if available.
 *       If clear, never place the buffer in cache coherent memory if validated.
 * Flags: The buffer is currently in cache-coherent memory.
 */
#define DRM_BO_FLAG_CACHED      (1ULL << 7)

/* Mask: Make sure that every time this buffer is validated,
 *       it ends up on the same location provided that the memory mask is the same.
 *       The buffer will also not be evicted when claiming space for
 *       other buffers. Basically a pinned buffer but it may be thrown out as
 *       part of buffer manager shutdown or locking.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_NO_MOVE     (1ULL << 8)

/* Mask: Make sure the buffer is in cached memory when mapped.  In conjunction
 * with DRM_BO_FLAG_CACHED it also allows the buffer to be bound into the GART
 * with unsnooped PTEs instead of snooped, by using chipset-specific cache
 * flushing at bind time.  A better name might be DRM_BO_FLAG_TT_UNSNOOPED,
 * as the eviction to local memory (TTM unbind) on map is just a side effect
 * to prevent aggressive cache prefetch from the GPU disturbing the cache
 * management that the DRM is doing.
 *
 * Flags: Acknowledge.
 * Buffers allocated with this flag should not be used for suballocators
 * This type may have issues on CPUs with over-aggressive caching
 * http://marc.info/?l=linux-kernel&m=102376926732464&w=2
 */
#define DRM_BO_FLAG_CACHED_MAPPED    (1ULL << 19)


/* Mask: Force DRM_BO_FLAG_CACHED flag strictly also if it is set.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_FORCE_CACHING  (1ULL << 13)

/*
 * Mask: Force DRM_BO_FLAG_MAPPABLE flag strictly also if it is clear.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_FORCE_MAPPABLE (1ULL << 14)
#define DRM_BO_FLAG_TILE           (1ULL << 15)

/*
 * Memory type flags that can be or'ed together in the mask, but only
 * one appears in flags.
 */

/* System memory */
#define DRM_BO_FLAG_MEM_LOCAL  (1ULL << 24)
/* Translation table memory */
#define DRM_BO_FLAG_MEM_TT     (1ULL << 25)
/* Vram memory */
#define DRM_BO_FLAG_MEM_VRAM   (1ULL << 26)
/* Up to the driver to define. */
#define DRM_BO_FLAG_MEM_PRIV0  (1ULL << 27)
#define DRM_BO_FLAG_MEM_PRIV1  (1ULL << 28)
#define DRM_BO_FLAG_MEM_PRIV2  (1ULL << 29)
#define DRM_BO_FLAG_MEM_PRIV3  (1ULL << 30)
#define DRM_BO_FLAG_MEM_PRIV4  (1ULL << 31)
/* We can add more of these now with a 64-bit flag type */

/*
 * This is a mask covering all of the memory type flags; easier to just
 * use a single constant than a bunch of | values. It covers
 * DRM_BO_FLAG_MEM_LOCAL through DRM_BO_FLAG_MEM_PRIV4
 */
#define DRM_BO_MASK_MEM         0x00000000FF000000ULL
/*
 * This adds all of the CPU-mapping options in with the memory
 * type to label all bits which change how the page gets mapped
 */
#define DRM_BO_MASK_MEMTYPE     (DRM_BO_MASK_MEM | \
				 DRM_BO_FLAG_CACHED_MAPPED | \
				 DRM_BO_FLAG_CACHED | \
				 DRM_BO_FLAG_MAPPABLE)
				 
/* Driver-private flags */
#define DRM_BO_MASK_DRIVER      0xFFFF000000000000ULL

/*
 * Don't block on validate and map. Instead, return EBUSY.
 */
#define DRM_BO_HINT_DONT_BLOCK  0x00000002
/*
 * Don't place this buffer on the unfenced list. This means
 * that the buffer will not end up having a fence associated
 * with it as a result of this operation
 */
#define DRM_BO_HINT_DONT_FENCE  0x00000004
/**
 * On hardware with no interrupt events for operation completion,
 * indicates that the kernel should sleep while waiting for any blocking
 * operation to complete rather than spinning.
 *
 * Has no effect otherwise.
 */
#define DRM_BO_HINT_WAIT_LAZY   0x00000008
/*
 * The client has compute relocations refering to this buffer using the
 * offset in the presumed_offset field. If that offset ends up matching
 * where this buffer lands, the kernel is free to skip executing those
 * relocations
 */
#define DRM_BO_HINT_PRESUMED_OFFSET 0x00000010

#define DRM_BO_INIT_MAGIC 0xfe769812
#define DRM_BO_INIT_MAJOR 1
#define DRM_BO_INIT_MINOR 0
#define DRM_BO_INIT_PATCH 0


struct drm_bo_info_req {
	uint64_t mask;
	uint64_t flags;
	unsigned int handle;
	unsigned int hint;
	unsigned int fence_class;
	unsigned int desired_tile_stride;
	unsigned int tile_info;
	unsigned int pad64;
	uint64_t presumed_offset;
};

struct drm_bo_create_req {
	uint64_t flags;
	uint64_t size;
	uint64_t buffer_start;
	unsigned int hint;
	unsigned int page_alignment;
};


/*
 * Reply flags
 */

#define DRM_BO_REP_BUSY 0x00000001

struct drm_bo_info_rep {
	uint64_t flags;
	uint64_t proposed_flags;
	uint64_t size;
	uint64_t offset;
	uint64_t arg_handle;
	uint64_t buffer_start;
	unsigned int handle;
	unsigned int fence_flags;
	unsigned int rep_flags;
	unsigned int page_alignment;
	unsigned int desired_tile_stride;
	unsigned int hw_tile_stride;
	unsigned int tile_info;
	unsigned int pad64;
	uint64_t expand_pad[4]; /*Future expansion */
};

struct drm_bo_arg_rep {
	struct drm_bo_info_rep bo_info;
	int ret;
	unsigned int pad64;
};

struct drm_bo_create_arg {
	union {
		struct drm_bo_create_req req;
		struct drm_bo_info_rep rep;
	} d;
};

struct drm_bo_handle_arg {
	unsigned int handle;
};

struct drm_bo_reference_info_arg {
	union {
		struct drm_bo_handle_arg req;
		struct drm_bo_info_rep rep;
	} d;
};

struct drm_bo_map_wait_idle_arg {
	union {
		struct drm_bo_info_req req;
		struct drm_bo_info_rep rep;
	} d;
};

struct drm_bo_op_req {
	enum {
		drm_bo_validate,
		drm_bo_fence,
		drm_bo_ref_fence,
	} op;
	unsigned int arg_handle;
	struct drm_bo_info_req bo_req;
};


struct drm_bo_op_arg {
	uint64_t next;
	union {
		struct drm_bo_op_req req;
		struct drm_bo_arg_rep rep;
	} d;
	int handled;
	unsigned int pad64;
};


#define DRM_BO_MEM_LOCAL 0
#define DRM_BO_MEM_TT 1
#define DRM_BO_MEM_VRAM 2
#define DRM_BO_MEM_PRIV0 3
#define DRM_BO_MEM_PRIV1 4
#define DRM_BO_MEM_PRIV2 5
#define DRM_BO_MEM_PRIV3 6
#define DRM_BO_MEM_PRIV4 7

#define DRM_BO_MEM_TYPES 8 /* For now. */

#define DRM_BO_LOCK_UNLOCK_BM       (1 << 0)
#define DRM_BO_LOCK_IGNORE_NO_EVICT (1 << 1)

struct drm_bo_version_arg {
	uint32_t major;
	uint32_t minor;
	uint32_t patchlevel;
};

struct drm_mm_type_arg {
	unsigned int mem_type;
	unsigned int lock_flags;
};

struct drm_mm_init_arg {
	unsigned int magic;
	unsigned int major;
	unsigned int minor;
	unsigned int mem_type;
	uint64_t p_offset;
	uint64_t p_size;
};

struct drm_mm_info_arg {
	unsigned int mem_type;
	uint64_t p_size;
};

struct drm_gem_close {
	/** Handle of the object to be closed. */
	uint32_t handle;
	uint32_t pad;
};

struct drm_gem_flink {
	/** Handle for the object being named */
	uint32_t handle;

	/** Returned global name */
	uint32_t name;
};

struct drm_gem_open {
	/** Name of object being opened */
	uint32_t name;

	/** Returned handle for the object */
	uint32_t handle;
	
	/** Returned size of the object */
	uint64_t size;
};

/**
 * \name Ioctls Definitions
 */
/*@{*/

#define DRM_IOCTL_BASE			'd'
#define DRM_IO(nr)			_IO(DRM_IOCTL_BASE,nr)
#define DRM_IOR(nr,type)		_IOR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOW(nr,type)		_IOW(DRM_IOCTL_BASE,nr,type)
#define DRM_IOWR(nr,type)		_IOWR(DRM_IOCTL_BASE,nr,type)

#define DRM_IOCTL_VERSION		DRM_IOWR(0x00, struct drm_version)
#define DRM_IOCTL_GET_UNIQUE		DRM_IOWR(0x01, struct drm_unique)
#define DRM_IOCTL_GET_MAGIC		DRM_IOR( 0x02, struct drm_auth)
#define DRM_IOCTL_IRQ_BUSID		DRM_IOWR(0x03, struct drm_irq_busid)
#define DRM_IOCTL_GET_MAP               DRM_IOWR(0x04, struct drm_map)
#define DRM_IOCTL_GET_CLIENT            DRM_IOWR(0x05, struct drm_client)
#define DRM_IOCTL_GET_STATS             DRM_IOR( 0x06, struct drm_stats)
#define DRM_IOCTL_SET_VERSION		DRM_IOWR(0x07, struct drm_set_version)
#define DRM_IOCTL_MODESET_CTL           DRM_IOW(0x08,  struct drm_modeset_ctl)

#define DRM_IOCTL_GEM_CLOSE		DRM_IOW (0x09, struct drm_gem_close)
#define DRM_IOCTL_GEM_FLINK		DRM_IOWR(0x0a, struct drm_gem_flink)
#define DRM_IOCTL_GEM_OPEN		DRM_IOWR(0x0b, struct drm_gem_open)

#define DRM_IOCTL_SET_UNIQUE		DRM_IOW( 0x10, struct drm_unique)
#define DRM_IOCTL_AUTH_MAGIC		DRM_IOW( 0x11, struct drm_auth)
#define DRM_IOCTL_BLOCK			DRM_IOWR(0x12, struct drm_block)
#define DRM_IOCTL_UNBLOCK		DRM_IOWR(0x13, struct drm_block)
#define DRM_IOCTL_CONTROL		DRM_IOW( 0x14, struct drm_control)
#define DRM_IOCTL_ADD_MAP		DRM_IOWR(0x15, struct drm_map)
#define DRM_IOCTL_ADD_BUFS		DRM_IOWR(0x16, struct drm_buf_desc)
#define DRM_IOCTL_MARK_BUFS		DRM_IOW( 0x17, struct drm_buf_desc)
#define DRM_IOCTL_INFO_BUFS		DRM_IOWR(0x18, struct drm_buf_info)
#define DRM_IOCTL_MAP_BUFS		DRM_IOWR(0x19, struct drm_buf_map)
#define DRM_IOCTL_FREE_BUFS		DRM_IOW( 0x1a, struct drm_buf_free)

#define DRM_IOCTL_RM_MAP		DRM_IOW( 0x1b, struct drm_map)

#define DRM_IOCTL_SET_SAREA_CTX		DRM_IOW( 0x1c, struct drm_ctx_priv_map)
#define DRM_IOCTL_GET_SAREA_CTX		DRM_IOWR(0x1d, struct drm_ctx_priv_map)

#define DRM_IOCTL_ADD_CTX		DRM_IOWR(0x20, struct drm_ctx)
#define DRM_IOCTL_RM_CTX		DRM_IOWR(0x21, struct drm_ctx)
#define DRM_IOCTL_MOD_CTX		DRM_IOW( 0x22, struct drm_ctx)
#define DRM_IOCTL_GET_CTX		DRM_IOWR(0x23, struct drm_ctx)
#define DRM_IOCTL_SWITCH_CTX		DRM_IOW( 0x24, struct drm_ctx)
#define DRM_IOCTL_NEW_CTX		DRM_IOW( 0x25, struct drm_ctx)
#define DRM_IOCTL_RES_CTX		DRM_IOWR(0x26, struct drm_ctx_res)
#define DRM_IOCTL_ADD_DRAW		DRM_IOWR(0x27, struct drm_draw)
#define DRM_IOCTL_RM_DRAW		DRM_IOWR(0x28, struct drm_draw)
#define DRM_IOCTL_DMA			DRM_IOWR(0x29, struct drm_dma)
#define DRM_IOCTL_LOCK			DRM_IOW( 0x2a, struct drm_lock)
#define DRM_IOCTL_UNLOCK		DRM_IOW( 0x2b, struct drm_lock)
#define DRM_IOCTL_FINISH		DRM_IOW( 0x2c, struct drm_lock)

#define DRM_IOCTL_AGP_ACQUIRE		DRM_IO(  0x30)
#define DRM_IOCTL_AGP_RELEASE		DRM_IO(  0x31)
#define DRM_IOCTL_AGP_ENABLE		DRM_IOW( 0x32, struct drm_agp_mode)
#define DRM_IOCTL_AGP_INFO		DRM_IOR( 0x33, struct drm_agp_info)
#define DRM_IOCTL_AGP_ALLOC		DRM_IOWR(0x34, struct drm_agp_buffer)
#define DRM_IOCTL_AGP_FREE		DRM_IOW( 0x35, struct drm_agp_buffer)
#define DRM_IOCTL_AGP_BIND		DRM_IOW( 0x36, struct drm_agp_binding)
#define DRM_IOCTL_AGP_UNBIND		DRM_IOW( 0x37, struct drm_agp_binding)

#define DRM_IOCTL_SG_ALLOC		DRM_IOWR(0x38, struct drm_scatter_gather)
#define DRM_IOCTL_SG_FREE		DRM_IOW( 0x39, struct drm_scatter_gather)

#define DRM_IOCTL_WAIT_VBLANK		DRM_IOWR(0x3a, union drm_wait_vblank)

#define DRM_IOCTL_UPDATE_DRAW           DRM_IOW(0x3f, struct drm_update_draw)

#define DRM_IOCTL_MM_INIT               DRM_IOWR(0xc0, struct drm_mm_init_arg)
#define DRM_IOCTL_MM_TAKEDOWN           DRM_IOWR(0xc1, struct drm_mm_type_arg)
#define DRM_IOCTL_MM_LOCK               DRM_IOWR(0xc2, struct drm_mm_type_arg)
#define DRM_IOCTL_MM_UNLOCK             DRM_IOWR(0xc3, struct drm_mm_type_arg)

#define DRM_IOCTL_FENCE_CREATE          DRM_IOWR(0xc4, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_REFERENCE       DRM_IOWR(0xc6, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_UNREFERENCE     DRM_IOWR(0xc7, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_SIGNALED        DRM_IOWR(0xc8, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_FLUSH           DRM_IOWR(0xc9, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_WAIT            DRM_IOWR(0xca, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_EMIT            DRM_IOWR(0xcb, struct drm_fence_arg)
#define DRM_IOCTL_FENCE_BUFFERS         DRM_IOWR(0xcc, struct drm_fence_arg)

#define DRM_IOCTL_BO_CREATE             DRM_IOWR(0xcd, struct drm_bo_create_arg)
#define DRM_IOCTL_BO_MAP                DRM_IOWR(0xcf, struct drm_bo_map_wait_idle_arg)
#define DRM_IOCTL_BO_UNMAP              DRM_IOWR(0xd0, struct drm_bo_handle_arg)
#define DRM_IOCTL_BO_REFERENCE          DRM_IOWR(0xd1, struct drm_bo_reference_info_arg)
#define DRM_IOCTL_BO_UNREFERENCE        DRM_IOWR(0xd2, struct drm_bo_handle_arg)
#define DRM_IOCTL_BO_SETSTATUS          DRM_IOWR(0xd3, struct drm_bo_map_wait_idle_arg)
#define DRM_IOCTL_BO_INFO               DRM_IOWR(0xd4, struct drm_bo_reference_info_arg)
#define DRM_IOCTL_BO_WAIT_IDLE          DRM_IOWR(0xd5, struct drm_bo_map_wait_idle_arg)
#define DRM_IOCTL_BO_VERSION          DRM_IOR(0xd6, struct drm_bo_version_arg)
#define DRM_IOCTL_MM_INFO               DRM_IOWR(0xd7, struct drm_mm_info_arg)

/*@}*/

/**
 * Device specific ioctls should only be in their respective headers
 * The device specific ioctl range is from 0x40 to 0x99.
 * Generic IOCTLS restart at 0xA0.
 *
 * \sa drmCommandNone(), drmCommandRead(), drmCommandWrite(), and
 * drmCommandReadWrite().
 */
#define DRM_COMMAND_BASE                0x40
#define DRM_COMMAND_END                 0xA0

/* typedef area */
#ifndef __KERNEL__
typedef struct drm_clip_rect drm_clip_rect_t;
typedef struct drm_tex_region drm_tex_region_t;
typedef struct drm_hw_lock drm_hw_lock_t;
typedef struct drm_version drm_version_t;
typedef struct drm_unique drm_unique_t;
typedef struct drm_list drm_list_t;
typedef struct drm_block drm_block_t;
typedef struct drm_control drm_control_t;
typedef enum drm_map_type drm_map_type_t;
typedef enum drm_map_flags drm_map_flags_t;
typedef struct drm_ctx_priv_map drm_ctx_priv_map_t;
typedef struct drm_map drm_map_t;
typedef struct drm_client drm_client_t;
typedef enum drm_stat_type drm_stat_type_t;
typedef struct drm_stats drm_stats_t;
typedef enum drm_lock_flags drm_lock_flags_t;
typedef struct drm_lock drm_lock_t;
typedef enum drm_dma_flags drm_dma_flags_t;
typedef struct drm_buf_desc drm_buf_desc_t;
typedef struct drm_buf_info drm_buf_info_t;
typedef struct drm_buf_free drm_buf_free_t;
typedef struct drm_buf_pub drm_buf_pub_t;
typedef struct drm_buf_map drm_buf_map_t;
typedef struct drm_dma drm_dma_t;
typedef union drm_wait_vblank drm_wait_vblank_t;
typedef struct drm_agp_mode drm_agp_mode_t;
typedef enum drm_ctx_flags drm_ctx_flags_t;
typedef struct drm_ctx drm_ctx_t;
typedef struct drm_ctx_res drm_ctx_res_t;
typedef struct drm_draw drm_draw_t;
typedef struct drm_update_draw drm_update_draw_t;
typedef struct drm_auth drm_auth_t;
typedef struct drm_irq_busid drm_irq_busid_t;
typedef enum drm_vblank_seq_type drm_vblank_seq_type_t;
typedef struct drm_agp_buffer drm_agp_buffer_t;
typedef struct drm_agp_binding drm_agp_binding_t;
typedef struct drm_agp_info drm_agp_info_t;
typedef struct drm_scatter_gather drm_scatter_gather_t;
typedef struct drm_set_version drm_set_version_t;

typedef struct drm_fence_arg drm_fence_arg_t;
typedef struct drm_mm_type_arg drm_mm_type_arg_t;
typedef struct drm_mm_init_arg drm_mm_init_arg_t;
typedef enum drm_bo_type drm_bo_type_t;
#endif

#endif

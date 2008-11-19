/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Including this file is mandatory for all USB related c-files in the
 * kernel.
 */

#ifndef _USB2_CORE_H_
#define	_USB2_CORE_H_

/* Default USB configuration */

#ifndef USB_NO_POLL
#define	USB_NO_POLL 0
#endif

#ifndef USB_USE_CONDVAR
#define	USB_USE_CONDVAR 0
#endif

#ifndef USB_TD_GET_PROC
#define	USB_TD_GET_PROC(td) (td)->td_proc
#endif

#ifndef USB_PROC_GET_GID
#define	USB_PROC_GET_GID(td) (td)->p_pgid
#endif

#ifndef USB_VNOPS_FO_CLOSE
#define	USB_VNOPS_FO_CLOSE(fp, td, perr) do {	\
    (td)->td_fpop = (fp);			\
    *(perr) = vnops.fo_close(fp, td);		\
    (td)->td_fpop = NULL;			\
} while (0)
#endif

#ifndef USB_VNOPS_FO_STAT
#define	USB_VNOPS_FO_STAT(fp, sb, cred, td) \
    vnops.fo_stat(fp, sb, cred, td)
#endif

#ifndef USB_VNOPS_FO_TRUNCATE
#define	USB_VNOPS_FO_TRUNCATE(fp, length, cred, td) \
    vnops.fo_truncate(fp, length, cred, td)
#endif

/* Include files */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_revision.h>

#include "usb2_if.h"
#include "opt_usb.h"
#include "opt_bus.h"

#define	USB_STACK_VERSION 2000		/* 2.0 */

#define	USB_HOST_ALIGN    8		/* bytes, must be power of two */

#define	USB_ROOT_HUB_ADDR 1		/* value */

#define	USB_ISOC_TIME_MAX 128		/* ms */
#define	USB_FS_ISOC_UFRAME_MAX 4	/* exclusive unit */

#if (USB_FS_ISOC_UFRAME_MAX > 6)
#error "USB_FS_ISOC_UFRAME_MAX cannot be set higher than 6"
#endif

#define	USB_MAX_FS_ISOC_FRAMES_PER_XFER (120)	/* units */
#define	USB_MAX_HS_ISOC_FRAMES_PER_XFER (8*120)	/* units */

#define	USB_MAX_IPACKET		8	/* maximum size of the initial USB
					 * data packet */
#ifndef USB_VERBOSE
#define	USB_VERBOSE 1
#endif

#define	USB_HUB_MAX_DEPTH 5

/* USB transfer states */

#define	USB_ST_SETUP       0
#define	USB_ST_TRANSFERRED 1
#define	USB_ST_ERROR       2

/*
 * The following macro will return the current state of an USB
 * transfer like defined by the "USB_ST_XXX" enums.
 */
#define	USB_GET_STATE(xfer) ((xfer)->usb2_state)

/*
 * The following macro will tell if an USB transfer is currently
 * receiving or transferring data.
 */
#define	USB_GET_DATA_ISREAD(xfer) (((xfer)->flags_int.usb2_mode == \
	USB_MODE_DEVICE) ? ((xfer->endpoint & UE_DIR_IN) ? 0 : 1) : \
	((xfer->endpoint & UE_DIR_IN) ? 1 : 0))

/*
 * The following macros are used used to convert milliseconds into
 * HZ. We use 1024 instead of 1000 milliseconds per second to save a
 * full division.
 */
#define	USB_MS_HZ 1024

#define	USB_MS_TO_TICKS(ms) \
  (((uint32_t)((((uint32_t)(ms)) * ((uint32_t)(hz))) + USB_MS_HZ - 1)) / USB_MS_HZ)

/* macros */

#define	usb2_callout_init_mtx(c,m,f) callout_init_mtx(&(c)->co,m,f)
#define	usb2_callout_reset(c,t,f,d) callout_reset(&(c)->co,t,f,d)
#define	usb2_callout_stop(c) callout_stop(&(c)->co)
#define	usb2_callout_drain(c) callout_drain(&(c)->co)
#define	usb2_callout_pending(c) callout_pending(&(c)->co)

#define	USB_BUS_LOCK(_b)		mtx_lock(&(_b)->bus_mtx)
#define	USB_BUS_UNLOCK(_b)		mtx_unlock(&(_b)->bus_mtx)
#define	USB_BUS_LOCK_ASSERT(_b, _t)	mtx_assert(&(_b)->bus_mtx, _t)
#define	USB_XFER_LOCK(_x)		mtx_lock((_x)->xfer_mtx)
#define	USB_XFER_UNLOCK(_x)		mtx_unlock((_x)->xfer_mtx)
#define	USB_XFER_LOCK_ASSERT(_x, _t)	mtx_assert((_x)->xfer_mtx, _t)
/* structure prototypes */

struct file;
struct usb2_bus;
struct usb2_device;
struct usb2_page;
struct usb2_page_cache;
struct usb2_xfer;
struct usb2_xfer_root;

/* typedefs */

typedef uint8_t usb2_error_t;

typedef void (usb2_callback_t)(struct usb2_xfer *);

/* structures */

/*
 * This structure contains permissions.
 */

struct usb2_perm {
	uint32_t uid;
	uint32_t gid;
	uint16_t mode;
};

/*
 * Common queue structure for USB transfers.
 */
struct usb2_xfer_queue {
	TAILQ_HEAD(, usb2_xfer) head;
	struct usb2_xfer *curr;		/* current USB transfer processed */
	void    (*command) (struct usb2_xfer_queue *pq);
	uint8_t	recurse_1:1;
	uint8_t	recurse_2:1;
};

/*
 * The following is a wrapper for the callout structure to ease
 * porting the code to other platforms.
 */
struct usb2_callout {
	struct callout co;
};

/*
 * The following structure defines a set of USB transfer flags.
 */
struct usb2_xfer_flags {
	uint8_t	force_short_xfer:1;	/* force a short transmit transfer
					 * last */
	uint8_t	short_xfer_ok:1;	/* allow short receive transfers */
	uint8_t	short_frames_ok:1;	/* allow short frames */
	uint8_t	pipe_bof:1;		/* block pipe on failure */
	uint8_t	proxy_buffer:1;		/* makes buffer size a factor of
					 * "max_frame_size" */
	uint8_t	ext_buffer:1;		/* uses external DMA buffer */
	uint8_t	manual_status:1;	/* non automatic status stage on
					 * control transfers */
	uint8_t	no_pipe_ok:1;		/* set if "USB_ERR_NO_PIPE" error can
					 * be ignored */
	uint8_t	stall_pipe:1;		/* set if the endpoint belonging to
					 * this USB transfer should be stalled
					 * before starting this transfer! */
};

/*
 * The following structure defines a set of internal USB transfer
 * flags.
 */
struct usb2_xfer_flags_int {
	uint16_t control_rem;		/* remainder in bytes */

	uint8_t	open:1;			/* set if USB pipe has been opened */
	uint8_t	transferring:1;		/* set if an USB transfer is in
					 * progress */
	uint8_t	did_dma_delay:1;	/* set if we waited for HW DMA */
	uint8_t	did_close:1;		/* set if we closed the USB transfer */
	uint8_t	draining:1;		/* set if we are draining an USB
					 * transfer */
	uint8_t	started:1;		/* keeps track of started or stopped */
	uint8_t	bandwidth_reclaimed:1;
	uint8_t	control_xfr:1;		/* set if control transfer */
	uint8_t	control_hdr:1;		/* set if control header should be
					 * sent */
	uint8_t	control_act:1;		/* set if control transfer is active */

	uint8_t	short_frames_ok:1;	/* filtered version */
	uint8_t	short_xfer_ok:1;	/* filtered version */
	uint8_t	bdma_enable:1;		/* filtered version (only set if
					 * hardware supports DMA) */
	uint8_t	bdma_no_post_sync:1;	/* set if the USB callback wrapper
					 * should not do the BUS-DMA post sync
					 * operation */
	uint8_t	bdma_setup:1;		/* set if BUS-DMA has been setup */
	uint8_t	isochronous_xfr:1;	/* set if isochronous transfer */
	uint8_t	usb2_mode:1;		/* shadow copy of "udev->usb2_mode" */
	uint8_t	curr_dma_set:1;		/* used by USB HC/DC driver */
	uint8_t	can_cancel_immed:1;	/* set if USB transfer can be
					 * cancelled immediately */
};

/*
 * The following structure defines the symmetric part of an USB config
 * structure.
 */
struct usb2_config_sub {
	usb2_callback_t *callback;	/* USB transfer callback */
	uint32_t bufsize;		/* total pipe buffer size in bytes */
	uint32_t frames;		/* maximum number of USB frames */
	uint16_t interval;		/* interval in milliseconds */
#define	USB_DEFAULT_INTERVAL	0
	uint16_t timeout;		/* transfer timeout in milliseconds */
	struct usb2_xfer_flags flags;	/* transfer flags */
};

/*
 * The following structure define an USB configuration, that basically
 * is used when setting up an USB transfer.
 */
struct usb2_config {
	struct usb2_config_sub mh;	/* parameters for USB_MODE_HOST */
	struct usb2_config_sub md;	/* parameters for USB_MODE_DEVICE */
	uint8_t	type;			/* pipe type */
	uint8_t	endpoint;		/* pipe number */
	uint8_t	direction;		/* pipe direction */
	uint8_t	ep_index;		/* pipe index match to use */
	uint8_t	if_index;		/* "ifaces" index to use */
};

/*
 * The following structure defines an USB transfer.
 */
struct usb2_xfer {
	struct usb2_callout timeout_handle;
	TAILQ_ENTRY(usb2_xfer) wait_entry;	/* used at various places */

	struct usb2_page_cache *buf_fixup;	/* fixup buffer(s) */
	struct usb2_xfer_queue *wait_queue;	/* pointer to queue that we
						 * are waiting on */
	struct usb2_page *dma_page_ptr;
	struct usb2_pipe *pipe;		/* our USB pipe */
	struct usb2_device *udev;
	struct mtx *xfer_mtx;		/* cannot be changed during operation */
	struct usb2_xfer_root *usb2_root;	/* used by HC driver */
	void   *usb2_sc;		/* used by HC driver */
	void   *qh_start[2];		/* used by HC driver */
	void   *td_start[2];		/* used by HC driver */
	void   *td_transfer_first;	/* used by HC driver */
	void   *td_transfer_last;	/* used by HC driver */
	void   *td_transfer_cache;	/* used by HC driver */
	void   *priv_sc;		/* device driver data pointer 1 */
	void   *priv_fifo;		/* device driver data pointer 2 */
	void   *local_buffer;
	uint32_t *frlengths;
	struct usb2_page_cache *frbuffers;
	usb2_callback_t *callback;

	uint32_t max_usb2_frame_size;
	uint32_t max_data_length;
	uint32_t sumlen;		/* sum of all lengths in bytes */
	uint32_t actlen;		/* actual length in bytes */
	uint32_t timeout;		/* milliseconds */
#define	USB_NO_TIMEOUT 0
#define	USB_DEFAULT_TIMEOUT 5000	/* 5000 ms = 5 seconds */

	uint32_t max_frame_count;	/* initial value of "nframes" after
					 * setup */
	uint32_t nframes;		/* number of USB frames to transfer */
	uint32_t aframes;		/* actual number of USB frames
					 * transferred */

	uint16_t max_packet_size;
	uint16_t max_frame_size;
	uint16_t qh_pos;
	uint16_t isoc_time_complete;	/* in ms */
	uint16_t interval;		/* milliseconds */

	uint8_t	address;		/* physical USB address */
	uint8_t	endpoint;		/* physical USB endpoint */
	uint8_t	max_packet_count;
	uint8_t	usb2_smask;
	uint8_t	usb2_cmask;
	uint8_t	usb2_uframe;
	uint8_t	usb2_state;

	usb2_error_t error;

	struct usb2_xfer_flags flags;
	struct usb2_xfer_flags_int flags_int;
};

/*
 * The following structure keeps information that is used to match
 * against an array of "usb2_device_id" elements.
 */
struct usb2_lookup_info {
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;
	uint8_t	bInterfaceClass;
	uint8_t	bInterfaceSubClass;
	uint8_t	bInterfaceProtocol;
	uint8_t	bIfaceIndex;
	uint8_t	bIfaceNum;
	uint8_t	bConfigIndex;
	uint8_t	bConfigNum;
};

/* Structure used by probe and attach */

struct usb2_attach_arg {
	struct usb2_lookup_info info;
	device_t temp_dev;		/* for internal use */
	const void *driver_info;	/* for internal use */
	struct usb2_device *device;	/* current device */
	struct usb2_interface *iface;	/* current interface */
	uint8_t	usb2_mode;		/* see USB_MODE_XXX */
	uint8_t	port;
	uint8_t	use_generic;		/* hint for generic drivers */
};

/* Structure used when referring an USB device */

struct usb2_location {
	struct usb2_bus *bus;
	struct usb2_device *udev;
	struct usb2_interface *iface;
	struct usb2_fifo *rxfifo;
	struct usb2_fifo *txfifo;
	uint32_t devloc;		/* original devloc */
	uint16_t bus_index;		/* bus index */
	uint8_t	dev_index;		/* device index */
	uint8_t	iface_index;		/* interface index */
	uint8_t	fifo_index;		/* FIFO index */
	uint8_t	is_read;		/* set if location has read access */
	uint8_t	is_write;		/* set if location has write access */
	uint8_t	is_uref;		/* set if USB refcount decr. needed */
	uint8_t	is_usbfs;		/* set if USB-FS is active */
};

/* external variables */

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
MALLOC_DECLARE(M_USBHC);

extern struct mtx usb2_ref_lock;

/* typedefs */

typedef struct malloc_type *usb2_malloc_type;

/* prototypes */

const char *usb2_errstr(usb2_error_t error);
struct usb2_config_descriptor *usb2_get_config_descriptor(struct usb2_device *udev);
struct usb2_device_descriptor *usb2_get_device_descriptor(struct usb2_device *udev);
struct usb2_interface *usb2_get_iface(struct usb2_device *udev, uint8_t iface_index);
struct usb2_interface_descriptor *usb2_get_interface_descriptor(struct usb2_interface *iface);
uint8_t	usb2_clear_stall_callback(struct usb2_xfer *xfer1, struct usb2_xfer *xfer2);
uint8_t	usb2_get_interface_altindex(struct usb2_interface *iface);
usb2_error_t usb2_set_alt_interface_index(struct usb2_device *udev, uint8_t iface_index, uint8_t alt_index);
uint8_t	usb2_get_speed(struct usb2_device *udev);
usb2_error_t usb2_transfer_setup(struct usb2_device *udev, const uint8_t *ifaces, struct usb2_xfer **pxfer, const struct usb2_config *setup_start, uint16_t n_setup, void *priv_sc, struct mtx *priv_mtx);
void	usb2_set_frame_data(struct usb2_xfer *xfer, void *ptr, uint32_t frindex);
void	usb2_set_frame_offset(struct usb2_xfer *xfer, uint32_t offset, uint32_t frindex);
void	usb2_start_hardware(struct usb2_xfer *xfer);
void	usb2_transfer_clear_stall(struct usb2_xfer *xfer);
void	usb2_transfer_drain(struct usb2_xfer *xfer);
void	usb2_transfer_set_stall(struct usb2_xfer *xfer);
void	usb2_transfer_start(struct usb2_xfer *xfer);
void	usb2_transfer_stop(struct usb2_xfer *xfer);
void	usb2_transfer_unsetup(struct usb2_xfer **pxfer, uint16_t n_setup);
usb2_error_t usb2_ref_device(struct file *fp, struct usb2_location *ploc, uint32_t devloc);
void	usb2_unref_device(struct usb2_location *ploc);
void	usb2_set_parent_iface(struct usb2_device *udev, uint8_t iface_index, uint8_t parent_index);
void	usb2_set_iface_perm(struct usb2_device *udev, uint8_t iface_index, uint32_t uid, uint32_t gid, uint16_t mode);
uint8_t	usb2_get_bus_index(struct usb2_device *udev);
uint8_t	usb2_get_device_index(struct usb2_device *udev);

#endif					/* _USB2_CORE_H_ */

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

#ifndef _USB2_DEV_H_
#define	_USB2_DEV_H_

#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>

#define	USB_FIFO_TX 0
#define	USB_FIFO_RX 1

struct usb2_fifo;

typedef int (usb2_fifo_open_t)(struct usb2_fifo *fifo, int fflags, struct thread *td);
typedef void (usb2_fifo_close_t)(struct usb2_fifo *fifo, int fflags, struct thread *td);
typedef int (usb2_fifo_ioctl_t)(struct usb2_fifo *fifo, u_long cmd, void *addr, int fflags, struct thread *td);
typedef void (usb2_fifo_cmd_t)(struct usb2_fifo *fifo);

struct usb2_symlink {
	TAILQ_ENTRY(usb2_symlink) sym_entry;
	char	src_path[32];		/* Source path - including terminating
					 * zero */
	char	dst_path[32];		/* Destination path - including
					 * terminating zero */
	uint8_t	src_len;		/* String length */
	uint8_t	dst_len;		/* String length */
};

/*
 * Locking note for the following functions.  All the
 * "usb2_fifo_cmd_t" functions are called locked. The others are
 * called unlocked.
 */
struct usb2_fifo_methods {
	usb2_fifo_open_t *f_open;
	usb2_fifo_close_t *f_close;
	usb2_fifo_ioctl_t *f_ioctl;
	usb2_fifo_cmd_t *f_start_read;
	usb2_fifo_cmd_t *f_stop_read;
	usb2_fifo_cmd_t *f_start_write;
	usb2_fifo_cmd_t *f_stop_write;
	const char *basename[4];
	const char *postfix[4];
};

/*
 * Most of the fields in the "usb2_fifo" structure are used by the
 * generic USB access layer.
 */
struct usb2_fifo {
	struct usb2_ifqueue free_q;
	struct usb2_ifqueue used_q;
	struct selinfo selinfo;
	struct cv cv_io;
	struct cv cv_drain;
	struct usb2_fifo_methods *methods;
	struct usb2_symlink *symlink[2];/* our symlinks */
	struct proc *async_p;		/* process that wants SIGIO */
	struct usb2_fs_endpoint *fs_ep_ptr;
	struct usb2_device *udev;
	struct usb2_xfer *xfer[2];
	struct usb2_xfer **fs_xfer;
	struct mtx *priv_mtx;		/* client data */
	struct file *curr_file;		/* set if FIFO is opened by a FILE */
	void   *priv_sc0;		/* client data */
	void   *priv_sc1;		/* client data */
	void   *queue_data;
	uint32_t timeout;		/* timeout in milliseconds */
	uint32_t bufsize;		/* BULK and INTERRUPT buffer size */
	uint16_t nframes;		/* for isochronous mode */
	uint16_t dev_ep_index;		/* our device endpoint index */
	uint8_t	flag_no_uref;		/* set if FIFO is not control endpoint */
	uint8_t	flag_sleeping;		/* set if FIFO is sleeping */
	uint8_t	flag_iscomplete;	/* set if a USB transfer is complete */
	uint8_t	flag_iserror;		/* set if FIFO error happened */
	uint8_t	flag_isselect;		/* set if FIFO is selected */
	uint8_t	flag_flushing;		/* set if FIFO is flushing data */
	uint8_t	flag_short;		/* set if short_ok or force_short
					 * transfer flags should be set */
	uint8_t	flag_stall;		/* set if clear stall should be run */
	uint8_t	iface_index;		/* set to the interface we belong to */
	uint8_t	fifo_index;		/* set to the FIFO index in "struct
					 * usb2_device" */
	uint8_t	fs_ep_max;
	uint8_t	fifo_zlp;		/* zero length packet count */
	uint8_t	refcount;
#define	USB_FIFO_REF_MAX 0xFF
};

struct usb2_fifo_sc {
	struct usb2_fifo *fp[2];
};

int	usb2_fifo_wait(struct usb2_fifo *fifo);
void	usb2_fifo_signal(struct usb2_fifo *fifo);
int	usb2_fifo_alloc_buffer(struct usb2_fifo *f, uint32_t bufsize, uint16_t nbuf);
void	usb2_fifo_free_buffer(struct usb2_fifo *f);
int	usb2_fifo_attach(struct usb2_device *udev, void *priv_sc, struct mtx *priv_mtx, struct usb2_fifo_methods *pm, struct usb2_fifo_sc *f_sc, uint16_t unit, uint16_t subunit, uint8_t iface_index);
void	usb2_fifo_detach(struct usb2_fifo_sc *f_sc);
uint32_t usb2_fifo_put_bytes_max(struct usb2_fifo *fifo);
void	usb2_fifo_put_data(struct usb2_fifo *fifo, struct usb2_page_cache *pc, uint32_t offset, uint32_t len, uint8_t what);
void	usb2_fifo_put_data_linear(struct usb2_fifo *fifo, void *ptr, uint32_t len, uint8_t what);
uint8_t	usb2_fifo_put_data_buffer(struct usb2_fifo *f, void *ptr, uint32_t len);
void	usb2_fifo_put_data_error(struct usb2_fifo *fifo);
uint8_t	usb2_fifo_get_data(struct usb2_fifo *fifo, struct usb2_page_cache *pc, uint32_t offset, uint32_t len, uint32_t *actlen, uint8_t what);
uint8_t	usb2_fifo_get_data_linear(struct usb2_fifo *fifo, void *ptr, uint32_t len, uint32_t *actlen, uint8_t what);
uint8_t	usb2_fifo_get_data_buffer(struct usb2_fifo *f, void **pptr, uint32_t *plen);
void	usb2_fifo_get_data_next(struct usb2_fifo *f);
void	usb2_fifo_get_data_error(struct usb2_fifo *fifo);
uint8_t	usb2_fifo_opened(struct usb2_fifo *fifo);
void	usb2_fifo_free(struct usb2_fifo *f);
void	usb2_fifo_reset(struct usb2_fifo *f);
int	usb2_check_thread_perm(struct usb2_device *udev, struct thread *td, int fflags, uint8_t iface_index, uint8_t ep_index);
void	usb2_fifo_wakeup(struct usb2_fifo *f);
struct usb2_symlink *usb2_alloc_symlink(const char *target, const char *fmt,...);
void	usb2_free_symlink(struct usb2_symlink *ps);
uint32_t usb2_lookup_symlink(const char *src_ptr, uint8_t src_len);
int	usb2_read_symlink(uint8_t *user_ptr, uint32_t startentry, uint32_t user_len);

#endif					/* _USB2_DEV_H_ */

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

#ifndef _USB2_TRANSFER_H_
#define	_USB2_TRANSFER_H_

/*
 * The following structure defines the messages that is used to signal
 * the "done_p" USB process.
 */
struct usb2_done_msg {
	struct usb2_proc_msg hdr;
	struct usb2_xfer_root *usb2_root;
};

/*
 * The following structure is used to keep information about memory
 * that should be automatically freed at the moment all USB transfers
 * have been freed.
 */
struct usb2_xfer_root {
	struct usb2_xfer_queue dma_q;
	struct usb2_xfer_queue done_q;
	struct usb2_done_msg done_m[2];
	struct cv cv_drain;

	struct usb2_dma_parent_tag dma_parent_tag;

	struct usb2_process done_p;
	void   *memory_base;
	struct mtx *priv_mtx;
	struct usb2_page_cache *dma_page_cache_start;
	struct usb2_page_cache *dma_page_cache_end;
	struct usb2_page_cache *xfer_page_cache_start;
	struct usb2_page_cache *xfer_page_cache_end;
	struct usb2_bus *bus;

	uint32_t memory_size;
	uint32_t setup_refcount;
	uint32_t page_size;
	uint32_t dma_nframes;		/* number of page caches to load */
	uint32_t dma_currframe;		/* currect page cache number */
	uint32_t dma_frlength_0;	/* length of page cache zero */
	uint8_t	dma_error;		/* set if virtual memory could not be
					 * loaded */
	uint8_t	done_sleep;		/* set if done thread is sleeping */
};

/*
 * The following structure is used when setting up an array of USB
 * transfers.
 */
struct usb2_setup_params {
	struct usb2_dma_tag *dma_tag_p;
	struct usb2_page *dma_page_ptr;
	struct usb2_page_cache *dma_page_cache_ptr;	/* these will be
							 * auto-freed */
	struct usb2_page_cache *xfer_page_cache_ptr;	/* these will not be
							 * auto-freed */
	struct usb2_device *udev;
	struct usb2_xfer *curr_xfer;
	const struct usb2_config *curr_setup;
	const struct usb2_config_sub *curr_setup_sub;
	const struct usb2_pipe_methods *methods;
	void   *buf;
	uint32_t *xfer_length_ptr;

	uint32_t size[7];
	uint32_t bufsize;
	uint32_t bufsize_max;
	uint32_t hc_max_frame_size;

	uint16_t hc_max_packet_size;
	uint8_t	hc_max_packet_count;
	uint8_t	speed;
	uint8_t	dma_tag_max;
	usb2_error_t err;
};

/* function prototypes */

uint8_t	usb2_transfer_pending(struct usb2_xfer *xfer);
uint8_t	usb2_transfer_setup_sub_malloc(struct usb2_setup_params *parm, struct usb2_page_cache **ppc, uint32_t size, uint32_t align, uint32_t count);
void	usb2_command_wrapper(struct usb2_xfer_queue *pq, struct usb2_xfer *xfer);
void	usb2_pipe_enter(struct usb2_xfer *xfer);
void	usb2_pipe_start(struct usb2_xfer_queue *pq);
void	usb2_transfer_dequeue(struct usb2_xfer *xfer);
void	usb2_transfer_done(struct usb2_xfer *xfer, usb2_error_t error);
void	usb2_transfer_enqueue(struct usb2_xfer_queue *pq, struct usb2_xfer *xfer);
void	usb2_transfer_setup_sub(struct usb2_setup_params *parm);
void	usb2_default_transfer_setup(struct usb2_device *udev);
void	usb2_clear_data_toggle(struct usb2_device *udev, struct usb2_pipe *pipe);
void	usb2_do_poll(struct usb2_xfer **ppxfer, uint16_t max);
usb2_callback_t usb2_do_request_callback;
usb2_callback_t usb2_handle_request_callback;
usb2_callback_t usb2_do_clear_stall_callback;
void	usb2_transfer_timeout_ms(struct usb2_xfer *xfer, void (*cb) (void *arg), uint32_t ms);

#endif					/* _USB2_TRANSFER_H_ */

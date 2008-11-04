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

#ifndef _USB2_CONFIG_TD_H_
#define	_USB2_CONFIG_TD_H_

struct usb2_config_td_softc;
struct usb2_config_td_cc;

#define	USB2_CONFIG_TD_SYNC 0xFFFF	/* magic value */

typedef void (usb2_config_td_command_t)(struct usb2_config_td_softc *sc, struct usb2_config_td_cc *cc, uint16_t reference);
typedef void (usb2_config_td_end_of_commands_t)(struct usb2_config_td_softc *sc);

/*
 * The following structure defines a command that should be executed
 * using the USB config thread system.
 */
struct usb2_config_td_item {
	struct usb2_proc_msg hdr;
	struct usb2_config_td *p_ctd;
	usb2_config_td_command_t *command_func;
	uint16_t command_ref;
} __aligned(USB_HOST_ALIGN);

/*
 * The following structure defines an USB config thread.
 */
struct usb2_config_td {
	struct usb2_process usb2_proc;
	struct usb2_config_td_softc *p_softc;
	usb2_config_td_end_of_commands_t *p_end_of_commands;
	void   *p_msgs;
	uint16_t msg_size;
	uint16_t msg_count;
};

/* prototypes */

uint8_t	usb2_config_td_setup(struct usb2_config_td *ctd, void *priv_sc, struct mtx *priv_mtx, usb2_config_td_end_of_commands_t *p_func_eoc, uint16_t item_size, uint16_t item_count);
void	usb2_config_td_drain(struct usb2_config_td *ctd);
void	usb2_config_td_unsetup(struct usb2_config_td *ctd);
void	usb2_config_td_queue_command(struct usb2_config_td *ctd, usb2_config_td_command_t *pre_func, usb2_config_td_command_t *post_func, uint16_t command_sync, uint16_t command_ref);
uint8_t	usb2_config_td_is_gone(struct usb2_config_td *ctd);
uint8_t	usb2_config_td_sleep(struct usb2_config_td *ctd, uint32_t timeout);
uint8_t	usb2_config_td_sync(struct usb2_config_td *ctd);

#endif					/* _USB2_CONFIG_TD_H_ */

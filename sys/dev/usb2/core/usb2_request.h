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

#ifndef _USB2_REQUEST_H_
#define	_USB2_REQUEST_H_

usb2_error_t usb2_do_request_flags(struct usb2_device *udev, struct mtx *mtx, struct usb2_device_request *req, void *data, uint32_t flags, uint16_t *actlen, uint32_t timeout);
usb2_error_t usb2_req_clear_hub_feature(struct usb2_device *udev, struct mtx *mtx, uint16_t sel);
usb2_error_t usb2_req_clear_port_feature(struct usb2_device *udev, struct mtx *mtx, uint8_t port, uint16_t sel);
usb2_error_t usb2_req_get_alt_interface_no(struct usb2_device *udev, struct mtx *mtx, uint8_t *alt_iface_no, uint8_t iface_index);
usb2_error_t usb2_req_get_config(struct usb2_device *udev, struct mtx *mtx, uint8_t *pconf);
usb2_error_t usb2_req_get_config_desc(struct usb2_device *udev, struct mtx *mtx, struct usb2_config_descriptor *d, uint8_t conf_index);
usb2_error_t usb2_req_get_config_desc_full(struct usb2_device *udev, struct mtx *mtx, struct usb2_config_descriptor **ppcd, struct malloc_type *mtype, uint8_t conf_index);
usb2_error_t usb2_req_get_desc(struct usb2_device *udev, struct mtx *mtx, void *desc, uint16_t min_len, uint16_t max_len, uint16_t id, uint8_t type, uint8_t index, uint8_t retries);
usb2_error_t usb2_req_get_device_desc(struct usb2_device *udev, struct mtx *mtx, struct usb2_device_descriptor *d);
usb2_error_t usb2_req_get_device_status(struct usb2_device *udev, struct mtx *mtx, struct usb2_status *st);
usb2_error_t usb2_req_get_hub_descriptor(struct usb2_device *udev, struct mtx *mtx, struct usb2_hub_descriptor *hd, uint8_t nports);
usb2_error_t usb2_req_get_hub_status(struct usb2_device *udev, struct mtx *mtx, struct usb2_hub_status *st);
usb2_error_t usb2_req_get_port_status(struct usb2_device *udev, struct mtx *mtx, struct usb2_port_status *ps, uint8_t port);
usb2_error_t usb2_req_get_report(struct usb2_device *udev, struct mtx *mtx, void *data, uint16_t len, uint8_t iface_index, uint8_t type, uint8_t id);
usb2_error_t usb2_req_get_report_descriptor(struct usb2_device *udev, struct mtx *mtx, void *d, uint16_t size, uint8_t iface_index);
usb2_error_t usb2_req_get_string_any(struct usb2_device *udev, struct mtx *mtx, char *buf, uint16_t len, uint8_t string_index);
usb2_error_t usb2_req_get_string_desc(struct usb2_device *udev, struct mtx *mtx, void *sdesc, uint16_t max_len, uint16_t lang_id, uint8_t string_index);
usb2_error_t usb2_req_reset_port(struct usb2_device *udev, struct mtx *mtx, uint8_t port);
usb2_error_t usb2_req_set_address(struct usb2_device *udev, struct mtx *mtx, uint16_t addr);
usb2_error_t usb2_req_set_alt_interface_no(struct usb2_device *udev, struct mtx *mtx, uint8_t iface_index, uint8_t alt_no);
usb2_error_t usb2_req_set_config(struct usb2_device *udev, struct mtx *mtx, uint8_t conf);
usb2_error_t usb2_req_set_hub_feature(struct usb2_device *udev, struct mtx *mtx, uint16_t sel);
usb2_error_t usb2_req_set_idle(struct usb2_device *udev, struct mtx *mtx, uint8_t iface_index, uint8_t duration, uint8_t id);
usb2_error_t usb2_req_set_port_feature(struct usb2_device *udev, struct mtx *mtx, uint8_t port, uint16_t sel);
usb2_error_t usb2_req_set_protocol(struct usb2_device *udev, struct mtx *mtx, uint8_t iface_index, uint16_t report);
usb2_error_t usb2_req_set_report(struct usb2_device *udev, struct mtx *mtx, void *data, uint16_t len, uint8_t iface_index, uint8_t type, uint8_t id);
usb2_error_t usb2_req_re_enumerate(struct usb2_device *udev, struct mtx *mtx);

#define	usb2_do_request(u,m,r,d) \
  usb2_do_request_flags(u,m,r,d,0,NULL,USB_DEFAULT_TIMEOUT)

#endif					/* _USB2_REQUEST_H_ */

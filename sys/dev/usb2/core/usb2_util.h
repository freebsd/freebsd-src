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

#ifndef _USB2_UTIL_H_
#define	_USB2_UTIL_H_

int	device_delete_all_children(device_t dev);
uint32_t usb2_get_devid(device_t dev);
uint8_t	usb2_make_str_desc(void *ptr, uint16_t max_len, const char *s);
void	device_set_usb2_desc(device_t dev);
void	usb2_pause_mtx(struct mtx *mtx, uint32_t ms);
void	usb2_printBCD(char *p, uint16_t p_len, uint16_t bcd);
void	usb2_trim_spaces(char *p);

#if (USB_USE_CONDVAR == 0)
void	usb2_cv_init(struct cv *cv, const char *desc);
void	usb2_cv_destroy(struct cv *cv);
void	usb2_cv_wait(struct cv *cv, struct mtx *mtx);
int	usb2_cv_wait_sig(struct cv *cv, struct mtx *mtx);
int	usb2_cv_timedwait(struct cv *cv, struct mtx *mtx, int timo);
void	usb2_cv_signal(struct cv *cv);
void	usb2_cv_broadcast(struct cv *cv);

#else
#define	usb2_cv_init cv_init
#define	usb2_cv_destroy cv_destroy
#define	usb2_cv_wait cv_wait
#define	usb2_cv_wait_sig cv_wait_sig
#define	usb2_cv_timedwait cv_timedwait
#define	usb2_cv_signal cv_signal
#define	usb2_cv_broadcast cv_broadcast
#endif

#endif					/* _USB2_UTIL_H_ */

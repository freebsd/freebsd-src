/*
 * Copyright © 2013 Philip Withnall
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef WAYLAND_ITC_BUFFER_H
#define WAYLAND_ITC_BUFFER_H

#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include "wayland-capabilities.h"
#include "wayland-util.h" /* TODO: just needed for malloc() */


struct wl_itc_buffer;

#ifdef CP2

struct wl_itc_buffer *wl_itc_buffer_create(unsigned int slots,
                                           struct chericap *cap_out);
void wl_itc_buffer_destroy(struct wl_itc_buffer *buf, struct chericap *cap);

ssize_t wl_itc_buffer_sendmsg(struct wl_itc_buffer *buf,
                              struct chericap *buf_cap,
                              const struct msghdr *msg, int flags);
ssize_t wl_itc_buffer_recvmsg(struct wl_itc_buffer *buf,
                              struct chericap *buf_cap, struct msghdr *msg,
                              int flags);

ssize_t wl_itc_buffer_read(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                           void *data_buf, size_t count);
ssize_t wl_itc_buffer_write(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                            const void *data_buf, size_t count);

unsigned int wl_itc_buffer_is_empty(struct wl_itc_buffer *buf,
                                    struct chericap *buf_cap);

#else /* if !CP2 */

struct wl_itc_buffer *wl_itc_buffer_create(unsigned int slots);
void wl_itc_buffer_destroy(struct wl_itc_buffer *buf);

ssize_t wl_itc_buffer_sendmsg(struct wl_itc_buffer *buf,
                              const struct msghdr *msg, int flags);
ssize_t wl_itc_buffer_recvmsg(struct wl_itc_buffer *buf, struct msghdr *msg,
                              int flags);

ssize_t wl_itc_buffer_read(struct wl_itc_buffer *buf, void *data_buf,
                           size_t count);
ssize_t wl_itc_buffer_write(struct wl_itc_buffer *buf, const void *data_buf,
                            size_t count);

unsigned int wl_itc_buffer_is_empty(struct wl_itc_buffer *buf);

#endif /* !CP2 */

#endif

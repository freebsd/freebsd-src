/*
 * Copyright © 2013 Red Hat, Inc.
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

#include <config.h>
#include <libevdev/libevdev.h>

#define DEFAULT_IDS NULL


struct uinput_device* uinput_device_new(const char *name);
int uinput_device_new_with_events(struct uinput_device **dev, const char *name, const struct input_id *ids, ...);
int uinput_device_new_with_events_v(struct uinput_device **dev, const char *name, const struct input_id *ids, va_list args);
void uinput_device_free(struct uinput_device *dev);

int uinput_device_create(struct uinput_device* dev);
int uinput_device_set_name(struct uinput_device* dev, const char *name);
int uinput_device_set_ids(struct uinput_device* dev, const struct input_id *ids);
int uinput_device_set_bit(struct uinput_device* dev, unsigned int bit);
int uinput_device_set_prop(struct uinput_device *dev, unsigned int prop);
int uinput_device_set_event_bit(struct uinput_device* dev, unsigned int type, unsigned int code);
int uinput_device_set_event_bits(struct uinput_device* dev, ...);
int uinput_device_set_event_bits_v(struct uinput_device* dev, va_list args);
int uinput_device_set_abs_bit(struct uinput_device* dev, unsigned int code, const struct input_absinfo *absinfo);
int uinput_device_event(const struct uinput_device* dev, unsigned int type, unsigned int code, int value);
int uinput_device_event_multiple(const struct uinput_device* dev, ...);
int uinput_device_event_multiple_v(const struct uinput_device* dev, va_list args);
int uinput_device_get_fd(const struct uinput_device *dev);
const char* uinput_device_get_devnode(const struct uinput_device *dev);

char *uinput_devnode_from_syspath(const char *syspath);

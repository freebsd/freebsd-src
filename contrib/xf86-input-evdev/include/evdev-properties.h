/*
 * Copyright © 2008 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Peter Hutterer (peter.hutterer@redhat.com)
 */


#ifndef _EVDEV_PROPERTIES_H_
#define _EVDEV_PROPERTIES_H_

/* Middle mouse button emulation */
/* BOOL */
#define EVDEV_PROP_MIDBUTTON "Evdev Middle Button Emulation"
/* CARD32 */
#define EVDEV_PROP_MIDBUTTON_TIMEOUT "Evdev Middle Button Timeout"

/* Wheel emulation */
/* BOOL */
#define EVDEV_PROP_WHEEL "Evdev Wheel Emulation"
/* CARD8, 4 values [x up, x down, y up, y down], 0 to disable a value*/
#define EVDEV_PROP_WHEEL_AXES "Evdev Wheel Emulation Axes"
/* CARD16 */
#define EVDEV_PROP_WHEEL_INERTIA "Evdev Wheel Emulation Inertia"
/* CARD16 */
#define EVDEV_PROP_WHEEL_TIMEOUT "Evdev Wheel Emulation Timeout"
/* CARD8, value range 0-32, 0 to always scroll */
#define EVDEV_PROP_WHEEL_BUTTON "Evdev Wheel Emulation Button"

/* Drag lock */
/* CARD8, either 1 value or pairs, value range 0-32, 0 to disable a value*/
#define EVDEV_PROP_DRAGLOCK "Evdev Drag Lock Buttons"

/* Axis inversion */
/* BOOL, 2 values [x, y], 1 inverts axis */
#define EVDEV_PROP_INVERT_AXES "Evdev Axis Inversion"

/* Reopen attempts. */
/* CARD8 */
#define EVDEV_PROP_REOPEN "Evdev Reopen Attempts"

/* Run-time calibration */
/* CARD32, 4 values [minx, maxx, miny, maxy], or no values for unset */
#define EVDEV_PROP_CALIBRATION "Evdev Axis Calibration"

/* Swap x and y axis. */
/* BOOL */
#define EVDEV_PROP_SWAP_AXES "Evdev Axes Swap"

#endif

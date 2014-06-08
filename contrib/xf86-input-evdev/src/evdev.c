/*
 * Copyright © 2004-2008 Red Hat, Inc.
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
 *	Kristian Høgsberg (krh@redhat.com)
 *	Adam Jackson (ajax@redhat.com)
 *	Peter Hutterer (peter.hutterer@redhat.com)
 *	Oliver McFadden (oliver.mcfadden@nokia.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/keysym.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <xorgVersion.h>
#include <xkbsrv.h>

#include "evdev.h"

#ifdef HAVE_PROPERTIES
#include <X11/Xatom.h>
#include <evdev-properties.h>
#include <xserver-properties.h>
/* 1.6 has properties, but no labels */
#ifdef AXIS_LABEL_PROP
#define HAVE_LABELS
#else
#undef HAVE_LABELS
#endif

#endif

#ifndef MAXDEVICES
#include <inputstr.h> /* for MAX_DEVICES */
#define MAXDEVICES MAX_DEVICES
#endif

/* 2.4 compatibility */
#ifndef EVIOCGRAB
#define EVIOCGRAB _IOW('E', 0x90, int)
#endif

#ifndef BTN_TASK
#define BTN_TASK 0x117
#endif

#ifndef EV_SYN
#define EV_SYN EV_RST
#endif
/* end compat */

#define ArrayLength(a) (sizeof(a) / (sizeof((a)[0])))

/* evdev flags */
#define EVDEV_KEYBOARD_EVENTS	(1 << 0)
#define EVDEV_BUTTON_EVENTS	(1 << 1)
#define EVDEV_RELATIVE_EVENTS	(1 << 2)
#define EVDEV_ABSOLUTE_EVENTS	(1 << 3)
#define EVDEV_TOUCHPAD		(1 << 4)
#define EVDEV_INITIALIZED	(1 << 5) /* WheelInit etc. called already? */
#define EVDEV_TOUCHSCREEN	(1 << 6)
#define EVDEV_CALIBRATED	(1 << 7) /* run-time calibrated? */
#define EVDEV_TABLET		(1 << 8) /* device looks like a tablet? */
#define EVDEV_UNIGNORE_ABSOLUTE (1 << 9) /* explicitly unignore abs axes */
#define EVDEV_UNIGNORE_RELATIVE (1 << 10) /* explicitly unignore rel axes */

#define MIN_KEYCODE 8
#define GLYPHS_PER_KEY 2
#define AltMask		Mod1Mask
#define NumLockMask	Mod2Mask
#define AltLangMask	Mod3Mask
#define KanaMask	Mod4Mask
#define ScrollLockMask	Mod5Mask

#define CAPSFLAG	1
#define NUMFLAG		2
#define SCROLLFLAG	4
#define MODEFLAG	8
#define COMPOSEFLAG	16

static const char *evdevDefaults[] = {
    "XkbRules",     "evdev",
    "XkbModel",     "evdev",
    "XkbLayout",    "us",
    NULL
};

static int EvdevOn(DeviceIntPtr);
static int EvdevCacheCompare(InputInfoPtr pInfo, BOOL compare);
static void EvdevKbdCtrl(DeviceIntPtr device, KeybdCtrl *ctrl);

#ifdef HAVE_PROPERTIES
static void EvdevInitAxesLabels(EvdevPtr pEvdev, int natoms, Atom *atoms);
static void EvdevInitButtonLabels(EvdevPtr pEvdev, int natoms, Atom *atoms);
static void EvdevInitProperty(DeviceIntPtr dev);
static int EvdevSetProperty(DeviceIntPtr dev, Atom atom,
                            XIPropertyValuePtr val, BOOL checkonly);
static Atom prop_invert = 0;
static Atom prop_reopen = 0;
static Atom prop_calibration = 0;
static Atom prop_swap = 0;
static Atom prop_axis_label = 0;
static Atom prop_btn_label = 0;
#endif

/* All devices the evdev driver has allocated and knows about.
 * MAXDEVICES is safe as null-terminated array, as two devices (VCP and VCK)
 * cannot be used by evdev, leaving us with a space of 2 at the end. */
static EvdevPtr evdev_devices[MAXDEVICES] = {NULL};

static size_t CountBits(unsigned long *array, size_t nlongs)
{
    unsigned int i;
    size_t count = 0;

    for (i = 0; i < nlongs; i++) {
        unsigned long x = array[i];

        while (x > 0)
        {
            count += (x & 0x1);
            x >>= 1;
        }
    }
    return count;
}

static int
EvdevGetMajorMinor(InputInfoPtr pInfo)
{
    struct stat st;

    if (fstat(pInfo->fd, &st) == -1)
    {
        xf86Msg(X_ERROR, "%s: stat failed (%s). cannot check for duplicates.\n",
                pInfo->name, strerror(errno));
        return 0;
    }

    return st.st_rdev;
}

/**
 * Return TRUE if one of the devices we know about has the same min/maj
 * number.
 */
static BOOL
EvdevIsDuplicate(InputInfoPtr pInfo)
{
    EvdevPtr pEvdev = pInfo->private;
    EvdevPtr* dev   = evdev_devices;

    if (pEvdev->min_maj)
    {
        while(*dev)
        {
            if ((*dev) != pEvdev &&
                (*dev)->min_maj &&
                (*dev)->min_maj == pEvdev->min_maj)
                return TRUE;
            dev++;
        }
    }
    return FALSE;
}

/**
 * Add to internal device list.
 */
static void
EvdevAddDevice(InputInfoPtr pInfo)
{
    EvdevPtr pEvdev = pInfo->private;
    EvdevPtr* dev = evdev_devices;

    while(*dev)
        dev++;

    *dev = pEvdev;
}

/**
 * Remove from internal device list.
 */
static void
EvdevRemoveDevice(InputInfoPtr pInfo)
{
    EvdevPtr pEvdev = pInfo->private;
    EvdevPtr *dev   = evdev_devices;
    int count       = 0;

    while(*dev)
    {
        count++;
        if (*dev == pEvdev)
        {
            memmove(dev, dev + 1,
                    sizeof(evdev_devices) - (count * sizeof(EvdevPtr)));
            break;
        }
        dev++;
    }
}


static void
SetXkbOption(InputInfoPtr pInfo, char *name, char **option)
{
    char *s;

    if ((s = xf86SetStrOption(pInfo->options, name, NULL))) {
        if (!s[0]) {
            xfree(s);
            *option = NULL;
        } else {
            *option = s;
        }
    }
}

static int wheel_up_button = 4;
static int wheel_down_button = 5;
static int wheel_left_button = 6;
static int wheel_right_button = 7;

void
EvdevQueueKbdEvent(InputInfoPtr pInfo, struct input_event *ev, int value)
{
    int code = ev->code + MIN_KEYCODE;
    static char warned[KEY_CNT];
    EventQueuePtr pQueue;
    EvdevPtr pEvdev = pInfo->private;

    /* Filter all repeated events from device.
       We'll do softrepeat in the server, but only since 1.6 */
    if (value == 2
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) <= 2
        && (ev->code == KEY_LEFTCTRL || ev->code == KEY_RIGHTCTRL ||
            ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT ||
            ev->code == KEY_LEFTALT || ev->code == KEY_RIGHTALT ||
            ev->code == KEY_LEFTMETA || ev->code == KEY_RIGHTMETA ||
            ev->code == KEY_CAPSLOCK || ev->code == KEY_NUMLOCK ||
            ev->code == KEY_SCROLLLOCK) /* XXX windows keys? */
#endif
            )
	return;

    if (code > 255)
    {
        if (ev->code <= KEY_MAX && !warned[ev->code])
        {
            xf86Msg(X_WARNING, "%s: unable to handle keycode %d\n",
                    pInfo->name, ev->code);
            warned[ev->code] = 1;
        }

        /* The X server can't handle keycodes > 255. */
        return;
    }

    if (pEvdev->num_queue >= EVDEV_MAXQUEUE)
    {
        xf86Msg(X_NONE, "%s: dropping event due to full queue!\n", pInfo->name);
        return;
    }

    pQueue = &pEvdev->queue[pEvdev->num_queue];
    pQueue->type = EV_QUEUE_KEY;
    pQueue->key = code;
    pQueue->val = value;
    pEvdev->num_queue++;
}

void
EvdevQueueButtonEvent(InputInfoPtr pInfo, int button, int value)
{
    EventQueuePtr pQueue;
    EvdevPtr pEvdev = pInfo->private;

    if (pEvdev->num_queue >= EVDEV_MAXQUEUE)
    {
        xf86Msg(X_NONE, "%s: dropping event due to full queue!\n", pInfo->name);
        return;
    }

    pQueue = &pEvdev->queue[pEvdev->num_queue];
    pQueue->type = EV_QUEUE_BTN;
    pQueue->key = button;
    pQueue->val = value;
    pEvdev->num_queue++;
}

/**
 * Post button event right here, right now.
 * Interface for MB emulation since these need to post immediately.
 */
void
EvdevPostButtonEvent(InputInfoPtr pInfo, int button, int value)
{
    xf86PostButtonEvent(pInfo->dev, 0, button, value, 0, 0);
}

void
EvdevQueueButtonClicks(InputInfoPtr pInfo, int button, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        EvdevQueueButtonEvent(pInfo, button, 1);
        EvdevQueueButtonEvent(pInfo, button, 0);
    }
}

/**
 * Coming back from resume may leave us with a file descriptor that can be
 * opened but fails on the first read (ENODEV).
 * In this case, try to open the device until it becomes available or until
 * the predefined count expires.
 */
static CARD32
EvdevReopenTimer(OsTimerPtr timer, CARD32 time, pointer arg)
{
    InputInfoPtr pInfo = (InputInfoPtr)arg;
    EvdevPtr pEvdev = pInfo->private;

    do {
        pInfo->fd = open(pEvdev->device, O_RDWR | O_NONBLOCK, 0);
    } while (pInfo->fd < 0 && errno == EINTR);

    if (pInfo->fd != -1)
    {
        if (EvdevCacheCompare(pInfo, TRUE) == Success)
        {
            xf86Msg(X_INFO, "%s: Device reopened after %d attempts.\n", pInfo->name,
                    pEvdev->reopen_attempts - pEvdev->reopen_left + 1);
            EvdevOn(pInfo->dev);
        } else
        {
            xf86Msg(X_ERROR, "%s: Device has changed - disabling.\n",
                    pInfo->name);
            xf86DisableDevice(pInfo->dev, FALSE);
            close(pInfo->fd);
            pInfo->fd = -1;
            pEvdev->min_maj = 0; /* don't hog the device */
        }
        pEvdev->reopen_left = 0;
        return 0;
    }

    pEvdev->reopen_left--;

    if (pEvdev->reopen_left <= 0)
    {
        xf86Msg(X_ERROR, "%s: Failed to reopen device after %d attempts.\n",
                pInfo->name, pEvdev->reopen_attempts);
        xf86DisableDevice(pInfo->dev, FALSE);
        pEvdev->min_maj = 0; /* don't hog the device */
        return 0;
    }

    return 100; /* come back in 100 ms */
}

#define ABS_X_VALUE 0x1
#define ABS_Y_VALUE 0x2
#define ABS_VALUE   0x4
/**
 * Take the valuators and process them accordingly.
 */
static void
EvdevProcessValuators(InputInfoPtr pInfo, int v[MAX_VALUATORS], int *num_v,
                      int *first_v)
{
    int tmp;
    EvdevPtr pEvdev = pInfo->private;

    *num_v = *first_v = 0;

    /* convert to relative motion for touchpads */
    if (pEvdev->abs && (pEvdev->flags & EVDEV_TOUCHPAD)) {
        if (pEvdev->tool) { /* meaning, touch is active */
            if (pEvdev->old_vals[0] != -1)
                pEvdev->delta[REL_X] = pEvdev->vals[0] - pEvdev->old_vals[0];
            if (pEvdev->old_vals[1] != -1)
                pEvdev->delta[REL_Y] = pEvdev->vals[1] - pEvdev->old_vals[1];
            if (pEvdev->abs & ABS_X_VALUE)
                pEvdev->old_vals[0] = pEvdev->vals[0];
            if (pEvdev->abs & ABS_Y_VALUE)
                pEvdev->old_vals[1] = pEvdev->vals[1];
        } else {
            pEvdev->old_vals[0] = pEvdev->old_vals[1] = -1;
        }
        pEvdev->abs = 0;
        pEvdev->rel = 1;
    }

    if (pEvdev->rel) {
        int first = REL_CNT, last = 0;
        int i;

        if (pEvdev->swap_axes) {
            tmp = pEvdev->delta[REL_X];
            pEvdev->delta[REL_X] = pEvdev->delta[REL_Y];
            pEvdev->delta[REL_Y] = tmp;
        }
        if (pEvdev->invert_x)
            pEvdev->delta[REL_X] *= -1;
        if (pEvdev->invert_y)
            pEvdev->delta[REL_Y] *= -1;

        for (i = 0; i < REL_CNT; i++)
        {
            int map = pEvdev->axis_map[i];
            if (map != -1)
            {
                v[map] = pEvdev->delta[i];
                if (map < first)
                    first = map;
                if (map > last)
                    last = map;
            }
        }

        *num_v = (last - first + 1);
        *first_v = first;
    }
    /*
     * Some devices only generate valid abs coords when BTN_DIGI is
     * pressed.  On wacom tablets, this means that the pen is in
     * proximity of the tablet.  After the pen is removed, BTN_DIGI is
     * released, and a (0, 0) absolute event is generated.  Checking
     * pEvdev->digi here, lets us ignore that event.  pEvdev is
     * initialized to 1 so devices that doesn't use this scheme still
     * just works.
     */
    else if (pEvdev->abs && pEvdev->tool) {
        memcpy(v, pEvdev->vals, sizeof(int) * pEvdev->num_vals);

        if (pEvdev->swap_axes) {
            int tmp = v[0];
            v[0] = v[1];
            v[1] = tmp;
        }

        if (pEvdev->flags & EVDEV_CALIBRATED)
        {
            v[0] = xf86ScaleAxis(v[0],
                    pEvdev->absinfo[ABS_X].maximum,
                    pEvdev->absinfo[ABS_X].minimum,
                    pEvdev->calibration.max_x, pEvdev->calibration.min_x);
            v[1] = xf86ScaleAxis(v[1],
                    pEvdev->absinfo[ABS_Y].maximum,
                    pEvdev->absinfo[ABS_Y].minimum,
                    pEvdev->calibration.max_y, pEvdev->calibration.min_y);
        }

        if (pEvdev->invert_x)
            v[0] = (pEvdev->absinfo[ABS_X].maximum - v[0] +
                    pEvdev->absinfo[ABS_X].minimum);
        if (pEvdev->invert_y)
            v[1] = (pEvdev->absinfo[ABS_Y].maximum - v[1] +
                    pEvdev->absinfo[ABS_Y].minimum);

        *num_v = pEvdev->num_vals;
        *first_v = 0;
    }
}

/**
 * Take a button input event and process it accordingly.
 */
static void
EvdevProcessButtonEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    unsigned int button;
    int value;
    EvdevPtr pEvdev = pInfo->private;

    button = EvdevUtilButtonEventToButtonNumber(pEvdev, ev->code);

    /* Get the signed value, earlier kernels had this as unsigned */
    value = ev->value;

    /* Handle drag lock */
    if (EvdevDragLockFilterEvent(pInfo, button, value))
        return;

    if (EvdevWheelEmuFilterButton(pInfo, button, value))
        return;

    if (EvdevMBEmuFilterEvent(pInfo, button, value))
        return;

    if (button)
        EvdevQueueButtonEvent(pInfo, button, value);
    else
        EvdevQueueKbdEvent(pInfo, ev, value);
}

/**
 * Take the relative motion input event and process it accordingly.
 */
static void
EvdevProcessRelativeMotionEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    static int value;
    EvdevPtr pEvdev = pInfo->private;

    /* Get the signed value, earlier kernels had this as unsigned */
    value = ev->value;

    pEvdev->rel = 1;

    switch (ev->code) {
        case REL_WHEEL:
            if (value > 0)
                EvdevQueueButtonClicks(pInfo, wheel_up_button, value);
            else if (value < 0)
                EvdevQueueButtonClicks(pInfo, wheel_down_button, -value);
            break;

        case REL_DIAL:
        case REL_HWHEEL:
            if (value > 0)
                EvdevQueueButtonClicks(pInfo, wheel_right_button, value);
            else if (value < 0)
                EvdevQueueButtonClicks(pInfo, wheel_left_button, -value);
            break;

        /* We don't post wheel events as axis motion. */
        default:
            /* Ignore EV_REL events if we never set up for them. */
            if (!(pEvdev->flags & EVDEV_RELATIVE_EVENTS))
                return;

            /* Handle mouse wheel emulation */
            if (EvdevWheelEmuFilterMotion(pInfo, ev))
                return;

            pEvdev->delta[ev->code] += value;
            break;
    }
}

/**
 * Take the absolute motion input event and process it accordingly.
 */
static void
EvdevProcessAbsoluteMotionEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    static int value;
    EvdevPtr pEvdev = pInfo->private;

    /* Get the signed value, earlier kernels had this as unsigned */
    value = ev->value;

    /* Ignore EV_ABS events if we never set up for them. */
    if (!(pEvdev->flags & EVDEV_ABSOLUTE_EVENTS))
        return;

    if (ev->code > ABS_MAX)
        return;

    pEvdev->vals[pEvdev->axis_map[ev->code]] = value;
    if (ev->code == ABS_X)
        pEvdev->abs |= ABS_X_VALUE;
    else if (ev->code == ABS_Y)
        pEvdev->abs |= ABS_Y_VALUE;
    else
        pEvdev->abs |= ABS_VALUE;
}

/**
 * Take the key press/release input event and process it accordingly.
 */
static void
EvdevProcessKeyEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    static int value;
    EvdevPtr pEvdev = pInfo->private;

    /* Get the signed value, earlier kernels had this as unsigned */
    value = ev->value;

    /* don't repeat mouse buttons */
    if (ev->code >= BTN_MOUSE && ev->code < KEY_OK)
        if (value == 2)
            return;

    switch (ev->code) {
        case BTN_TOOL_PEN:
        case BTN_TOOL_RUBBER:
        case BTN_TOOL_BRUSH:
        case BTN_TOOL_PENCIL:
        case BTN_TOOL_AIRBRUSH:
        case BTN_TOOL_FINGER:
        case BTN_TOOL_MOUSE:
        case BTN_TOOL_LENS:
            pEvdev->tool = value ? ev->code : 0;
            break;

        case BTN_TOUCH:
            pEvdev->tool = value ? ev->code : 0;
            if (!(pEvdev->flags & (EVDEV_TOUCHSCREEN | EVDEV_TABLET)))
                break;
            /* Treat BTN_TOUCH from devices that only have BTN_TOUCH as
             * BTN_LEFT. */
            ev->code = BTN_LEFT;
            /* Intentional fallthrough! */

        default:
            EvdevProcessButtonEvent(pInfo, ev);
            break;
    }
}

/**
 * Post the relative motion events.
 */
void
EvdevPostRelativeMotionEvents(InputInfoPtr pInfo, int *num_v, int *first_v,
                              int v[MAX_VALUATORS])
{
    EvdevPtr pEvdev = pInfo->private;

    if (pEvdev->rel) {
        xf86PostMotionEventP(pInfo->dev, FALSE, *first_v, *num_v, v + *first_v);
    }
}

/**
 * Post the absolute motion events.
 */
void
EvdevPostAbsoluteMotionEvents(InputInfoPtr pInfo, int *num_v, int *first_v,
                              int v[MAX_VALUATORS])
{
    EvdevPtr pEvdev = pInfo->private;

    /*
     * Some devices only generate valid abs coords when BTN_DIGI is
     * pressed.  On wacom tablets, this means that the pen is in
     * proximity of the tablet.  After the pen is removed, BTN_DIGI is
     * released, and a (0, 0) absolute event is generated.  Checking
     * pEvdev->digi here, lets us ignore that event.  pEvdev is
     * initialized to 1 so devices that doesn't use this scheme still
     * just works.
     */
    if (pEvdev->abs && pEvdev->tool) {
        xf86PostMotionEventP(pInfo->dev, TRUE, *first_v, *num_v, v);
    }
}

/**
 * Post the queued key/button events.
 */
static void EvdevPostQueuedEvents(InputInfoPtr pInfo, int *num_v, int *first_v,
                                  int v[MAX_VALUATORS])
{
    int i;
    EvdevPtr pEvdev = pInfo->private;

    for (i = 0; i < pEvdev->num_queue; i++) {
        switch (pEvdev->queue[i].type) {
        case EV_QUEUE_KEY:
            xf86PostKeyboardEvent(pInfo->dev, pEvdev->queue[i].key,
                                  pEvdev->queue[i].val);
            break;
        case EV_QUEUE_BTN:
            /* FIXME: Add xf86PostButtonEventP to the X server so that we may
             * pass the valuators on ButtonPress/Release events, too.  Currently
             * only MotionNotify events contain the pointer position. */
            xf86PostButtonEvent(pInfo->dev, 0, pEvdev->queue[i].key,
                                pEvdev->queue[i].val, 0, 0);
            break;
        }
    }
}

/**
 * Take the synchronization input event and process it accordingly; the motion
 * notify events are sent first, then any button/key press/release events.
 */
static void
EvdevProcessSyncEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    int num_v = 0, first_v = 0;
    int v[MAX_VALUATORS];
    EvdevPtr pEvdev = pInfo->private;

    EvdevProcessValuators(pInfo, v, &num_v, &first_v);

    EvdevPostRelativeMotionEvents(pInfo, &num_v, &first_v, v);
    EvdevPostAbsoluteMotionEvents(pInfo, &num_v, &first_v, v);
    EvdevPostQueuedEvents(pInfo, &num_v, &first_v, v);

    memset(pEvdev->delta, 0, sizeof(pEvdev->delta));
    memset(pEvdev->queue, 0, sizeof(pEvdev->queue));
    pEvdev->num_queue = 0;
    pEvdev->abs = 0;
    pEvdev->rel = 0;
}

/**
 * Process the events from the device; nothing is actually posted to the server
 * until an EV_SYN event is received.
 */
static void
EvdevProcessEvent(InputInfoPtr pInfo, struct input_event *ev)
{
    switch (ev->type) {
        case EV_REL:
            EvdevProcessRelativeMotionEvent(pInfo, ev);
            break;
        case EV_ABS:
            EvdevProcessAbsoluteMotionEvent(pInfo, ev);
            break;
        case EV_KEY:
            EvdevProcessKeyEvent(pInfo, ev);
            break;
        case EV_SYN:
            EvdevProcessSyncEvent(pInfo, ev);
            break;
    }
}

#undef ABS_X_VALUE
#undef ABS_Y_VALUE
#undef ABS_VALUE

/* just a magic number to reduce the number of reads */
#define NUM_EVENTS 16

static void
EvdevReadInput(InputInfoPtr pInfo)
{
    struct input_event ev[NUM_EVENTS];
    int i, len = sizeof(ev);
    EvdevPtr pEvdev = pInfo->private;

    while (len == sizeof(ev))
    {
        len = read(pInfo->fd, &ev, sizeof(ev));
        if (len <= 0)
        {
            if (errno == ENODEV) /* May happen after resume */
            {
                EvdevMBEmuFinalize(pInfo);
                xf86RemoveEnabledDevice(pInfo);
                close(pInfo->fd);
                pInfo->fd = -1;
                if (pEvdev->reopen_timer)
                {
                    pEvdev->reopen_left = pEvdev->reopen_attempts;
                    pEvdev->reopen_timer = TimerSet(pEvdev->reopen_timer, 0, 100, EvdevReopenTimer, pInfo);
                }
            } else if (errno != EAGAIN)
            {
                /* We use X_NONE here because it doesn't alloc */
                xf86MsgVerb(X_NONE, 0, "%s: Read error: %s\n", pInfo->name,
                        strerror(errno));
            }
            break;
        }

        /* The kernel promises that we always only read a complete
         * event, so len != sizeof ev is an error. */
        if (len % sizeof(ev[0])) {
            /* We use X_NONE here because it doesn't alloc */
            xf86MsgVerb(X_NONE, 0, "%s: Read error: %s\n", pInfo->name, strerror(errno));
            break;
        }

        for (i = 0; i < len/sizeof(ev[0]); i++)
            EvdevProcessEvent(pInfo, &ev[i]);
    }
}

#define TestBit(bit, array) ((array[(bit) / LONG_BITS]) & (1L << ((bit) % LONG_BITS)))

static void
EvdevPtrCtrlProc(DeviceIntPtr device, PtrCtrl *ctrl)
{
    /* Nothing to do, dix handles all settings */
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
static KeySym map[] = {
    /* 0x00 */  NoSymbol,       NoSymbol,
    /* 0x01 */  XK_Escape,      NoSymbol,
    /* 0x02 */  XK_1,           XK_exclam,
    /* 0x03 */  XK_2,           XK_at,
    /* 0x04 */  XK_3,           XK_numbersign,
    /* 0x05 */  XK_4,           XK_dollar,
    /* 0x06 */  XK_5,           XK_percent,
    /* 0x07 */  XK_6,           XK_asciicircum,
    /* 0x08 */  XK_7,           XK_ampersand,
    /* 0x09 */  XK_8,           XK_asterisk,
    /* 0x0a */  XK_9,           XK_parenleft,
    /* 0x0b */  XK_0,           XK_parenright,
    /* 0x0c */  XK_minus,       XK_underscore,
    /* 0x0d */  XK_equal,       XK_plus,
    /* 0x0e */  XK_BackSpace,   NoSymbol,
    /* 0x0f */  XK_Tab,         XK_ISO_Left_Tab,
    /* 0x10 */  XK_Q,           NoSymbol,
    /* 0x11 */  XK_W,           NoSymbol,
    /* 0x12 */  XK_E,           NoSymbol,
    /* 0x13 */  XK_R,           NoSymbol,
    /* 0x14 */  XK_T,           NoSymbol,
    /* 0x15 */  XK_Y,           NoSymbol,
    /* 0x16 */  XK_U,           NoSymbol,
    /* 0x17 */  XK_I,           NoSymbol,
    /* 0x18 */  XK_O,           NoSymbol,
    /* 0x19 */  XK_P,           NoSymbol,
    /* 0x1a */  XK_bracketleft, XK_braceleft,
    /* 0x1b */  XK_bracketright,XK_braceright,
    /* 0x1c */  XK_Return,      NoSymbol,
    /* 0x1d */  XK_Control_L,   NoSymbol,
    /* 0x1e */  XK_A,           NoSymbol,
    /* 0x1f */  XK_S,           NoSymbol,
    /* 0x20 */  XK_D,           NoSymbol,
    /* 0x21 */  XK_F,           NoSymbol,
    /* 0x22 */  XK_G,           NoSymbol,
    /* 0x23 */  XK_H,           NoSymbol,
    /* 0x24 */  XK_J,           NoSymbol,
    /* 0x25 */  XK_K,           NoSymbol,
    /* 0x26 */  XK_L,           NoSymbol,
    /* 0x27 */  XK_semicolon,   XK_colon,
    /* 0x28 */  XK_quoteright,  XK_quotedbl,
    /* 0x29 */  XK_quoteleft,	XK_asciitilde,
    /* 0x2a */  XK_Shift_L,     NoSymbol,
    /* 0x2b */  XK_backslash,   XK_bar,
    /* 0x2c */  XK_Z,           NoSymbol,
    /* 0x2d */  XK_X,           NoSymbol,
    /* 0x2e */  XK_C,           NoSymbol,
    /* 0x2f */  XK_V,           NoSymbol,
    /* 0x30 */  XK_B,           NoSymbol,
    /* 0x31 */  XK_N,           NoSymbol,
    /* 0x32 */  XK_M,           NoSymbol,
    /* 0x33 */  XK_comma,       XK_less,
    /* 0x34 */  XK_period,      XK_greater,
    /* 0x35 */  XK_slash,       XK_question,
    /* 0x36 */  XK_Shift_R,     NoSymbol,
    /* 0x37 */  XK_KP_Multiply, NoSymbol,
    /* 0x38 */  XK_Alt_L,	XK_Meta_L,
    /* 0x39 */  XK_space,       NoSymbol,
    /* 0x3a */  XK_Caps_Lock,   NoSymbol,
    /* 0x3b */  XK_F1,          NoSymbol,
    /* 0x3c */  XK_F2,          NoSymbol,
    /* 0x3d */  XK_F3,          NoSymbol,
    /* 0x3e */  XK_F4,          NoSymbol,
    /* 0x3f */  XK_F5,          NoSymbol,
    /* 0x40 */  XK_F6,          NoSymbol,
    /* 0x41 */  XK_F7,          NoSymbol,
    /* 0x42 */  XK_F8,          NoSymbol,
    /* 0x43 */  XK_F9,          NoSymbol,
    /* 0x44 */  XK_F10,         NoSymbol,
    /* 0x45 */  XK_Num_Lock,    NoSymbol,
    /* 0x46 */  XK_Scroll_Lock,	NoSymbol,
    /* These KP keys should have the KP_7 keysyms in the numlock
     * modifer... ? */
    /* 0x47 */  XK_KP_Home,	XK_KP_7,
    /* 0x48 */  XK_KP_Up,	XK_KP_8,
    /* 0x49 */  XK_KP_Prior,	XK_KP_9,
    /* 0x4a */  XK_KP_Subtract, NoSymbol,
    /* 0x4b */  XK_KP_Left,	XK_KP_4,
    /* 0x4c */  XK_KP_Begin,	XK_KP_5,
    /* 0x4d */  XK_KP_Right,	XK_KP_6,
    /* 0x4e */  XK_KP_Add,      NoSymbol,
    /* 0x4f */  XK_KP_End,	XK_KP_1,
    /* 0x50 */  XK_KP_Down,	XK_KP_2,
    /* 0x51 */  XK_KP_Next,	XK_KP_3,
    /* 0x52 */  XK_KP_Insert,	XK_KP_0,
    /* 0x53 */  XK_KP_Delete,	XK_KP_Decimal,
    /* 0x54 */  NoSymbol,	NoSymbol,
    /* 0x55 */  XK_F13,		NoSymbol,
    /* 0x56 */  XK_less,	XK_greater,
    /* 0x57 */  XK_F11,		NoSymbol,
    /* 0x58 */  XK_F12,		NoSymbol,
    /* 0x59 */  XK_F14,		NoSymbol,
    /* 0x5a */  XK_F15,		NoSymbol,
    /* 0x5b */  XK_F16,		NoSymbol,
    /* 0x5c */  XK_F17,		NoSymbol,
    /* 0x5d */  XK_F18,		NoSymbol,
    /* 0x5e */  XK_F19,		NoSymbol,
    /* 0x5f */  XK_F20,		NoSymbol,
    /* 0x60 */  XK_KP_Enter,	NoSymbol,
    /* 0x61 */  XK_Control_R,	NoSymbol,
    /* 0x62 */  XK_KP_Divide,	NoSymbol,
    /* 0x63 */  XK_Print,	XK_Sys_Req,
    /* 0x64 */  XK_Alt_R,	XK_Meta_R,
    /* 0x65 */  NoSymbol,	NoSymbol, /* KEY_LINEFEED */
    /* 0x66 */  XK_Home,	NoSymbol,
    /* 0x67 */  XK_Up,		NoSymbol,
    /* 0x68 */  XK_Prior,	NoSymbol,
    /* 0x69 */  XK_Left,	NoSymbol,
    /* 0x6a */  XK_Right,	NoSymbol,
    /* 0x6b */  XK_End,		NoSymbol,
    /* 0x6c */  XK_Down,	NoSymbol,
    /* 0x6d */  XK_Next,	NoSymbol,
    /* 0x6e */  XK_Insert,	NoSymbol,
    /* 0x6f */  XK_Delete,	NoSymbol,
    /* 0x70 */  NoSymbol,	NoSymbol, /* KEY_MACRO */
    /* 0x71 */  NoSymbol,	NoSymbol,
    /* 0x72 */  NoSymbol,	NoSymbol,
    /* 0x73 */  NoSymbol,	NoSymbol,
    /* 0x74 */  NoSymbol,	NoSymbol,
    /* 0x75 */  XK_KP_Equal,	NoSymbol,
    /* 0x76 */  NoSymbol,	NoSymbol,
    /* 0x77 */  NoSymbol,	NoSymbol,
    /* 0x78 */  XK_F21,		NoSymbol,
    /* 0x79 */  XK_F22,		NoSymbol,
    /* 0x7a */  XK_F23,		NoSymbol,
    /* 0x7b */  XK_F24,		NoSymbol,
    /* 0x7c */  XK_KP_Separator, NoSymbol,
    /* 0x7d */  XK_Meta_L,	NoSymbol,
    /* 0x7e */  XK_Meta_R,	NoSymbol,
    /* 0x7f */  XK_Multi_key,	NoSymbol,
    /* 0x80 */  NoSymbol,	NoSymbol,
    /* 0x81 */  NoSymbol,	NoSymbol,
    /* 0x82 */  NoSymbol,	NoSymbol,
    /* 0x83 */  NoSymbol,	NoSymbol,
    /* 0x84 */  NoSymbol,	NoSymbol,
    /* 0x85 */  NoSymbol,	NoSymbol,
    /* 0x86 */  NoSymbol,	NoSymbol,
    /* 0x87 */  NoSymbol,	NoSymbol,
    /* 0x88 */  NoSymbol,	NoSymbol,
    /* 0x89 */  NoSymbol,	NoSymbol,
    /* 0x8a */  NoSymbol,	NoSymbol,
    /* 0x8b */  NoSymbol,	NoSymbol,
    /* 0x8c */  NoSymbol,	NoSymbol,
    /* 0x8d */  NoSymbol,	NoSymbol,
    /* 0x8e */  NoSymbol,	NoSymbol,
    /* 0x8f */  NoSymbol,	NoSymbol,
    /* 0x90 */  NoSymbol,	NoSymbol,
    /* 0x91 */  NoSymbol,	NoSymbol,
    /* 0x92 */  NoSymbol,	NoSymbol,
    /* 0x93 */  NoSymbol,	NoSymbol,
    /* 0x94 */  NoSymbol,	NoSymbol,
    /* 0x95 */  NoSymbol,	NoSymbol,
    /* 0x96 */  NoSymbol,	NoSymbol,
    /* 0x97 */  NoSymbol,	NoSymbol,
    /* 0x98 */  NoSymbol,	NoSymbol,
    /* 0x99 */  NoSymbol,	NoSymbol,
    /* 0x9a */  NoSymbol,	NoSymbol,
    /* 0x9b */  NoSymbol,	NoSymbol,
    /* 0x9c */  NoSymbol,	NoSymbol,
    /* 0x9d */  NoSymbol,	NoSymbol,
    /* 0x9e */  NoSymbol,	NoSymbol,
    /* 0x9f */  NoSymbol,	NoSymbol,
    /* 0xa0 */  NoSymbol,	NoSymbol,
    /* 0xa1 */  NoSymbol,	NoSymbol,
    /* 0xa2 */  NoSymbol,	NoSymbol,
    /* 0xa3 */  NoSymbol,	NoSymbol,
    /* 0xa4 */  NoSymbol,	NoSymbol,
    /* 0xa5 */  NoSymbol,	NoSymbol,
    /* 0xa6 */  NoSymbol,	NoSymbol,
    /* 0xa7 */  NoSymbol,	NoSymbol,
    /* 0xa8 */  NoSymbol,	NoSymbol,
    /* 0xa9 */  NoSymbol,	NoSymbol,
    /* 0xaa */  NoSymbol,	NoSymbol,
    /* 0xab */  NoSymbol,	NoSymbol,
    /* 0xac */  NoSymbol,	NoSymbol,
    /* 0xad */  NoSymbol,	NoSymbol,
    /* 0xae */  NoSymbol,	NoSymbol,
    /* 0xaf */  NoSymbol,	NoSymbol,
    /* 0xb0 */  NoSymbol,	NoSymbol,
    /* 0xb1 */  NoSymbol,	NoSymbol,
    /* 0xb2 */  NoSymbol,	NoSymbol,
    /* 0xb3 */  NoSymbol,	NoSymbol,
    /* 0xb4 */  NoSymbol,	NoSymbol,
    /* 0xb5 */  NoSymbol,	NoSymbol,
    /* 0xb6 */  NoSymbol,	NoSymbol,
    /* 0xb7 */  NoSymbol,	NoSymbol,
    /* 0xb8 */  NoSymbol,	NoSymbol,
    /* 0xb9 */  NoSymbol,	NoSymbol,
    /* 0xba */  NoSymbol,	NoSymbol,
    /* 0xbb */  NoSymbol,	NoSymbol,
    /* 0xbc */  NoSymbol,	NoSymbol,
    /* 0xbd */  NoSymbol,	NoSymbol,
    /* 0xbe */  NoSymbol,	NoSymbol,
    /* 0xbf */  NoSymbol,	NoSymbol,
    /* 0xc0 */  NoSymbol,	NoSymbol,
    /* 0xc1 */  NoSymbol,	NoSymbol,
    /* 0xc2 */  NoSymbol,	NoSymbol,
    /* 0xc3 */  NoSymbol,	NoSymbol,
    /* 0xc4 */  NoSymbol,	NoSymbol,
    /* 0xc5 */  NoSymbol,	NoSymbol,
    /* 0xc6 */  NoSymbol,	NoSymbol,
    /* 0xc7 */  NoSymbol,	NoSymbol,
    /* 0xc8 */  NoSymbol,	NoSymbol,
    /* 0xc9 */  NoSymbol,	NoSymbol,
    /* 0xca */  NoSymbol,	NoSymbol,
    /* 0xcb */  NoSymbol,	NoSymbol,
    /* 0xcc */  NoSymbol,	NoSymbol,
    /* 0xcd */  NoSymbol,	NoSymbol,
    /* 0xce */  NoSymbol,	NoSymbol,
    /* 0xcf */  NoSymbol,	NoSymbol,
    /* 0xd0 */  NoSymbol,	NoSymbol,
    /* 0xd1 */  NoSymbol,	NoSymbol,
    /* 0xd2 */  NoSymbol,	NoSymbol,
    /* 0xd3 */  NoSymbol,	NoSymbol,
    /* 0xd4 */  NoSymbol,	NoSymbol,
    /* 0xd5 */  NoSymbol,	NoSymbol,
    /* 0xd6 */  NoSymbol,	NoSymbol,
    /* 0xd7 */  NoSymbol,	NoSymbol,
    /* 0xd8 */  NoSymbol,	NoSymbol,
    /* 0xd9 */  NoSymbol,	NoSymbol,
    /* 0xda */  NoSymbol,	NoSymbol,
    /* 0xdb */  NoSymbol,	NoSymbol,
    /* 0xdc */  NoSymbol,	NoSymbol,
    /* 0xdd */  NoSymbol,	NoSymbol,
    /* 0xde */  NoSymbol,	NoSymbol,
    /* 0xdf */  NoSymbol,	NoSymbol,
    /* 0xe0 */  NoSymbol,	NoSymbol,
    /* 0xe1 */  NoSymbol,	NoSymbol,
    /* 0xe2 */  NoSymbol,	NoSymbol,
    /* 0xe3 */  NoSymbol,	NoSymbol,
    /* 0xe4 */  NoSymbol,	NoSymbol,
    /* 0xe5 */  NoSymbol,	NoSymbol,
    /* 0xe6 */  NoSymbol,	NoSymbol,
    /* 0xe7 */  NoSymbol,	NoSymbol,
    /* 0xe8 */  NoSymbol,	NoSymbol,
    /* 0xe9 */  NoSymbol,	NoSymbol,
    /* 0xea */  NoSymbol,	NoSymbol,
    /* 0xeb */  NoSymbol,	NoSymbol,
    /* 0xec */  NoSymbol,	NoSymbol,
    /* 0xed */  NoSymbol,	NoSymbol,
    /* 0xee */  NoSymbol,	NoSymbol,
    /* 0xef */  NoSymbol,	NoSymbol,
    /* 0xf0 */  NoSymbol,	NoSymbol,
    /* 0xf1 */  NoSymbol,	NoSymbol,
    /* 0xf2 */  NoSymbol,	NoSymbol,
    /* 0xf3 */  NoSymbol,	NoSymbol,
    /* 0xf4 */  NoSymbol,	NoSymbol,
    /* 0xf5 */  NoSymbol,	NoSymbol,
    /* 0xf6 */  NoSymbol,	NoSymbol,
    /* 0xf7 */  NoSymbol,	NoSymbol,
};

static struct { KeySym keysym; CARD8 mask; } modifiers[] = {
    { XK_Shift_L,		ShiftMask },
    { XK_Shift_R,		ShiftMask },
    { XK_Control_L,		ControlMask },
    { XK_Control_R,		ControlMask },
    { XK_Caps_Lock,		LockMask },
    { XK_Alt_L,		AltMask },
    { XK_Alt_R,		AltMask },
    { XK_Meta_L,		Mod4Mask },
    { XK_Meta_R,		Mod4Mask },
    { XK_Num_Lock,		NumLockMask },
    { XK_Scroll_Lock,	ScrollLockMask },
    { XK_Mode_switch,	AltLangMask }
};

/* Server 1.6 and earlier */
static int
EvdevInitKeysyms(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;
    KeySymsRec keySyms;
    CARD8 modMap[MAP_LENGTH];
    KeySym sym;
    int i, j;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

     /* Compute the modifier map */
    memset(modMap, 0, sizeof modMap);

    for (i = 0; i < ArrayLength(map) / GLYPHS_PER_KEY; i++) {
        sym = map[i * GLYPHS_PER_KEY];
        for (j = 0; j < ArrayLength(modifiers); j++) {
            if (modifiers[j].keysym == sym)
                modMap[i + MIN_KEYCODE] = modifiers[j].mask;
        }
    }

    keySyms.map        = map;
    keySyms.mapWidth   = GLYPHS_PER_KEY;
    keySyms.minKeyCode = MIN_KEYCODE;
    keySyms.maxKeyCode = MIN_KEYCODE + ArrayLength(map) / GLYPHS_PER_KEY - 1;

    XkbSetRulesDflts(pEvdev->rmlvo.rules, pEvdev->rmlvo.model,
            pEvdev->rmlvo.layout, pEvdev->rmlvo.variant,
            pEvdev->rmlvo.options);
    if (!XkbInitKeyboardDeviceStruct(device, &pEvdev->xkbnames,
                &keySyms, modMap, NULL,
                EvdevKbdCtrl))
        return 0;

    return 1;
}
#endif

static void
EvdevKbdCtrl(DeviceIntPtr device, KeybdCtrl *ctrl)
{
    static struct { int xbit, code; } bits[] = {
        { CAPSFLAG,	LED_CAPSL },
        { NUMFLAG,	LED_NUML },
        { SCROLLFLAG,	LED_SCROLLL },
        { MODEFLAG,	LED_KANA },
        { COMPOSEFLAG,	LED_COMPOSE }
    };

    InputInfoPtr pInfo;
    struct input_event ev[ArrayLength(bits)];
    int i;

    memset(ev, 0, sizeof(ev));

    pInfo = device->public.devicePrivate;
    for (i = 0; i < ArrayLength(bits); i++) {
        ev[i].type = EV_LED;
        ev[i].code = bits[i].code;
        ev[i].value = (ctrl->leds & bits[i].xbit) > 0;
    }

    write(pInfo->fd, ev, sizeof ev);
}

static int
EvdevAddKeyClass(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    /* sorry, no rules change allowed for you */
    xf86ReplaceStrOption(pInfo->options, "xkb_rules", "evdev");
    SetXkbOption(pInfo, "xkb_rules", &pEvdev->rmlvo.rules);
    SetXkbOption(pInfo, "xkb_model", &pEvdev->rmlvo.model);
    if (!pEvdev->rmlvo.model)
        SetXkbOption(pInfo, "XkbModel", &pEvdev->rmlvo.model);
    SetXkbOption(pInfo, "xkb_layout", &pEvdev->rmlvo.layout);
    if (!pEvdev->rmlvo.layout)
        SetXkbOption(pInfo, "XkbLayout", &pEvdev->rmlvo.layout);
    SetXkbOption(pInfo, "xkb_variant", &pEvdev->rmlvo.variant);
    if (!pEvdev->rmlvo.variant)
        SetXkbOption(pInfo, "XkbVariant", &pEvdev->rmlvo.variant);
    SetXkbOption(pInfo, "xkb_options", &pEvdev->rmlvo.options);
    if (!pEvdev->rmlvo.options)
        SetXkbOption(pInfo, "XkbOptions", &pEvdev->rmlvo.options);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 5
    if (!InitKeyboardDeviceStruct(device, &pEvdev->rmlvo, NULL, EvdevKbdCtrl))
        return !Success;
#else
    if (!EvdevInitKeysyms(device))
        return !Success;

#endif

    pInfo->flags |= XI86_KEYBOARD_CAPABLE;

    return Success;
}

static int
EvdevAddAbsClass(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;
    int num_axes, axis, i = 0;
    Atom *atoms;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    if (!TestBit(EV_ABS, pEvdev->bitmask))
            return !Success;

    num_axes = CountBits(pEvdev->abs_bitmask, NLONGS(ABS_MAX));
    if (num_axes < 1)
        return !Success;
    pEvdev->num_vals = num_axes;
    memset(pEvdev->vals, 0, num_axes * sizeof(int));
    memset(pEvdev->old_vals, -1, num_axes * sizeof(int));
    atoms = xalloc(pEvdev->num_vals * sizeof(Atom));

    for (axis = ABS_X; axis <= ABS_MAX; axis++) {
        pEvdev->axis_map[axis] = -1;
        if (!TestBit(axis, pEvdev->abs_bitmask))
            continue;
        pEvdev->axis_map[axis] = i;
        i++;
    }

    EvdevInitAxesLabels(pEvdev, pEvdev->num_vals, atoms);

    if (!InitValuatorClassDeviceStruct(device, num_axes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                                       atoms,
#endif
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
                                       GetMotionHistory,
#endif
                                       GetMotionHistorySize(), Absolute))
        return !Success;

    for (axis = ABS_X; axis <= ABS_MAX; axis++) {
        int axnum = pEvdev->axis_map[axis];
        if (axnum == -1)
            continue;
        xf86InitValuatorAxisStruct(device, axnum,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                                   atoms[axnum],
#endif
                                   pEvdev->absinfo[axis].minimum,
                                   pEvdev->absinfo[axis].maximum,
                                   10000, 0, 10000);
        xf86InitValuatorDefaults(device, axnum);
        pEvdev->old_vals[axnum] = -1;
    }

    xfree(atoms);

    if (!InitPtrFeedbackClassDeviceStruct(device, EvdevPtrCtrlProc))
        return !Success;

    if ((TestBit(ABS_X, pEvdev->abs_bitmask) &&
         TestBit(ABS_Y, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_RX, pEvdev->abs_bitmask) &&
         TestBit(ABS_RY, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_HAT0X, pEvdev->abs_bitmask) &&
         TestBit(ABS_HAT0Y, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_HAT1X, pEvdev->abs_bitmask) &&
         TestBit(ABS_HAT1Y, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_HAT2X, pEvdev->abs_bitmask) &&
         TestBit(ABS_HAT2Y, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_HAT3X, pEvdev->abs_bitmask) &&
         TestBit(ABS_HAT3Y, pEvdev->abs_bitmask)) ||
        (TestBit(ABS_TILT_X, pEvdev->abs_bitmask) &&
         TestBit(ABS_TILT_Y, pEvdev->abs_bitmask)))
        pInfo->flags |= XI86_POINTER_CAPABLE;

    return Success;
}

static int
EvdevAddRelClass(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;
    int num_axes, axis, i = 0;
    Atom *atoms;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    if (!TestBit(EV_REL, pEvdev->bitmask))
        return !Success;

    num_axes = CountBits(pEvdev->rel_bitmask, NLONGS(REL_MAX));
    if (num_axes < 1)
        return !Success;

    /* Wheels are special, we post them as button events. So let's ignore them
     * in the axes list too */
    if (TestBit(REL_WHEEL, pEvdev->rel_bitmask))
        num_axes--;
    if (TestBit(REL_HWHEEL, pEvdev->rel_bitmask))
        num_axes--;
    if (TestBit(REL_DIAL, pEvdev->rel_bitmask))
        num_axes--;

    if (num_axes <= 0)
        return !Success;

    pEvdev->num_vals = num_axes;
    memset(pEvdev->vals, 0, num_axes * sizeof(int));
    atoms = xalloc(pEvdev->num_vals * sizeof(Atom));

    for (axis = REL_X; axis <= REL_MAX; axis++)
    {
        pEvdev->axis_map[axis] = -1;
        /* We don't post wheel events, so ignore them here too */
        if (axis == REL_WHEEL || axis == REL_HWHEEL || axis == REL_DIAL)
            continue;
        if (!TestBit(axis, pEvdev->rel_bitmask))
            continue;
        pEvdev->axis_map[axis] = i;
        i++;
    }

    EvdevInitAxesLabels(pEvdev, pEvdev->num_vals, atoms);

    if (!InitValuatorClassDeviceStruct(device, num_axes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                                       atoms,
#endif
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
                                       GetMotionHistory,
#endif
                                       GetMotionHistorySize(), Relative))
        return !Success;

    for (axis = REL_X; axis <= REL_MAX; axis++)
    {
        int axnum = pEvdev->axis_map[axis];

        if (axnum == -1)
            continue;
        xf86InitValuatorAxisStruct(device, axnum,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                atoms[axnum],
#endif
                -1, -1, 1, 0, 1);
        xf86InitValuatorDefaults(device, axnum);
    }

    xfree(atoms);

    if (!InitPtrFeedbackClassDeviceStruct(device, EvdevPtrCtrlProc))
        return !Success;

    pInfo->flags |= XI86_POINTER_CAPABLE;

    return Success;
}

static int
EvdevAddButtonClass(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;
    Atom *labels;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    labels = xalloc(pEvdev->num_buttons * sizeof(Atom));
    EvdevInitButtonLabels(pEvdev, pEvdev->num_buttons, labels);

    if (!InitButtonClassDeviceStruct(device, pEvdev->num_buttons,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                                     labels,
#endif
                                     pEvdev->btnmap))
        return !Success;

    xfree(labels);
    return Success;
}

/**
 * Init the button mapping for the device. By default, this is a 1:1 mapping,
 * i.e. Button 1 maps to Button 1, Button 2 to 2, etc.
 *
 * If a mapping has been specified, the mapping is the default, with the
 * user-defined ones overwriting the defaults.
 * i.e. a user-defined mapping of "3 2 1" results in a mapping of 3 2 1 4 5 6 ...
 *
 * Invalid button mappings revert to the default.
 *
 * Note that index 0 is unused, button 0 does not exist.
 * This mapping is initialised for all devices, but only applied if the device
 * has buttons (in EvdevAddButtonClass).
 */
static void
EvdevInitButtonMapping(InputInfoPtr pInfo)
{
    int         i, nbuttons     = 1;
    char       *mapping         = NULL;
    EvdevPtr    pEvdev          = pInfo->private;

    /* Check for user-defined button mapping */
    if ((mapping = xf86CheckStrOption(pInfo->options, "ButtonMapping", NULL)))
    {
        char    *s  = " ";
        int     btn = 0;

        xf86Msg(X_CONFIG, "%s: ButtonMapping '%s'\n", pInfo->name, mapping);
        while (s && *s != '\0' && nbuttons < EVDEV_MAXBUTTONS)
        {
            btn = strtol(mapping, &s, 10);

            if (s == mapping || btn < 0 || btn > EVDEV_MAXBUTTONS)
            {
                xf86Msg(X_ERROR,
                        "%s: ... Invalid button mapping. Using defaults\n",
                        pInfo->name);
                nbuttons = 1; /* ensure defaults start at 1 */
                break;
            }

            pEvdev->btnmap[nbuttons++] = btn;
            mapping = s;
        }
    }

    for (i = nbuttons; i < ArrayLength(pEvdev->btnmap); i++)
        pEvdev->btnmap[i] = i;

}

static void
EvdevInitAnyClass(DeviceIntPtr device, EvdevPtr pEvdev)
{
    if (pEvdev->flags & EVDEV_RELATIVE_EVENTS &&
        EvdevAddRelClass(device) == Success)
        xf86Msg(X_INFO, "%s: initialized for relative axes.\n", device->name);
    if (pEvdev->flags & EVDEV_ABSOLUTE_EVENTS &&
        EvdevAddAbsClass(device) == Success)
        xf86Msg(X_INFO, "%s: initialized for absolute axes.\n", device->name);
}

static void
EvdevInitAbsClass(DeviceIntPtr device, EvdevPtr pEvdev)
{
    if (EvdevAddAbsClass(device) == Success) {

        xf86Msg(X_INFO,"%s: initialized for absolute axes.\n", device->name);

    } else {

        xf86Msg(X_ERROR,"%s: failed to initialize for absolute axes.\n",
                device->name);

        pEvdev->flags &= ~EVDEV_ABSOLUTE_EVENTS;

    }
}

static void
EvdevInitRelClass(DeviceIntPtr device, EvdevPtr pEvdev)
{
    int has_abs_axes = pEvdev->flags & EVDEV_ABSOLUTE_EVENTS;

    if (EvdevAddRelClass(device) == Success) {

        xf86Msg(X_INFO,"%s: initialized for relative axes.\n", device->name);

        if (has_abs_axes) {

            xf86Msg(X_WARNING,"%s: ignoring absolute axes.\n", device->name);
            pEvdev->flags &= ~EVDEV_ABSOLUTE_EVENTS;
        }

    } else {

        xf86Msg(X_ERROR,"%s: failed to initialize for relative axes.\n",
                device->name);

        pEvdev->flags &= ~EVDEV_RELATIVE_EVENTS;

        if (has_abs_axes)
            EvdevInitAbsClass(device, pEvdev);
    }
}

static void
EvdevInitTouchDevice(DeviceIntPtr device, EvdevPtr pEvdev)
{
    if (pEvdev->flags & EVDEV_RELATIVE_EVENTS) {

        xf86Msg(X_WARNING,"%s: touchpads, tablets and touchscreens ignore "
                "relative axes.\n", device->name);

        pEvdev->flags &= ~EVDEV_RELATIVE_EVENTS;
    }

    EvdevInitAbsClass(device, pEvdev);
}

static int
EvdevInit(DeviceIntPtr device)
{
    int i;
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    /* clear all axis_map entries */
    for(i = 0; i < max(ABS_CNT,REL_CNT); i++)
      pEvdev->axis_map[i]=-1;

    if (pEvdev->flags & EVDEV_KEYBOARD_EVENTS)
	EvdevAddKeyClass(device);
    if (pEvdev->flags & EVDEV_BUTTON_EVENTS)
	EvdevAddButtonClass(device);

    /* We don't allow relative and absolute axes on the same device. The
     * reason is that some devices (MS Optical Desktop 2000) register both
     * rel and abs axes for x/y.
     *
     * The abs axes register min/max; this min/max then also applies to the
     * relative device (the mouse) and caps it at 0..255 for both axes.
     * So, unless you have a small screen, you won't be enjoying it much;
     * consequently, absolute axes are generally ignored.
     *
     * However, currenly only a device with absolute axes can be registered
     * as a touch{pad,screen}. Thus, given such a device, absolute axes are
     * used and relative axes are ignored.
     */

    if (pEvdev->flags & (EVDEV_UNIGNORE_RELATIVE | EVDEV_UNIGNORE_ABSOLUTE))
        EvdevInitAnyClass(device, pEvdev);
    else if (pEvdev->flags & (EVDEV_TOUCHPAD | EVDEV_TOUCHSCREEN | EVDEV_TABLET))
        EvdevInitTouchDevice(device, pEvdev);
    else if (pEvdev->flags & EVDEV_RELATIVE_EVENTS)
        EvdevInitRelClass(device, pEvdev);
    else if (pEvdev->flags & EVDEV_ABSOLUTE_EVENTS)
        EvdevInitAbsClass(device, pEvdev);

#ifdef HAVE_PROPERTIES
    /* We drop the return value, the only time we ever want the handlers to
     * unregister is when the device dies. In which case we don't have to
     * unregister anyway */
    EvdevInitProperty(device);
    XIRegisterPropertyHandler(device, EvdevSetProperty, NULL, NULL);
    EvdevMBEmuInitProperty(device);
    EvdevWheelEmuInitProperty(device);
    EvdevDragLockInitProperty(device);
#endif

    return Success;
}

/**
 * Init all extras (wheel emulation, etc.) and grab the device.
 *
 * Coming from a resume, the grab may fail with ENODEV. In this case, we set a
 * timer to wake up and try to reopen the device later.
 */
static int
EvdevOn(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;
    int rc = 0;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    if (pInfo->fd != -1 && pEvdev->grabDevice &&
        (rc = ioctl(pInfo->fd, EVIOCGRAB, (void *)1)))
    {
        xf86Msg(X_WARNING, "%s: Grab failed (%s)\n", pInfo->name,
                strerror(errno));

        /* ENODEV - device has disappeared after resume */
        if (rc && errno == ENODEV)
        {
            close(pInfo->fd);
            pInfo->fd = -1;
        }
    }

    if (pInfo->fd == -1)
    {
        pEvdev->reopen_left = pEvdev->reopen_attempts;
        pEvdev->reopen_timer = TimerSet(pEvdev->reopen_timer, 0, 100, EvdevReopenTimer, pInfo);
    } else
    {
        pEvdev->min_maj = EvdevGetMajorMinor(pInfo);
        if (EvdevIsDuplicate(pInfo))
        {
            xf86Msg(X_WARNING, "%s: Refusing to enable duplicate device.\n",
                    pInfo->name);
            return !Success;
        }

        pEvdev->reopen_timer = TimerSet(pEvdev->reopen_timer, 0, 0, NULL, NULL);

        xf86FlushInput(pInfo->fd);
        xf86AddEnabledDevice(pInfo);
        EvdevMBEmuOn(pInfo);
        pEvdev->flags |= EVDEV_INITIALIZED;
        device->public.on = TRUE;
    }

    return Success;
}


static int
EvdevProc(DeviceIntPtr device, int what)
{
    InputInfoPtr pInfo;
    EvdevPtr pEvdev;

    pInfo = device->public.devicePrivate;
    pEvdev = pInfo->private;

    switch (what)
    {
    case DEVICE_INIT:
	return EvdevInit(device);

    case DEVICE_ON:
        return EvdevOn(device);

    case DEVICE_OFF:
        if (pEvdev->flags & EVDEV_INITIALIZED)
            EvdevMBEmuFinalize(pInfo);
        if (pInfo->fd != -1)
        {
            if (pEvdev->grabDevice && ioctl(pInfo->fd, EVIOCGRAB, (void *)0))
                xf86Msg(X_WARNING, "%s: Release failed (%s)\n", pInfo->name,
                        strerror(errno));
            xf86RemoveEnabledDevice(pInfo);
            close(pInfo->fd);
            pInfo->fd = -1;
        }
        pEvdev->min_maj = 0;
        pEvdev->flags &= ~EVDEV_INITIALIZED;
	device->public.on = FALSE;
        if (pEvdev->reopen_timer)
        {
            TimerFree(pEvdev->reopen_timer);
            pEvdev->reopen_timer = NULL;
        }
	break;

    case DEVICE_CLOSE:
	xf86Msg(X_INFO, "%s: Close\n", pInfo->name);
        if (pInfo->fd != -1) {
            close(pInfo->fd);
            pInfo->fd = -1;
        }
        EvdevRemoveDevice(pInfo);
        pEvdev->min_maj = 0;
	break;
    }

    return Success;
}

/**
 * Get as much information as we can from the fd and cache it.
 * If compare is True, then the information retrieved will be compared to the
 * one already cached. If the information does not match, then this function
 * returns an error.
 *
 * @return Success if the information was cached, or !Success otherwise.
 */
static int
EvdevCacheCompare(InputInfoPtr pInfo, BOOL compare)
{
    EvdevPtr pEvdev = pInfo->private;
    size_t len;
    int i;

    char name[1024]                  = {0};
    unsigned long bitmask[NLONGS(EV_CNT)]      = {0};
    unsigned long key_bitmask[NLONGS(KEY_CNT)] = {0};
    unsigned long rel_bitmask[NLONGS(REL_CNT)] = {0};
    unsigned long abs_bitmask[NLONGS(ABS_CNT)] = {0};
    unsigned long led_bitmask[NLONGS(LED_CNT)] = {0};

    if (ioctl(pInfo->fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) {
        xf86Msg(X_ERROR, "ioctl EVIOCGNAME failed: %s\n", strerror(errno));
        goto error;
    }

    if (!compare) {
        strcpy(pEvdev->name, name);
    } else if (strcmp(pEvdev->name, name)) {
        xf86Msg(X_ERROR, "%s: device name changed: %s != %s\n",
                pInfo->name, pEvdev->name, name);
        goto error;
    }

    len = ioctl(pInfo->fd, EVIOCGBIT(0, sizeof(bitmask)), bitmask);
    if (len < 0) {
        xf86Msg(X_ERROR, "%s: ioctl EVIOCGBIT failed: %s\n",
                pInfo->name, strerror(errno));
        goto error;
    }

    if (!compare) {
        memcpy(pEvdev->bitmask, bitmask, len);
    } else if (memcmp(pEvdev->bitmask, bitmask, len)) {
        xf86Msg(X_ERROR, "%s: device bitmask has changed\n", pInfo->name);
        goto error;
    }

    len = ioctl(pInfo->fd, EVIOCGBIT(EV_REL, sizeof(rel_bitmask)), rel_bitmask);
    if (len < 0) {
        xf86Msg(X_ERROR, "%s: ioctl EVIOCGBIT failed: %s\n",
                pInfo->name, strerror(errno));
        goto error;
    }

    if (!compare) {
        memcpy(pEvdev->rel_bitmask, rel_bitmask, len);
    } else if (memcmp(pEvdev->rel_bitmask, rel_bitmask, len)) {
        xf86Msg(X_ERROR, "%s: device rel_bitmask has changed\n", pInfo->name);
        goto error;
    }

    len = ioctl(pInfo->fd, EVIOCGBIT(EV_ABS, sizeof(abs_bitmask)), abs_bitmask);
    if (len < 0) {
        xf86Msg(X_ERROR, "%s: ioctl EVIOCGBIT failed: %s\n",
                pInfo->name, strerror(errno));
        goto error;
    }

    if (!compare) {
        memcpy(pEvdev->abs_bitmask, abs_bitmask, len);
    } else if (memcmp(pEvdev->abs_bitmask, abs_bitmask, len)) {
        xf86Msg(X_ERROR, "%s: device abs_bitmask has changed\n", pInfo->name);
        goto error;
    }

    len = ioctl(pInfo->fd, EVIOCGBIT(EV_LED, sizeof(led_bitmask)), led_bitmask);
    if (len < 0) {
        xf86Msg(X_ERROR, "%s: ioctl EVIOCGBIT failed: %s\n",
                pInfo->name, strerror(errno));
        goto error;
    }

    if (!compare) {
        memcpy(pEvdev->led_bitmask, led_bitmask, len);
    } else if (memcmp(pEvdev->led_bitmask, led_bitmask, len)) {
        xf86Msg(X_ERROR, "%s: device led_bitmask has changed\n", pInfo->name);
        goto error;
    }

    /*
     * Do not try to validate absinfo data since it is not expected
     * to be static, always refresh it in evdev structure.
     */
    for (i = ABS_X; i <= ABS_MAX; i++) {
        if (TestBit(i, abs_bitmask)) {
            len = ioctl(pInfo->fd, EVIOCGABS(i), &pEvdev->absinfo[i]);
            if (len < 0) {
                xf86Msg(X_ERROR, "%s: ioctl EVIOCGABSi(%d) failed: %s\n",
                        pInfo->name, i, strerror(errno));
                goto error;
            }
        }
    }

    len = ioctl(pInfo->fd, EVIOCGBIT(EV_KEY, sizeof(key_bitmask)), key_bitmask);
    if (len < 0) {
        xf86Msg(X_ERROR, "%s: ioctl EVIOCGBIT failed: %s\n",
                pInfo->name, strerror(errno));
        goto error;
    }

    if (compare) {
        /*
         * Keys are special as user can adjust keymap at any time (on
         * devices that support EVIOCSKEYCODE. However we do not expect
         * buttons reserved for mice/tablets/digitizers and so on to
         * appear/disappear so we will check only those in
         * [BTN_MISC, KEY_OK) range.
         */
        size_t start_word = BTN_MISC / LONG_BITS;
        size_t start_byte = start_word * sizeof(unsigned long);
        size_t end_word = KEY_OK / LONG_BITS;
        size_t end_byte = end_word * sizeof(unsigned long);

        if (len >= start_byte &&
            memcmp(&pEvdev->key_bitmask[start_word], &key_bitmask[start_word],
                   min(len, end_byte) - start_byte + 1)) {
            xf86Msg(X_ERROR, "%s: device key_bitmask has changed\n", pInfo->name);
            goto error;
        }
    }

    /* Copy the data so we have reasonably up-to-date info */
    memcpy(pEvdev->key_bitmask, key_bitmask, len);

    return Success;

error:
    return !Success;

}

static int
EvdevProbe(InputInfoPtr pInfo)
{
    int i, has_rel_axes, has_abs_axes, has_keys, num_buttons, has_scroll;
    int kernel24 = 0;
    int ignore_abs = 0, ignore_rel = 0;
    EvdevPtr pEvdev = pInfo->private;

    if (pEvdev->grabDevice && ioctl(pInfo->fd, EVIOCGRAB, (void *)1)) {
        if (errno == EINVAL) {
            /* keyboards are unsafe in 2.4 */
            kernel24 = 1;
            pEvdev->grabDevice = 0;
        } else {
            xf86Msg(X_ERROR, "Grab failed. Device already configured?\n");
            return 1;
        }
    } else if (pEvdev->grabDevice) {
        ioctl(pInfo->fd, EVIOCGRAB, (void *)0);
    }

    /* Trinary state for ignoring axes:
       - unset: do the normal thing.
       - TRUE: explicitly ignore them.
       - FALSE: unignore axes, use them at all cost if they're present.
     */
    if (xf86FindOption(pInfo->options, "IgnoreRelativeAxes"))
    {
        if (xf86SetBoolOption(pInfo->options, "IgnoreRelativeAxes", FALSE))
            ignore_rel = TRUE;
        else
            pEvdev->flags |= EVDEV_UNIGNORE_RELATIVE;

    }
    if (xf86FindOption(pInfo->options, "IgnoreAbsoluteAxes"))
    {
        if (xf86SetBoolOption(pInfo->options, "IgnoreAbsoluteAxes", FALSE))
           ignore_abs = TRUE;
        else
            pEvdev->flags |= EVDEV_UNIGNORE_ABSOLUTE;
    }

    has_rel_axes = FALSE;
    has_abs_axes = FALSE;
    has_keys = FALSE;
    has_scroll = FALSE;
    num_buttons = 0;

    /* count all buttons */
    for (i = BTN_MISC; i < BTN_JOYSTICK; i++)
    {
        int mapping = 0;
        if (TestBit(i, pEvdev->key_bitmask))
        {
            mapping = EvdevUtilButtonEventToButtonNumber(pEvdev, i);
            if (mapping > num_buttons)
                num_buttons = mapping;
        }
    }

    if (num_buttons)
    {
        pEvdev->flags |= EVDEV_BUTTON_EVENTS;
        pEvdev->num_buttons = num_buttons;
        xf86Msg(X_INFO, "%s: Found %d mouse buttons\n", pInfo->name,
                num_buttons);
    }

    for (i = 0; i < REL_MAX; i++) {
        if (TestBit(i, pEvdev->rel_bitmask)) {
            has_rel_axes = TRUE;
            break;
        }
    }

    if (has_rel_axes) {
        if (TestBit(REL_WHEEL, pEvdev->rel_bitmask) ||
            TestBit(REL_HWHEEL, pEvdev->rel_bitmask) ||
            TestBit(REL_DIAL, pEvdev->rel_bitmask)) {
            xf86Msg(X_INFO, "%s: Found scroll wheel(s)\n", pInfo->name);
            has_scroll = TRUE;
            if (!num_buttons)
                xf86Msg(X_INFO, "%s: Forcing buttons for scroll wheel(s)\n",
                        pInfo->name);
            num_buttons = (num_buttons < 3) ? 7 : num_buttons + 4;
            pEvdev->num_buttons = num_buttons;
        }

        if (!ignore_rel)
        {
            xf86Msg(X_INFO, "%s: Found relative axes\n", pInfo->name);
            pEvdev->flags |= EVDEV_RELATIVE_EVENTS;

            if (TestBit(REL_X, pEvdev->rel_bitmask) &&
                TestBit(REL_Y, pEvdev->rel_bitmask)) {
                xf86Msg(X_INFO, "%s: Found x and y relative axes\n", pInfo->name);
            }
        } else {
            xf86Msg(X_INFO, "%s: Relative axes present but ignored.\n", pInfo->name);
            has_rel_axes = FALSE;
        }
    }

    for (i = 0; i < ABS_MAX; i++) {
        if (TestBit(i, pEvdev->abs_bitmask)) {
            has_abs_axes = TRUE;
            break;
        }
    }

    if (ignore_abs && has_abs_axes)
    {
        xf86Msg(X_INFO, "%s: Absolute axes present but ignored.\n", pInfo->name);
        has_abs_axes = FALSE;
    } else if (has_abs_axes) {
        xf86Msg(X_INFO, "%s: Found absolute axes\n", pInfo->name);
        pEvdev->flags |= EVDEV_ABSOLUTE_EVENTS;

        if ((TestBit(ABS_X, pEvdev->abs_bitmask) &&
             TestBit(ABS_Y, pEvdev->abs_bitmask))) {
            xf86Msg(X_INFO, "%s: Found x and y absolute axes\n", pInfo->name);
            if (TestBit(BTN_TOOL_PEN, pEvdev->key_bitmask))
            {
                xf86Msg(X_INFO, "%s: Found absolute tablet.\n", pInfo->name);
                pEvdev->flags |= EVDEV_TABLET;
                if (!pEvdev->num_buttons)
                {
                    pEvdev->num_buttons = 7; /* LMR + scroll wheels */
                    pEvdev->flags |= EVDEV_BUTTON_EVENTS;
                }
            } else if (TestBit(ABS_PRESSURE, pEvdev->abs_bitmask) ||
                TestBit(BTN_TOUCH, pEvdev->key_bitmask)) {
                if (num_buttons || TestBit(BTN_TOOL_FINGER, pEvdev->key_bitmask)) {
                    xf86Msg(X_INFO, "%s: Found absolute touchpad.\n", pInfo->name);
                    pEvdev->flags |= EVDEV_TOUCHPAD;
                    memset(pEvdev->old_vals, -1, sizeof(int) * pEvdev->num_vals);
                } else {
                    xf86Msg(X_INFO, "%s: Found absolute touchscreen\n", pInfo->name);
                    pEvdev->flags |= EVDEV_TOUCHSCREEN;
                    pEvdev->flags |= EVDEV_BUTTON_EVENTS;
                }
            }
        }
    }

    for (i = 0; i < BTN_MISC; i++) {
        if (TestBit(i, pEvdev->key_bitmask)) {
            xf86Msg(X_INFO, "%s: Found keys\n", pInfo->name);
            pEvdev->flags |= EVDEV_KEYBOARD_EVENTS;
            has_keys = TRUE;
            break;
        }
    }

    if (has_rel_axes || has_abs_axes || num_buttons) {
        pInfo->flags |= XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS |
                        XI86_CONFIGURED;
	if (pEvdev->flags & EVDEV_TOUCHPAD) {
	    xf86Msg(X_INFO, "%s: Configuring as touchpad\n", pInfo->name);
	    pInfo->type_name = XI_TOUCHPAD;
	} else if (pEvdev->flags & EVDEV_TABLET) {
	    xf86Msg(X_INFO, "%s: Configuring as tablet\n", pInfo->name);
	    pInfo->type_name = XI_TABLET;
        } else if (pEvdev->flags & EVDEV_TOUCHSCREEN) {
            xf86Msg(X_INFO, "%s: Configuring as touchscreen\n", pInfo->name);
            pInfo->type_name = XI_TOUCHSCREEN;
	} else {
	    xf86Msg(X_INFO, "%s: Configuring as mouse\n", pInfo->name);
	    pInfo->type_name = XI_MOUSE;
	}
    }

    if (has_keys) {
        if (kernel24) {
            xf86Msg(X_INFO, "%s: Kernel < 2.6 is too old, ignoring keyboard\n",
                    pInfo->name);
        } else {
            xf86Msg(X_INFO, "%s: Configuring as keyboard\n", pInfo->name);
            pInfo->flags |= XI86_KEYBOARD_CAPABLE | XI86_CONFIGURED;
	    pInfo->type_name = XI_KEYBOARD;
        }
    }

    if (has_scroll && (pInfo->flags & XI86_CONFIGURED) &&
        (pInfo->flags & XI86_POINTER_CAPABLE) == 0)
    {
        xf86Msg(X_INFO, "%s: Adding scrollwheel support\n", pInfo->name);
        pInfo->flags  |= XI86_POINTER_CAPABLE;
        pEvdev->flags |= EVDEV_BUTTON_EVENTS;
        pEvdev->flags |= EVDEV_RELATIVE_EVENTS;
    }

    if ((pInfo->flags & XI86_CONFIGURED) == 0) {
        xf86Msg(X_WARNING, "%s: Don't know how to use device\n",
		pInfo->name);
        return 1;
    }

    return 0;
}

static void
EvdevSetCalibration(InputInfoPtr pInfo, int num_calibration, int calibration[4])
{
    EvdevPtr pEvdev = pInfo->private;

    if (num_calibration == 0) {
        pEvdev->flags &= ~EVDEV_CALIBRATED;
        pEvdev->calibration.min_x = 0;
        pEvdev->calibration.max_x = 0;
        pEvdev->calibration.min_y = 0;
        pEvdev->calibration.max_y = 0;
    } else if (num_calibration == 4) {
        pEvdev->flags |= EVDEV_CALIBRATED;
        pEvdev->calibration.min_x = calibration[0];
        pEvdev->calibration.max_x = calibration[1];
        pEvdev->calibration.min_y = calibration[2];
        pEvdev->calibration.max_y = calibration[3];
    }
}

static InputInfoPtr
EvdevPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr pInfo;
    const char *device, *str;
    int num_calibration = 0, calibration[4] = { 0, 0, 0, 0 };
    EvdevPtr pEvdev;

    if (!(pInfo = xf86AllocateInput(drv, 0)))
	return NULL;

    /* Initialise the InputInfoRec. */
    pInfo->name = dev->identifier;
    pInfo->flags = 0;
    pInfo->type_name = "UNKNOWN";
    pInfo->device_control = EvdevProc;
    pInfo->read_input = EvdevReadInput;
    pInfo->history_size = 0;
    pInfo->control_proc = NULL;
    pInfo->close_proc = NULL;
    pInfo->switch_mode = NULL;
    pInfo->conversion_proc = NULL;
    pInfo->reverse_conversion_proc = NULL;
    pInfo->dev = NULL;
    pInfo->private_flags = 0;
    pInfo->always_core_feedback = NULL;
    pInfo->conf_idev = dev;

    if (!(pEvdev = xcalloc(sizeof(EvdevRec), 1)))
        return pInfo;

    pInfo->private = pEvdev;

    xf86CollectInputOptions(pInfo, evdevDefaults, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options);

    /*
     * We initialize pEvdev->tool to 1 so that device that doesn't use
     * proximity will still report events.
     */
    pEvdev->tool = 1;

    device = xf86CheckStrOption(dev->commonOptions, "Device", NULL);
    if (!device) {
        xf86Msg(X_ERROR, "%s: No device specified.\n", pInfo->name);
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    pEvdev->device = device;

    xf86Msg(X_CONFIG, "%s: Device: \"%s\"\n", pInfo->name, device);
    do {
        pInfo->fd = open(device, O_RDWR | O_NONBLOCK, 0);
    } while (pInfo->fd < 0 && errno == EINTR);

    if (pInfo->fd < 0) {
        xf86Msg(X_ERROR, "Unable to open evdev device \"%s\".\n", device);
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    /* Check major/minor of device node to avoid adding duplicate devices. */
    pEvdev->min_maj = EvdevGetMajorMinor(pInfo);
    if (EvdevIsDuplicate(pInfo))
    {
        xf86Msg(X_WARNING, "%s: device file already in use. Ignoring.\n",
                pInfo->name);
        close(pInfo->fd);
        xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    pEvdev->reopen_attempts = xf86SetIntOption(pInfo->options, "ReopenAttempts", 10);
    pEvdev->invert_x = xf86SetBoolOption(pInfo->options, "InvertX", FALSE);
    pEvdev->invert_y = xf86SetBoolOption(pInfo->options, "InvertY", FALSE);
    pEvdev->swap_axes = xf86SetBoolOption(pInfo->options, "SwapAxes", FALSE);

    str = xf86CheckStrOption(pInfo->options, "Calibration", NULL);
    if (str) {
        num_calibration = sscanf(str, "%d %d %d %d",
                                 &calibration[0], &calibration[1],
                                 &calibration[2], &calibration[3]);
        if (num_calibration == 4)
            EvdevSetCalibration(pInfo, num_calibration, calibration);
        else
            xf86Msg(X_ERROR,
                    "%s: Insufficient calibration factors (%d). Ignoring calibration\n",
                    pInfo->name, num_calibration);
    }

    /* Grabbing the event device stops in-kernel event forwarding. In other
       words, it disables rfkill and the "Macintosh mouse button emulation".
       Note that this needs a server that sets the console to RAW mode. */
    pEvdev->grabDevice = xf86CheckBoolOption(dev->commonOptions, "GrabDevice", 0);

    EvdevInitButtonMapping(pInfo);

    if (EvdevCacheCompare(pInfo, FALSE) ||
        EvdevProbe(pInfo)) {
	close(pInfo->fd);
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    EvdevAddDevice(pInfo);

    if (pEvdev->flags & EVDEV_BUTTON_EVENTS)
    {
        EvdevMBEmuPreInit(pInfo);
        EvdevWheelEmuPreInit(pInfo);
        EvdevDragLockPreInit(pInfo);
    }

    return pInfo;
}

_X_EXPORT InputDriverRec EVDEV = {
    1,
    "evdev",
    NULL,
    EvdevPreInit,
    NULL,
    NULL,
    0
};

static void
EvdevUnplug(pointer	p)
{
}

static pointer
EvdevPlug(pointer	module,
          pointer	options,
          int		*errmaj,
          int		*errmin)
{
    xf86AddInputDriver(&EVDEV, module, 0);
    return module;
}

static XF86ModuleVersionInfo EvdevVersionRec =
{
    "evdev",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData evdevModuleData =
{
    &EvdevVersionRec,
    EvdevPlug,
    EvdevUnplug
};


/* Return an index value for a given button event code
 * returns 0 on non-button event.
 */
unsigned int
EvdevUtilButtonEventToButtonNumber(EvdevPtr pEvdev, int code)
{
    unsigned int button = 0;

    switch(code) {
    case BTN_LEFT:
	button = 1;
	break;

    case BTN_RIGHT:
	button = 3;
	break;

    case BTN_MIDDLE:
	button = 2;
	break;

        /* Treat BTN_[0-2] as LMR buttons on devices that do not advertise
           BTN_LEFT, BTN_MIDDLE, BTN_RIGHT.
           Otherwise, treat BTN_[0+n] as button 5+n.
           XXX: This causes duplicate mappings for BTN_0 + n and BTN_SIDE + n
         */
    case BTN_0:
        button = (TestBit(BTN_LEFT, pEvdev->key_bitmask)) ?  8 : 1;
        break;
    case BTN_1:
        button = (TestBit(BTN_MIDDLE, pEvdev->key_bitmask)) ?  9 : 2;
        break;
    case BTN_2:
        button = (TestBit(BTN_RIGHT, pEvdev->key_bitmask)) ?  10 : 3;
        break;

        /* FIXME: BTN_3.. and BTN_SIDE.. have the same button mapping */
    case BTN_3:
    case BTN_4:
    case BTN_5:
    case BTN_6:
    case BTN_7:
    case BTN_8:
    case BTN_9:
	button = (code - BTN_0 + 5);
        break;

    case BTN_SIDE:
    case BTN_EXTRA:
    case BTN_FORWARD:
    case BTN_BACK:
    case BTN_TASK:
	button = (code - BTN_LEFT + 5);
	break;

    default:
	if ((code > BTN_TASK) && (code < KEY_OK)) {
	    if (code < BTN_JOYSTICK) {
                if (code < BTN_MOUSE)
                    button = (code - BTN_0 + 5);
                else
                    button = (code - BTN_LEFT + 5);
            }
	}
    }

    if (button > EVDEV_MAXBUTTONS)
	return 0;

    return button;
}

#ifdef HAVE_PROPERTIES
#ifdef HAVE_LABELS
/* Aligned with linux/input.h.
   Note that there are holes in the ABS_ range, these are simply replaced with
   MISC here */
static char* abs_labels[] = {
    AXIS_LABEL_PROP_ABS_X,              /* 0x00 */
    AXIS_LABEL_PROP_ABS_Y,              /* 0x01 */
    AXIS_LABEL_PROP_ABS_Z,              /* 0x02 */
    AXIS_LABEL_PROP_ABS_RX,             /* 0x03 */
    AXIS_LABEL_PROP_ABS_RY,             /* 0x04 */
    AXIS_LABEL_PROP_ABS_RZ,             /* 0x05 */
    AXIS_LABEL_PROP_ABS_THROTTLE,       /* 0x06 */
    AXIS_LABEL_PROP_ABS_RUDDER,         /* 0x07 */
    AXIS_LABEL_PROP_ABS_WHEEL,          /* 0x08 */
    AXIS_LABEL_PROP_ABS_GAS,            /* 0x09 */
    AXIS_LABEL_PROP_ABS_BRAKE,          /* 0x0a */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_HAT0X,          /* 0x10 */
    AXIS_LABEL_PROP_ABS_HAT0Y,          /* 0x11 */
    AXIS_LABEL_PROP_ABS_HAT1X,          /* 0x12 */
    AXIS_LABEL_PROP_ABS_HAT1Y,          /* 0x13 */
    AXIS_LABEL_PROP_ABS_HAT2X,          /* 0x14 */
    AXIS_LABEL_PROP_ABS_HAT2Y,          /* 0x15 */
    AXIS_LABEL_PROP_ABS_HAT3X,          /* 0x16 */
    AXIS_LABEL_PROP_ABS_HAT3Y,          /* 0x17 */
    AXIS_LABEL_PROP_ABS_PRESSURE,       /* 0x18 */
    AXIS_LABEL_PROP_ABS_DISTANCE,       /* 0x19 */
    AXIS_LABEL_PROP_ABS_TILT_X,         /* 0x1a */
    AXIS_LABEL_PROP_ABS_TILT_Y,         /* 0x1b */
    AXIS_LABEL_PROP_ABS_TOOL_WIDTH,     /* 0x1c */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_MISC,           /* undefined */
    AXIS_LABEL_PROP_ABS_VOLUME          /* 0x20 */
};

static char* rel_labels[] = {
    AXIS_LABEL_PROP_REL_X,
    AXIS_LABEL_PROP_REL_Y,
    AXIS_LABEL_PROP_REL_Z,
    AXIS_LABEL_PROP_REL_RX,
    AXIS_LABEL_PROP_REL_RY,
    AXIS_LABEL_PROP_REL_RZ,
    AXIS_LABEL_PROP_REL_HWHEEL,
    AXIS_LABEL_PROP_REL_DIAL,
    AXIS_LABEL_PROP_REL_WHEEL,
    AXIS_LABEL_PROP_REL_MISC
};

static char* btn_labels[][16] = {
    { /* BTN_MISC group                 offset 0x100*/
        BTN_LABEL_PROP_BTN_0,           /* 0x00 */
        BTN_LABEL_PROP_BTN_1,           /* 0x01 */
        BTN_LABEL_PROP_BTN_2,           /* 0x02 */
        BTN_LABEL_PROP_BTN_3,           /* 0x03 */
        BTN_LABEL_PROP_BTN_4,           /* 0x04 */
        BTN_LABEL_PROP_BTN_5,           /* 0x05 */
        BTN_LABEL_PROP_BTN_6,           /* 0x06 */
        BTN_LABEL_PROP_BTN_7,           /* 0x07 */
        BTN_LABEL_PROP_BTN_8,           /* 0x08 */
        BTN_LABEL_PROP_BTN_9            /* 0x09 */
    },
    { /* BTN_MOUSE group                offset 0x110 */
        BTN_LABEL_PROP_BTN_LEFT,        /* 0x00 */
        BTN_LABEL_PROP_BTN_RIGHT,       /* 0x01 */
        BTN_LABEL_PROP_BTN_MIDDLE,      /* 0x02 */
        BTN_LABEL_PROP_BTN_SIDE,        /* 0x03 */
        BTN_LABEL_PROP_BTN_EXTRA,       /* 0x04 */
        BTN_LABEL_PROP_BTN_FORWARD,     /* 0x05 */
        BTN_LABEL_PROP_BTN_BACK,        /* 0x06 */
        BTN_LABEL_PROP_BTN_TASK         /* 0x07 */
    },
    { /* BTN_JOYSTICK group             offset 0x120 */
        BTN_LABEL_PROP_BTN_TRIGGER,     /* 0x00 */
        BTN_LABEL_PROP_BTN_THUMB,       /* 0x01 */
        BTN_LABEL_PROP_BTN_THUMB2,      /* 0x02 */
        BTN_LABEL_PROP_BTN_TOP,         /* 0x03 */
        BTN_LABEL_PROP_BTN_TOP2,        /* 0x04 */
        BTN_LABEL_PROP_BTN_PINKIE,      /* 0x05 */
        BTN_LABEL_PROP_BTN_BASE,        /* 0x06 */
        BTN_LABEL_PROP_BTN_BASE2,       /* 0x07 */
        BTN_LABEL_PROP_BTN_BASE3,       /* 0x08 */
        BTN_LABEL_PROP_BTN_BASE4,       /* 0x09 */
        BTN_LABEL_PROP_BTN_BASE5,       /* 0x0a */
        BTN_LABEL_PROP_BTN_BASE6,       /* 0x0b */
        NULL,
        NULL,
        NULL,
        BTN_LABEL_PROP_BTN_DEAD         /* 0x0f */
    },
    { /* BTN_GAMEPAD group              offset 0x130 */
        BTN_LABEL_PROP_BTN_A,           /* 0x00 */
        BTN_LABEL_PROP_BTN_B,           /* 0x01 */
        BTN_LABEL_PROP_BTN_C,           /* 0x02 */
        BTN_LABEL_PROP_BTN_X,           /* 0x03 */
        BTN_LABEL_PROP_BTN_Y,           /* 0x04 */
        BTN_LABEL_PROP_BTN_Z,           /* 0x05 */
        BTN_LABEL_PROP_BTN_TL,          /* 0x06 */
        BTN_LABEL_PROP_BTN_TR,          /* 0x07 */
        BTN_LABEL_PROP_BTN_TL2,         /* 0x08 */
        BTN_LABEL_PROP_BTN_TR2,         /* 0x09 */
        BTN_LABEL_PROP_BTN_SELECT,      /* 0x0a */
        BTN_LABEL_PROP_BTN_START,       /* 0x0b */
        BTN_LABEL_PROP_BTN_MODE,        /* 0x0c */
        BTN_LABEL_PROP_BTN_THUMBL,      /* 0x0d */
        BTN_LABEL_PROP_BTN_THUMBR       /* 0x0e */
    },
    { /* BTN_DIGI group                         offset 0x140 */
        BTN_LABEL_PROP_BTN_TOOL_PEN,            /* 0x00 */
        BTN_LABEL_PROP_BTN_TOOL_RUBBER,         /* 0x01 */
        BTN_LABEL_PROP_BTN_TOOL_BRUSH,          /* 0x02 */
        BTN_LABEL_PROP_BTN_TOOL_PENCIL,         /* 0x03 */
        BTN_LABEL_PROP_BTN_TOOL_AIRBRUSH,       /* 0x04 */
        BTN_LABEL_PROP_BTN_TOOL_FINGER,         /* 0x05 */
        BTN_LABEL_PROP_BTN_TOOL_MOUSE,          /* 0x06 */
        BTN_LABEL_PROP_BTN_TOOL_LENS,           /* 0x07 */
        NULL,
        NULL,
        BTN_LABEL_PROP_BTN_TOUCH,               /* 0x0a */
        BTN_LABEL_PROP_BTN_STYLUS,              /* 0x0b */
        BTN_LABEL_PROP_BTN_STYLUS2,             /* 0x0c */
        BTN_LABEL_PROP_BTN_TOOL_DOUBLETAP,      /* 0x0d */
        BTN_LABEL_PROP_BTN_TOOL_TRIPLETAP       /* 0x0e */
    },
    { /* BTN_WHEEL group                        offset 0x150 */
        BTN_LABEL_PROP_BTN_GEAR_DOWN,           /* 0x00 */
        BTN_LABEL_PROP_BTN_GEAR_UP              /* 0x01 */
    }
};

#endif /* HAVE_LABELS */

static void EvdevInitAxesLabels(EvdevPtr pEvdev, int natoms, Atom *atoms)
{
#ifdef HAVE_LABELS
    Atom atom;
    int axis;
    char **labels;
    int labels_len = 0;
    char *misc_label;

    if (pEvdev->flags & EVDEV_ABSOLUTE_EVENTS)
    {
        labels     = abs_labels;
        labels_len = ArrayLength(abs_labels);
        misc_label = AXIS_LABEL_PROP_ABS_MISC;
    } else if ((pEvdev->flags & EVDEV_RELATIVE_EVENTS))
    {
        labels     = rel_labels;
        labels_len = ArrayLength(rel_labels);
        misc_label = AXIS_LABEL_PROP_REL_MISC;
    }

    memset(atoms, 0, natoms * sizeof(Atom));

    /* Now fill the ones we know */
    for (axis = 0; axis < labels_len; axis++)
    {
        if (pEvdev->axis_map[axis] == -1)
            continue;

        atom = XIGetKnownProperty(labels[axis]);
        if (!atom) /* Should not happen */
            continue;

        atoms[pEvdev->axis_map[axis]] = atom;
    }
#endif
}

static void EvdevInitButtonLabels(EvdevPtr pEvdev, int natoms, Atom *atoms)
{
#ifdef HAVE_LABELS
    Atom atom;
    int button, bmap;

    /* First, make sure all atoms are initialized */
    atom = XIGetKnownProperty(BTN_LABEL_PROP_BTN_UNKNOWN);
    for (button = 0; button < natoms; button++)
        atoms[button] = atom;

    for (button = BTN_MISC; button < BTN_JOYSTICK; button++)
    {
        if (TestBit(button, pEvdev->key_bitmask))
        {
            int group = (button % 0x100)/16;
            int idx = button - ((button/16) * 16);

            if (!btn_labels[group][idx])
                continue;

            atom = XIGetKnownProperty(btn_labels[group][idx]);
            if (!atom)
                continue;

            /* Props are 0-indexed, button numbers start with 1 */
            bmap = EvdevUtilButtonEventToButtonNumber(pEvdev, button) - 1;
            atoms[bmap] = atom;
        }
    }

    /* wheel buttons, hardcoded anyway */
    if (natoms > 3)
        atoms[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
    if (natoms > 4)
        atoms[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
    if (natoms > 5)
        atoms[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
    if (natoms > 6)
        atoms[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
#endif
}

static void
EvdevInitProperty(DeviceIntPtr dev)
{
    InputInfoPtr pInfo  = dev->public.devicePrivate;
    EvdevPtr     pEvdev = pInfo->private;
    int          rc;
    BOOL         invert[2];
    char         reopen;

    prop_reopen = MakeAtom(EVDEV_PROP_REOPEN, strlen(EVDEV_PROP_REOPEN),
            TRUE);

    reopen = pEvdev->reopen_attempts;
    rc = XIChangeDeviceProperty(dev, prop_reopen, XA_INTEGER, 8,
                                PropModeReplace, 1, &reopen, FALSE);
    if (rc != Success)
        return;

    XISetDevicePropertyDeletable(dev, prop_reopen, FALSE);

    if (pEvdev->flags & (EVDEV_RELATIVE_EVENTS | EVDEV_ABSOLUTE_EVENTS))
    {
        invert[0] = pEvdev->invert_x;
        invert[1] = pEvdev->invert_y;

        prop_invert = MakeAtom(EVDEV_PROP_INVERT_AXES, strlen(EVDEV_PROP_INVERT_AXES), TRUE);

        rc = XIChangeDeviceProperty(dev, prop_invert, XA_INTEGER, 8,
                PropModeReplace, 2,
                invert, FALSE);
        if (rc != Success)
            return;

        XISetDevicePropertyDeletable(dev, prop_invert, FALSE);

        prop_calibration = MakeAtom(EVDEV_PROP_CALIBRATION,
                strlen(EVDEV_PROP_CALIBRATION), TRUE);
        if (pEvdev->flags & EVDEV_CALIBRATED) {
            int calibration[4];

            calibration[0] = pEvdev->calibration.min_x;
            calibration[1] = pEvdev->calibration.max_x;
            calibration[2] = pEvdev->calibration.min_y;
            calibration[3] = pEvdev->calibration.max_y;

            rc = XIChangeDeviceProperty(dev, prop_calibration, XA_INTEGER,
                    32, PropModeReplace, 4, calibration,
                    FALSE);
        } else if (pEvdev->flags & EVDEV_ABSOLUTE_EVENTS) {
            rc = XIChangeDeviceProperty(dev, prop_calibration, XA_INTEGER,
                    32, PropModeReplace, 0, NULL,
                    FALSE);
        }
        if (rc != Success)
            return;

        XISetDevicePropertyDeletable(dev, prop_calibration, FALSE);

        prop_swap = MakeAtom(EVDEV_PROP_SWAP_AXES,
                strlen(EVDEV_PROP_SWAP_AXES), TRUE);

        rc = XIChangeDeviceProperty(dev, prop_swap, XA_INTEGER, 8,
                PropModeReplace, 1, &pEvdev->swap_axes, FALSE);
        if (rc != Success)
            return;

        XISetDevicePropertyDeletable(dev, prop_swap, FALSE);

#ifdef HAVE_LABELS
        /* Axis labelling */
        if ((pEvdev->num_vals > 0) && (prop_axis_label = XIGetKnownProperty(AXIS_LABEL_PROP)))
        {
            Atom atoms[pEvdev->num_vals];
            EvdevInitAxesLabels(pEvdev, pEvdev->num_vals, atoms);
            XIChangeDeviceProperty(dev, prop_axis_label, XA_ATOM, 32,
                                   PropModeReplace, pEvdev->num_vals, atoms, FALSE);
            XISetDevicePropertyDeletable(dev, prop_axis_label, FALSE);
        }
        /* Button labelling */
        if ((pEvdev->num_buttons > 0) && (prop_btn_label = XIGetKnownProperty(BTN_LABEL_PROP)))
        {
            Atom atoms[EVDEV_MAXBUTTONS];
            EvdevInitButtonLabels(pEvdev, EVDEV_MAXBUTTONS, atoms);
            XIChangeDeviceProperty(dev, prop_btn_label, XA_ATOM, 32,
                                   PropModeReplace, pEvdev->num_buttons, atoms, FALSE);
            XISetDevicePropertyDeletable(dev, prop_btn_label, FALSE);
        }
#endif /* HAVE_LABELS */
    }

}

static int
EvdevSetProperty(DeviceIntPtr dev, Atom atom, XIPropertyValuePtr val,
                 BOOL checkonly)
{
    InputInfoPtr pInfo  = dev->public.devicePrivate;
    EvdevPtr     pEvdev = pInfo->private;

    if (atom == prop_invert)
    {
        BOOL* data;
        if (val->format != 8 || val->size != 2 || val->type != XA_INTEGER)
            return BadMatch;

        if (!checkonly)
        {
            data = (BOOL*)val->data;
            pEvdev->invert_x = data[0];
            pEvdev->invert_y = data[1];
        }
    } else if (atom == prop_reopen)
    {
        if (val->format != 8 || val->size != 1 || val->type != XA_INTEGER)
            return BadMatch;

        if (!checkonly)
            pEvdev->reopen_attempts = *((CARD8*)val->data);
    } else if (atom == prop_calibration)
    {
        if (val->format != 32 || val->type != XA_INTEGER)
            return BadMatch;
        if (val->size != 4 && val->size != 0)
            return BadMatch;

        if (!checkonly)
            EvdevSetCalibration(pInfo, val->size, val->data);
    } else if (atom == prop_swap)
    {
        if (val->format != 8 || val->type != XA_INTEGER || val->size != 1)
            return BadMatch;

        if (!checkonly)
            pEvdev->swap_axes = *((BOOL*)val->data);
    } else if (atom == prop_axis_label || atom == prop_btn_label)
        return BadAccess; /* Axis/Button labels can't be changed */

    return Success;
}
#endif

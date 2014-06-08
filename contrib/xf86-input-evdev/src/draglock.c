/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@xfree86.org>
 * Copyright 2002 by SuSE Linux AG, Author: Egbert Eich
 * Copyright 1994-2002 by The XFree86 Project, Inc.
 * Copyright 2002 by Paul Elliott
 * (Ported from xf86-input-mouse, above copyrights taken from there)
 * Copyright © 2008 University of South Australia
 * Copyright 2008 by Chris Salch
 * Copyright 2008 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the authors
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
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
 */

/* Draglock code */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xf86.h>
#include <xf86Xinput.h>
#include <X11/Xatom.h>
#include <exevents.h>

#include <evdev-properties.h>
#include "evdev.h"

#ifdef HAVE_PROPERTIES
static Atom prop_dlock     = 0; /* Drag lock buttons. */
#endif

void EvdevDragLockLockButton(InputInfoPtr pInfo, unsigned int button);


/* Setup and configuration code */
void
EvdevDragLockPreInit(InputInfoPtr pInfo)
{
    EvdevPtr pEvdev = (EvdevPtr)pInfo->private;
    char *option_string = NULL;
    int meta_button = 0;
    int lock_button = 0;
    char *next_num = NULL;
    char *end_str = NULL;
    BOOL pairs = FALSE;

    option_string = xf86CheckStrOption(pInfo->options, "DragLockButtons",NULL);

    if (!option_string)
        return;

    next_num = option_string;

    /* Loop until we hit the end of our option string */
    while (next_num != NULL) {
        lock_button = 0;
        meta_button = strtol(next_num, &end_str, 10);

        /* check to see if we found anything */
        if (next_num != end_str) {
            /* setup for the next number */
            next_num = end_str;
        } else {
            /* we have nothing more to parse, drop out of the loop */
            next_num = NULL;
        }

        /* Check for a button to lock if we have a meta button */
        if (meta_button != 0 && next_num != NULL ) {
            lock_button = strtol(next_num, &end_str, 10);

            /* check to see if we found anything */
            if (next_num != end_str) {
                /* setup for the next number */
                next_num = end_str;
            } else {
                /* we have nothing more to parse, drop out of the loop */
                next_num = NULL;
            }
        }

        /* Ok, let the user know what we found on this look */
        if (meta_button != 0) {
            if (lock_button == 0) {
                if (!pairs) {
                    /* We only have a meta button */
                    pEvdev->dragLock.meta = meta_button;

                    xf86Msg(X_CONFIG, "%s: DragLockButtons : "
                            "%i as meta\n",
                            pInfo->name, meta_button);
                } else {
                    xf86Msg(X_ERROR, "%s: DragLockButtons : "
                            "Incomplete pair specifying button pairs %s\n",
                            pInfo->name, option_string);
                }
            } else {

                /* Do bounds checking to make sure we don't crash */
                if ((meta_button <= EVDEV_MAXBUTTONS) && (meta_button >= 0 ) &&
                    (lock_button <= EVDEV_MAXBUTTONS) && (lock_button >= 0)) {

                    xf86Msg(X_CONFIG, "%s: DragLockButtons : %i -> %i\n",
                            pInfo->name, meta_button, lock_button);

                    pEvdev->dragLock.lock_pair[meta_button - 1] = lock_button;
                    pairs=TRUE;
                } else {
                    /* Let the user know something was wrong
                       with this pair of buttons */
                    xf86Msg(X_CONFIG, "%s: DragLockButtons : "
                            "Invalid button pair %i -> %i\n",
                            pInfo->name, meta_button, lock_button);
                }
            }
        } else {
            xf86Msg(X_ERROR, "%s: Found DragLockButtons "
                    "with  invalid lock button string : '%s'\n",
                    pInfo->name, option_string);

            /* This should be the case anyhow, just make sure */
            next_num = NULL;
        }

        /* Check for end of string, to avoid annoying error */
        if (next_num != NULL && *next_num == '\0')
            next_num = NULL;
    }
}

/* Updates DragLock button state and fires button event messges */
void
EvdevDragLockLockButton(InputInfoPtr pInfo, unsigned int button)
{
    EvdevPtr pEvdev = (EvdevPtr)pInfo->private;
    BOOL state = 0;

    /* update button state */
    state = pEvdev->dragLock.lock_state[button - 1] ? FALSE : TRUE;
    pEvdev->dragLock.lock_state[button - 1] = state;

    EvdevQueueButtonEvent(pInfo, button, state);
}

/* Filter button presses looking for either a meta button or the
 * control of a button pair.
 *
 * @param button button number (1 for left, 3 for right)
 * @param value TRUE if button press, FALSE if release
 *
 * @return TRUE if the event was swallowed here, FALSE otherwise.
 */
BOOL
EvdevDragLockFilterEvent(InputInfoPtr pInfo, unsigned int button, int value)
{
    EvdevPtr pEvdev = (EvdevPtr)pInfo->private;

    if (button == 0)
        return FALSE;

    /* Do we have a single meta key or
       several button pairings? */
    if (pEvdev->dragLock.meta != 0) {

        if (pEvdev->dragLock.meta == button) {

            /* setup up for button lock */
            if (value)
                pEvdev->dragLock.meta_state = TRUE;

            return TRUE;
        } else if (pEvdev->dragLock.meta_state) { /* waiting to lock */

            pEvdev->dragLock.meta_state = FALSE;

            EvdevDragLockLockButton(pInfo, button);

            return TRUE;
        }
    } else if (pEvdev->dragLock.lock_pair[button - 1] && value) {
        /* A meta button in a meta/lock pair was pressed */
        EvdevDragLockLockButton(pInfo, pEvdev->dragLock.lock_pair[button - 1]);
        return TRUE;
    }

    /* Eat events for buttons that are locked */
    if (pEvdev->dragLock.lock_state[button - 1])
        return TRUE;

    return FALSE;
}

#ifdef HAVE_PROPERTIES
/**
 * Set the drag lock property.
 * If only one value is supplied, then this is used as the meta button.
 * If more than one value is supplied, then each value is the drag lock button
 * for the pair. 0 disables a pair.
 * i.e. to set bt 3 to draglock button 1, supply 0,0,1
 */
static int
EvdevDragLockSetProperty(DeviceIntPtr dev, Atom atom, XIPropertyValuePtr val,
                         BOOL checkonly)
{
    InputInfoPtr pInfo  = dev->public.devicePrivate;
    EvdevPtr     pEvdev = pInfo->private;

    if (atom == prop_dlock)
    {
        int i;

        if (val->format != 8 || val->type != XA_INTEGER)
            return BadMatch;

        /* Don't allow changes while a lock is active */
        if (pEvdev->dragLock.meta)
        {
            if (pEvdev->dragLock.meta_state)
                return BadAccess;
        } else
        {
            for (i = 0; i < EVDEV_MAXBUTTONS; i++)
                if (pEvdev->dragLock.lock_state[i])
                    return BadValue;
        }

        if (val->size == 0)
            return BadMatch;
        else if (val->size == 1)
        {
            int meta = *((CARD8*)val->data);
            if (meta > EVDEV_MAXBUTTONS)
                return BadValue;

            if (!checkonly)
            {
                pEvdev->dragLock.meta = meta;
                memset(pEvdev->dragLock.lock_pair, 0, sizeof(pEvdev->dragLock.lock_pair));
            }
        } else if ((val->size % 2) == 0)
        {
            CARD8* vals = (CARD8*)val->data;

            for (i = 0; i < val->size && i < EVDEV_MAXBUTTONS; i++)
                if (vals[i] > EVDEV_MAXBUTTONS)
                    return BadValue;

            if (!checkonly)
            {
                pEvdev->dragLock.meta = 0;
                memset(pEvdev->dragLock.lock_pair, 0, sizeof(pEvdev->dragLock.lock_pair));

                for (i = 0; i < val->size && i < EVDEV_MAXBUTTONS; i += 2)
                    pEvdev->dragLock.lock_pair[vals[i] - 1] = vals[i + 1];
            }
        } else
            return BadMatch;
    }

    return Success;
}

/**
 * Initialise property for drag lock buttons setting.
 */
void
EvdevDragLockInitProperty(DeviceIntPtr dev)
{
    InputInfoPtr pInfo  = dev->public.devicePrivate;
    EvdevPtr     pEvdev = pInfo->private;

    if (!dev->button) /* don't init prop for keyboards */
        return;

    prop_dlock = MakeAtom(EVDEV_PROP_DRAGLOCK, strlen(EVDEV_PROP_DRAGLOCK), TRUE);
    if (pEvdev->dragLock.meta)
    {
        XIChangeDeviceProperty(dev, prop_dlock, XA_INTEGER, 8,
                               PropModeReplace, 1, &pEvdev->dragLock.meta,
                               FALSE);
    } else {
        int highest = 0;
        int i;
        CARD8 pair[EVDEV_MAXBUTTONS] = {0};

        for (i = 0; i < EVDEV_MAXBUTTONS; i++)
        {
            if (pEvdev->dragLock.lock_pair[i])
                highest = i;
            pair[i] = pEvdev->dragLock.lock_pair[i];
        }

        XIChangeDeviceProperty(dev, prop_dlock, XA_INTEGER, 8, PropModeReplace,
                               highest + 1, pair, FALSE);
    }

    XISetDevicePropertyDeletable(dev, prop_dlock, FALSE);

    XIRegisterPropertyHandler(dev, EvdevDragLockSetProperty, NULL, NULL);
}

#endif

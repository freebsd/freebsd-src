/*
 * hid.c
 *
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: hid.c,v 1.3 2004/02/26 21:47:35 max Exp $
 * $FreeBSD$
 */

#include <sys/consio.h>
#include <sys/mouse.h>
#include <sys/queue.h>
#include <assert.h>
#include <bluetooth.h>
#include <errno.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <usbhid.h>
#include "bthidd.h"
#include "bthid_config.h"

#undef	min
#define	min(x, y)	(((x) < (y))? (x) : (y))

/*
 * Process data from control channel
 */

int
hid_control(bthid_session_p s, char *data, int len)
{
	assert(s != NULL);
	assert(data != NULL);
	assert(len > 0);

	switch (data[0] >> 4) {
        case 0: /* Handshake (response to command) */
		if (data[0] & 0xf)
			syslog(LOG_ERR, "Got handshake message with error " \
				"response 0x%x from %s",
				data[0], bt_ntoa(&s->bdaddr, NULL));
		break;

	case 1: /* HID Control */
		switch (data[0] & 0xf) {
		case 0: /* NOP */
			break;

		case 1: /* Hard reset */
		case 2: /* Soft reset */
			syslog(LOG_WARNING, "Device %s requested %s reset",
				bt_ntoa(&s->bdaddr, NULL),
				((data[0] & 0xf) == 1)? "hard" : "soft");
			break;

		case 3: /* Suspend */
			syslog(LOG_NOTICE, "Device %s requested Suspend",
				bt_ntoa(&s->bdaddr, NULL));
			break;

		case 4: /* Exit suspend */
			syslog(LOG_NOTICE, "Device %s requested Exit Suspend",
				bt_ntoa(&s->bdaddr, NULL));
			break;

		case 5: /* Virtual cable unplug */
			syslog(LOG_NOTICE, "Device %s unplugged virtual cable",
				bt_ntoa(&s->bdaddr, NULL));
			session_close(s);
			break;

		default:
			syslog(LOG_WARNING, "Device %s sent unknown " \
                                "HID_Control message 0x%x",
				bt_ntoa(&s->bdaddr, NULL), data[0]);
			break;
		}
		break;

	default:
		syslog(LOG_WARNING, "Got unexpected message 0x%x on Control " \
			"channel from %s", data[0], bt_ntoa(&s->bdaddr, NULL));
		break;
	}

	return (0);
}

/*
 * Process data from the interrupt channel
 */

int
hid_interrupt(bthid_session_p s, char *data, int len)
{
	hid_device_p	hid_device = NULL;
	hid_data_t	d;
	hid_item_t	h;
	int		report_id, usage, page, val,
			mouse_x, mouse_y, mouse_z, mouse_butt,
			nkeys, keys[32]; /* XXX how big keys[] should be? */

	assert(s != NULL);
	assert(data != NULL);

	if (len < 3) {
		syslog(LOG_ERR, "Got short message (%d bytes) on Interrupt " \
			"channel from %s", len, bt_ntoa(&s->bdaddr, NULL));
		return (-1);
	}

	if ((unsigned char) data[0] != 0xa1) {
		syslog(LOG_ERR, "Got unexpected message 0x%x on " \
			"Interrupt channel from %s",
			data[0], bt_ntoa(&s->bdaddr, NULL));
		return (-1);
	}

	report_id = data[1];
	data += 2;
	len -= 2;

	hid_device = get_hid_device(&s->bdaddr);
	assert(hid_device != NULL);

	mouse_x = mouse_y = mouse_z = mouse_butt = nkeys = 0;

	for (d = hid_start_parse(hid_device->desc, 1 << hid_input, -1);
	     hid_get_item(d, &h) > 0; ) {
		if ((h.flags & HIO_CONST) || (h.report_ID != report_id))
			continue;

		page = HID_PAGE(h.usage);
		usage = HID_USAGE(h.usage);
		val = hid_get_data(data, &h);

		switch (page) {
		case HUP_GENERIC_DESKTOP:
			switch (usage) {
			case HUG_X:
				mouse_x = val;
				break;

			case HUG_Y:
				mouse_y = val;
				break;

			case HUG_WHEEL:
				mouse_z = -val;
				break;

			case HUG_SYSTEM_SLEEP:
				if (val)
					syslog(LOG_NOTICE, "Sleep button pressed");
				break;
			}
			break;

		case HUP_KEYBOARD:
			if (h.flags & HIO_VARIABLE) {
				if (val && nkeys < sizeof(keys))
					keys[nkeys ++] = usage;
			} else {
				if (val && nkeys < sizeof(keys))
					keys[nkeys ++] = val;
				data ++;
				len --;

				len = min(len, h.report_size);
				while (len > 0) {
					val = hid_get_data(data, &h);
					if (val && nkeys < sizeof(keys))
						keys[nkeys ++] = val;
					data ++;
					len --;
				}
			}
			break;

		case HUP_BUTTON:
			mouse_butt |= (val << (usage - 1));
			break;

		case HUP_MICROSOFT:
			switch (usage) {
			case 0xfe01:
				if (!hid_device->battery_power)
					break;

				switch (val) {
				case 1:
					syslog(LOG_INFO, "Battery is OK on %s",
						bt_ntoa(&s->bdaddr, NULL));
					break;

				case 2:
					syslog(LOG_NOTICE, "Low battery on %s",
						bt_ntoa(&s->bdaddr, NULL));
					break;

				case 3:
					syslog(LOG_WARNING, "Very low battery "\
                                                "on %s",
						bt_ntoa(&s->bdaddr, NULL));
					break;
                                }
				break;
			}
			break;
		}
	}
	hid_end_parse(d);

	/* 
	 * XXX FIXME	Feed mouse and keyboard events into kernel
	 *		The code block below works, but it is not
	 *		good enough
	 */

	if (mouse_x != 0 || mouse_y != 0 || mouse_z != 0 || mouse_butt != 0) {
		struct mouse_info	mi;

		mi.operation = MOUSE_ACTION;
		mi.u.data.x = mouse_x;
		mi.u.data.y = mouse_y;
		mi.u.data.z = mouse_z;
		mi.u.data.buttons = mouse_butt;

		if (ioctl(s->srv->cons, CONS_MOUSECTL, &mi) < 0)
			syslog(LOG_ERR, "Could not process mouse events from " \
				"%s. %s (%d)", bt_ntoa(&s->bdaddr, NULL),
				strerror(errno), errno);
	}

	return (0);
}


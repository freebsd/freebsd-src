/*
 * bthid_config.h
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
 * $Id: bthid_config.h,v 1.3 2004/02/17 22:05:02 max Exp $
 * $FreeBSD$
 */

#ifndef _BTHID_CONFIG_H_
#define _BTHID_CONFIG_H_ 1

#define BTHIDD_CONFFILE		"/etc/bluetooth/bthidd.conf"
#define BTHIDD_HIDSFILE		"/var/db/bthidd.hids"

struct hid_device
{
	bdaddr_t		bdaddr;		/* HID device BDADDR */
	uint16_t		control_psm;	/* control PSM */
	uint16_t		interrupt_psm;	/* interrupt PSM */
	unsigned		new_device           : 1;
	unsigned		reconnect_initiate   : 1;
	unsigned		battery_power        : 1;
	unsigned		normally_connectable : 1;
	unsigned		reserved             : 12;
	report_desc_t		desc;		/* HID report descriptor */
	LIST_ENTRY(hid_device)	next;		/* link to the next */
};
typedef struct hid_device	hid_device_t;
typedef struct hid_device *	hid_device_p;

extern char	*config_file;
extern char	*hids_file;

int		read_config_file	(void);
void		clean_config		(void);
hid_device_p	get_hid_device		(bdaddr_p bdaddr);
hid_device_p	get_next_hid_device	(hid_device_p d);
void		print_hid_device	(hid_device_p hid_device, FILE *f);

int		read_hids_file		(void);
int		write_hids_file		(void);

#endif /* ndef _BTHID_CONFIG_H_ */


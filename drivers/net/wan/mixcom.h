/*
 * Defines for the mixcom board
 *
 * Author: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#define	MIXCOM_IO_EXTENT	0x20

#define	MIXCOM_DEFAULT_IO	0x180
#define	MIXCOM_DEFAULT_IRQ	5

#define MIXCOM_ID		0x11
#define MIXCOM_SERIAL_OFFSET	0x1000
#define MIXCOM_CHANNEL_OFFSET	0x400
#define MIXCOM_IT_OFFSET	0xc14
#define MIXCOM_STATUS_OFFSET	0xc14
#define MIXCOM_ID_OFFSET	0xc10
#define MIXCOM_ON		0x1
#define MIXCOM_OFF		0x0

/* Status register bits */

#define MIXCOM_CTSB		0x1
#define MIXCOM_CTSA		0x2
#define MIXCOM_CHANNELNO	0x20
#define MIXCOM_POWERFAIL	0x40
#define MIXCOM_BOOT		0x80

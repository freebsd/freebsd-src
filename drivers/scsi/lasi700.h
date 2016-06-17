/* -*- mode: c; c-basic-offset: 8 -*- */

/* PARISC LASI driver for the 53c700 chip
 *
 * Copyright (C) 2001 by James.Bottomley@HansenPartnership.com
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
 */

#ifndef _LASI700_H
#define _LASI700_H

static int lasi700_detect(Scsi_Host_Template *);
static int lasi700_driver_callback(struct parisc_device *dev);
static int lasi700_release(struct Scsi_Host *host);


#define LASI700_SCSI {				\
	name:		"LASI SCSI 53c700",	\
	proc_name:	"lasi700",		\
	detect:		lasi700_detect,		\
	release:	lasi700_release,	\
	this_id:	7,			\
}

#define LASI_710_SVERSION	0x082
#define LASI_700_SVERSION	0x071

#define LASI700_ID_TABLE {			\
	hw_type:	HPHW_FIO,		\
	sversion:	LASI_700_SVERSION,	\
	hversion:	HVERSION_ANY_ID,	\
	hversion_rev:	HVERSION_REV_ANY_ID,	\
}

#define LASI710_ID_TABLE {			\
	hw_type:	HPHW_FIO,		\
	sversion:	LASI_710_SVERSION,	\
	hversion:	HVERSION_ANY_ID,	\
	hversion_rev:	HVERSION_REV_ANY_ID,	\
}

#define LASI700_DRIVER {			\
	name:		"Lasi SCSI",		\
	id_table:	lasi700_scsi_tbl,	\
	probe:		lasi700_driver_callback,\
}

#define LASI700_CLOCK	25
#define LASI710_CLOCK	40
#define LASI_SCSI_CORE_OFFSET 0x100

#endif

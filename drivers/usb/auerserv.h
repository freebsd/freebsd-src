/*****************************************************************************/
/*
 *      auerserv.h  --  Auerswald PBX/System Telephone service request structure.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

/* The auerswald ISDN devices have a logical channel concept. Many channels are
 * realized via one control endpoint or INT endpoint. At the receiver side, these
 * messages must be dispatched. Some data may be for an application which is
 * connected through the char interface, other data may be D-channel information
 * routed to ISDN4LINUX. The auerscon struct is used to dispatch the data.
 */

#ifndef AUERSERV_H
#define AUERSERV_H

#include "auerbuf.h"

/* service context */
struct auerscon;
typedef void (*auer_dispatch_t) (struct auerscon *, struct auerbuf *);
typedef void (*auer_disconn_t) (struct auerscon *);

struct auerscon {
	unsigned int id;		/* protocol service id AUH_xxxx */
	auer_dispatch_t dispatch;	/* dispatch read buffer */
	auer_disconn_t disconnect;	/* disconnect from device, wake up all readers */
};


#endif	/* AUERSERV_H */

/*
 * sound/awe_version.h
 *
 * Version defines for the AWE32/Sound Blaster 32 wave table synth driver.
 *   version 0.4.2c; Oct. 7, 1997
 *
 * Copyright (C) 1996,1997 Takashi Iwai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* AWE32 driver version number */

#ifndef AWE_VERSION_H_DEF
#define AWE_VERSION_H_DEF

#define AWE_VERSION_NUMBER	0x00040203
#define AWEDRV_VERSION		"0.4.2c"
#define AWE_MAJOR_VERSION(id)	(((id) >> 16) & 0xff)
#define AWE_MINOR_VERSION(id)	(((id) >> 8) & 0xff)
#define AWE_TINY_VERSION(id)	((id) & 0xff)

#endif

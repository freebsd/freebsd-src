/* 
    zr36060.h - zr36060 register offsets

    Copyright (C) 1998 Dave Perks <dperks@ibm.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _ZR36060_H_
#define _ZR36060_H_


/* Zoran ZR36060 registers */

#define ZR36060_LoadParameters  	0x000
#define ZR36060_Load                    (1<<7)
#define ZR36060_SyncRst                 (1<<0)

#define ZR36060_CodeFifoStatus  	0x001
#define ZR36060_Load                    (1<<7)
#define ZR36060_SyncRst                 (1<<0)

#endif

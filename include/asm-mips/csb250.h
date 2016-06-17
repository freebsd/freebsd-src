/*
 * Cogent Computer Systems CSB250 Alchemy Au1500
 *
 * Copyright 2002 Cogent Computer Systems, Inc.
 * 	dan@embeddededge.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * 
 */
#ifndef __ASM_CSB250_H
#define __ASM_CSB250_H

/* I don't know how to define all of these, yet.
 *
 * 0x0 0000 0000	32/64 MByte SDRAM
 * 0x0 1C00 0000	AM29LLV642 Flash
 * 0x0 1E00 0000	AM29LLV642 Flash
 * 0xD 1000 0000	HT6542B PS/2 controller
 * 0xE 1800 0000	S1D13506 LCD Controller
 */

#define RTC_I2C		0x58	/* I2C address of DS1307 RTC */

#endif /* __ASM_CSB250_H */

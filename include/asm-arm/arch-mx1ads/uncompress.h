/*
 *  linux/include/asm-arm/arch-mx1ads/uncompress.h
 *
 *
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) Shane Nay (shane@minirl.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define UA1_USR2	(*(volatile unsigned char *)0x206098)
#define UA1_TXR 	(*(volatile unsigned char *)0x206040)
#define TXFEBIT         (14)

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s) {
		while (UA1_USR2 & (1 << TXFEBIT))
			barrier();

		UA1_TXR = *s;

		if (*s == '\n') {
			while (UA1_USR2 & (1 << TXFEBIT))
				barrier();

			UA1_TXR = '\r';
		}
		s++;
	}
	while (UA1_USR2 & (1 << TXFEBIT))
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()

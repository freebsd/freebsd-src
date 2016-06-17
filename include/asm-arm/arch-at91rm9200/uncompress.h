/*
 * linux/include/asm-arm/arch-at91rm9200/uncompress.h
 *
 *  Copyright (C) 2003 SAN People
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

#ifndef __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

#include <asm/arch/hardware.h>

static void puts(const char *s)
{
	AT91PS_SYS pSYS = (AT91PS_SYS) AT91C_BASE_SYS;	/* physical address */

	while (*s) {
		while (!(pSYS->DBGU_CSR & AT91C_DBGU_TXRDY)) { barrier(); }
		pSYS->DBGU_THR = *s;
		if (*s == '\n')	{
			while (!(pSYS->DBGU_CSR & AT91C_DBGU_TXRDY)) { barrier(); }
			pSYS->DBGU_THR = '\r';
		}
		s++;
	}
	/* wait for transmission to complete */
	while (!(pSYS->DBGU_CSR & AT91C_DBGU_TXEMPTY)) { barrier(); }
}

#define arch_decomp_setup()

#define arch_decomp_wdog()

#endif

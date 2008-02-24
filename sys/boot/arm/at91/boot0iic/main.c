/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/boot/arm/at91/boot0iic/main.c,v 1.4 2006/11/16 00:49:50 imp Exp $
 */

#include "at91rm9200.h"
#include "lib.h"
#include "at91rm9200_lowlevel.h"

int
main(void)
{
	char *addr = (char *)SDRAM_BASE + (1 << 20); /* Load to base + 1MB */
	int len, sec;

	printf("\nSend data to be written into EEPROM\n");
	while ((len = xmodem_rx(addr)) == -1)
		continue;
	sec = GetSeconds() + 1;
	while (sec >= GetSeconds())
		continue;
	printf("\nWriting EEPROM from 0x%x to addr 0, 0x%x bytes\n", addr,
	    len);
	InitEEPROM();
	printf("init done\n");
	WriteEEPROM(0, addr, len);
	printf("\nWrote %d bytes.  Press reset\n", len);
	return (1);
}

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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include "emac.h"
#include "lib.h"
#include "board.h"
#include "sd-card.h"

unsigned char mac[6] = { 0x42, 0x53, 0x44, 0, 1, 1 };

static void
MacFromEE()
{	
#if 0
	uint32_t sig;
	sig = 0;
	ReadEEPROM(12 * 1024, (uint8_t *)&sig, sizeof(sig));
	if (sig != 0x92021054)
		return;
	ReadEEPROM(12 * 1024 + 4, mac, 6);
#endif
	printf("MAC %x:%x:%x:%x:%x:%x\n", mac[0],
	  mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void
board_init(void)
{
    InitEEPROM();
    MacFromEE();
    EMAC_Init();
    EMAC_SetMACAddress(mac);
    while (sdcard_init() == 0)
	printf("Looking for SD card\n");
}

int
drvread(void *buf, unsigned lba, unsigned nblk)
{
    return (MCI_read((char *)buf, lba << 9, nblk << 9));
}

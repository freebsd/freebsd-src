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
 * This software is derived from software provide by Kwikbyte who specifically
 * disclaimed copyright on the code.  This version of xmodem has been nearly
 * completely rewritten, but the CRC is from the original.
 *
 * $FreeBSD$
 */

#include "lib.h"

#define PACKET_SIZE	128

/* Line control codes */
#define SOH			0x01	/* start of header */
#define ACK			0x06	/* Acknowledge */
#define NAK			0x15	/* Negative acknowledge */
#define CAN			0x18	/* Cancel */
#define EOT			0x04	/* end of text */

#define TO	10
/*
 * int GetRecord(char , char *)
 *  This private function receives a x-modem record to the pointer and
 * returns non-zero on success.
 */
static int
GetRecord(char blocknum, char *dest)
{
	int		size;
	int		ch;
	unsigned	chk, j;

	chk = 0;

	if ((ch = getc(TO)) == -1)
		goto err;
	if (ch != blocknum) 
		goto err;
	if ((ch = getc(TO)) == -1) 
		goto err;
	if (ch != (~blocknum & 0xff))
		goto err;
	
	for (size = 0; size < PACKET_SIZE; ++size) {
		if ((ch = getc(TO)) == -1)
			goto err;
		chk = chk ^ ch << 8;
		for (j = 0; j < 8; ++j) {
			if (chk & 0x8000)
				chk = chk << 1 ^ 0x1021;
			else
				chk = chk << 1;
		}
		*dest++ = ch;
	}

	chk &= 0xFFFF;

	if (((ch = getc(TO)) == -1) || ((ch & 0xff) != ((chk >> 8) & 0xFF)))
		goto err;
	if (((ch = getc(TO)) == -1) || ((ch & 0xff) != (chk & 0xFF)))
		goto err;
	putchar(ACK);

	return (1);
err:;
	putchar(CAN);
	// We should allow for resend, but we don't.
	return (0);
}

/*
 * int xmodem_rx(char *)
 *  This global function receives a x-modem transmission consisting of
 * (potentially) several blocks.  Returns the number of bytes received or
 * -1 on error.
 */
int
xmodem_rx(char *dest)
{
	int		starting, ch;
	char		packetNumber, *startAddress = dest;

	packetNumber = 1;
	starting = 1;

	while (1) {
		if (starting)
			putchar('C');
		if (((ch = getc(1)) == -1) || (ch != SOH && ch != EOT))
			continue;
		if (ch == EOT) {
			putchar(ACK);
			return (dest - startAddress);
		}
		starting = 0;
		// Xmodem packets: SOH PKT# ~PKT# 128-bytes CRC16
		if (!GetRecord(packetNumber, dest))
			return (-1);
		dest += PACKET_SIZE;
		packetNumber++;
	}

	// the loop above should return in all cases
	return (-1);
}

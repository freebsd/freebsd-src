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
 * disclaimed copyright on the code.
 *
 * $FreeBSD$
 */

#ifndef __SD_CARD_H
#define __SD_CARD_H

/* MCI_read() is the original read function, taking a byte offset and byte
 * count.  It is preserved to support existing customized boot code that still
 * refers to it; it will work fine even on SDHC cards as long as the kernel and
 * the metadata for locating it all exist within the first 4GB of the card.
 *
 * MCI_readblocks() is the new read function, taking offset and length in terms
 * of block counts (where the SD spec defines a block as 512 bytes), allowing
 * the kernel and filesystem metadata to be located anywhere on an SDHC card.
 *
 * Returns 0 on success, non-zero on failure.
 */

int MCI_read (char* dest, unsigned bytenum, unsigned length);
int MCI_readblocks (char* dest, unsigned blknum, unsigned blkcount);

/* sdcard_init() - get things set up to read from an SD or SDHC card.
 *
 * Returns 0 on failure, non-zero on success.
 */

int sdcard_init(void);

/* By default sdcard_init() sets things up for a 1-wire interface to the
 * SD card.  Calling sdcard_4wire(true) after sdcard_init() allows customized
 * boot code to change to 4-bit transfers when the hardware supports it.
 *
 * Returns 0 on failure, non-zero on success.
 */
int sdcard_use4wire(int use4wire);

#endif


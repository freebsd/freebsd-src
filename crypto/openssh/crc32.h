/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1992 Tatu Ylonen, Espoo, Finland
 *                    All rights reserved
 * Functions for computing 32-bit CRC.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: crc32.h,v 1.8 2000/09/07 20:27:51 deraadt Exp $"); */

#ifndef CRC32_H
#define CRC32_H

/*
 * This computes a 32 bit CRC of the data in the buffer, and returns the CRC.
 * The polynomial used is 0xedb88320.
 */
unsigned int ssh_crc32(const unsigned char *buf, unsigned int len);

#endif				/* CRC32_H */

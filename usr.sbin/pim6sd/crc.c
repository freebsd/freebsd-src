/*
 * Copyright (C) 1999 LSIIT Laboratory.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/crc.c,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */
/* CRC implantation : stolen from RFC 2083 section 15.*/

#include <sys/cdefs.h>
#include "crc.h"

/* Table of CRCs of all 8-bit messages. */
	unsigned long crc_table[256];
   
/* Flag: has the table been computed? Initially false. */
	int crc_table_computed = 0;
  

/* Make the table for a fast CRC. */ 

static void make_crc_table __P((void));
static unsigned long update_crc __P((unsigned long, unsigned char *, int));

static void make_crc_table(void)
{
	unsigned long c; 
	int n, k;
	for (n = 0; n < 256; n++) 
	{
		c = (unsigned long) n;
          	for (k = 0; k < 8; k++) 
		{
            		if (c & 1) 
              			c = 0xedb88320L ^ (c >> 1);
            		else    
              			c = c >> 1; 
          	}       
          crc_table[n] = c;
        }       
        crc_table_computed = 1;
}       
   
/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
   
static unsigned long update_crc(unsigned long crc, unsigned char *buf,
                               int len)
{       
	unsigned long c = crc;
	int n;  
   
	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++) 
	{
          	c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
        }       
        return c;
}       
   

/* Return the CRC of the bytes buf[0..len-1]. */

unsigned long crc(unsigned char *buf, int len)
{       
        return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}       

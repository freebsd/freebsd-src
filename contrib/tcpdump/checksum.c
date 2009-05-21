/*
 * Copyright (c) 1998-2006 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * miscellaneous checksumming routines
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/checksum.c,v 1.4 2006-09-25 09:23:32 hannes Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"

#define CRC10_POLYNOMIAL 0x633
static u_int16_t crc10_table[256];

static void
init_crc10_table(void)
{   
    register int i, j;
    register u_int16_t accum;
    
    for ( i = 0;  i < 256;  i++ )
    {
        accum = ((unsigned short) i << 2);
        for ( j = 0;  j < 8;  j++ )
        {
            if ((accum <<= 1) & 0x400) accum ^= CRC10_POLYNOMIAL;
        }
        crc10_table[i] = accum;
    }
    return;
}

u_int16_t
verify_crc10_cksum(u_int16_t accum, const u_char *p, int length)
{
    register int i;

    for ( i = 0;  i < length;  i++ )
    {
        accum = ((accum << 8) & 0x3ff)
            ^ crc10_table[( accum >> 2) & 0xff]
            ^ *p++;
    }
    return accum;
}

/* precompute checksum tables */
void
init_checksum(void) {

    init_crc10_table();

}

/*
 * Creates the OSI Fletcher checksum. See 8473-1, Appendix C, section C.3.
 * The checksum field of the passed PDU does not need to be reset to zero.
 */
u_int16_t
create_osi_cksum (const u_int8_t *pptr, int checksum_offset, int length)
{

    int x;
    int y;
    u_int32_t mul;
    u_int32_t c0;
    u_int32_t c1;
    u_int16_t checksum;
    int index;

    checksum = 0;

    c0 = 0;
    c1 = 0;

    for (index = 0; index < length; index++) {
        /*
         * Ignore the contents of the checksum field.
         */
        if (index == checksum_offset ||
            index == checksum_offset+1) {
            c1 += c0;
            pptr++;
        } else {
            c0 = c0 + *(pptr++);
            c1 += c0;
        } 
    }

    c0 = c0 % 255;
    c1 = c1 % 255;

    mul = (length - checksum_offset)*(c0);
  
    x = mul - c0 - c1;
    y = c1 - mul - 1;

    if ( y >= 0 ) y++;
    if ( x < 0 ) x--;

    x %= 255;
    y %= 255;


    if (x == 0) x = 255;
    if (y == 0) y = 255;

    y &= 0x00FF;
    checksum = ((x << 8) | y);
  
    return checksum;
}

/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI int1a.c,v 2.2 1996/04/08 19:32:49 bostic Exp
 *
 * $FreeBSD$
 */

#include "doscmd.h"

static inline int
to_BCD (int n)
{
    n &= 0xFF;
    return n%10 + ((n/10)<<4);
}

void
int1a(regcontext_t *REGS)
{
    struct	timeval	tod;
    struct	timezone zone;
    struct	tm *tm;
    time_t	tv_sec;
    long	value;
    static long midnight = 0;

    R_FLAGS &= ~PSL_C;

    switch (R_AH) {
    case 0x00:
	gettimeofday(&tod, &zone);

	if (midnight == 0) {
	    tv_sec = boot_time.tv_sec;
	    tm = localtime(&tv_sec);
	    midnight = boot_time.tv_sec - (((tm->tm_hour * 60)
					    + tm->tm_min) * 60
					   + tm->tm_sec);
	}

	R_AL = (tod.tv_sec - midnight) / (24 * 60 * 60);

	if (R_AL) {
	    tv_sec = boot_time.tv_sec;
	    tm = localtime(&tv_sec);
	    midnight = boot_time.tv_sec - (((tm->tm_hour * 60)
					    + tm->tm_min) * 60
					   + tm->tm_sec);
	}

	tod.tv_sec -= midnight;
	tod.tv_usec -= boot_time.tv_usec;

	value = (tod.tv_sec * 182 + tod.tv_usec / (1000000L/182)) / 10;
	R_CX = value >> 16;
	R_DX = value & 0xffff;
	break;

    case 0x01:				/* set current clock count */
	tv_sec = boot_time.tv_sec;
	tm = localtime(&tv_sec);
	midnight = boot_time.tv_sec - (((tm->tm_hour * 60)
	    + tm->tm_min) * 60 + tm->tm_sec);
        break;

    case 0x02:
	gettimeofday(&tod, &zone);
	tv_sec = tod.tv_sec;
	tm = localtime(&tv_sec);
	R_CH = to_BCD(tm->tm_hour);
	R_CL = to_BCD(tm->tm_min);
	R_DH = to_BCD(tm->tm_sec);
	break;

    case 0x04:
	gettimeofday(&tod, &zone);
	tv_sec = tod.tv_sec;
	tm = localtime(&tv_sec);
	R_CH = to_BCD((tm->tm_year + 1900) / 100);
	R_CL = to_BCD((tm->tm_year + 1900) % 100);
	R_DH = to_BCD(tm->tm_mon + 1);
	R_DL = to_BCD(tm->tm_mday);
	break;

    default:
	unknown_int2(0x1a, R_AH, REGS);
	break;
    }
}

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb_locl.h"

RCSID("$Id: lsb_addr_comp.c,v 1.9 1997/04/01 08:18:37 joda Exp $");

#include "lsb_addr_comp.h"

int
krb_lsb_antinet_ulong_cmp(u_int32_t x, u_int32_t y)
{
    int i;
    u_int32_t a = 0, b = 0;
    u_int8_t *p = (u_int8_t*) &x;
    u_int8_t *q = (u_int8_t*) &y;

    for(i = sizeof(u_int32_t) - 1; i >= 0; i--){
	a = (a << 8) | p[i];
	b = (b << 8) | q[i];
    }
    if(a > b)
	return 1;
    if(a < b)
	return -1;
    return 0;
}

int
krb_lsb_antinet_ushort_cmp(u_int16_t x, u_int16_t y)
{
    int i;
    u_int16_t a = 0, b = 0;
    u_int8_t *p = (u_int8_t*) &x;
    u_int8_t *q = (u_int8_t*) &y;

    for(i = sizeof(u_int16_t) - 1; i >= 0; i--){
	a = (a << 8) | p[i];
	b = (b << 8) | q[i];
    }
    if(a > b)
	return 1;
    if(a < b)
	return -1;
    return 0;
}

u_int32_t
lsb_time(time_t t, struct sockaddr_in *src, struct sockaddr_in *dst)
{
    /*
     * direction bit is the sign bit of the timestamp.  Ok until
     * 2038??
     */
    /* For compatibility with broken old code, compares are done in VAX 
       byte order (LSBFIRST) */ 
    if (krb_lsb_antinet_ulong_less(src->sin_addr.s_addr, /* src < recv */ 
				   dst->sin_addr.s_addr) < 0) 
        t = -t;
    else if (krb_lsb_antinet_ulong_less(src->sin_addr.s_addr, 
					dst->sin_addr.s_addr)==0) 
        if (krb_lsb_antinet_ushort_less(src->sin_port, dst->sin_port) < 0)
            t = -t;
    /*
     * all that for one tiny bit!  Heaven help those that talk to
     * themselves.
     */
    t = t & 0xffffffff;
    return t;
}

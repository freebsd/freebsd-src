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

RCSID("$Id: rd_err.c,v 1.8 1997/04/01 08:18:40 joda Exp $");

/*
 * Given an AUTH_MSG_APPL_ERR message, "in" and its length "in_length",
 * return the error code from the message in "code" and the text in
 * "m_data" as follows:
 *
 *	m_data->app_data	points to the error text
 *	m_data->app_length	points to the length of the error text
 *
 * If all goes well, return RD_AP_OK.  If the version number
 * is wrong, return RD_AP_VERSION, and if it's not an AUTH_MSG_APPL_ERR
 * type message, return RD_AP_MSG_TYPE.
 *
 * The AUTH_MSG_APPL_ERR message format can be found in mk_err.c
 */

int
krb_rd_err(u_char *in, u_int32_t in_length, int32_t *code, MSG_DAT *m_data)
{
    unsigned char *p = (unsigned char*)in;
    
    unsigned char pvno, type;
    int little_endian;

    pvno = *p++;
    if(pvno != KRB_PROT_VERSION)
	return RD_AP_VERSION;
    
    type = *p++;
    little_endian = type & 1;
    type &= ~1;
    
    if(type != AUTH_MSG_APPL_ERR)
	return RD_AP_MSG_TYPE;
    
    p += krb_get_int(p, (u_int32_t *)&code, 4, little_endian);
    
    m_data->app_data = p;
    m_data->app_length = in_length; /* XXX is this correct? */
    return KSUCCESS;
}

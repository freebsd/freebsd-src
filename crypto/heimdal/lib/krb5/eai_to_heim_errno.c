/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors 
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

#include <krb5_locl.h>

RCSID("$Id: eai_to_heim_errno.c,v 1.1 2000/07/08 13:03:36 joda Exp $");

krb5_error_code
krb5_eai_to_heim_errno(int eai_errno)
{
    switch(eai_errno) {
    case EAI_NOERROR:
	return 0;
    case EAI_ADDRFAMILY:
	return HEIM_EAI_ADDRFAMILY;
    case EAI_AGAIN:
	return HEIM_EAI_AGAIN;
    case EAI_BADFLAGS:
	return HEIM_EAI_BADFLAGS;
    case EAI_FAIL:
	return HEIM_EAI_FAIL;
    case EAI_FAMILY:
	return HEIM_EAI_FAMILY;
    case EAI_MEMORY:
	return HEIM_EAI_MEMORY;
    case EAI_NODATA:
	return HEIM_EAI_NODATA;
    case EAI_NONAME:
	return HEIM_EAI_NONAME;
    case EAI_SERVICE:
	return HEIM_EAI_SERVICE;
    case EAI_SOCKTYPE:
	return HEIM_EAI_SOCKTYPE;
    case EAI_SYSTEM:
	return errno;
    default:
	return HEIM_EAI_UNKNOWN; /* XXX */
    }
}

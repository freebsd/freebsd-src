/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "sia_locl.h"

RCSID("$Id: posix_getpw.c,v 1.1 1999/03/21 17:07:02 joda Exp $");

#ifndef POSIX_GETPWNAM_R
/* 
 * These functions translate from the old Digital UNIX 3.x interface
 * to POSIX.1c.
 */

int
posix_getpwnam_r(const char *name, struct passwd *pwd, 
		 char *buffer, int len, struct passwd **result)
{
    int ret = getpwnam_r(name, pwd, buffer, len);
    if(ret == 0)
	*result = pwd;
    else{
	*result = NULL;
	ret = _Geterrno();
	if(ret == 0){
	    ret = ERANGE;
	    _Seterrno(ret);
	}
    }
    return ret;
}

int
posix_getpwuid_r(uid_t uid, struct passwd *pwd, 
		 char *buffer, int len, struct passwd **result)
{
    int ret = getpwuid_r(uid, pwd, buffer, len);
    if(ret == 0)
	*result = pwd;
    else{
	*result = NULL;
	ret = _Geterrno();
	if(ret == 0){
	    ret = ERANGE;
	    _Seterrno(ret);
	}
    }
    return ret;
}
#endif /* POSIX_GETPWNAM_R */

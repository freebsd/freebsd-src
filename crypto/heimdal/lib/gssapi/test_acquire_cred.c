/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"
#include <err.h>

RCSID("$Id: test_acquire_cred.c,v 1.2 2003/04/06 00:20:37 lha Exp $");

static void
print_time(OM_uint32 time_rec)
{
    if (time_rec == GSS_C_INDEFINITE) {
	printf("cred never expire\n");
    } else {
	time_t t = time_rec;
	printf("expiration time: %s", ctime(&t));
    }
}

int
main(int argc, char **argv)
{
    OM_uint32 major_status, minor_status;
    gss_cred_id_t cred_handle, copy_cred;
    OM_uint32 time_rec;

    major_status = gss_acquire_cred(&minor_status, 
				    GSS_C_NO_NAME,
				    0,
				    NULL,
				    GSS_C_INITIATE,
				    &cred_handle,
				    NULL,
				    &time_rec);
    if (GSS_ERROR(major_status))
	errx(1, "acquire_cred failed");
	

    print_time(time_rec);

    major_status = gss_add_cred (&minor_status,
				 cred_handle,
				 GSS_C_NO_NAME,
				 GSS_KRB5_MECHANISM,
				 GSS_C_INITIATE,
				 0,
				 0,
				 &copy_cred,
				 NULL,
				 &time_rec,
				 NULL);
			    
    if (GSS_ERROR(major_status))
	errx(1, "add_cred failed");

    print_time(time_rec);

    major_status = gss_release_cred(&minor_status,
				    &cred_handle);
    if (GSS_ERROR(major_status))
	errx(1, "release_cred failed");

    major_status = gss_release_cred(&minor_status,
				    &copy_cred);
    if (GSS_ERROR(major_status))
	errx(1, "release_cred failed");

    return 0;
}

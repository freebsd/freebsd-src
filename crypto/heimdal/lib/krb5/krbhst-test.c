/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$Id: krbhst-test.c,v 1.2 2001/06/17 12:22:59 assar Exp $");

int
main(int argc, char **argv)
{
    int i, j;
    krb5_context context;
    int types[] = {KRB5_KRBHST_KDC, KRB5_KRBHST_ADMIN, KRB5_KRBHST_CHANGEPW,
		   KRB5_KRBHST_KRB524};
    const char *type_str[] = {"kdc", "admin", "changepw", "krb524"};
    
    krb5_init_context (&context);
    for(i = 1; i < argc; i++) {
	krb5_krbhst_handle handle;
	char host[MAXHOSTNAMELEN];

	for (j = 0; j < sizeof(types)/sizeof(*types); ++j) {
	    printf ("%s for %s:\n", type_str[j], argv[i]);

	    krb5_krbhst_init(context, argv[i], types[j], &handle);
	    while(krb5_krbhst_next_as_string(context, handle,
					     host, sizeof(host)) == 0)
		printf("%s\n", host);
	    krb5_krbhst_reset(context, handle);
	    printf ("\n");
	}
    }
    return 0;
}

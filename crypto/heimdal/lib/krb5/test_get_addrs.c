/*
 * Copyright (c) 2000 - 2001 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include <err.h>

RCSID("$Id: test_get_addrs.c,v 1.3 2001/01/25 12:45:15 assar Exp $");

/* print all addresses that we find */

static void
print_addresses (krb5_context context, const krb5_addresses *addrs)
{
    int i;
    char buf[256];
    size_t len;
    
    for (i = 0; i < addrs->len; ++i) {
	krb5_print_address (&addrs->val[i], buf, sizeof(buf), &len);
	printf ("%s\n", buf);
    }
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_addresses addrs;

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_get_all_client_addrs (context, &addrs);
    if (ret)
	krb5_err (context, 1, ret, "krb5_get_all_client_addrs");
    printf ("client addresses\n");
    print_addresses (context, &addrs);
    krb5_free_addresses (context, &addrs);

    ret = krb5_get_all_server_addrs (context, &addrs);
    if (ret)
	krb5_err (context, 1, ret, "krb5_get_all_server_addrs");
    printf ("server addresses\n");
    print_addresses (context, &addrs);
    krb5_free_addresses (context, &addrs);
    return 0;
}

/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
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

/*
 *
 */

static void
check_linear(krb5_context context,
	     const char *client_realm,
	     const char *server_realm,
	     const char *realm,
	     ...)
{
    unsigned int num_inrealms = 0, num_realms = 0, n;
    char **inrealms = NULL;
    char **realms = NULL;
    krb5_error_code ret;
    krb5_data tr;
    va_list va;

    krb5_data_zero(&tr);

    va_start(va, realm);

    while (realm) {
	inrealms = erealloc(inrealms, (num_inrealms + 2) * sizeof(inrealms[0]));
	inrealms[num_inrealms] = rk_UNCONST(realm);
	num_inrealms++;
	realm = va_arg(va, const char *);
    }
    if (inrealms)
	inrealms[num_inrealms] = NULL;

    ret = krb5_domain_x500_encode(inrealms, num_inrealms, &tr);
    if (ret)
	krb5_err(context, 1, ret, "krb5_domain_x500_encode");

    ret = krb5_domain_x500_decode(context, tr,
				  &realms, &num_realms,
				  client_realm, server_realm);
    if (ret)
	krb5_err(context, 1, ret, "krb5_domain_x500_decode");

    krb5_data_free(&tr);

    if (num_inrealms != num_realms)
	errx(1, "num_inrealms != num_realms");

    for(n = 0; n < num_realms; n++)
	free(realms[n]);
    free(realms);

    free(inrealms);
}


int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context");


    check_linear(context, "KTH1.SE", "KTH1.SE", NULL);
    check_linear(context, "KTH1.SE", "KTH2.SE", NULL);
    check_linear(context, "KTH1.SE", "KTH3.SE", "KTH2.SE", NULL);
    check_linear(context, "KTH1.SE", "KTH4.SE", "KTH3.SE", "KTH2.SE", NULL);
    check_linear(context, "KTH1.SE", "KTH5.SE", "KTH4.SE", "KTH3.SE", "KTH2.SE", NULL);

    return 0;
}

/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: kuserok.c,v 1.5 1999/12/02 17:05:11 joda Exp $");

/*
 * Return TRUE iff `principal' is allowed to login as `luser'.
 */

krb5_boolean
krb5_kuserok (krb5_context context,
	      krb5_principal principal,
	      const char *luser)
{
    char buf[BUFSIZ];
    struct passwd *pwd;
    FILE *f;
    krb5_realm *realms, *r;
    krb5_error_code ret;
    krb5_boolean b;

    ret = krb5_get_default_realms (context, &realms);
    if (ret)
	return FALSE;

    for (r = realms; *r != NULL; ++r) {
	krb5_principal local_principal;

	ret = krb5_build_principal (context,
				    &local_principal,
				    strlen(*r),
				    *r,
				    luser,
				    NULL);
	if (ret) {
	    krb5_free_host_realm (context, realms);
	    return FALSE;
	}

	b = krb5_principal_compare (context, principal, local_principal);
	krb5_free_principal (context, local_principal);
	if (b) {
	    krb5_free_host_realm (context, realms);
	    return TRUE;
	}
    }
    krb5_free_host_realm (context, realms);

    pwd = getpwnam (luser);	/* XXX - Should use k_getpwnam? */
    if (pwd == NULL)
	return FALSE;
    snprintf (buf, sizeof(buf), "%s/.k5login", pwd->pw_dir);
    f = fopen (buf, "r");
    if (f == NULL)
	return FALSE;
    while (fgets (buf, sizeof(buf), f) != NULL) {
	krb5_principal tmp;

	if(buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';

	ret = krb5_parse_name (context, buf, &tmp);
	if (ret) {
	    fclose (f);
	    return FALSE;
	}
	b = krb5_principal_compare (context, principal, tmp);
	krb5_free_principal (context, tmp);
	if (b) {
	    fclose (f);
	    return TRUE;
	}
    }
    fclose (f);
    return FALSE;
}

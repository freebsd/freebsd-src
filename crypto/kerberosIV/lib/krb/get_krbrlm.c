/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

#include "krb_locl.h"

RCSID("$Id: get_krbrlm.c,v 1.25 1999/12/02 16:58:41 joda Exp $");

/*
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure.  If the
 * config file does not exist, and if n=1, a successful return will occur
 * with r = KRB_REALM (also defined in "krb.h").
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 */

static int
krb_get_lrealm_f(char *r, int n, const char *fname)
{
    char buf[1024];
    char *p;
    int nchar;
    FILE *f;
    int ret = KFAILURE;

    if (n < 0)
        return KFAILURE;
    if(n == 0)
	n = 1;

    f = fopen(fname, "r");
    if (f == 0)
        return KFAILURE;

    for (; n > 0; n--)
        if (fgets(buf, sizeof(buf), f) == 0)
            goto done;

    /* We now have the n:th line, remove initial white space. */
    p = buf + strspn(buf, " \t");

    /* Collect realmname. */
    nchar    = strcspn(p, " \t\n");
    if (nchar == 0 || nchar > REALM_SZ)
        goto done;		/* No realmname */
    strncpy(r, p, nchar);
    r[nchar] = 0;

    /* Does more junk follow? */
    p += nchar;
    nchar = strspn(p, " \t\n");
    if (p[nchar] == 0)
        ret = KSUCCESS;		/* This was a realm name only line. */

  done:
    fclose(f);
    return ret;
}

static const char *no_default_realm = "NO.DEFAULT.REALM";

int
krb_get_lrealm(char *r, int n)
{
    int i;
    char file[MaxPathLen];

    for (i = 0; krb_get_krbconf(i, file, sizeof(file)) == 0; i++)
	if (krb_get_lrealm_f(r, n, file) == KSUCCESS)
	    return KSUCCESS;

    /* When nothing else works try default realm */
    if (n == 1) {
      char *t = krb_get_default_realm();

      if (strcmp(t, no_default_realm) == 0)
	return KFAILURE;	/* Can't figure out default realm */

      strcpy(r, t);
      return KSUCCESS;
    }
    else
	return(KFAILURE);
}

/* Returns local realm if that can be figured out else NO.DEFAULT.REALM */
char *
krb_get_default_realm(void)
{
    static char local_realm[REALM_SZ]; /* Local kerberos realm */
    
    if (local_realm[0] == 0) {
	char *t, hostname[MaxHostNameLen];

	strlcpy(local_realm, no_default_realm, 
			sizeof(local_realm)); /* Provide default */

	gethostname(hostname, sizeof(hostname));
	t = krb_realmofhost(hostname);
	if (t && strcmp(t, no_default_realm) != 0)
	    strlcpy(local_realm, t, sizeof(local_realm));
    }
    return local_realm;
}

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: get_krbrlm.c,v 1.16 1997/05/02 01:26:22 assar Exp $");

/*
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure.  If the
 * config file does not exist, and if n=1, a successful return will occur
 * with r = KRB_REALM (also defined in "krb.h").
 *
 * NOTE: for archaic & compatibility reasons, this routine will only return
 * valid results when n = 1.
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 */

static int
krb_get_lrealm_f(char *r, int n, const char *fname)
{
    FILE *f;
    int ret = KFAILURE;
    f = fopen(fname, "r");
    if(f){
	char buf[REALM_SZ];
	if(fgets(buf, sizeof(buf), f)){
	    char *p = buf + strspn(buf, " \t");
	    p[strcspn(p, " \t\r\n")] = 0;
	    p[REALM_SZ - 1] = 0;
	    strcpy(r, p);
	    ret = KSUCCESS;
	}
	fclose(f);
    }
    return ret;
}

int
krb_get_lrealm(char *r, int n)
{
  static const char *const files[] = KRB_CNF_FILES;
  int i;
  
  const char *dir = getenv("KRBCONFDIR");

  if (n > 1)
    return(KFAILURE);		/* Temporary restriction */

  /* First try user specified file */
  if (dir != 0) {
    char fname[MaxPathLen];
    if(k_concat(fname, sizeof(fname), dir, "/krb.conf", NULL) == 0)
	if (krb_get_lrealm_f(r, n, fname) == KSUCCESS)
	    return KSUCCESS;
  }

  for (i = 0; files[i] != 0; i++)
    if (krb_get_lrealm_f(r, n, files[i]) == KSUCCESS)
      return KSUCCESS;

  /* If nothing else works try LOCALDOMAIN, if it exists */
  if (n == 1)
    {
      char *t, hostname[MaxHostNameLen];
      k_gethostname(hostname, sizeof(hostname));
      t = krb_realmofhost(hostname);
      if (t) {
	strcpy (r, t);
	return KSUCCESS;
      }
      t = strchr(hostname, '.');
      if (t == 0)
	return KFAILURE;	/* No domain part, you loose */

      t++;			/* Skip leading dot and upcase the rest */
      for (; *t; t++, r++)
	*r = toupper(*t);
      *r = 0;
      return(KSUCCESS);
    }
  else
    return(KFAILURE);
}

/* For SunOS5 compat. */
char *
krb_get_default_realm(void)
{
  static char local_realm[REALM_SZ]; /* local kerberos realm */
  if (krb_get_lrealm(local_realm, 1) != KSUCCESS)
    strcpy(local_realm, "NO.DEFAULT.REALM");
  return local_realm;
}

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

RCSID("$Id: getrealm.c,v 1.25 1997/05/02 14:29:14 assar Exp $");

#define MATCH_SUBDOMAINS        0

/*
 * krb_realmofhost.
 * Given a fully-qualified domain-style primary host name,
 * return the name of the Kerberos realm for the host.
 * If the hostname contains no discernable domain, or an error occurs,
 * return the local realm name, as supplied by get_krbrlm().
 * If the hostname contains a domain, but no translation is found,
 * the hostname's domain is converted to upper-case and returned.
 *
 * The format of each line of the translation file is:
 * domain_name kerberos_realm
 * -or-
 * host_name kerberos_realm
 *
 * domain_name should be of the form .XXX.YYY (e.g. .LCS.MIT.EDU)
 * host names should be in the usual form (e.g. FOO.BAR.BAZ)
 */

/* To automagically find the correct realm of a host (without
 * krb.realms) add a text record for your domain with the name of your
 * realm, like this:
 *
 * krb4-realm	IN	TXT	FOO.SE
 *
 * The search is recursive, so you can also add entries for specific
 * hosts. To find the realm of host a.b.c, it first tries
 * krb4-realm.a.b.c, then krb4-realm.b.c and so on.
 */

static int
dns_find_realm(char *hostname, char *realm)
{
    char domain[MaxHostNameLen + sizeof("krb4-realm..")];
    char *p;
    int level = 0;
    struct dns_reply *r;
    
    p = hostname;

    while(1){
	snprintf(domain, sizeof(domain), "krb4-realm.%s.", p);
	r = dns_lookup(domain, "TXT");
	if(r){
	    struct resource_record *rr = r->head;
	    while(rr){
		if(rr->type == T_TXT){
		    strncpy(realm, rr->u.txt, REALM_SZ);
		    realm[REALM_SZ - 1] = 0;
		    dns_free_data(r);
		    return level;
		}
		rr = rr->next;
	    }
	    dns_free_data(r);
	}
	level++;
	p = strchr(p, '.');
	if(p == NULL)
	    break;
	p++;
    }
    return -1;
}


static FILE *
open_krb_realms(void)
{
  static const char *const files[] = KRB_RLM_FILES;
  FILE *res;
  int i;
  
  const char *dir = getenv("KRBCONFDIR");

  /* First try user specified file */
  if (dir != 0) {
    char fname[MaxPathLen];

    if(k_concat(fname, sizeof(fname), dir, "/krb.realms", NULL) == 0)
	if ((res = fopen(fname, "r")) != NULL)
	    return res;
  }

  for (i = 0; files[i] != 0; i++)
    if ((res = fopen(files[i], "r")) != NULL)
      return res;

  return NULL;
}

char *
krb_realmofhost(const char *host)
{
  static char ret_realm[REALM_SZ];
  char *domain;
  FILE *trans_file;
  char trans_host[MaxHostNameLen];
  char trans_realm[REALM_SZ];
  char buf[1024];

  char phost[MaxHostNameLen];
	
  krb_name_to_name(host, phost, sizeof(phost));
	
  domain = strchr(phost, '.');

  /* prepare default */
  if(dns_find_realm(phost, ret_realm) < 0){
      if (domain) {
	  char *cp;
	  
	  strncpy(ret_realm, &domain[1], REALM_SZ);
	  ret_realm[REALM_SZ - 1] = 0;
	  /* Upper-case realm */
	  for (cp = ret_realm; *cp; cp++)
	      *cp = toupper(*cp);
      } else {
	  krb_get_lrealm(ret_realm, 1);
      }
  }

  if ((trans_file = open_krb_realms()) == NULL)
      return(ret_realm); /* krb_errno = KRB_NO_TRANS */

  while (fgets(buf, sizeof(buf), trans_file)) {
      char *save = NULL;
      char *tok = strtok_r(buf, " \t\r\n", &save);
      if(tok == NULL)
	  continue;
      strncpy(trans_host, tok, MaxHostNameLen);
      trans_host[MaxHostNameLen - 1] = 0;
      tok = strtok_r(NULL, " \t\r\n", &save);
      if(tok == NULL)
	  continue;
      strcpy(trans_realm, tok);
      trans_realm[REALM_SZ - 1] = 0;
      if (!strcasecmp(trans_host, phost)) {
	  /* exact match of hostname, so return the realm */
	  strcpy(ret_realm, trans_realm);
	  fclose(trans_file);
	  return(ret_realm);
      }
      if ((trans_host[0] == '.') && domain) { 
	  char *cp = domain;
	  do {
	      if(strcasecmp(trans_host, domain) == 0){
		  /* domain match, save for later */ 
		  strcpy(ret_realm, trans_realm);
		  break;
	      }
	      cp = strchr(cp + 1, '.');
	  } while(MATCH_SUBDOMAINS && cp);
      }
  }
  fclose(trans_file);
  return ret_realm;
}

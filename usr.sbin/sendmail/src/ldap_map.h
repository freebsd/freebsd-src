/*
**  Support for LDAP.
**
**	Contributed by Booker C. Bense <bbense@networking.stanford.edu>.
**	Please go to him for support -- since I (Eric) don't run LDAP, I
**	can't help you at all.
**
**	@(#)ldap_map.h	8.4 (Berkeley) 6/3/97
*/

#ifndef _LDAP_MAP_H
#define _LDAP_MAP_H

#include <sys/time.h>

struct ldap_map_struct
{
	/* needed for ldap_open */
	char		*ldaphost;
	int		ldapport;

	/* Options set in ld struct before ldap_bind_s */
	int		deref;
	int		timelimit;
	int		sizelimit;
	int		ldap_options;

	/* args for ldap_bind_s */
	LDAP		*ld;
	char		*binddn;
	char		*passwd;
	int		method;

	/* args for ldap_search_st */
	char		*base;
	int		scope;
	char		*filter;
	char		*attr[2];
	int		attrsonly;
	struct timeval	timeout;
	LDAPMessage	*res;
};

typedef struct ldap_map_struct	LDAP_MAP_STRUCT;

#define DEFAULT_LDAP_MAP_PORT		LDAP_PORT
#define DEFAULT_LDAP_MAP_SCOPE		LDAP_SCOPE_SUBTREE
#define DEFAULT_LDAP_MAP_BINDDN		NULL
#define DEFAULT_LDAP_MAP_PASSWD		NULL
#define DEFAULT_LDAP_MAP_METHOD		LDAP_AUTH_SIMPLE
#define DEFAULT_LDAP_MAP_TIMELIMIT	5
#define DEFAULT_LDAP_MAP_DEREF		LDAP_DEREF_NEVER
#define DEFAULT_LDAP_MAP_SIZELIMIT	0
#define DEFAULT_LDAP_MAP_ATTRSONLY	0
#define LDAP_MAP_MAX_FILTER		256
#ifdef LDAP_REFERRALS
# define DEFAULT_LDAP_MAP_LDAP_OPTIONS	LDAP_OPT_REFERRALS
#else /* LDAP_REFERRALS */
# define DEFAULT_LDAP_MAP_LDAP_OPTIONS	0
#endif /* LDAP_REFERRALS */

#endif /* _LDAP_MAP_H */

/*
 * Copyright (c) 2001-2002 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: ldap.h,v 1.9 2002/01/11 22:06:50 gshapiro Exp $
 */

#ifndef	SM_LDAP_H
# define SM_LDAP_H

# include <sm/conf.h>
# include <sm/rpool.h>

# ifndef LDAPMAP_MAX_ATTR
#  define LDAPMAP_MAX_ATTR	64
# endif /* ! LDAPMAP_MAX_ATTR */
# ifndef LDAPMAP_MAX_FILTER
#  define LDAPMAP_MAX_FILTER	1024
# endif /* ! LDAPMAP_MAX_FILTER */
# ifndef LDAPMAP_MAX_PASSWD
#  define LDAPMAP_MAX_PASSWD	256
# endif /* ! LDAPMAP_MAX_PASSWD */

# if LDAPMAP

#  if _FFR_LDAP_RECURSION

/* Attribute types */
#   define LDAPMAP_ATTR_NORMAL	0
#   define LDAPMAP_ATTR_DN	1
#   define LDAPMAP_ATTR_FILTER	2
#   define LDAPMAP_ATTR_URL	3
#   define LDAPMAP_ATTR_FINAL	4

/* sm_ldap_results() flags */
#   define SM_LDAP_SINGLEMATCH	0x0001
#   define SM_LDAP_MATCHONLY	0x0002
#  endif /* _FFR_LDAP_RECURSION */

struct sm_ldap_struct
{
	/* needed for ldap_open or ldap_init */
	char		*ldap_host;
	int		ldap_port;
	pid_t		ldap_pid;

	/* options set in ld struct before ldap_bind_s */
	int		ldap_deref;
	time_t		ldap_timelimit;
	int		ldap_sizelimit;
	int		ldap_options;

	/* args for ldap_bind_s */
	LDAP		*ldap_ld;
	char		*ldap_binddn;
	char		*ldap_secret;
	int		ldap_method;

	/* args for ldap_search */
	char		*ldap_base;
	int		ldap_scope;
	char		*ldap_filter;
	char		*ldap_attr[LDAPMAP_MAX_ATTR + 1];
#  if _FFR_LDAP_RECURSION
	int		ldap_attr_type[LDAPMAP_MAX_ATTR + 1];
	char		*ldap_attr_final[LDAPMAP_MAX_ATTR + 1];
#  endif /* _FFR_LDAP_RECURSION */
	bool		ldap_attrsonly;

	/* args for ldap_result */
	struct timeval	ldap_timeout;
	LDAPMessage	*ldap_res;

	/* ldapmap_lookup options */
	char		ldap_attrsep;

	/* Linked list of maps sharing the same LDAP binding */
	void		*ldap_next;
};

typedef struct sm_ldap_struct		SM_LDAP_STRUCT;

#  if _FFR_LDAP_RECURSION
struct sm_ldap_recurse_list
{
	char *lr_search;
	int lr_type;
	struct sm_ldap_recurse_list *lr_next;
};

typedef struct sm_ldap_recurse_list	SM_LDAP_RECURSE_LIST;
#  endif /* _FFR_LDAP_RECURSION */

/* functions */
extern void	sm_ldap_clear __P((SM_LDAP_STRUCT *));
extern bool	sm_ldap_start __P((char *, SM_LDAP_STRUCT *));
extern int	sm_ldap_search __P((SM_LDAP_STRUCT *, char *));
#  if _FFR_LDAP_RECURSION
extern int	sm_ldap_results __P((SM_LDAP_STRUCT *, int, int, char,
				     SM_RPOOL_T *, char **,
				     SM_LDAP_RECURSE_LIST *));
#  endif /* _FFR_LDAP_RECURSION */
extern void	sm_ldap_setopts __P((LDAP *, SM_LDAP_STRUCT *));
extern int	sm_ldap_geterrno __P((LDAP *));
extern void	sm_ldap_close __P((SM_LDAP_STRUCT *));
# endif /* LDAPMAP */

#endif /* ! SM_LDAP_H */

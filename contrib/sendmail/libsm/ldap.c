/*
 * Copyright (c) 2001-2002 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: ldap.c,v 1.18 2002/01/11 22:06:51 gshapiro Exp $")

#if LDAPMAP
# include <sys/types.h>
# include <errno.h>
# include <setjmp.h>
# include <stdlib.h>
# include <unistd.h>

# include <sm/bitops.h>
# include <sm/clock.h>
# include <sm/conf.h>
# include <sm/debug.h>
# include <sm/errstring.h>
# include <sm/ldap.h>
# include <sm/string.h>
# include <sm/sysexits.h>

SM_DEBUG_T SmLDAPTrace = SM_DEBUG_INITIALIZER("sm_trace_ldap",
	"@(#)$Debug: sm_trace_ldap - trace LDAP operations $");

static void	ldaptimeout __P((int));

/*
**  SM_LDAP_CLEAR -- set default values for SM_LDAP_STRUCT
**
**	Parameters:
**		lmap -- pointer to SM_LDAP_STRUCT to clear
**
**	Returns:
**		None.
**
*/

void
sm_ldap_clear(lmap)
	SM_LDAP_STRUCT *lmap;
{
	if (lmap == NULL)
		return;

	lmap->ldap_host = NULL;
	lmap->ldap_port = LDAP_PORT;
	lmap->ldap_deref = LDAP_DEREF_NEVER;
	lmap->ldap_timelimit = LDAP_NO_LIMIT;
	lmap->ldap_sizelimit = LDAP_NO_LIMIT;
# ifdef LDAP_REFERRALS
	lmap->ldap_options = LDAP_OPT_REFERRALS;
# else /* LDAP_REFERRALS */
	lmap->ldap_options = 0;
# endif /* LDAP_REFERRALS */
	lmap->ldap_attrsep = '\0';
	lmap->ldap_binddn = NULL;
	lmap->ldap_secret = NULL;
	lmap->ldap_method = LDAP_AUTH_SIMPLE;
	lmap->ldap_base = NULL;
	lmap->ldap_scope = LDAP_SCOPE_SUBTREE;
	lmap->ldap_attrsonly = LDAPMAP_FALSE;
	lmap->ldap_timeout.tv_sec = 0;
	lmap->ldap_timeout.tv_usec = 0;
	lmap->ldap_ld = NULL;
	lmap->ldap_filter = NULL;
	lmap->ldap_attr[0] = NULL;
#if _FFR_LDAP_RECURSION
	lmap->ldap_attr_type[0] = LDAPMAP_ATTR_NORMAL;
	lmap->ldap_attr_final[0] = NULL;
#endif /* _FFR_LDAP_RECURSION */
	lmap->ldap_res = NULL;
	lmap->ldap_next = NULL;
	lmap->ldap_pid = 0;
}

/*
**  SM_LDAP_START -- actually connect to an LDAP server
**
**	Parameters:
**		name -- name of map for debug output.
**		lmap -- the LDAP map being opened.
**
**	Returns:
**		true if connection is successful, false otherwise.
**
**	Side Effects:
**		Populates lmap->ldap_ld.
*/

static jmp_buf	LDAPTimeout;

#define SM_LDAP_SETTIMEOUT(to)						\
do									\
{									\
	if (to != 0)							\
	{								\
		if (setjmp(LDAPTimeout) != 0)				\
		{							\
			errno = ETIMEDOUT;				\
			return false;					\
		}							\
		ev = sm_setevent(to, ldaptimeout, 0);			\
	}								\
} while (0)

#define SM_LDAP_CLEARTIMEOUT()						\
do									\
{									\
	if (ev != NULL)							\
		sm_clrevent(ev);					\
} while (0)

bool
sm_ldap_start(name, lmap)
	char *name;
	SM_LDAP_STRUCT *lmap;
{
	int bind_result;
	int save_errno;
	SM_EVENT *ev = NULL;
	LDAP *ld;

	if (sm_debug_active(&SmLDAPTrace, 2))
		sm_dprintf("ldapmap_start(%s)\n", name == NULL ? "" : name);

	if (sm_debug_active(&SmLDAPTrace, 9))
		sm_dprintf("ldapmap_start(%s, %d)\n",
			   lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host,
			   lmap->ldap_port);

# if USE_LDAP_INIT
	ld = ldap_init(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;
# else /* USE_LDAP_INIT */
	/*
	**  If using ldap_open(), the actual connection to the server
	**  happens now so we need the timeout here.  For ldap_init(),
	**  the connection happens at bind time.
	*/

	SM_LDAP_SETTIMEOUT(lmap->ldap_timeout.tv_sec);
	ld = ldap_open(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;

	/* clear the event if it has not sprung */
	SM_LDAP_CLEARTIMEOUT();
# endif /* USE_LDAP_INIT */

	errno = save_errno;
	if (ld == NULL)
		return false;

	sm_ldap_setopts(ld, lmap);

# if USE_LDAP_INIT
	/*
	**  If using ldap_init(), the actual connection to the server
	**  happens at ldap_bind_s() so we need the timeout here.
	*/

	SM_LDAP_SETTIMEOUT(lmap->ldap_timeout.tv_sec);
# endif /* USE_LDAP_INIT */

# ifdef LDAP_AUTH_KRBV4
	if (lmap->ldap_method == LDAP_AUTH_KRBV4 &&
	    lmap->ldap_secret != NULL)
	{
		/*
		**  Need to put ticket in environment here instead of
		**  during parseargs as there may be different tickets
		**  for different LDAP connections.
		*/

		(void) putenv(lmap->ldap_secret);
	}
# endif /* LDAP_AUTH_KRBV4 */

	bind_result = ldap_bind_s(ld, lmap->ldap_binddn,
				  lmap->ldap_secret, lmap->ldap_method);

# if USE_LDAP_INIT
	/* clear the event if it has not sprung */
	SM_LDAP_CLEARTIMEOUT();
# endif /* USE_LDAP_INIT */

	if (bind_result != LDAP_SUCCESS)
	{
		errno = bind_result + E_LDAPBASE;
		return false;
	}

	/* Save PID to make sure only this PID closes the LDAP connection */
	lmap->ldap_pid = getpid();
	lmap->ldap_ld = ld;
	return true;
}

/* ARGSUSED */
static void
ldaptimeout(unused)
	int unused;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(LDAPTimeout, 1);
}

/*
**  SM_LDAP_SEARCH -- iniate LDAP search
**
**	Initiate an LDAP search, return the msgid.
**	The calling function must collect the results.
**
**	Parameters:
**		lmap -- LDAP map information
**		key -- key to substitute in LDAP filter
**
**	Returns:
**		-1 on failure, msgid on success
**
*/

int
sm_ldap_search(lmap, key)
	SM_LDAP_STRUCT *lmap;
	char *key;
{
	int msgid;
	char *fp, *p, *q;
	char filter[LDAPMAP_MAX_FILTER + 1];

	/* substitute key into filter, perhaps multiple times */
	memset(filter, '\0', sizeof filter);
	fp = filter;
	p = lmap->ldap_filter;
	while ((q = strchr(p, '%')) != NULL)
	{
		if (q[1] == 's')
		{
			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
					   "%.*s%s", (int) (q - p), p, key);
			fp += strlen(fp);
			p = q + 2;
		}
		else if (q[1] == '0')
		{
			char *k = key;

			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
					   "%.*s", (int) (q - p), p);
			fp += strlen(fp);
			p = q + 2;

			/* Properly escape LDAP special characters */
			while (SPACELEFT(filter, fp) > 0 &&
			       *k != '\0')
			{
				if (*k == '*' || *k == '(' ||
				    *k == ')' || *k == '\\')
				{
					(void) sm_strlcat(fp,
						       (*k == '*' ? "\\2A" :
							(*k == '(' ? "\\28" :
							 (*k == ')' ? "\\29" :
							  (*k == '\\' ? "\\5C" :
							   "\00")))),
						SPACELEFT(filter, fp));
					fp += strlen(fp);
					k++;
				}
				else
					*fp++ = *k++;
			}
		}
		else
		{
			(void) sm_snprintf(fp, SPACELEFT(filter, fp),
				"%.*s", (int) (q - p + 1), p);
			p = q + (q[1] == '%' ? 2 : 1);
			fp += strlen(fp);
		}
	}
	(void) sm_strlcpy(fp, p, SPACELEFT(filter, fp));
	if (sm_debug_active(&SmLDAPTrace, 20))
		sm_dprintf("ldap search filter=%s\n", filter);

	lmap->ldap_res = NULL;
	msgid = ldap_search(lmap->ldap_ld, lmap->ldap_base, lmap->ldap_scope,
			    filter,
			    (lmap->ldap_attr[0] == NULL ? NULL :
			     lmap->ldap_attr),
			    lmap->ldap_attrsonly);
	return msgid;
}

# if _FFR_LDAP_RECURSION
/*
**  SM_LDAP_RESULTS -- return results from an LDAP lookup in result
**
**	Parameters:
**		lmap -- pointer to SM_LDAP_STRUCT in use
**		msgid -- msgid returned by sm_ldap_search()
**		flags -- flags for the lookup
**		delim -- delimiter for result concatenation
**		rpool -- memory pool for storage
**		result -- return string
**		recurse -- recursion list
**
**	Returns:
**		status (sysexit)
*/

# define LDAPMAP_ERROR_CLEANUP()				\
{								\
	if (lmap->ldap_res != NULL)				\
	{							\
		ldap_msgfree(lmap->ldap_res);			\
		lmap->ldap_res = NULL;				\
	}							\
	(void) ldap_abandon(lmap->ldap_ld, msgid);		\
}

static int
ldapmap_add_recurse(top, item, type, rpool)
	SM_LDAP_RECURSE_LIST **top;
	char *item;
	int type;
	SM_RPOOL_T *rpool;
{
	SM_LDAP_RECURSE_LIST *p;
	SM_LDAP_RECURSE_LIST *last;

	last = NULL;
	for (p = *top; p != NULL; p = p->lr_next)
	{
		if (strcasecmp(item, p->lr_search) == 0 &&
		    type == p->lr_type)
		{
			/* already on list */
			return 1;
		}
		last = p;
	}

	/* not on list, add it */
	p = sm_rpool_malloc_x(rpool, sizeof *p);
	p->lr_search = sm_rpool_strdup_x(rpool, item);
	p->lr_type = type;
	p->lr_next = NULL;
	if (last == NULL)
		*top = p;
	else
		last->lr_next = p;
	return 0;
}

int
sm_ldap_results(lmap, msgid, flags, delim, rpool, result, recurse)
	SM_LDAP_STRUCT *lmap;
	int msgid;
	int flags;
	char delim;
	SM_RPOOL_T *rpool;
	char **result;
	SM_LDAP_RECURSE_LIST *recurse;
{
	bool toplevel;
	int i;
	int entries = 0;
	int statp;
	int vsize;
	int ret;
	int save_errno;
	char *p;

	/* Are we the top top level of the search? */
	toplevel = (recurse == NULL);

	/* Get results */
	statp = EX_NOTFOUND;
	while ((ret = ldap_result(lmap->ldap_ld, msgid, 0,
				  (lmap->ldap_timeout.tv_sec == 0 ? NULL :
				   &(lmap->ldap_timeout)),
				  &(lmap->ldap_res))) == LDAP_RES_SEARCH_ENTRY)
	{
		LDAPMessage *entry;

		if (bitset(SM_LDAP_SINGLEMATCH, flags))
		{
			entries += ldap_count_entries(lmap->ldap_ld,
						      lmap->ldap_res);
			if (entries > 1)
			{
				LDAPMAP_ERROR_CLEANUP();
				errno = ENOENT;
				return EX_NOTFOUND;
			}
		}

		/* If we don't want multiple values and we have one, break */
		if (delim == '\0' && *result != NULL)
			break;

		/* Cycle through all entries */
		for (entry = ldap_first_entry(lmap->ldap_ld, lmap->ldap_res);
		     entry != NULL;
		     entry = ldap_next_entry(lmap->ldap_ld, lmap->ldap_res))
		{
			BerElement *ber;
			char *attr;
			char **vals = NULL;
			char *dn;

			/*
			**  If matching only and found an entry,
			**  no need to spin through attributes
			*/

			if (statp == EX_OK &&
			    bitset(SM_LDAP_MATCHONLY, flags))
				continue;

			/* record completed DN's to prevent loops */
			dn = ldap_get_dn(lmap->ldap_ld, entry);
			if (dn == NULL)
			{
				save_errno = sm_ldap_geterrno(lmap->ldap_ld);
				save_errno += E_LDAPBASE;
				LDAPMAP_ERROR_CLEANUP();
				errno = save_errno;
				return EX_OSERR;
			}

			switch (ldapmap_add_recurse(&recurse, dn,
						    LDAPMAP_ATTR_NORMAL,
						    rpool))
			{
			  case -1:
				/* error adding */
				ldap_memfree(dn);
				LDAPMAP_ERROR_CLEANUP();
				errno = ENOMEM;
				return EX_OSERR;

			  case 1:
				/* already on list, skip it */
				ldap_memfree(dn);
				continue;
			}
			ldap_memfree(dn);

# if !defined(LDAP_VERSION_MAX) && !defined(LDAP_OPT_SIZELIMIT)
			/*
			**  Reset value to prevent lingering
			**  LDAP_DECODING_ERROR due to
			**  OpenLDAP 1.X's hack (see below)
			*/

			lmap->ldap_ld->ld_errno = LDAP_SUCCESS;
# endif /* !defined(LDAP_VERSION_MAX) !defined(LDAP_OPT_SIZELIMIT) */

			for (attr = ldap_first_attribute(lmap->ldap_ld, entry,
							 &ber);
			     attr != NULL;
			     attr = ldap_next_attribute(lmap->ldap_ld, entry,
							ber))
			{
				char *tmp, *vp_tmp;
				int type;

				for (i = 0; lmap->ldap_attr[i] != NULL; i++)
				{
					if (sm_strcasecmp(lmap->ldap_attr[i],
							  attr) == 0)
					{
						type = lmap->ldap_attr_type[i];
						break;
					}
				}
				if (lmap->ldap_attr[i] == NULL)
				{
					/* attribute not requested */
# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					LDAPMAP_ERROR_CLEANUP();
					errno = EFAULT;
					return EX_SOFTWARE;
				}

				if (lmap->ldap_attrsonly == LDAPMAP_FALSE)
				{
					vals = ldap_get_values(lmap->ldap_ld,
							       entry,
							       attr);
					if (vals == NULL)
					{
						save_errno = sm_ldap_geterrno(lmap->ldap_ld);
						if (save_errno == LDAP_SUCCESS)
						{
# if USING_NETSCAPE_LDAP
							ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
							continue;
						}

						/* Must be an error */
						save_errno += E_LDAPBASE;
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						LDAPMAP_ERROR_CLEANUP();
						errno = save_errno;
						return EX_TEMPFAIL;
					}
				}

				statp = EX_OK;

# if !defined(LDAP_VERSION_MAX) && !defined(LDAP_OPT_SIZELIMIT)
				/*
				**  Reset value to prevent lingering
				**  LDAP_DECODING_ERROR due to
				**  OpenLDAP 1.X's hack (see below)
				*/

				lmap->ldap_ld->ld_errno = LDAP_SUCCESS;
# endif /* !defined(LDAP_VERSION_MAX) !defined(LDAP_OPT_SIZELIMIT) */

				/*
				**  If matching only,
				**  no need to spin through entries
				*/

				if (bitset(SM_LDAP_MATCHONLY, flags))
				{
					if (lmap->ldap_attrsonly == LDAPMAP_FALSE)
						ldap_value_free(vals);

# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					continue;
				}

				/*
				**  If we don't want multiple values,
				**  return first found.
				*/

				if (delim == '\0')
				{
					if (lmap->ldap_attrsonly == LDAPMAP_TRUE)
					{
						*result = sm_rpool_strdup_x(rpool,
									    attr);
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						break;
					}

					if (vals[0] == NULL)
					{
						ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						continue;
					}

					vsize = strlen(vals[0]) + 1;
					if (lmap->ldap_attrsep != '\0')
						vsize += strlen(attr) + 1;
					*result = sm_rpool_malloc_x(rpool,
								    vsize);
					if (lmap->ldap_attrsep != '\0')
						sm_snprintf(*result, vsize,
							    "%s%c%s",
							    attr,
							    lmap->ldap_attrsep,
							    vals[0]);
					else
						sm_strlcpy(*result, vals[0],
							   vsize);
					ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					break;
				}

				/* attributes only */
				if (lmap->ldap_attrsonly == LDAPMAP_TRUE)
				{
					if (*result == NULL)
						*result = sm_rpool_strdup_x(rpool,
									    attr);
					else
					{
						vsize = strlen(*result) +
							strlen(attr) + 2;
						tmp = sm_rpool_malloc_x(rpool,
									vsize);
						(void) sm_snprintf(tmp,
							vsize, "%s%c%s",
							*result, delim,
							attr);
						*result = tmp;
					}
# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					continue;
				}

				/*
				**  If there is more than one,
				**  munge then into a map_coldelim
				**  separated string
				*/

				vsize = 0;
				for (i = 0; vals[i] != NULL; i++)
				{
					if (type == LDAPMAP_ATTR_DN ||
					    type == LDAPMAP_ATTR_FILTER ||
					    type == LDAPMAP_ATTR_URL)
					{
						if (ldapmap_add_recurse(&recurse,
									vals[i],
									type) < 0)
						{
							LDAPMAP_ERROR_CLEANUP();
							errno = ENOMEM;
							return EX_OSERR;
						}
					}
					if (type != LDAPMAP_ATTR_NORMAL)
					{
#  if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
#  endif /* USING_NETSCAPE_LDAP */
						continue;
					}
					vsize += strlen(vals[i]) + 1;
					if (lmap->ldap_attrsep != '\0')
						vsize += strlen(attr) + 1;
				}
				vp_tmp = sm_rpool_malloc_x(rpool, vsize);
				*vp_tmp = '\0';

				p = vp_tmp;
				for (i = 0; vals[i] != NULL; i++)
				{
					if (lmap->ldap_attrsep != '\0')
					{
						p += sm_strlcpy(p, attr,
								vsize - (p - vp_tmp));
						*p++ = lmap->ldap_attrsep;
					}
					p += sm_strlcpy(p, vals[i],
							vsize - (p - vp_tmp));
					if (p >= vp_tmp + vsize)
					{
						/* Internal error: buffer too small for LDAP values */
						LDAPMAP_ERROR_CLEANUP();
						errno = ENOMEM;
						return EX_OSERR;
					}
					if (vals[i + 1] != NULL)
						*p++ = delim;
				}

				ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
				ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
				if (*result == NULL)
				{
					*result = vp_tmp;
					continue;
				}
				vsize = strlen(*result) + strlen(vp_tmp) + 2;
				tmp = sm_rpool_malloc_x(rpool, vsize);
				(void) sm_snprintf(tmp, vsize, "%s%c%s",
						   *result, delim, vp_tmp);
				*result = tmp;
			}
			save_errno = sm_ldap_geterrno(lmap->ldap_ld);

			/*
			**  We check save_errno != LDAP_DECODING_ERROR since
			**  OpenLDAP 1.X has a very ugly *undocumented*
			**  hack of returning this error code from
			**  ldap_next_attribute() if the library freed the
			**  ber attribute.  See:
			**  http://www.openldap.org/lists/openldap-devel/9901/msg00064.html
			*/

			if (save_errno != LDAP_SUCCESS &&
			    save_errno != LDAP_DECODING_ERROR)
			{
				/* Must be an error */
				save_errno += E_LDAPBASE;
				LDAPMAP_ERROR_CLEANUP();
				errno = save_errno;
				return EX_TEMPFAIL;
			}

			/* We don't want multiple values and we have one */
			if (delim == '\0' && *result != NULL)
				break;
		}
		save_errno = sm_ldap_geterrno(lmap->ldap_ld);
		if (save_errno != LDAP_SUCCESS &&
		    save_errno != LDAP_DECODING_ERROR)
		{
			/* Must be an error */
			save_errno += E_LDAPBASE;
			LDAPMAP_ERROR_CLEANUP();
			errno = save_errno;
			return EX_TEMPFAIL;
		}
		ldap_msgfree(lmap->ldap_res);
		lmap->ldap_res = NULL;
	}

	if (ret == 0)
		save_errno = ETIMEDOUT;
	else
		save_errno = sm_ldap_geterrno(lmap->ldap_ld);
	if (save_errno != LDAP_SUCCESS)
	{
		statp = EX_TEMPFAIL;
		if (ret != 0)
		{
			switch (save_errno)
			{
#ifdef LDAP_SERVER_DOWN
			  case LDAP_SERVER_DOWN:
#endif /* LDAP_SERVER_DOWN */
			  case LDAP_TIMEOUT:
			  case LDAP_UNAVAILABLE:
				/* server disappeared, try reopen on next search */
				statp = EX_RESTART;
				break;
			}
			save_errno += E_LDAPBASE;
		}
		LDAPMAP_ERROR_CLEANUP();
		errno = save_errno;
		return statp;
	}

	if (lmap->ldap_res != NULL)
	{
		ldap_msgfree(lmap->ldap_res);
		lmap->ldap_res = NULL;
	}

	if (toplevel)
	{
		SM_LDAP_RECURSE_LIST *rl;

		/*
		**  Spin through the built-up recurse list at the top
		**  of the recursion.  Since new items are added at the
		**  end of the shared list, we actually only ever get
		**  one level of recursion before things pop back to the
		**  top.  Any items added to the list during that recursion
		**  will be expanded by the top level.
		*/

		for (rl = recurse; rl != NULL; rl = rl->lr_next)
		{
			int sid;
			int status;

			if (rl->lr_type == LDAPMAP_ATTR_NORMAL)
			{
				/* already expanded */
				continue;
			}
			else if (rl->lr_type == LDAPMAP_ATTR_DN)
			{
				/* do DN search */
				sid = ldap_search(lmap->ldap_ld,
						  rl->lr_search,
						  lmap->ldap_scope,
						  "(objectClass=*)",
						  lmap->ldap_attr_final,
						  lmap->ldap_attrsonly);
			}
			else if (rl->lr_type == LDAPMAP_ATTR_FILTER)
			{
				/* do new search */
				sid = ldap_search(lmap->ldap_ld,
						  lmap->ldap_base,
						  lmap->ldap_scope,
						  rl->lr_search,
						  lmap->ldap_attr_final,
						  lmap->ldap_attrsonly);
			}
			else if (rl->lr_type == LDAPMAP_ATTR_URL)
			{
				/* do new URL search */
				sid = ldap_url_search(lmap->ldap_ld,
						      rl->lr_search,
						      lmap->ldap_attrsonly);
			}
			else
			{
				/* unknown or illegal attribute type */
				errno = EFAULT;
				return EX_SOFTWARE;
			}

			/* Collect results */
			if (sid == -1)
			{
				save_errno = sm_ldap_geterrno(lmap->ldap_ld);
				statp = EX_TEMPFAIL;
				switch (save_errno)
				{
#ifdef LDAP_SERVER_DOWN
				  case LDAP_SERVER_DOWN:
#endif /* LDAP_SERVER_DOWN */
				  case LDAP_TIMEOUT:
				  case LDAP_UNAVAILABLE:
					/* server disappeared, try reopen on next search */
					statp = EX_RESTART;
					break;
				}
				errno = save_errno + E_LDAPBASE;
				return statp;
			}

			status = sm_ldap_results(lmap, sid, flags, delim,
						 rpool, result, recurse);
			save_errno = errno;
			if (status != EX_OK && status != EX_NOTFOUND)
			{
				errno = save_errno;
				return status;
			}

			/* Mark as done */
			rl->lr_type = LDAPMAP_ATTR_NORMAL;
		}
	}
	return statp;
}
#endif /* _FFR_LDAP_RECURSION */

/*
**  SM_LDAP_CLOSE -- close LDAP connection
**
**	Parameters:
**		lmap -- LDAP map information
**
**	Returns:
**		None.
**
*/

void
sm_ldap_close(lmap)
	SM_LDAP_STRUCT *lmap;
{
	if (lmap->ldap_ld == NULL)
		return;

	if (lmap->ldap_pid == getpid())
		ldap_unbind(lmap->ldap_ld);
	lmap->ldap_ld = NULL;
	lmap->ldap_pid = 0;
}

/*
**  SM_LDAP_SETOPTS -- set LDAP options
**
**	Parameters:
**		ld -- LDAP session handle
**		lmap -- LDAP map information
**
**	Returns:
**		None.
**
*/

void
sm_ldap_setopts(ld, lmap)
	LDAP *ld;
	SM_LDAP_STRUCT *lmap;
{
# if USE_LDAP_SET_OPTION
	ldap_set_option(ld, LDAP_OPT_DEREF, &lmap->ldap_deref);
	if (bitset(LDAP_OPT_REFERRALS, lmap->ldap_options))
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
	else
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
	ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &lmap->ldap_sizelimit);
	ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &lmap->ldap_timelimit);
# else /* USE_LDAP_SET_OPTION */
	/* From here on in we can use ldap internal timelimits */
	ld->ld_deref = lmap->ldap_deref;
	ld->ld_options = lmap->ldap_options;
	ld->ld_sizelimit = lmap->ldap_sizelimit;
	ld->ld_timelimit = lmap->ldap_timelimit;
# endif /* USE_LDAP_SET_OPTION */
}

/*
**  SM_LDAP_GETERRNO -- get ldap errno value
**
**	Parameters:
**		ld -- LDAP session handle
**
**	Returns:
**		LDAP errno.
**
*/

int
sm_ldap_geterrno(ld)
	LDAP *ld;
{
	int err = LDAP_SUCCESS;

# if defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3
	(void) ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
# else /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
#  ifdef LDAP_OPT_SIZELIMIT
	err = ldap_get_lderrno(ld, NULL, NULL);
#  else /* LDAP_OPT_SIZELIMIT */
	err = ld->ld_errno;

	/*
	**  Reset value to prevent lingering LDAP_DECODING_ERROR due to
	**  OpenLDAP 1.X's hack (see above)
	*/

	ld->ld_errno = LDAP_SUCCESS;
#  endif /* LDAP_OPT_SIZELIMIT */
# endif /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
	return err;
}
# endif /* LDAPMAP */

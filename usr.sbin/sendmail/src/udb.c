/*
 * Copyright (c) 1983, 1995 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "sendmail.h"

#ifndef lint
#if USERDB
static char sccsid [] = "@(#)udb.c	8.33 (Berkeley) 11/29/95 (with USERDB)";
#else
static char sccsid [] = "@(#)udb.c	8.33 (Berkeley) 11/29/95 (without USERDB)";
#endif
#endif

#if USERDB

#include <errno.h>

#ifdef NEWDB
# include <db.h>
#else
# define DBT	struct _data_base_thang_
DBT
{
	void	*data;		/* pointer to data */
	size_t	size;		/* length of data */
};
#endif

#ifdef HESIOD
# include <hesiod.h>
#endif /* HESIOD */

/*
**  UDB.C -- interface between sendmail and Berkeley User Data Base.
**
**	This depends on the 4.4BSD db package.
*/


struct udbent
{
	char	*udb_spec;		/* string version of spec */
	int	udb_type;		/* type of entry */
	char	*udb_default;		/* default host for outgoing mail */
	union
	{
		/* type UE_REMOTE -- do remote call for lookup */
		struct
		{
			struct sockaddr_in _udb_addr;	/* address */
			int		_udb_timeout;	/* timeout */
		} udb_remote;
#define udb_addr	udb_u.udb_remote._udb_addr
#define udb_timeout	udb_u.udb_remote._udb_timeout

		/* type UE_FORWARD -- forward message to remote */
		struct
		{
			char	*_udb_fwdhost;	/* name of forward host */
		} udb_forward;
#define udb_fwdhost	udb_u.udb_forward._udb_fwdhost

#ifdef NEWDB
		/* type UE_FETCH -- lookup in local database */
		struct
		{
			char	*_udb_dbname;	/* pathname of database */
			DB	*_udb_dbp;	/* open database ptr */
		} udb_lookup;
#define udb_dbname	udb_u.udb_lookup._udb_dbname
#define udb_dbp		udb_u.udb_lookup._udb_dbp
#endif
	} udb_u;
};

#define UDB_EOLIST	0	/* end of list */
#define UDB_SKIP	1	/* skip this entry */
#define UDB_REMOTE	2	/* look up in remote database */
#define UDB_DBFETCH	3	/* look up in local database */
#define UDB_FORWARD	4	/* forward to remote host */
#define UDB_HESIOD	5	/* look up via hesiod */

#define MAXUDBENT	10	/* maximum number of UDB entries */


struct option
{
	char	*name;
	char	*val;
};
/*
**  UDBEXPAND -- look up user in database and expand
**
**	Parameters:
**		a -- address to expand.
**		sendq -- pointer to head of sendq to put the expansions in.
**		aliaslevel -- the current alias nesting depth.
**		e -- the current envelope.
**
**	Returns:
**		EX_TEMPFAIL -- if something "odd" happened -- probably due
**			to accessing a file on an NFS server that is down.
**		EX_OK -- otherwise.
**
**	Side Effects:
**		Modifies sendq.
*/

int	UdbPort = 1616;
int	UdbTimeout = 10;

struct udbent	UdbEnts[MAXUDBENT + 1];
int		UdbSock = -1;
bool		UdbInitialized = FALSE;

int
udbexpand(a, sendq, aliaslevel, e)
	register ADDRESS *a;
	ADDRESS **sendq;
	int aliaslevel;
	register ENVELOPE *e;
{
	int i;
	DBT key;
	DBT info;
	bool breakout;
	register struct udbent *up;
	int keylen;
	int naddrs;
	char keybuf[MAXKEY];
	char buf[BUFSIZ];

	if (tTd(28, 1))
		printf("udbexpand(%s)\n", a->q_paddr);

	/* make certain we are supposed to send to this address */
	if (bitset(QDONTSEND|QVERIFIED, a->q_flags))
		return EX_OK;
	e->e_to = a->q_paddr;

	/* on first call, locate the database */
	if (!UdbInitialized)
	{
		extern int _udbx_init();

		if (_udbx_init() == EX_TEMPFAIL)
			return EX_TEMPFAIL;
	}

	/* short circuit the process if no chance of a match */
	if (UdbSpec == NULL || UdbSpec[0] == '\0')
		return EX_OK;

	/* short circuit name begins with '\\' since it can't possibly match */
	if (a->q_user[0] == '\\')
		return EX_OK;

	/* if name is too long, assume it won't match */
	if (strlen(a->q_user) > sizeof keybuf - 12)
		return EX_OK;

	/* if name begins with a colon, it indicates our metadata */
	if (a->q_user[0] == ':')
		return EX_OK;

	/* build actual database key */
	(void) strcpy(keybuf, a->q_user);
	(void) strcat(keybuf, ":maildrop");
	keylen = strlen(keybuf);

	breakout = FALSE;
	for (up = UdbEnts; !breakout; up++)
	{
		char *user;

		/*
		**  Select action based on entry type.
		**
		**	On dropping out of this switch, "class" should
		**	explain the type of the data, and "user" should
		**	contain the user information.
		*/

		switch (up->udb_type)
		{
#ifdef NEWDB
		  case UDB_DBFETCH:
			key.data = keybuf;
			key.size = keylen;
			if (tTd(28, 80))
				printf("udbexpand: trying %s (%d) via db\n",
					keybuf, keylen);
			i = (*up->udb_dbp->seq)(up->udb_dbp, &key, &info, R_CURSOR);
			if (i > 0 || info.size <= 0)
			{
				if (tTd(28, 2))
					printf("udbexpand: no match on %s (%d)\n",
						keybuf, keylen);
				continue;
			}
			if (tTd(28, 80))
				printf("udbexpand: match %.*s: %.*s\n",
					key.size, key.data, info.size, info.data);

			naddrs = 0;
			a->q_flags &= ~QSELFREF;
			while (i == 0 && key.size == keylen &&
					bcmp(key.data, keybuf, keylen) == 0)
			{
				if (bitset(EF_VRFYONLY, e->e_flags))
				{
					a->q_flags |= QVERIFIED;
					return EX_OK;
				}

				breakout = TRUE;
				if (info.size < sizeof buf)
					user = buf;
				else
					user = xalloc(info.size + 1);
				bcopy(info.data, user, info.size);
				user[info.size] = '\0';

				message("expanded to %s", user);
#ifdef LOG
				if (LogLevel >= 10)
					syslog(LOG_INFO, "%s: expand %.100s => %s",
						e->e_id, e->e_to,
						shortenstring(user, 203));
#endif
				naddrs += sendtolist(user, a, sendq, aliaslevel + 1, e);

				if (user != buf)
					free(user);

				/* get the next record */
				i = (*up->udb_dbp->seq)(up->udb_dbp, &key, &info, R_NEXT);
			}

			/* if nothing ever matched, try next database */
			if (!breakout)
				continue;

			if (naddrs > 0 && !bitset(QSELFREF, a->q_flags))
			{
				if (tTd(28, 5))
				{
					printf("udbexpand: QDONTSEND ");
					printaddr(a, FALSE);
				}
				a->q_flags |= QDONTSEND;
			}
			if (i < 0)
			{
				syserr("udbexpand: db-get %.*s stat %d",
					key.size, key.data, i);
				return EX_TEMPFAIL;
			}

			/*
			**  If this address has a -request address, reflect
			**  it into the envelope.
			*/

			(void) strcpy(keybuf, a->q_user);
			(void) strcat(keybuf, ":mailsender");
			keylen = strlen(keybuf);
			key.data = keybuf;
			key.size = keylen;
			i = (*up->udb_dbp->get)(up->udb_dbp, &key, &info, 0);
			if (i != 0 || info.size <= 0)
				break;
			a->q_owner = xalloc(info.size + 1);
			bcopy(info.data, a->q_owner, info.size);
			a->q_owner[info.size] = '\0';

			/* announce delivery; NORECEIPT bit set later */
			if (e->e_xfp != NULL)
			{
				fprintf(e->e_xfp,
					"Message delivered to mailing list %s\n",
					a->q_paddr);
			}
			e->e_flags |= EF_SENDRECEIPT;
			a->q_flags |= QDELIVERED|QEXPANDED;
			break;
#endif

#ifdef HESIOD
		  case UDB_HESIOD:
			key.data = keybuf;
			key.size = keylen;
			if (tTd(28, 80))
				printf("udbexpand: trying %s (%d) via hesiod\n",
					keybuf, keylen);
			/* look up the key via hesiod */
			i = hes_udb_get(&key, &info);
			if (i < 0)
			{
				syserr("udbexpand: hesiod-get %.*s stat %d",
					key.size, key.data, i);
				return EX_TEMPFAIL;
			}
			else if (i > 0 || info.size <= 0)
			{
#if HES_GETMAILHOST
				struct hes_postoffice *hp;
#endif

				if (tTd(28, 2))
					printf("udbexpand: no match on %s (%d)\n",
						keybuf, keylen);
#if HES_GETMAILHOST
				if (tTd(28, 8))
					printf("  ... trying hes_getmailhost(%s)\n",
						a->q_user);
				hp = hes_getmailhost(a->q_user);
				if (hp == NULL)
				{
					if (hes_error() == HES_ER_NET)
					{
						syserr("udbexpand: hesiod-getmail %s stat %d",
							a->q_user, hes_error());
						return EX_TEMPFAIL;
					}
					if (tTd(28, 2))
						printf("hes_getmailhost(%s): %d\n",
							a->q_user, hes_error());
					continue;
				}
				sprintf(info.data, "%s@%s",
					hp->po_name, hp->po_host);
				info.size = strlen(info.data);
#else
				continue;
#endif
			}
			if (tTd(28, 80))
				printf("udbexpand: match %.*s: %.*s\n",
					key.size, key.data, info.size, info.data);
			a->q_flags &= ~QSELFREF;

			if (bitset(EF_VRFYONLY, e->e_flags))
			{
				a->q_flags |= QVERIFIED;
				return EX_OK;
			}

			breakout = TRUE;
			if (info.size < sizeof buf)
				user = buf;
			else
				user = xalloc(info.size + 1);
			bcopy(info.data, user, info.size);
			user[info.size] = '\0';

			message("hesioded to %s", user);
#ifdef LOG
			if (LogLevel >= 10)
				syslog(LOG_INFO, "%s: hesiod %.100s => %s",
					e->e_id, e->e_to,
					shortenstring(user, 203));
#endif
			naddrs = sendtolist(user, a, sendq, aliaslevel + 1, e);

			if (user != buf)
				free(user);

			if (naddrs > 0 && !bitset(QSELFREF, a->q_flags))
			{
				if (tTd(28, 5))
				{
					printf("udbexpand: QDONTSEND ");
					printaddr(a, FALSE);
				}
				a->q_flags |= QDONTSEND;
			}

			/*
			**  If this address has a -request address, reflect
			**  it into the envelope.
			*/

			(void) strcpy(keybuf, a->q_user);
			(void) strcat(keybuf, ":mailsender");
			keylen = strlen(keybuf);
			key.data = keybuf;
			key.size = keylen;
			i = hes_udb_get(&key, &info);
			if (i != 0 || info.size <= 0)
				break;
			a->q_owner = xalloc(info.size + 1);
			bcopy(info.data, a->q_owner, info.size);
			a->q_owner[info.size] = '\0';
			break;
#endif /* HESIOD */

		  case UDB_REMOTE:
			/* not yet implemented */
			continue;

		  case UDB_FORWARD:
			if (bitset(EF_VRFYONLY, e->e_flags))
				return EX_OK;
			i = strlen(up->udb_fwdhost) + strlen(a->q_user) + 1;
			if (i < sizeof buf)
				user = buf;
			else
				user = xalloc(i + 1);
			(void) sprintf(user, "%s@%s", a->q_user, up->udb_fwdhost);
			message("expanded to %s", user);
			a->q_flags &= ~QSELFREF;
			naddrs = sendtolist(user, a, sendq, aliaslevel + 1, e);
			if (naddrs > 0 && !bitset(QSELFREF, a->q_flags))
			{
				if (tTd(28, 5))
				{
					printf("udbexpand: QDONTSEND ");
					printaddr(a, FALSE);
				}
				a->q_flags |= QDONTSEND;
			}
			if (user != buf)
				free(user);
			breakout = TRUE;
			break;

		  case UDB_EOLIST:
			breakout = TRUE;
			continue;

		  default:
			/* unknown entry type */
			continue;
		}
	}
	return EX_OK;
}
/*
**  UDBSENDER -- return canonical external name of sender, given local name
**
**	Parameters:
**		sender -- the name of the sender on the local machine.
**
**	Returns:
**		The external name for this sender, if derivable from the
**			database.
**		NULL -- if nothing is changed from the database.
**
**	Side Effects:
**		none.
*/

char *
udbsender(sender)
	char *sender;
{
	extern char *udbmatch();

	return udbmatch(sender, "mailname");
}


char *
udbmatch(user, field)
	char *user;
	char *field;
{
	register char *p;
	register struct udbent *up;
	int i;
	int keylen;
	DBT key, info;
	char keybuf[MAXKEY];

	if (tTd(28, 1))
		printf("udbmatch(%s, %s)\n", user, field);

	if (!UdbInitialized)
	{
		if (_udbx_init() == EX_TEMPFAIL)
			return NULL;
	}

	/* short circuit if no spec */
	if (UdbSpec == NULL || UdbSpec[0] == '\0')
		return NULL;

	/* short circuit name begins with '\\' since it can't possibly match */
	if (user[0] == '\\')
		return NULL;

	/* long names can never match and are a pain to deal with */
	if ((strlen(user) + strlen(field)) > sizeof keybuf - 4)
		return NULL;

	/* names beginning with colons indicate metadata */
	if (user[0] == ':')
		return NULL;

	/* build database key */
	(void) strcpy(keybuf, user);
	(void) strcat(keybuf, ":");
	(void) strcat(keybuf, field);
	keylen = strlen(keybuf);

	for (up = UdbEnts; up->udb_type != UDB_EOLIST; up++)
	{
		/*
		**  Select action based on entry type.
		*/

		switch (up->udb_type)
		{
#ifdef NEWDB
		  case UDB_DBFETCH:
			key.data = keybuf;
			key.size = keylen;
			i = (*up->udb_dbp->get)(up->udb_dbp, &key, &info, 0);
			if (i != 0 || info.size <= 0)
			{
				if (tTd(28, 2))
					printf("udbmatch: no match on %s (%d) via db\n",
							keybuf, keylen);
				continue;
			}

			p = xalloc(info.size + 1);
			bcopy(info.data, p, info.size);
			p[info.size] = '\0';
			if (tTd(28, 1))
				printf("udbmatch ==> %s\n", p);
			return p;
			break;
#endif

#ifdef HESIOD
		  case UDB_HESIOD:
			key.data = keybuf;
			key.size = keylen;
			i = hes_udb_get(&key, &info);
			if (i != 0 || info.size <= 0)
			{
				if (tTd(28, 2))
					printf("udbmatch: no match on %s (%d) via hesiod\n",
							keybuf, keylen);
				continue;
			}

			p = xalloc(info.size + 1);
			bcopy(info.data, p, info.size);
			p[info.size] = '\0';
			if (tTd(28, 1))
				printf("udbmatch ==> %s\n", p);
			return p;
#endif /* HESIOD */
		}
	}

	if (strcmp(field, "mailname") != 0)
		return NULL;

	/*
	**  Nothing yet.  Search again for a default case.  But only
	**  use it if we also have a forward (:maildrop) pointer already
	**  in the database.
	*/

	/* build database key */
	(void) strcpy(keybuf, user);
	(void) strcat(keybuf, ":maildrop");
	keylen = strlen(keybuf);

	for (up = UdbEnts; up->udb_type != UDB_EOLIST; up++)
	{
		switch (up->udb_type)
		{
#ifdef NEWDB
		  case UDB_DBFETCH:
			/* get the default case for this database */
			if (up->udb_default == NULL)
			{
				key.data = ":default:mailname";
				key.size = strlen(key.data);
				i = (*up->udb_dbp->get)(up->udb_dbp, &key, &info, 0);
				if (i != 0 || info.size <= 0)
				{
					/* no default case */
					up->udb_default = "";
					continue;
				}

				/* save the default case */
				up->udb_default = xalloc(info.size + 1);
				bcopy(info.data, up->udb_default, info.size);
				up->udb_default[info.size] = '\0';
			}
			else if (up->udb_default[0] == '\0')
				continue;

			/* we have a default case -- verify user:maildrop */
			key.data = keybuf;
			key.size = keylen;
			i = (*up->udb_dbp->get)(up->udb_dbp, &key, &info, 0);
			if (i != 0 || info.size <= 0)
			{
				/* nope -- no aliasing for this user */
				continue;
			}

			/* they exist -- build the actual address */
			p = xalloc(strlen(user) + strlen(up->udb_default) + 2);
			(void) strcpy(p, user);
			(void) strcat(p, "@");
			(void) strcat(p, up->udb_default);
			if (tTd(28, 1))
				printf("udbmatch ==> %s\n", p);
			return p;
			break;
#endif

#ifdef HESIOD
		  case UDB_HESIOD:
			/* get the default case for this database */
			if (up->udb_default == NULL)
			{
				key.data = ":default:mailname";
				key.size = strlen(key.data);
				i = hes_udb_get(&key, &info);

				if (i != 0 || info.size <= 0)
				{
					/* no default case */
					up->udb_default = "";
					continue;
				}

				/* save the default case */
				up->udb_default = xalloc(info.size + 1);
				bcopy(info.data, up->udb_default, info.size);
				up->udb_default[info.size] = '\0';
			}
			else if (up->udb_default[0] == '\0')
				continue;

			/* we have a default case -- verify user:maildrop */
			key.data = keybuf;
			key.size = keylen;
			i = hes_udb_get(&key, &info);
			if (i != 0 || info.size <= 0)
			{
				/* nope -- no aliasing for this user */
				continue;
			}

			/* they exist -- build the actual address */
			p = xalloc(strlen(user) + strlen(up->udb_default) + 2);
			(void) strcpy(p, user);
			(void) strcat(p, "@");
			(void) strcat(p, up->udb_default);
			if (tTd(28, 1))
				printf("udbmatch ==> %s\n", p);
			return p;
			break;
#endif /* HESIOD */
		}
	}

	/* still nothing....  too bad */
	return NULL;
}
/*
**  UDB_MAP_LOOKUP -- look up arbitrary entry in user database map
**
**	Parameters:
**		map -- the map being queried.
**		name -- the name to look up.
**		av -- arguments to the map lookup.
**		statp -- to get any error status.
**
**	Returns:
**		NULL if name not found in map.
**		The rewritten name otherwise.
*/

char *
udb_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *val;
	char *key;
	char keybuf[MAXNAME + 1];

	if (tTd(28, 20) || tTd(38, 20))
		printf("udb_map_lookup(%s, %s)\n", map->map_mname, name);

	if (bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		key = name;
	}
	else
	{
		int keysize = strlen(name);

		if (keysize > sizeof keybuf - 1)
			keysize = sizeof keybuf - 1;
		bcopy(name, keybuf, keysize);
		keybuf[keysize] = '\0';
		makelower(keybuf);
		key = keybuf;
	}
	val = udbmatch(key, map->map_file);
	if (val == NULL)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, val, strlen(val), av);
}
/*
**  _UDBX_INIT -- parse the UDB specification, opening any valid entries.
**
**	Parameters:
**		none.
**
**	Returns:
**		EX_TEMPFAIL -- if it appeared it couldn't get hold of a
**			database due to a host being down or some similar
**			(recoverable) situation.
**		EX_OK -- otherwise.
**
**	Side Effects:
**		Fills in the UdbEnts structure from UdbSpec.
*/

#define MAXUDBOPTS	27

int
_udbx_init()
{
	register char *p;
	register struct udbent *up;

	if (UdbInitialized)
		return EX_OK;

# ifdef UDB_DEFAULT_SPEC
	if (UdbSpec == NULL)
		UdbSpec = UDB_DEFAULT_SPEC;
# endif

	p = UdbSpec;
	up = UdbEnts;
	while (p != NULL)
	{
		char *spec;
		int nopts;
# if 0
		auto int rcode;
		int nmx;
		int i;
		register struct hostent *h;
		char *mxhosts[MAXMXHOSTS + 1];
# endif
		struct option opts[MAXUDBOPTS + 1];

		while (*p == ' ' || *p == '\t' || *p == ',')
			p++;
		if (*p == '\0')
			break;
		spec = p;
		p = strchr(p, ',');
		if (p != NULL)
			*p++ = '\0';

		/* extract options */
		nopts = _udb_parsespec(spec, opts, MAXUDBOPTS);

		/*
		**  Decode database specification.
		**
		**	In the sendmail tradition, the leading character
		**	defines the semantics of the rest of the entry.
		**
		**	+hostname --	send a datagram to the udb server
		**			on host "hostname" asking for the
		**			home mail server for this user.
		**	*hostname --	similar to +hostname, except that the
		**			hostname is searched as an MX record;
		**			resulting hosts are searched as for
		**			+mxhostname.  If no MX host is found,
		**			this is the same as +hostname.
		**	@hostname --	forward email to the indicated host.
		**			This should be the last in the list,
		**			since it always matches the input.
		**	/dbname	 --	search the named database on the local
		**			host using the Berkeley db package.
		**	Hesiod --	search the named database with BIND
		**			using the MIT Hesiod package.
		*/

		switch (*spec)
		{
#if 0
		  case '+':	/* search remote database */
		  case '*':	/* search remote database (expand MX) */
			if (*spec == '*')
			{
#if NAMED_BIND
				nmx = getmxrr(spec + 1, mxhosts, FALSE, &rcode);
#else
				mxhosts[0] = spec + 1;
				nmx = 1;
				rcode = 0;
#endif
				if (tTd(28, 16))
				{
					int i;

					printf("getmxrr(%s): %d", spec + 1, nmx);
					for (i = 0; i <= nmx; i++)
						printf(" %s", mxhosts[i]);
					printf("\n");
				}
			}
			else
			{
				nmx = 1;
				mxhosts[0] = spec + 1;
			}

			for (i = 0; i < nmx; i++)
			{
				h = sm_gethostbyname(mxhosts[i]);
				if (h == NULL)
					continue;
				up->udb_type = UDB_REMOTE;
				up->udb_addr.sin_family = h->h_addrtype;
				bcopy(h->h_addr_list[0],
				      (char *) &up->udb_addr.sin_addr,
				      INADDRSZ);
				up->udb_addr.sin_port = UdbPort;
				up->udb_timeout = UdbTimeout;
				up++;
			}

			/* set up a datagram socket */
			if (UdbSock < 0)
			{
				UdbSock = socket(AF_INET, SOCK_DGRAM, 0);
				(void) fcntl(UdbSock, F_SETFD, 1);
			}
			break;
#endif

		  case '@':	/* forward to remote host */
			up->udb_type = UDB_FORWARD;
			up->udb_fwdhost = spec + 1;
			up++;
			break;

#ifdef HESIOD
		  case 'h':	/* use hesiod */
		  case 'H':
			if (strcasecmp(spec, "hesiod") != 0)
				goto badspec;
			up->udb_type = UDB_HESIOD;
			up++;
			break;
#endif /* HESIOD */

#ifdef NEWDB
		  case '/':	/* look up remote name */
			up->udb_dbname = spec;
			errno = 0;
			up->udb_dbp = dbopen(spec, O_RDONLY, 0644, DB_BTREE, NULL);
			if (up->udb_dbp == NULL)
			{
				if (tTd(28, 1))
				{
					int saveerrno = errno;

					printf("dbopen(%s): %s",
						spec, errstring(errno));
					errno = saveerrno;
				}
				if (errno != ENOENT && errno != EACCES)
				{
#ifdef LOG
					if (LogLevel > 2)
						syslog(LOG_ERR, "dbopen(%s): %s",
							spec, errstring(errno));
#endif
					up->udb_type = UDB_EOLIST;
					goto tempfail;
				}
				break;
			}
			up->udb_type = UDB_DBFETCH;
			up++;
			break;
#endif

		  default:
badspec:
			syserr("Unknown UDB spec %s", spec);
			break;
		}
	}
	up->udb_type = UDB_EOLIST;

	if (tTd(28, 4))
	{
		for (up = UdbEnts; up->udb_type != UDB_EOLIST; up++)
		{
			switch (up->udb_type)
			{
#ifdef DAEMON
			  case UDB_REMOTE:
				printf("REMOTE: addr %s, timeo %d\n",
					anynet_ntoa((SOCKADDR *) &up->udb_addr),
					up->udb_timeout);
				break;
#endif

			  case UDB_DBFETCH:
#ifdef NEWDB
				printf("FETCH: file %s\n",
					up->udb_dbname);
#else
				printf("FETCH\n");
#endif
				break;

			  case UDB_FORWARD:
				printf("FORWARD: host %s\n",
					up->udb_fwdhost);
				break;

			  case UDB_HESIOD:
				printf("HESIOD\n");
				break;

			  default:
				printf("UNKNOWN\n");
				break;
			}
		}
	}

	UdbInitialized = TRUE;
	errno = 0;
	return EX_OK;

	/*
	**  On temporary failure, back out anything we've already done
	*/

  tempfail:
#ifdef NEWDB
	for (up = UdbEnts; up->udb_type != UDB_EOLIST; up++)
	{
		if (up->udb_type == UDB_DBFETCH)
		{
			(*up->udb_dbp->close)(up->udb_dbp);
		}
	}
#endif
	return EX_TEMPFAIL;
}

int
_udb_parsespec(udbspec, opt, maxopts)
	char *udbspec;
	struct option opt[];
	int maxopts;
{
	register char *spec;
	register char *spec_end;
	register int optnum;

	spec_end = strchr(udbspec, ':');
	for (optnum = 0; optnum < maxopts && (spec = spec_end) != NULL; optnum++)
	{
		register char *p;

		while (isascii(*spec) && isspace(*spec))
			spec++;
		spec_end = strchr(spec, ':');
		if (spec_end != NULL)
			*spec_end++ = '\0';

		opt[optnum].name = spec;
		opt[optnum].val = NULL;
		p = strchr(spec, '=');
		if (p != NULL)
			opt[optnum].val = ++p;
	}
	return optnum;
}

#ifdef HESIOD

int
hes_udb_get(key, info)
	DBT *key;
	DBT *info;
{
	char *name, *type;
	char *p, **hp;
	char kbuf[MAXKEY + 1];

	strcpy(kbuf, key->data);
	name = kbuf;
	type = strrchr(name, ':');
	if (type == NULL)
		return 1;
	*type++ = '\0';
	if (strchr(name, '@') != NULL)
		return 1;

	if (tTd(28, 1))
		printf("hes_udb_get(%s, %s)\n", name, type);

	/* make the hesiod query */
	hp = hes_resolve(name, type);
	*--type = ':';
	if (hp == NULL || hp[0] == NULL)
	{
		/* network problem or timeout */
		if (hes_error() == HES_ER_NET)
			return -1;

		return 1;
	}
	else
	{
		/*
		**  If there are multiple matches, just return the
		**  first one.
		**
		**  XXX These should really be returned; for example,
		**  XXX it is legal for :maildrop to be multi-valued.
		*/

		info->data = hp[0];
		info->size = (size_t) strlen(info->data);
	}

	if (tTd(28, 80))
		printf("hes_udb_get => %s\n", *hp);

	return 0;
}
#endif /* HESIOD */

#else /* not USERDB */

int
udbexpand(a, sendq, aliaslevel, e)
	ADDRESS *a;
	ADDRESS **sendq;
	int aliaslevel;
	ENVELOPE *e;
{
	return EX_OK;
}

#endif /* USERDB */

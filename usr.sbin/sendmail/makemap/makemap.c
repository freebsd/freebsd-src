/*
 * Copyright (c) 1992 Eric P. Allman.
 * Copyright (c) 1992, 1993
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

#ifndef lint
static char sccsid[] = "@(#)makemap.c	8.35 (Berkeley) 6/10/97";
#endif /* not lint */

#include <sys/types.h>
#include <sys/errno.h>
#ifndef ISC_UNIX
# include <sys/file.h>
#endif
#include "sendmail.h"

#ifdef NDBM
#include <ndbm.h>
#endif

#ifdef NEWDB
#include <db.h>
#endif

enum type { T_DBM, T_BTREE, T_HASH, T_ERR, T_UNKNOWN };

union dbent
{
#ifdef NDBM
	datum	dbm;
#endif
#ifdef NEWDB
	DBT	db;
#endif
	struct
	{
		char	*data;
		size_t	size;
	} xx;
};

uid_t	RealUid;
gid_t	RealGid;
char	*RealUserName;
uid_t	RunAsUid;
uid_t	RunAsGid;
char	*RunAsUserName;
int	Verbose = 2;
bool	DontInitGroups = TRUE;
bool	UnsafeGroupWrites = FALSE;
u_char	tTdvect[100];

#define BUFSIZE		1024

main(argc, argv)
	int argc;
	char **argv;
{
	char *progname;
	bool inclnull = FALSE;
	bool notrunc = FALSE;
	bool allowreplace = FALSE;
	bool allowdups = FALSE;
	bool verbose = FALSE;
	bool foldcase = TRUE;
	bool ignoresafeties = FALSE;
	int exitstat;
	int opt;
	char *typename;
	char *mapname;
	char *ext;
	int lineno;
	int st;
	int mode;
	int putflags;
	long dbcachesize = 1024 * 1024;
	enum type type;
	int fd;
	union
	{
#ifdef NDBM
		DBM	*dbm;
#endif
#ifdef NEWDB
		DB	*db;
#endif
		void	*dbx;
	} dbp;
	union dbent key, val;
#ifdef NEWDB
	BTREEINFO bti;
	HASHINFO hinfo;
#endif
	char ibuf[BUFSIZE];
	char fbuf[MAXNAME];
	char dbuf[MAXNAME];
	char pbuf[MAXNAME];
	static char rnamebuf[MAXNAME];	/* holds RealUserName */
	struct passwd *pw;
	int sff = SFF_ROOTOK|SFF_REGONLY|SFF_NOLINK|SFF_NOWLINK;
	struct stat std, stp;
	extern char *optarg;
	extern int optind;
	extern bool lockfile();

	progname = argv[0];

	RunAsUid = RealUid = getuid();
	RunAsGid = RealGid = getgid();
	pw = getpwuid(RealUid);
	if (pw != NULL) 
	{
		if (strlen(pw->pw_name) > MAXNAME - 1)
			pw->pw_name[MAXNAME] = 0;
		sprintf(rnamebuf, "%s", pw->pw_name);
	}
	else
		sprintf(rnamebuf, "Unknown UID %d", RealUid);
	RunAsUserName = RealUserName = rnamebuf;

#if _FFR_NEW_MAKEMAP_FLAGS
#define OPTIONS		"Nc:dforsv"
#else
#define OPTIONS		"Ndforv"
#endif
	while ((opt = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch (opt)
		{
		  case 'N':
			inclnull = TRUE;
			break;

#if _FFR_NEW_MAKEMAP_FLAGS
		  case 'c':
			dbcachesize = atol(optarg);
			break;
#endif

		  case 'd':
			allowdups = TRUE;
			break;

		  case 'f':
			foldcase = FALSE;
			break;

		  case 'o':
			notrunc = TRUE;
			break;

		  case 'r':
			allowreplace = TRUE;
			break;

#if _FFR_NEW_MAKEMAP_FLAGS
		  case 's':
			ignoresafeties = TRUE;
			break;
#endif

		  case 'v':
			verbose = TRUE;
			break;

		  default:
			type = T_ERR;
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 2)
		type = T_ERR;
	else
	{
		typename = argv[0];
		mapname = argv[1];
		ext = NULL;

		if (strcmp(typename, "dbm") == 0)
		{
			type = T_DBM;
		}
		else if (strcmp(typename, "btree") == 0)
		{
			type = T_BTREE;
			ext = ".db";
		}
		else if (strcmp(typename, "hash") == 0)
		{
			type = T_HASH;
			ext = ".db";
		}
		else
			type = T_UNKNOWN;
	}

	switch (type)
	{
	  case T_ERR:
#if _FFR_NEW_MAKEMAP_FLAGS
		fprintf(stderr,
			"Usage: %s [-N] [-c cachesize] [-d] [-f] [-o] [-r] [-s] [-v] type mapname\n",
			progname);
#else
		fprintf(stderr, "Usage: %s [-N] [-d] [-f] [-o] [-r] [-v] type mapname\n", progname);
#endif
		exit(EX_USAGE);

	  case T_UNKNOWN:
		fprintf(stderr, "%s: Unknown database type %s\n",
			progname, typename);
		exit(EX_USAGE);

#ifndef NDBM
	  case T_DBM:
#endif
#ifndef NEWDB
	  case T_BTREE:
	  case T_HASH:
#endif
		fprintf(stderr, "%s: Type %s not supported in this version\n",
			progname, typename);
		exit(EX_UNAVAILABLE);

#ifdef NEWDB
	  case T_BTREE:
		bzero(&bti, sizeof bti);
		if (allowdups)
			bti.flags |= R_DUP;
		if (allowdups || allowreplace)
			putflags = 0;
		else
			putflags = R_NOOVERWRITE;
		break;

	  case T_HASH:
		bzero(&hinfo, sizeof hinfo);
		if (allowreplace)
			putflags = 0;
		else
			putflags = R_NOOVERWRITE;
		break;
#endif
#ifdef NDBM
	  case T_DBM:
		if (allowdups)
		{
			fprintf(stderr, "%s: Type %s does not support -d (allow dups)\n",
				progname, typename);
			exit(EX_UNAVAILABLE);
		}
		if (allowreplace)
			putflags = DBM_REPLACE;
		else
			putflags = DBM_INSERT;
		break;
#endif
	}

	/*
	**  Adjust file names.
	*/

	if (ext != NULL)
	{
		int el, fl;

		el = strlen(ext);
		fl = strlen(mapname);
		if (el + fl + 1 >= sizeof fbuf)
		{
			fprintf(stderr, "%s: file name too long", mapname);
			exit(EX_USAGE);
		}
		if (fl < el || strcmp(&mapname[fl - el], ext) != 0)
		{
			strcpy(fbuf, mapname);
			strcat(fbuf, ext);
			mapname = fbuf;
		}
	}

	if (!notrunc)
		sff |= SFF_CREAT;
	switch (type) 
	{
#ifdef NEWDB
	  case T_BTREE:
	  case T_HASH:
		if (strlen(mapname) >= sizeof dbuf)
		{
			fprintf(stderr,
				"%s: map name too long\n", mapname);
			exit(EX_USAGE);
		}
		strcpy(dbuf, mapname);
		if (!ignoresafeties &&
		    (st = safefile(dbuf, RealUid, RealGid, RealUserName,
				   sff, S_IWUSR, &std)) != 0) 
		{
			fprintf(stderr,
				"%s: could not create: %s\n",
				dbuf, errstring(st));
			exit(EX_CANTCREAT);
		}
		break;
#endif
#ifdef NDBM
	  case T_DBM:
		if (strlen(mapname) + 5 > sizeof dbuf)
		{
			fprintf(stderr,
				"%s: map name too long\n", mapname);
			exit(EX_USAGE);
		}
		sprintf(dbuf, "%s.dir", mapname);
		if (!ignoresafeties &&
		    (st = safefile(dbuf, RealUid, RealGid, RealUserName,
				   sff, S_IWUSR, &std)) != 0) 
		{
			fprintf(stderr,
				"%s: could not create: %s\n",
				dbuf, errstring(st));
			exit(EX_CANTCREAT);
		}
		sprintf(pbuf, "%s.pag", mapname);
		if (!ignoresafeties &&
		    (st = safefile(pbuf, RealUid, RealGid, RealUserName,
				   sff, S_IWUSR, &stp)) != 0) 
		{
			fprintf(stderr,
				"%s: could not create: %s\n",
				pbuf, errstring(st));
			exit(EX_CANTCREAT);
		}
		break;
#endif
	  default:
		fprintf(stderr,
			"%s: internal error: type %d\n",
			progname,
			type);
		exit(EX_SOFTWARE);
	}

	/*
	**  Create the database.
	*/

	mode = O_RDWR;
	if (!notrunc)
		mode |= O_CREAT|O_TRUNC;
#if O_EXLOCK
	mode |= O_EXLOCK;
#else
	/* pre-lock the database */
	if (ignoresafeties)
		fd = dfopen(dbuf, mode & ~O_TRUNC, 0644, sff);
	else
		fd = safeopen(dbuf, mode & ~O_TRUNC, 0644, sff);
	if (fd < 0)
	{
		fprintf(stderr, "%s: cannot create type %s map %s\n",
			progname, typename, mapname);
		exit(EX_CANTCREAT);
	}
#endif
	switch (type)
	{
#ifdef NDBM
	  case T_DBM:
		dbp.dbm = dbm_open(mapname, mode, 0644);
		if (!ignoresafeties && dbp.dbm != NULL &&
		    (filechanged(dbuf, dbm_dirfno(dbp.dbm), &std, sff) ||
		     filechanged(pbuf, dbm_pagfno(dbp.dbm), &stp, sff)))
		{
			fprintf(stderr,
				"dbm map %s: file changed after open\n",
				mapname);
			dbm_close(dbp.dbm);
			exit(EX_CANTCREAT);
		}
		break;
#endif

#ifdef NEWDB
	  case T_HASH:
		/* tweak some parameters for performance */
		hinfo.nelem = 4096;
		hinfo.cachesize = dbcachesize;
		
		dbp.db = dbopen(mapname, mode, 0644, DB_HASH, &hinfo);
		if (dbp.db != NULL)
		{
			if (!ignoresafeties &&
			    filechanged(dbuf, dbp.db->fd(dbp.db), &std, sff))
			{
				fprintf(stderr,
					"db map %s: file changed after open\n",
					mapname);
				dbp.db->close(dbp.db);
				exit(EX_CANTCREAT);
			}
# if OLD_NEWDB
			(void) (*dbp.db->sync)(dbp.db);
# else
			(void) (*dbp.db->sync)(dbp.db, 0);
# endif
		}
		break;

	  case T_BTREE:
		/* tweak some parameters for performance */
		bti.cachesize = dbcachesize;

		dbp.db = dbopen(mapname, mode, 0644, DB_BTREE, &bti);
		if (dbp.db != NULL)
		{
			if (!ignoresafeties &&
			    filechanged(dbuf, dbp.db->fd(dbp.db), &std, sff))
			{
				fprintf(stderr,
					"db map %s: file changed after open\n",
					mapname);
				dbp.db->close(dbp.db);
				exit(EX_CANTCREAT);
			}
# if OLD_NEWDB
			(void) (*dbp.db->sync)(dbp.db);
# else
			(void) (*dbp.db->sync)(dbp.db, 0);
# endif
		}
		break;
#endif

	  default:
		fprintf(stderr, "%s: internal error: type %d\n",
			progname, type);
		exit(EX_SOFTWARE);
	}

	if (dbp.dbx == NULL)
	{
		fprintf(stderr, "%s: cannot open type %s map %s\n",
			progname, typename, mapname);
		exit(EX_CANTCREAT);
	}

	/*
	**  Copy the data
	*/

	lineno = 0;
	exitstat = EX_OK;
	while (fgets(ibuf, sizeof ibuf, stdin) != NULL)
	{
		register char *p;

		lineno++;

		/*
		**  Parse the line.
		*/

		p = strchr(ibuf, '\n');
		if (p != NULL)
			*p = '\0';
		else if (!feof(stdin))
		{
			fprintf(stderr, "%s: %s: line %d: line too long (%d bytes max)\n",
				progname, mapname, lineno, sizeof ibuf);
			continue;
		}

		if (ibuf[0] == '\0' || ibuf[0] == '#')
			continue;
		if (isspace(ibuf[0]))
		{
			fprintf(stderr, "%s: %s: line %d: syntax error (leading space)\n",
				progname, mapname, lineno);
			continue;
		}
		key.xx.data = ibuf;
		for (p = ibuf; *p != '\0' && !isspace(*p); p++)
		{
			if (foldcase && isupper(*p))
				*p = tolower(*p);
		}
		key.xx.size = p - key.xx.data;
		if (inclnull)
			key.xx.size++;
		if (*p != '\0')
			*p++ = '\0';
		while (isspace(*p))
			p++;
		if (*p == '\0')
		{
			fprintf(stderr, "%s: %s: line %d: no RHS for LHS %s\n",
				progname, mapname, lineno, key.xx.data);
			continue;
		}
		val.xx.data = p;
		val.xx.size = strlen(p);
		if (inclnull)
			val.xx.size++;

		/*
		**  Do the database insert.
		*/

		if (verbose)
		{
			printf("key=`%s', val=`%s'\n", key.xx.data, val.xx.data);
		}

		switch (type)
		{
#ifdef NDBM
		  case T_DBM:
			st = dbm_store(dbp.dbm, key.dbm, val.dbm, putflags);
			break;
#endif

#ifdef NEWDB
		  case T_BTREE:
		  case T_HASH:
			st = (*dbp.db->put)(dbp.db, &key.db, &val.db, putflags);
			break;
#endif
		}

		if (st < 0)
		{
			fprintf(stderr, "%s: %s: line %d: key %s: put error\n",
				progname, mapname, lineno, key.xx.data);
			perror(mapname);
			exitstat = EX_IOERR;
		}
		else if (st > 0)
		{
			fprintf(stderr,
				"%s: %s: line %d: key %s: duplicate key\n",
				progname, mapname, lineno, key.xx.data);
		}
	}

	/*
	**  Now close the database.
	*/

	switch (type)
	{
#ifdef NDBM
	  case T_DBM:
		dbm_close(dbp.dbm);
		break;
#endif

#ifdef NEWDB
	  case T_HASH:
	  case T_BTREE:
		if ((*dbp.db->close)(dbp.db) < 0)
		{
			fprintf(stderr, "%s: %s: error on close\n",
				progname, mapname);
			perror(mapname);
			exitstat = EX_IOERR;
		}
#endif
	}

#if !O_EXLOCK
	/* release locks */
	close(fd);
#endif

	exit (exitstat);
}
/*
**  LOCKFILE -- lock a file using flock or (shudder) fcntl locking
**
**	Parameters:
**		fd -- the file descriptor of the file.
**		filename -- the file name (for error messages).
**		ext -- the filename extension.
**		type -- type of the lock.  Bits can be:
**			LOCK_EX -- exclusive lock.
**			LOCK_NB -- non-blocking.
**
**	Returns:
**		TRUE if the lock was acquired.
**		FALSE otherwise.
*/

bool
lockfile(fd, filename, ext, type)
	int fd;
	char *filename;
	char *ext;
	int type;
{
# if !HASFLOCK
	int action;
	struct flock lfd;
	extern int errno;

	bzero(&lfd, sizeof lfd);
	if (bitset(LOCK_UN, type))
		lfd.l_type = F_UNLCK;
	else if (bitset(LOCK_EX, type))
		lfd.l_type = F_WRLCK;
	else
		lfd.l_type = F_RDLCK;
	if (bitset(LOCK_NB, type))
		action = F_SETLK;
	else
		action = F_SETLKW;

	if (fcntl(fd, action, &lfd) >= 0)
		return TRUE;

	/*
	**  On SunOS, if you are testing using -oQ/tmp/mqueue or
	**  -oA/tmp/aliases or anything like that, and /tmp is mounted
	**  as type "tmp" (that is, served from swap space), the
	**  previous fcntl will fail with "Invalid argument" errors.
	**  Since this is fairly common during testing, we will assume
	**  that this indicates that the lock is successfully grabbed.
	*/

	if (errno == EINVAL)
		return TRUE;

# else	/* HASFLOCK */

	if (flock(fd, type) >= 0)
		return TRUE;

# endif

	return FALSE;
}

/*VARARGS2*/
void
#ifdef __STDC__
message(const char *msg, ...)
#else
message(msg, va_alist)
	const char *msg;
	va_dcl
#endif
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isdigit(m[0]) && isdigit(m[1]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	vfprintf(stderr, m, ap);
	VA_END;
	fprintf(stderr, "\n");
}

/*VARARGS2*/
void
#ifdef __STDC__
syserr(const char *msg, ...)
#else
syserr(msg, va_alist)
	const char *msg;
	va_dcl
#endif
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isdigit(m[0]) && isdigit(m[1]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	vfprintf(stderr, m, ap);
	VA_END;
	fprintf(stderr, "\n");
}

const char *
errstring(err)
	int err;
{
	static char errstr[64];
#if !HASSTRERROR && !defined(ERRLIST_PREDEFINED)
	extern char *sys_errlist[];
	extern int sys_nerr;
#endif

	/* handle pseudo-errors internal to sendmail */
	switch (err)
	{
	  case E_SM_OPENTIMEOUT:
		return "Timeout on file open";

	  case E_SM_NOSLINK:
		return "Symbolic links not allowed";

	  case E_SM_NOHLINK:
		return "Hard links not allowed";

	  case E_SM_REGONLY:
		return "Regular files only";

	  case E_SM_ISEXEC:
		return "Executable files not allowed";

	  case E_SM_WWDIR:
		return "World writable directory";

	  case E_SM_GWDIR:
		return "Group writable directory";

	  case E_SM_FILECHANGE:
		return "File changed after open";

	  case E_SM_WWFILE:
		return "World writable file";

	  case E_SM_GWFILE:
		return "Group writable file";
	}

#if HASSTRERROR
	return strerror(err);
#else
	if (err < 0 || err > sys_nerr) 
	{
		sprintf(errstr, "Error %d", err);
		return errstr;
	}
	return sys_errlist[err];
#endif
}

/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1992 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)makemap.c	8.71 (Berkeley) 11/29/1998";
#endif /* not lint */

#include <sys/types.h>
#include <sys/errno.h>
#ifndef ISC_UNIX
# include <sys/file.h>
#endif
#include "sendmail.h"
#include "pathnames.h"

#ifdef NDBM
# include <ndbm.h>
#endif

#ifdef NEWDB
# include <db.h>
# ifndef DB_VERSION_MAJOR
#  define DB_VERSION_MAJOR 1
# endif
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
bool	DontInitGroups = FALSE;
long	DontBlameSendmail = DBS_SAFE;
u_char	tTdvect[100];
uid_t	TrustedUid = 0;

#define BUFSIZE		1024

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *progname;
	char *cfile;
	bool inclnull = FALSE;
	bool notrunc = FALSE;
	bool allowreplace = FALSE;
	bool allowdups = FALSE;
	bool verbose = FALSE;
	bool foldcase = TRUE;
	int exitstat;
	int opt;
	char *typename = NULL;
	char *mapname = NULL;
	char *ext = NULL;
	int lineno;
	int st;
	int mode;
	int putflags = 0;
#ifdef NEWDB
	long dbcachesize = 1024 * 1024;
#endif
	enum type type;
#if !O_EXLOCK
	int fd;
#endif
	int sff = SFF_ROOTOK|SFF_REGONLY;
	struct passwd *pw;
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
# if DB_VERSION_MAJOR < 2
	BTREEINFO bti;
	HASHINFO hinfo;
# else
	DB_INFO dbinfo;
# endif
#endif
	char ibuf[BUFSIZE];
	char fbuf[MAXNAME];
	char dbuf[MAXNAME];
#ifdef NDBM
	char pbuf[MAXNAME];
#endif
#if _FFR_TRUSTED_USER
	FILE *cfp;
	char buf[MAXLINE];
#endif
	static char rnamebuf[MAXNAME];	/* holds RealUserName */
	struct stat std;
#ifdef NDBM
	struct stat stp;
#endif
	extern char *optarg;
	extern int optind;

	progname = argv[0];
	cfile = _PATH_SENDMAILCF;

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
		sprintf(rnamebuf, "Unknown UID %d", (int) RealUid);
	RunAsUserName = RealUserName = rnamebuf;

#if _FFR_NEW_MAKEMAP_FLAGS
#define OPTIONS		"C:Nc:dflorsv"
#else
#define OPTIONS		"C:Ndforsv"
#endif
	while ((opt = getopt(argc, argv, OPTIONS)) != EOF)
	{
		switch (opt)
		{
		  case 'C':
			cfile = optarg;
			break;

		  case 'N':
			inclnull = TRUE;
			break;

#if _FFR_NEW_MAKEMAP_FLAGS
		  case 'c':
# ifdef NEWDB
			dbcachesize = atol(optarg);
# endif
			break;
#endif

		  case 'd':
			allowdups = TRUE;
			break;

		  case 'f':
			foldcase = FALSE;
			break;

#if _FFR_NEW_MAKEMAP_FLAGS
		  case 'l':
# ifdef NDBM
			printf("dbm\n");
# endif
# ifdef NEWDB
			printf("hash\n");
			printf("btree\n");
# endif
			exit(EX_OK);
			break;
#endif

		  case 'o':
			notrunc = TRUE;
			break;

		  case 'r':
			allowreplace = TRUE;
			break;

		  case 's':
			DontBlameSendmail |= DBS_MAPINUNSAFEDIRPATH|DBS_WRITEMAPTOHARDLINK|DBS_WRITEMAPTOSYMLINK|DBS_LINKEDMAPINWRITABLEDIR;
			break;

		  case 'v':
			verbose = TRUE;
			break;

		  default:
			type = T_ERR;
			break;
		}
	}

	if (!bitset(DBS_WRITEMAPTOSYMLINK, DontBlameSendmail))
		sff |= SFF_NOSLINK;
        if (!bitset(DBS_WRITEMAPTOHARDLINK, DontBlameSendmail))
		sff |= SFF_NOHLINK;
	if (!bitset(DBS_LINKEDMAPINWRITABLEDIR, DontBlameSendmail))
		sff |= SFF_NOWLINK;

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

#if _FFR_TRUSTED_USER
	if ((cfp = fopen(cfile, "r")) == NULL)
	{
		fprintf(stderr, "mailstats: ");
		perror(cfile);
		exit(EX_NOINPUT);
	}
	while (fgets(buf, sizeof(buf), cfp) != NULL)
	{
		register char *b;

		if ((b = strchr(buf, '\n')) != NULL)
			*b = '\0';

		b = buf;
		switch (*b++)
		{
		  case 'O':		/* option */
			if (strncasecmp(b, " TrustedUser", 12) == 0 &&
			    !(isascii(b[12]) && isalnum(b[12])))
			{
				b = strchr(b, '=');
				if (b == NULL)
					continue;
				while (isascii(*++b) && isspace(*b))
					continue;
				if (isascii(*b) && isdigit(*b))
					TrustedUid = atoi(b);
				else
				{
					register struct passwd *pw;
					
					TrustedUid = 0;
					pw = getpwnam(b);
					if (pw == NULL)
						fprintf(stderr,
							"TrustedUser: unknown user %s\n", b);
					else
						TrustedUid = pw->pw_uid;
				}
				
# ifdef UID_MAX
				if (TrustedUid > UID_MAX)
				{
					syserr("TrustedUser: uid value (%ld) > UID_MAX (%ld)",
					       TrustedUid, UID_MAX);
					TrustedUid = 0;
				}
# endif
				break;
			}


		  default:
			continue;
		}
	}
	(void) fclose(cfp);
#endif
	switch (type)
	{
	  case T_ERR:
#if _FFR_NEW_MAKEMAP_FLAGS
		fprintf(stderr,
			"Usage: %s [-N] [-c cachesize] [-d] [-f] [-l] [-o] [-r] [-s] [-v] type mapname\n",
			progname);
#else
		fprintf(stderr, "Usage: %s [-N] [-d] [-f] [-o] [-r] [-s] [-v] type mapname\n", progname);
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
# if DB_VERSION_MAJOR < 2
		bzero(&bti, sizeof bti);
# else
		bzero(&dbinfo, sizeof dbinfo);
# endif
		if (allowdups)
		{
# if DB_VERSION_MAJOR < 2
			bti.flags |= R_DUP;
# else
			dbinfo.flags |= DB_DUP;
# endif
		}
		if (allowdups || allowreplace)
			putflags = 0;
		else
		{
# if DB_VERSION_MAJOR < 2
			putflags = R_NOOVERWRITE;
# else
			putflags = DB_NOOVERWRITE;
# endif
		}
		break;

	  case T_HASH:
# if DB_VERSION_MAJOR < 2
		bzero(&hinfo, sizeof hinfo);
# else
		bzero(&dbinfo, sizeof dbinfo);
# endif
		if (allowreplace)
			putflags = 0;
		else
		{
# if DB_VERSION_MAJOR < 2
			putflags = R_NOOVERWRITE;
# else
			putflags = DB_NOOVERWRITE;
# endif
		}
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
		if ((st = safefile(dbuf, RealUid, RealGid, RealUserName,
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
		if ((st = safefile(dbuf, RealUid, RealGid, RealUserName,
			   sff, S_IWUSR, &std)) != 0)
		{
			fprintf(stderr,
				"%s: could not create: %s\n",
				dbuf, errstring(st));
			exit(EX_CANTCREAT);
		}
		sprintf(pbuf, "%s.pag", mapname);
		if ((st = safefile(pbuf, RealUid, RealGid, RealUserName,
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
		if (dbp.dbm != NULL &&
		    dbm_dirfno(dbp.dbm) == dbm_pagfno(dbp.dbm))
		{
			fprintf(stderr, "dbm map %s: cannot run with GDBM\n",
				mapname);
			dbm_close(dbp.dbm);
			exit(EX_CONFIG);
		}
		if (dbp.dbm != NULL &&
		    (filechanged(dbuf, dbm_dirfno(dbp.dbm), &std) ||
		     filechanged(pbuf, dbm_pagfno(dbp.dbm), &stp)))
		{
			fprintf(stderr,
				"dbm map %s: file changed after open\n",
				mapname);
			dbm_close(dbp.dbm);
			exit(EX_CANTCREAT);
		}
#if _FFR_TRUSTED_USER
		if (geteuid() == 0 && TrustedUid != 0)
		{
			if (fchown(dbm_dirfno(dbp.dbm), TrustedUid, -1) < 0 ||
			    fchown(dbm_pagfno(dbp.dbm), TrustedUid, -1) < 0)
			{
				fprintf(stderr,
					"WARNING: ownership change on %s failed: %s",
					mapname, errstring(errno));
			}
		}
#endif

		break;
#endif

#ifdef NEWDB
	  case T_HASH:
		/* tweak some parameters for performance */
# if DB_VERSION_MAJOR < 2
		hinfo.nelem = 4096;
		hinfo.cachesize = dbcachesize;
# else
		dbinfo.h_nelem = 4096;
		dbinfo.db_cachesize = dbcachesize;
# endif
		
# if DB_VERSION_MAJOR < 2
		dbp.db = dbopen(mapname, mode, 0644, DB_HASH, &hinfo);
# else
		{
			int flags = 0;
			
			if (bitset(O_CREAT, mode))
				flags |= DB_CREATE;
			if (bitset(O_TRUNC, mode))
				flags |= DB_TRUNCATE;
			
			dbp.db = NULL;
			errno = db_open(mapname, DB_HASH, flags, 0644,
					NULL, &dbinfo, &dbp.db);
		}
# endif
		if (dbp.db != NULL)
		{
			int fd;
			
# if DB_VERSION_MAJOR < 2
			fd = dbp.db->fd(dbp.db);
# else
			fd = -1;
			errno = dbp.db->fd(dbp.db, &fd);
# endif
			if (filechanged(dbuf, fd, &std))
			{
				fprintf(stderr,
					"db map %s: file changed after open\n",
					mapname);
# if DB_VERSION_MAJOR < 2
				dbp.db->close(dbp.db);
# else
				errno = dbp.db->close(dbp.db, 0);
# endif
				exit(EX_CANTCREAT);
			}
			(void) (*dbp.db->sync)(dbp.db, 0);
#if _FFR_TRUSTED_USER
			if (geteuid() == 0 && TrustedUid != 0)
			{
				if (fchown(fd, TrustedUid, -1) < 0)
				{
					fprintf(stderr,
						"WARNING: ownership change on %s failed: %s",
						mapname, errstring(errno));
				}
			}
#endif
		}
		break;

	  case T_BTREE:
		/* tweak some parameters for performance */
# if DB_VERSION_MAJOR < 2
		bti.cachesize = dbcachesize;
# else
		dbinfo.db_cachesize = dbcachesize;
# endif

# if DB_VERSION_MAJOR < 2
		dbp.db = dbopen(mapname, mode, 0644, DB_BTREE, &bti);
# else
		{
			int flags = 0;
			
			if (bitset(O_CREAT, mode))
				flags |= DB_CREATE;
			if (bitset(O_TRUNC, mode))
				flags |= DB_TRUNCATE;
			
			dbp.db = NULL;
			errno = db_open(mapname, DB_BTREE, flags, 0644,
					NULL, &dbinfo, &dbp.db);
		}
# endif
		if (dbp.db != NULL)
		{
			int fd;
			
# if DB_VERSION_MAJOR < 2
			fd = dbp.db->fd(dbp.db);
# else
			fd = -1;
			errno = dbp.db->fd(dbp.db, &fd);
# endif
			if (filechanged(dbuf, fd, &std))
			{
				fprintf(stderr,
					"db map %s: file changed after open\n",
					mapname);
# if DB_VERSION_MAJOR < 2
				dbp.db->close(dbp.db);
# else
				errno = dbp.db->close(dbp.db, 0);
# endif
				exit(EX_CANTCREAT);
			}
			(void) (*dbp.db->sync)(dbp.db, 0);
#if _FFR_TRUSTED_USER
			if (geteuid() == 0 && TrustedUid != 0)
			{
				if (fchown(fd, TrustedUid, -1) < 0)
				{
					fprintf(stderr,
						"WARNING: ownership change on %s failed: %s",
						mapname, errstring(errno));
				}
			}
#endif
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
			fprintf(stderr, "%s: %s: line %d: line too long (%ld bytes max)\n",
				progname, mapname, lineno, (long) sizeof ibuf);
			continue;
		}
			
		if (ibuf[0] == '\0' || ibuf[0] == '#')
			continue;
		if (isascii(ibuf[0]) && isspace(ibuf[0]))
		{
			fprintf(stderr, "%s: %s: line %d: syntax error (leading space)\n",
				progname, mapname, lineno);
			continue;
		}
#ifdef NEWDB
		if (type == T_HASH || type == T_BTREE)
		{
			bzero(&key.db, sizeof key.db);
			bzero(&val.db, sizeof val.db);
		}
#endif

		key.xx.data = ibuf;
		for (p = ibuf; *p != '\0' && !(isascii(*p) && isspace(*p)); p++)
		{
			if (foldcase && isascii(*p) && isupper(*p))
				*p = tolower(*p);
		}
		key.xx.size = p - key.xx.data;
		if (inclnull)
			key.xx.size++;
		if (*p != '\0')
			*p++ = '\0';
		while (isascii(*p) && isspace(*p))
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
# if DB_VERSION_MAJOR < 2
			st = (*dbp.db->put)(dbp.db, &key.db, &val.db, putflags);
# else
			errno = (*dbp.db->put)(dbp.db, NULL, &key.db,
					       &val.db, putflags);
			switch (errno)
			{
			  case DB_KEYEXIST:
				st = 1;
				break;

			  case 0:
				st = 0;
				break;

			  default:
				st = -1;
				break;
			}
# endif
			break;
#endif
		  default:
			break;
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
# if DB_VERSION_MAJOR < 2
		if ((*dbp.db->close)(dbp.db) < 0)
# else
		if ((errno = (*dbp.db->close)(dbp.db, 0)) != 0)
# endif
		{
			fprintf(stderr, "%s: %s: error on close\n",
				progname, mapname);
			perror(mapname);
			exitstat = EX_IOERR;
		}
#endif
	  default:
		break;
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

/*VARARGS1*/
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
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	vfprintf(stderr, m, ap);
	VA_END;
	fprintf(stderr, "\n");
}

/*VARARGS1*/
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
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
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
#if !HASSTRERROR
	static char errstr[64];
# if !defined(ERRLIST_PREDEFINED)
	extern char *sys_errlist[];
	extern int sys_nerr;
# endif
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
	if (err < 0 || err >= sys_nerr)
	{
		sprintf(errstr, "Error %d", err);
		return errstr;
	}
	return sys_errlist[err];
#endif
}

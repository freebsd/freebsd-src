/* cmds.c */

/*  $RCSfile: cmds.c,v $
 *  $Revision: 1.1.1.1 $
 *  $Date: 1994/09/22 23:45:33 $
 */

#include "sys.h"

#include <sys/wait.h>

#include <sys/stat.h>
#include <arpa/ftp.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#ifdef SYSLOG
#	include <syslog.h>
#endif

#include "util.h"
#include "cmds.h"
#include "main.h"
#include "ftp.h"
#include "ftprc.h"
#include "getpass.h"
#include "glob.h"
#include "open.h"
#include "set.h"
#include "defaults.h"
#include "copyright.h"

/* cmds.c globals */
int					curtype;			/* file transfer type */
char				*typeabbrs = "abiet";
str32				curtypename;		/* name of file transfer type */
int					verbose; 			/* verbosity level of output */
int					mprompt;			/* interactively prompt on m* cmds */
int					passivemode;		/* no reverse FTP connections */
int					restricted_data_ports;		/* high port range */
int					debug;				/* debugging level */
int					options;			/* used during socket creation */
int					macnum;				/* number of defined macros */
int					paging = 0;
int					creating = 0;
struct macel		macros[MAXMACROS];
char				*macbuf;			/* holds ALL macros */
int					doingInitMacro = 0;	/* TRUE if executing "init" macro. */
static char			pad1a[8] = "Pad 1a";
jmp_buf				jabort;
static char			pad1b[8] = "Pad 1b";
char				*mname;				/* name of current m* command */
int					activemcmd;			/* flag: if != 0, then active multi command */
int					warnNoLSFlagsWithWildcards = 0;
										/* Tells whether the user has been
										 * warned about not being able to use
										 * flags with ls when using wildcards.
										 */
longstring			cwd;				/* current remote directory */
longstring			lcwd;				/* current local directory */
Hostname			lasthostname;		/* name of last host w/ lookup(). */
int					logged_in = 0;		/* TRUE if connected and user/pw OK. */
int					is_ls = 0;			/* are we doing an ls?  if so, then
										   read input into a line buffer
										   for re-use. */
extern int					buffer_only;
struct lslist       		*lshead = NULL;	/* hold last output from host */
struct lslist       		*lstail = NULL;

/* cmds.c externs */
extern char					*globerr, *home, *reply_string;
extern int					margc, connected, ansi_escapes;
extern int					code, connected;
extern int					toatty, fromatty;
extern int					data, progress_meter, remote_is_unix;
extern int					parsing_rc, keep_recent;
extern char					*altarg, *line, *margv[];
extern char					*globchars;
extern Hostname				hostname;
extern RemoteSiteInfo		gRmtInfo;
extern string				progname, pager, anon_password;
extern string				prompt, version, indataline;
extern longstring			logfname;
extern long					logsize;
extern size_t				xferbufsize;
extern struct servent		serv;
extern struct cmd			cmdtab[];
extern struct userinfo		uinfo;
extern FILE					*cin, *cout, *logf;
extern int					Optind;
extern char					*Optarg;
extern int					Optind;
extern char					*Optarg;

#ifdef STRICT_PROTOS
extern int gethostname(char *, int), getdomainname(char *, int);
#endif


struct types types[] = {
    { "ascii",  "A",    TYPE_A, 0 },
    { "binary", "I",    TYPE_I, 0 },
    { "image",  "I",    TYPE_I, 0 },
    { "ebcdic", "E",    TYPE_E, 0 },
    { "tenex",  "L",    TYPE_L, "8" },
    { 0 }
};



long GetDateSizeFromLSLine(char *fName, unsigned long *mod_time)
{
	char *cp, *np;
	string lsline;
	long size = SIZE_UNKNOWN;
	int n, v;
	struct lslist *savedh, *savedt;
	static int depth = 0;

	depth++;	/* Try to prevent infinite recursion. */
	*mod_time = MDTM_UNKNOWN;
	v = verbose; verbose = V_QUIET;
	is_ls = 1;
	buffer_only = 1;
	savedh = lshead;
	savedt = lstail;
	lshead = NULL;
	(void) recvrequest("LIST", "-", fName, "w");
	is_ls = 0;
	buffer_only = 0;
	verbose = v;
	if (lshead == NULL) {
		PurgeLineBuffer();
		lshead = savedh;
		lstail = savedt;
		goto aa;
	}
	(void) Strncpy(lsline, lshead->string);
	PurgeLineBuffer();
	lshead = savedh;
	lstail = savedt;

	if (code >= 400 && code < 500)
		goto aa;

	/* See if this line looks like a unix-style ls line. 
	 * If so, we can grab the date and size from it.
	 */	
	if (strpbrk(lsline, "-dlsbcp") == lsline) {
		/* See if it looks like a typical '-rwxrwxrwx' line. */
		cp = lsline + 1;
		if (*cp != 'r' && *cp != '-')
			goto aa;
		++cp;
		if (*cp != 'w' && *cp != '-')
			goto aa;
		cp += 2;
		if (*cp != 'r' && *cp != '-')
			goto aa;
 
 		/* skip mode, links, owner (and possibly group) */
 		for (n = 0; n < 4; n++) {
 			np = cp;
 			while (*cp != '\0' && !isspace(*cp))
 				cp++;
 			while (*cp != '\0' &&  isspace(*cp))
 				cp++;
 		}
 		if (!isdigit(*cp))
 			cp = np;	/* back up (no group) */
 		(void) sscanf(cp, "%ld%n", &size, &n);
 
 		*mod_time = UnLSDate(cp + n + 1);

		if (size < 100) {
			/* May be the size of a link to the file, instead of the file. */
			if ((cp = strstr(lsline, " -> ")) != NULL) {
				/* Yes, it was a link. */
				size = (depth>4) ? SIZE_UNKNOWN :
					GetDateAndSize(cp + 4, mod_time);
				/* Try the file. */
			}
		}
	}	
aa:
	--depth;
	return (size);
}	/* GetDateSizeFromLSLine */




/* The caller wanted to know the modification date and size of the remote
 * file given to us.  We try to get this information by using the SIZE
 * and MDTM ftp commands, and if that didn't work we try sending the site
 * a "ls -l <fName>" and try to get that information from the line it
 * sends us back.  It is possible that we won't be able to determine
 * either of these, though.
 */
long GetDateAndSize(char *fName, unsigned long *mod_time)
{
	unsigned long mdtm, ls_mdtm;
	long size, ls_size;
	int have_mdtm, have_size;
	string cmd;

	size = SIZE_UNKNOWN;
	mdtm = MDTM_UNKNOWN;
	if (fName != NULL) {
		have_mdtm = have_size = 0;
		if (gRmtInfo.hasSIZE) {
			(void) Strncpy(cmd, "SIZE ");
			(void) Strncat(cmd, fName);
			if (quiet_command(cmd) == 2) {
				if (sscanf(reply_string, "%*d %ld", &size) == 1)
					have_size = 1;
			} else if (strncmp(reply_string, "550", (size_t)3) != 0)
				gRmtInfo.hasSIZE = 0;
		}

#ifndef NO_MKTIME
		/* We'll need mktime() to un-mangle this. */
		if (gRmtInfo.hasMDTM) {
			(void) Strncpy(cmd, "MDTM ");
			(void) Strncat(cmd, fName);
			if (quiet_command(cmd) == 2) {
				/* Result should look like "213 19930602204445\n" */
				mdtm = UnMDTMDate(reply_string);
				if (mdtm != MDTM_UNKNOWN)
					have_mdtm = 1;
			} else if (strncmp(reply_string, "550", (size_t)3) != 0)
				gRmtInfo.hasMDTM = 0;
		}
#endif /* NO_MKTIME */

		if (!have_mdtm || !have_size)
			ls_size = GetDateSizeFromLSLine(fName, &ls_mdtm);

		/* Try to use the information from the real SIZE/MDTM commands if
		 * we could, since some maverick ftp server may be using a non-standard
		 * ls command, and we could parse it wrong.
		 */
		
		if (!have_mdtm)
			mdtm = ls_mdtm;
		if (!have_size)
			size = ls_size;

		dbprintf("Used SIZE: %s;  Used MDTM: %s\n",
			have_size ? "yes" : "no",
			have_mdtm ? "yes" : "no"
		);

		if (debug > 0) {
			if (size != SIZE_UNKNOWN)
				dbprintf("Size: %ld\n", size);
			if (mdtm != MDTM_UNKNOWN)
				dbprintf("Mdtm: %s\n", ctime((time_t *) &mdtm));
		}
	}
	*mod_time = mdtm;
	return size;
}	/* GetDateAndSize */





int _settype(char *typename)
{
	register struct types	*p;
	int						comret, c;
	string					cmd;
	char					*cp;
 
	c = isupper(*typename) ? tolower(*typename) : (*typename);
	if ((cp = index(typeabbrs, c)) != NULL)
		p = &types[(int) (cp - typeabbrs)];
	else {
		(void) printf("%s: unknown type\n", typename);
		return USAGE;
	}
	if (c == 't')
		(void) strcpy(cmd, "TYPE L 8");
	else	
		(void) sprintf(cmd, "TYPE %s", p->t_mode);
	comret = command(cmd);
	if (comret == COMPLETE) {
		(void) Strncpy(curtypename, p->t_name);
		curtype = p->t_type;
	}
	return NOERR;
}	/* _settype */




int SetTypeByNumber(int i)
{
	char tstr[4], *tp = tstr, c;

	tp[1] = c = 0;
	switch (i) {
		case TYPE_A: c = 'a'; break;
		case TYPE_I: c = 'b'; break;
		case TYPE_E: c = 'e'; break;
		case TYPE_L: c = 't';
	}
	*tp = c;
	return (c == 0 ? -1 : _settype(tp));
}	/* SetTypeByNumber */




/*
 * Set transfer type.
 */
int settype(int argc, char **argv)
{
	int result = NOERR;

	if (argc > 2) {
		result = USAGE;
	} else {
		if (argc < 2)
			goto xx;
		result = _settype(argv[1]);
		if (IS_VVERBOSE)
xx:			(void) printf("Using %s mode to transfer files.\n", curtypename);
	}
	return result;
}	/* settype */




/*ARGSUSED*/
int setbinary(int argc, char **argv) {	return (_settype("binary")); }
/*ARGSUSED*/
int setascii(int argc, char **argv) {	return (_settype("ascii")); }



/*
 * Send a single file.
 */
int put(int argc, char **argv)
{
	char *cmd;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
	}
	if (argc < 2)
		argv = re_makeargv("(local-file) ", &argc);
	if (argc < 2) {
usage:
		return USAGE;
	}
	if (argc < 3)
		argv = re_makeargv("(remote-file) ", &argc);
	if (argc < 3) 
		goto usage;
	cmd = (argv[0][0] == 'a') ? "APPE" : "STOR";
	(void) sendrequest(cmd, argv[1], argv[2]);
	return NOERR;
}	/* put */




/*
 * Send multiple files.
 */
int mput(int argc, char **argv)
{
	register int i;
	Sig_t oldintr;
	char *tp;

	if (argc < 2)
		argv = re_makeargv("(local-files) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	mname = argv[0];
	activemcmd = 1;
	oldintr = Signal(SIGINT, mabort);
	(void) setjmp(jabort);
	for (i = 1; i < argc; i++) {
		register char **cpp, **gargs;
		char *icopy;
		
		/* Make a copy of the argument, because glob() will just copy
		 * the pointer you give it to the glob-arg vector, and blkfree()
		 * will want to free each element of the glob-arg vector
		 * later.
		 */
		if ((icopy = NewString(argv[i])) == NULL)
			break;
		gargs = glob(icopy);
		if (globerr != NULL) {
			(void) printf("%s\n", globerr);
			if (gargs) {
				blkfree(gargs);
				Free(gargs);
			}
			continue;
		}
		for (cpp = gargs; cpp && *cpp != NULL; cpp++) {
			if (activemcmd && confirm(argv[0], *cpp)) {
				tp = *cpp;
				(void) sendrequest("STOR", *cpp, tp);
				if (!activemcmd && fromatty) {
					if (confirm("Continue with","mput")) {
						activemcmd++;
					}
				}
			}
		}
		if (gargs != NULL) {
			blkfree(gargs);
			Free(gargs);
		}
	}
	(void) Signal(SIGINT, oldintr);
	activemcmd = 0;
	return NOERR;
}	/* mput */




int rem_glob_one(char *pattern)
{
	int			oldverbose, result = 0;
	char		*cp;
	string		str, tname;
	FILE		*ftemp;

	/* Check for wildcard characters. */
	if (*pattern == '|' || strpbrk(pattern, globchars) == NULL)
		return 0;

	(void) tmp_name(tname);
	oldverbose = verbose;
	verbose = V_QUIET;
	(void) recvrequest ("NLST", tname, pattern, "w");
	verbose = oldverbose;
	ftemp = fopen(tname, "r");
	(void) chmod(tname, 0600);
	if (ftemp == NULL || FGets(str, ftemp) == NULL) {
		if (NOT_VQUIET)
			(void) printf("%s: no match.\n", pattern);
		result = -1;
		goto done;
	}
	if ((cp = index(str, '\n')) != NULL)
		*cp = '\0';
	(void) strcpy(pattern, str);
	cp = FGets(str, ftemp);
	/* It is an error if the pattern matched more than one file. */
	if (cp != NULL) {
		if (NOT_VQUIET)
			(void) printf("?Ambiguous remote file name.\n");
		result = -2;
	}
done:
	if (ftemp != NULL)
		(void) fclose(ftemp);
	(void) unlink(tname);
	return (result);
}	/* rem_glob_one */




/*
 * Receive (and maybe page) one file.
 */
int get(int argc, char **argv)
{
	string local_file;
	char remote_file[256];
	char *cp;
	int oldtype = curtype, try_zcat;
	size_t len;

	/* paging mode is set if the command name is 'page' or 'more.' */
	paging = (**argv != 'g');

	if (argc < 2)
		argv = re_makeargv("(remote-file) ", &argc);

	if (argc < 2) {
		return USAGE;
	}
	cp = Strncpy(remote_file, argv[1]);
	argv[1] = cp;
	if (rem_glob_one(argv[1]) < 0)
		return CMDERR;

	if (paging) {
		try_zcat = 0;
		len = strlen(remote_file);

		if (len > (size_t) 2) {
 		    if (remote_file[len-2] == '.') {
				/* Check for .Z files. */
				if (remote_file[len-1] == 'Z')
					try_zcat = 1;
#ifdef GZCAT
				/* Check for .z (gzip) files. */
				if (remote_file[len-1] == 'z')
					try_zcat = 1;
#endif	/* GZCAT */
			}
		}

#ifdef GZCAT
		if (len > (size_t) 3) {
			/* Check for ".gz" (gzip) files. */
			if (strcmp(remote_file + len - 3, ".gz") == 0)
				try_zcat = 1;
		}
#endif	/* GZCAT */

		/* Run compressed remote files through zcat, then the pager.
		 * If GZCAT was defined, we also try paging gzipped files.
		 * Note that ZCAT is defined to be GZCAT if you defined
		 * GZCAT.
		 */
		
 		if (try_zcat) {
			(void) _settype("b");
			(void) sprintf(local_file, "|%s ", ZCAT);
			argv[2] = Strncat(local_file, pager);
		} else {
			/* Try to use text mode for paging, so newlines get converted. */
			(void) _settype("a");
			argv[2] = pager;
		}
	} else {
		/* normal get */
		if (argc == 2) {
			(void) Strncpy(local_file, argv[1]);
			argv[2] = local_file;
		} else {
			if (argc < 3)
				argv = re_makeargv("(local-file) ", &argc);
			if (argc < 3) 
				return USAGE;
			(void) LocalDotPath(argv[2]);
		}
	}
	(void) recvrequest("RETR", argv[2], argv[1], "w");
	if (paging) {
		(void) SetTypeByNumber(oldtype);	/* Restore it to what it was. */
		paging = 0;
	}
	return NOERR;
}	/* get */



/*ARGSUSED*/
void mabort SIG_PARAMS
{
	(void) printf("\n");
	(void) fflush(stdout);
	if (activemcmd && fromatty) {
		if (confirm("Continue with", mname)) {
			longjmp(jabort,0);
		}
	}
	activemcmd = 0;
	longjmp(jabort,0);
}	/* mabort */




/*
 * Get multiple files.
 */
int mget(int argc, char **argv)
{
	char *cp;
	longstring local;
	Sig_t oldintr;
	int errs;

	if (argc < 2)
		argv = re_makeargv("(remote-files) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	mname = argv[0];
	activemcmd = 1;
	oldintr = Signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv, &errs)) != NULL) {
		if (*cp == '\0') {
			activemcmd = 0;
			continue;
		}
		if (activemcmd && confirm(argv[0], cp)) {
			(void) Strncpy(local, cp);
			(void) recvrequest("RETR", local, cp, "w");
			if (!activemcmd && fromatty) {
				if (confirm("Continue with","mget")) {
					activemcmd++;
				}
			}
		}
	}
	(void) Signal(SIGINT,oldintr);
	activemcmd = 0;
	if (!errs)
	return NOERR;
	else
		return CMDERR;
}	/* mget */




char *remglob(char *argv[], int *errs)
{
	static FILE			*ftemp = NULL;
	int					oldverbose, i;
	char				*cp, *mode;
	static string		tmpname, str;
	int					result;

	if (!activemcmd) {
xx:
		if (ftemp) {
			(void) fclose(ftemp);
			ftemp = NULL;
			(void) unlink(tmpname);
		}
		return(NULL);
	}
	if (ftemp == NULL) {
		(void) tmp_name(tmpname);
		oldverbose = verbose, verbose = V_QUIET;
		*errs = 0;
		for (mode = "w", i=1; argv[i] != NULL; i++, mode = "a") {
			result = recvrequest ("NLST", tmpname, argv[i], mode);
			if (i == 1)
				(void) chmod(tmpname, 0600);
			if (result < 0) {
				fprintf(stderr, "%s: %s.\n",
					argv[i],
					(strpbrk(argv[i], globchars) != NULL) ? "No match" :
						"No such file"
				);
				++(*errs);
			}
		}
              verbose = oldverbose;
		if (*errs == (i - 1)) {
			/* Every pattern was in error, so we can't try anything. */
			(void) unlink(tmpname);		/* Shouldn't be there anyway. */
			return NULL;
		}
		ftemp = fopen(tmpname, "r");
		if (ftemp == NULL) {
			PERROR("remglob", tmpname);
			return (NULL);
		}
	}
	if (FGets(str, ftemp) == NULL) 
		goto xx;
	if ((cp = index(str, '\n')) != NULL)
		*cp = '\0';
	return (str);
}	/* remglob */


/*
 * Turn on/off printing of server echo's, messages, and statistics.
 */
int setverbose(int argc, char **argv)
{
	if (argc > 1)
		set_verbose(argv[1], 0);
	else set_verbose(argv[1], -1);
	return NOERR;
}	/* setverbose */



/*
 * Toggle interactive prompting
 * during mget, mput, and mdelete.
 */
int setprompt(int argc, char **argv)
{
	if (argc > 1)
		mprompt = StrToBool(argv[1]);
	else mprompt = !mprompt;
	if (IS_VVERBOSE)
		(void) printf("Interactive prompting for m* commmands %s.\n", onoff(mprompt));
	return NOERR;
}	/* setprompt */




void fix_options(void)
{
	if (debug)
		options |= SO_DEBUG;
	else
	options &= ~SO_DEBUG;
}   /* fix_options */


/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
int setdebug(int argc, char **argv)
{
	int val;

	if (argc > 1) {
		val = StrToBool(argv[1]);
		if (val < 0) {
			(void) printf("%s: bad debugging value.\n", argv[1]);
			return USAGE;
		}
	} else
		val = !debug;
	debug = val;
	fix_options();
	if (IS_VVERBOSE)
		(void) printf("Debugging %s (debug=%d).\n", onoff(debug), debug);
	return NOERR;
}	/* debug */



/*
 * Set current working directory
 * on remote machine.
 */
int cd(int argc, char **argv)
{
	if (argc < 2)
		argv = re_makeargv("(remote-directory) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	(void) _cd(argv[1]);
	return NOERR;
}	/* cd */




int implicit_cd(char *dir)
{
	int i, j = 0;
	
	if (connected) {
		i = verbose;
		/* Special verbosity level that ignores errors and prints other stuff,
		 * so you will get just the unknown command message and not an error
		 * message from cd.
		 */
		verbose = V_IMPLICITCD;
		j = _cd(dir);
		verbose = i;
	}
	return j;
}	/* implicit_cd */




int _cd(char *dir)
{
	register char *cp;
	int result = 0;
	string str;

	if (dir == NULL)
		goto getrwd;
	/* Won't work because glob really is a ls, so 'cd pu*' will match
	 * pub/README, pub/file2, etc.
	 *	if (result = rem_glob_one(dir) < 0)
	 *	return result;
	 */
	if (strncmp(dir, "CDUP", (size_t) 4) == 0)
		(void) Strncpy(str, dir);
	else
		(void) sprintf(str, "CWD %s", dir);
	if (command(str) != 5) {
getrwd:
		(void) quiet_command("PWD");
		cp = rindex(reply_string, '\"');
		if (cp != NULL) {
			result = 1;
			*cp = '\0';
			cp = index(reply_string, '\"');
			if (cp != NULL)
				(void) Strncpy(cwd, ++cp);
		}
	}
	dbprintf("Current remote directory is \"%s\"\n", cwd);
	return (result);
}	/* _cd */




/*
 * Set current working directory
 * on local machine.
 */
int lcd(int argc, char **argv)
{
	longstring ldir;

	if (argc < 2)
		argc++, argv[1] = home;
	if (argc != 2) {
		return USAGE;
	}
	(void) Strncpy(ldir, argv[1]);
	if (chdir(LocalDotPath(ldir)) < 0) {
		PERROR("lcd", ldir);
		return CMDERR;
	}
	(void) get_cwd(lcwd, (int) sizeof(lcwd));
	if (NOT_VQUIET) 
		(void) printf("Local directory now %s\n", lcwd);
	return NOERR;
}	/* lcd */




/*
 * Delete a single file.
 */
int do_delete(int argc, char **argv)
{
	string str;

	if (argc < 2)
		argv = re_makeargv("(remote file to delete) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	if (rem_glob_one(argv[1]) == 0) {
		(void) sprintf(str, "DELE %s", argv[1]);
		(void) command(str);
	}
	return NOERR;
}	/* do_delete */




/*
 * Delete multiple files.
 */
int mdelete(int argc, char **argv)
{
	char *cp;
	Sig_t oldintr;
	string str;
	int errs;

	if (argc < 2)
		argv = re_makeargv("(remote-files) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	mname = argv[0];
	activemcmd = 1;
	oldintr = Signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv, &errs)) != NULL) {
		if (*cp == '\0') {
			activemcmd = 0;
			continue;
		}
		if (activemcmd && confirm(argv[0], cp)) {
			(void) sprintf(str, "DELE %s", cp);
			(void) command(str);
			if (!activemcmd && fromatty) {
				if (confirm("Continue with", "mdelete")) {
					activemcmd++;
				}
			}
		}
	}
	(void) Signal(SIGINT, oldintr);
	activemcmd = 0;
	if (errs > 0)
		return CMDERR;
	return NOERR;
}	/* mdelete */




/*
 * Rename a remote file.
 */
int renamefile(int argc, char **argv)
{
	string str;

	if (argc < 2)
		argv = re_makeargv("(from-name) ", &argc);
	if (argc < 2) {
usage:
		return USAGE;
	}
	if (argc < 3)
		argv = re_makeargv("(to-name) ", &argc);
	if (argc < 3)
		goto usage;
	if (rem_glob_one(argv[1]) < 0)
		return CMDERR;
	(void) sprintf(str, "RNFR %s", argv[1]);
	if (command(str) == CONTINUE) {
		(void) sprintf(str, "RNTO %s", argv[2]);
		(void) command(str);
	}
	return NOERR;
}	/* renamefile */



/*
 * Get a directory listing
 * of remote files.
 */
int ls(int argc, char **argv)
{
	char		*whichcmd, *cp;
	str32		lsflags;
	string		remote, local, str;
	int			listmode, pagemode, i;

	PurgeLineBuffer();
	pagemode = 0;
	switch (**argv) {
		case 'p':							/* pls, pdir, pnlist */
			pagemode = 1;
			listmode = argv[0][1] == 'd';
			break;
		case 'd':							/* dir */
			listmode = 1;
			break;
		default:							/* ls, nlist */
			listmode = 0;
	}
	whichcmd = listmode ? "LIST" : "NLST";

	(void) strncpy(local, (pagemode ? pager : "-"), sizeof(local));
	remote[0] = lsflags[0] = 0;
	
	/* Possible scenarios:
	 *  1.	ls
	 *  2.	ls -flags
	 *  3.	ls directory
	 *  4.  ls -flags >outfile
	 *  5.  ls directory >outfile
     *  6.  ls -flags directory
	 *  7.  ls -flags directory >outfile
	 *
	 * Note that using a wildcard will choke with flags.  I.e., don't do
	 * "ls -CF *.tar," but instead do "ls *.tar."
	 */

	for (i=1; i<argc; i++) {
		switch (argv[i][0]) {
			case '-': 
				/*
				 * If you give more than one set of flags, concat the each
				 * additional set to the first one (without the dash).
				 */
				(void) strncat(lsflags, (argv[i] + (lsflags[0] == '-')), sizeof(lsflags));
				break;
			case '|':
				(void) Strncpy(local, argv[i]);
				LocalDotPath(local + 1);
				break;
			case '>':
				/* We don't want the '>'. */
				(void) Strncpy(local, argv[i] + 1);
				LocalDotPath(local);
				break;
			default:  
				cp = argv[i];
				/*
				 * In case you want to get a remote file called '--README--'
				 * or '>README,' you can use '\--README--' and '\>README.'
				 */
				if ((cp[1] != 0) && (*cp == '\\'))
					++cp;
				if (remote[0] != 0) {
					(void) Strncat(remote, " ");
					(void) Strncat(remote, cp);
				} else {
					(void) Strncpy(remote, cp);
				}
		}	/* end switch */	
	}		/* end loop */

	/*
	 *	If we are given an ls with some flags, make sure we use 
	 *	columnized output (-C) unless one column output (-1) is
	 *	specified.
	 */
	if (!listmode) {
		if (lsflags[0] != 0) {
			(void) Strncpy(str, lsflags);
			for (cp = str + 1; *cp; cp++)
				if (*cp == '1')
					goto aa;
			(void) sprintf(lsflags, "-FC%s", str + 1);
		} else {
			if (remote_is_unix)
				(void) strcpy(lsflags, "-FC");
		}
		/* As noted above, we can't use -flags if the user gave a
		 * wildcard expr.
		 */
		if (remote_is_unix && (strpbrk(remote, globchars) != NULL)) {
			lsflags[0] = 0;
			/* Warn the user what's going on. */
			if ((warnNoLSFlagsWithWildcards == 0) && NOT_VQUIET) {
				(void) fprintf(stderr, "Warning: ls flags disabled with wildcard expressions.\n");
				warnNoLSFlagsWithWildcards++;
			}
		}
	}

aa:
	is_ls = 1; /* tells getreply() to start saving input to a buffer. */
	(void) Strncpy(str, remote);
	if (lsflags[0] && remote[0])
		(void) sprintf(remote, "%s%c%s", lsflags, LS_FLAGS_AND_FILE, str);
	else
		(void) strncpy(remote, lsflags[0] ? lsflags : str, sizeof(remote));
	(void) recvrequest(whichcmd, local, (remote[0] == 0 ? NULL : remote), "w");
	is_ls=0;
	return NOERR;
}	/* ls */



/*
 * Do a shell escape
 */
/*ARGSUSED*/
int shell(int argc, char **argv)
{
	int				pid;
	Sig_t			old1, old2;
	char			*theShell, *namep;
#ifndef U_WAIT
	int				Status;
#else
	union wait		Status;
#endif
	string			str;

	old1 = signal (SIGINT, SIG_IGN);
	old2 = signal (SIGQUIT, SIG_IGN);
	/* This will prevent <defunct> zombie processes. */
	/* (void) signal(SIGCHLD, SIG_IGN); */

	if ((pid = fork()) == 0) {
		for (pid = 3; pid < 20; pid++)
			(void) close(pid);
		(void) Signal(SIGINT, SIG_DFL);
		(void) Signal(SIGQUIT, SIG_DFL);
		if ((theShell = getenv("SHELL")) == NULL)
			theShell = uinfo.shell;
		if (theShell == NULL)
			theShell = "/bin/sh";
		namep = rindex(theShell, '/');
		if (namep == NULL)
			namep = theShell;
		(void) strcpy(str, "-");
		(void) strcat(str, ++namep);
		if (strcmp(namep, "sh") != 0)
			str[0] = '+';
		dbprintf ("%s\n", theShell);
#if defined(BSD) || defined(_POSIX_SOURCE)
		setreuid(-1,getuid());
		setregid(-1,getgid());
#endif
		if (argc > 1)
			(void) execl(theShell, str, "-c", altarg, (char *)0);
		else
			(void) execl(theShell, str, (char *)0);
		PERROR("shell", theShell);
		exit(1);
		}
	if (pid > 0)
		while (wait((void *) &Status) != pid)
			;
	(void) Signal(SIGINT, old1);
	(void) Signal(SIGQUIT, old2);
	if (pid == -1) {
		PERROR("shell", "Try again later");
	}
	return NOERR;
}	/* shell */




/*
 * Send new user information (re-login)
 */
int do_user(int argc, char **argv)
{
	char			acct[80];
	int				n, aflag = 0;
	string			str;

	if (argc < 2)
		argv = re_makeargv("(username) ", &argc);
	if (argc > 4) {
		return USAGE;
	}
	(void) sprintf(str, "USER %s", argv[1]);
	n = command(str);
	if (n == CONTINUE) {
		if (argc < 3 )
			argv[2] = Getpass("Password: "), argc++;
		(void) sprintf(str, "PASS %s", argv[2]);
		n = command(str);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			(void) printf("Account: "); (void) fflush(stdout);
			(void) FGets(acct, stdin);
			acct[strlen(acct) - 1] = '\0';
			argv[3] = acct; argc++;
		}
		(void) sprintf(str, "ACCT %s", argv[3]);
		n = command(str);
		aflag++;
	}
	if (n != COMPLETE) {
		(void) fprintf(stdout, "Login failed.\n");
		logged_in = 0;
		return (0);
	}
	if (!aflag && argc == 4) {
		(void) sprintf(str, "ACCT %s", argv[3]);
		(void) command(str);
	}
	logged_in = 1;
	CheckRemoteSystemType(0);
	return NOERR;
}	/* do_user */




/*
 * Print working directory.
 */
/*ARGSUSED*/
int pwd(int argc, char **argv)
{
	(void) verbose_command("PWD");
	return NOERR;
}	/* pwd */




/*
 * Make a directory.
 */
int makedir(int argc, char **argv)
{
	string str;

	if (argc < 2)
		argv = re_makeargv("(directory-name) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	(void) sprintf(str, "MKD %s", argv[1]);
	(void) command(str);
	return NOERR;
}	/* makedir */




/*
 * Remove a directory.
 */
int removedir(int argc, char **argv)
{
	string str;
	if (argc < 2)
		argv = re_makeargv("(directory-name) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	if (rem_glob_one(argv[1]) == 0) {
		(void) sprintf(str, "RMD %s", argv[1]);
		(void) command(str);
	}
	return NOERR;
}	/* removedir */




/*
 * Send a line, verbatim, to the remote machine.
 */
int quote(int argc, char **argv)
{
	int i, tmpverbose;
	string str;

	if (argc < 2)
		argv = re_makeargv("(command line to send) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	str[0] = 0;
	if (*argv[0] == 's')	/* Command was 'site' instead of 'quote.' */
		(void) Strncpy(str, "site ");
	(void) Strncat(str, argv[1]);
	for (i = 2; i < argc; i++) {
		(void) Strncat(str, " ");
		(void) Strncat(str, argv[i]);
	}
	tmpverbose = verbose;
	verbose = V_VERBOSE;
	if (command(str) == PRELIM) {
		while (getreply(0) == PRELIM);
	}
	verbose = tmpverbose;
	return NOERR;
}	/* quote */




/*
 * Ask the other side for help.
 */
int rmthelp(int argc, char **argv)
{
	string str;

	if (argc == 1) (void) verbose_command("HELP");
	else {
		(void) sprintf(str, "HELP %s", argv[1]);
		(void) verbose_command(str);
	}
	return NOERR;
}	/* rmthelp */




/*
 * Terminate session and exit.
 */
/*ARGSUSED*/
int quit(int argc, char **argv)
{
	int rc;

	/* slightly kludge.  argc == -1 means failure from some other caller */
	rc = close_up_shop() || argc == -1;
	trim_log();
	exit(rc);
}	/* quit */



void close_streams(int wantShutDown)
{
	if (cout != NULL) {
		if (wantShutDown)
			(void) shutdown(fileno(cout), 1+1);
		(void) fclose(cout);
		cout = NULL;
	}
	if (cin != NULL) {
		if (wantShutDown)
			(void) shutdown(fileno(cin), 1+1);
		(void) fclose(cin);
		cin = NULL;
	}
}	/* close_streams */




/*
 * Terminate session, but don't exit.
 */
/*ARGSUSED*/
int disconnect(int argc, char **argv)
{
#ifdef SYSLOG
	syslog (LOG_INFO, "%s disconnected from %s.", uinfo.username, hostname);
#endif

	(void) command("QUIT");
	close_streams(0);
	if (logged_in)
		UpdateRecentSitesList(hostname, cwd);
	hostname[0] = cwd[0] = 0;
	logged_in = connected = 0;
	data = -1;
	macnum = 0;
	return NOERR;
}	/* disconnect */



int
close_up_shop(void)
{
	static int only_once = 0;
	int rcode = 0;

	if (only_once++ > 0)
		return (0);
	if (connected)
		(void) disconnect(0, NULL);
	rcode = WriteRecentSitesFile();
	if (logf != NULL) {
		(void) fclose(logf);
		logf = NULL;
	}
	return rcode;
}	/* close_up_shop */




/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
int globulize(char **cpp)
{
	char **globbed;

	(void) LocalPath(*cpp);
	globbed = glob(*cpp);
	if (globerr != NULL) {
		(void) printf("%s: %s\n", *cpp, globerr);
		if (globbed) {
			blkfree(globbed);
			Free(globbed);
		}
		return (0);
	}
	if (globbed) {
		*cpp = *globbed++;
		/* don't waste too much memory */
		if (*globbed) {
			blkfree(globbed);
			Free(globbed);
		}
	}
	return (1);
}	/* globulize */



/* change directory to perent directory */
/*ARGSUSED*/
int cdup(int argc, char **argv)
{
	(void) _cd("CDUP");
	return NOERR;
}	/* cdup */


/* show remote system type */
/*ARGSUSED*/
int syst(int argc, char **argv)
{
	(void) verbose_command("SYST");
	return NOERR;
}	/* syst */




int make_macro(char *name, FILE *fp)
{
	char			*tmp;
	char			*cp;
	string			str;
	size_t			len;
	int				i;

	if (macnum == MAXMACROS) {
		(void) fprintf(stderr, "Limit of %d macros have already been defined.\n", MAXMACROS);
		return -1;
	}

	/* Make sure macros have unique names.  If 'init' was attempted to be
	 * redefined, just return, since it was probably cmdOpen() in a redial
	 * mode which tried to define it again.
	 */
	for (i = 0; i<macnum; i++) {
		if (strncmp(name, macros[i].mac_name, (size_t)8) == 0) {
			if (parsing_rc) {
				/* Just shut up and read in the macro, but don't save it,
				 * because we already have it.
				 */
				while ((cp = FGets(str, fp)) != NULL) {
					/* See if we have a 'blank' line: just whitespace. */
					while (*cp && isspace(*cp)) ++cp;
					if (!*cp)
						break;
				}
			} else
				(void) fprintf(stderr,
					"There is already a macro named '%s.'\n", name);
			return -1;
		}
	}
	(void) strncpy(macros[macnum].mac_name, name, (size_t)8);
	if (macnum == 0)
		macros[macnum].mac_start = macbuf;
	else
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	tmp = macros[macnum].mac_start;
	while (1) {
		cp = FGets(str, fp);
		if (cp == NULL) {
			/*
			 * If we had started a macro, we will say it is
			 * okay to skip the blank line delimiter if we
			 * are at the EOF.
			 */
			if (tmp > macros[macnum].mac_start)
				goto endmac;
			(void) fprintf(stderr, "No text supplied for macro \"%s.\"\n", name);
		}
		/* see if we have a 'blank' line: just whitespace. */
		while (*cp && isspace(*cp)) ++cp;
		if (*cp == '\0') {
			/* Blank line; end this macro. */
endmac:
			macros[macnum++].mac_end = tmp;
			return 0;
		}
		/* Add the text of this line to the macro. */
		len = strlen(cp) + 1;	/* we need the \0 too. */
		if (tmp + len >= macbuf + MACBUFLEN) {
			(void) fprintf(stderr, "Macro \"%s\" not defined -- %d byte buffer exceeded.\n", name, MACBUFLEN);
			return -1;
		}
		(void) strcpy(tmp, cp);
		tmp += len;
	}
}	/* make_macro */




int macdef(int argc, char **argv)
{
	if (argc < 2)
		argv = re_makeargv("(macro name) ", &argc);
	if (argc != 2) {
		(void) domacro(0, NULL);
		return USAGE;
	}
	(void) printf("Enter macro line by line, terminating it with a blank line\n");
	(void) make_macro(argv[1], stdin);
	return NOERR;
}	/* macdef */




int domacro(int argc, char **argv)
{
	register int			i, j;
	register char			*cp1, *cp2;
	int						count = 2, loopflg = 0;
	string					str;
	struct cmd				*c;

	if (argc < 2) {
		/* print macros. */
		if (macnum == 0)
			(void) printf("No macros defined.\n");
		else {
			(void) printf("Current macro definitions:\n");
			for (i = 0; i < macnum; ++i) {
				(void) printf("%s:\n", macros[i].mac_name);
				cp1 = macros[i].mac_start;
				cp2 = macros[i].mac_end;
				while (cp1 < cp2) {
					(void) printf("   > ");
					while (cp1 < cp2 && *cp1)
						putchar(*cp1++);
					++cp1;
				}
			}
		}
		if (argc == 0) return (NOERR);	/* called from macdef(), above. */
		argv = re_makeargv("(macro to run) ", &argc);
	}			
	if (argc < 2) {
		return USAGE;
	}
	for (i = 0; i < macnum; ++i) {
		if (!strncmp(argv[1], macros[i].mac_name, (size_t) 9)) {
			break;
		}
	}
	if (i == macnum) {
		(void) printf("'%s' macro not found.\n", argv[1]);
		return USAGE;
	}
	doingInitMacro = (strcmp(macros[i].mac_name, "init") == 0);
	(void) Strncpy(str, line);
TOP:
	cp1 = macros[i].mac_start;
	while (cp1 != macros[i].mac_end) {
		while (isspace(*cp1)) {
			cp1++;
		}
		cp2 = line;
		while (*cp1 != '\0') {
		      switch(*cp1) {
		   	    case '\\':
				 *cp2++ = *++cp1;
				 break;
			    case '$':
				 if (isdigit(*(cp1+1))) {
				    j = 0;
				    while (isdigit(*++cp1)) {
					  j = 10*j +  *cp1 - '0';
				    }
				    cp1--;
				    if (argc - 2 >= j) {
					(void) strcpy(cp2, argv[j+1]);
					cp2 += strlen(argv[j+1]);
				    }
				    break;
				 }
				 if (*(cp1+1) == 'i') {
					loopflg = 1;
					cp1++;
					if (count < argc) {
					   (void) strcpy(cp2, argv[count]);
					   cp2 += strlen(argv[count]);
					}
					break;
				}
				/* intentional drop through */
			    default:
				*cp2++ = *cp1;
				break;
		      }
		      if (*cp1 != '\0') {
					cp1++;
		      }
		}
		*cp2 = '\0';
		makeargv();
		c = getcmd(margv[0]);
		if ((c == (struct cmd *) -1) && !parsing_rc) {
			(void) printf("?Ambiguous command\n");
		} else if (c == NULL && !parsing_rc) {
			(void) printf("?Invalid command\n");
		} else if (c->c_conn && !connected) {
			(void) printf("Not connected.\n");
		} else {
			if (IS_VVERBOSE)
				(void) printf("%s\n",line);
			if ((*c->c_handler)(margc, margv) == USAGE)
				cmd_usage(c);
			(void) strcpy(line, str);
			makeargv();
			argc = margc;
			argv = margv;
		}
		if (cp1 != macros[i].mac_end) {
			cp1++;
		}
	}
	if (loopflg && ++count < argc) {
		goto TOP;
	}
	doingInitMacro = 0;
	return NOERR;
}	/* domacro */



/*
 * get size of file on remote machine
 */
int sizecmd(int argc, char **argv)
{
	string str;

	if (argc < 2)
		argv = re_makeargv("(remote-file) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	if (rem_glob_one(argv[1]) == 0) {
		(void) sprintf(str, "SIZE %s", argv[1]);
		(void) verbose_command(str);
	}
	return NOERR;
}	/* sizecmd */




/*
 * get last modification time of file on remote machine
 */
int modtime(int argc, char **argv)
{
	int overbose;
	string str;

	if (argc < 2)
		argv = re_makeargv("(remote-file) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	if (rem_glob_one(argv[1]) == 0) {
		overbose = verbose;
		if (debug == 0)
			verbose = V_QUIET;
		(void) sprintf(str, "MDTM %s", argv[1]);
		if (command(str) == COMPLETE) {
			int yy, mo, day, hour, min, sec;
			(void) sscanf(reply_string, "%*s %04d%02d%02d%02d%02d%02d",
				&yy, &mo, &day, &hour, &min, &sec);
			/* might want to print this in local time */
			(void) printf("%s\t%02d/%02d/%04d %02d:%02d:%02d GMT\n", argv[1],
				mo, day, yy, hour, min, sec);
		} else
			(void) fputs(reply_string, stdout);
		verbose = overbose;
	}
	return NOERR;
}	/* modtime */



int lookup(int argc, char **argv)
{
	int i, j, by_name, result = NOERR;
	struct hostent *host;		/* structure returned by gethostbyaddr() */
	extern int h_errno;
#ifdef BAD_INETADDR
	struct in_addr addr;		/* address in host order */
# define ADDR	addr.s_addr
#else
	unsigned long addr;			/* address in host order */
# define ADDR	addr
#endif

	if (argc < 2)
		argv = re_makeargv("(sitename) ", &argc);
	if (argc < 2) {
		return USAGE;
	}

 	lasthostname[0] = 0;
	for (i=1; i<argc; i++) {
		/* does the argument look like an address? */
		if (4 == sscanf (argv[i], "%d.%d.%d.%d", &j, &j, &j, &j)) {
			/* ip */
  			addr = inet_addr (argv[i]);
  			if (ADDR == 0xffffffff) {
     			(void) fprintf(stderr, "## could not convert \"%s\" into a valid IP address.\n", argv[i]);
     			continue;
     		}
			host = gethostbyaddr ((char *) &ADDR, 4, AF_INET);
			by_name = 0;
		} else {
			/* name */
			host = gethostbyname (argv[i]);
			by_name = 1;
		}
		if (host == NULL) {
			if (NOT_VQUIET) {
				/* gethostxxx error */				
				if (h_errno == HOST_NOT_FOUND) {
	     			(void) printf("%s: lookup error (%d).\n",
	     				argv[i], h_errno);
	     			result = h_errno;
	 			} else {
	     			(void) printf("%s \"%s\"\n",
	     				(by_name==0 ? "unknown address" : "unknown host"),
	     				argv[i]);
	     			result = 
	     				h_errno != 0 ? h_errno :
	     				-1;
				}
			}
 		} else {
 			if (*host->h_name)
 				(void) Strncpy(lasthostname, host->h_name);
			for (j=0; host->h_aliases[j] != NULL; j++) {
				if (strlen(host->h_aliases[j]) >
					strlen(host->h_name) &&
					strstr(host->h_aliases[j],host->h_name) != NULL)
						(void) Strncpy(lasthostname,host->h_aliases[j]);
			}
			if (NOT_VQUIET) {
				(void) printf("%-32s  ", *host->h_name ? host->h_name : "???");
				if (*host->h_addr_list) {
					unsigned long horder;
	
					horder = ntohl (*(unsigned long *) *(char **)host->h_addr_list);
					(void) printf ("%lu.%lu.%lu.%lu\n",
						(horder >> 24),
						(horder >> 16) & 0xff,
						(horder >> 8) & 0xff,
						horder & 0xff);
				}
				else (void) printf("???\n");
			}
		}
    }	/* loop thru all sites */
    return result;
}	/* lookup */




int getlocalhostname(char *host, size_t size)
{
	int oldv, r;
	char *argv[2];
	char domain[64];

#ifdef HOSTNAME
	(void) strncpy(host, HOSTNAME, size);
	return NOERR;
#else
	host[0] = '\0';
	if ((r = gethostname(host, size)) == 0) {
		if (host[0] == '\0') {
			(void) fprintf(stderr,
"Could not determine the hostname. Re-compile with HOSTNAME defined\n\
to be the full name of your hostname.\n");
			exit(1);
		}
		oldv = verbose;
		verbose = V_QUIET;
		argv[0] = "lookup";
		(void) sprintf(line, "lookup %s", host);
		(void) makeargv();
		if (lookup(margc, margv) == 0 && lasthostname[0]) {
			(void) _Strncpy(host, lasthostname, size);
			domain[0] = '\0';
#ifdef HAS_DOMAINNAME
			/* getdomainname() returns just the domain name, without a
			 * preceding period.  For example, on "cse.unl.edu", it would
			 * return "unl.edu".
			 *
			 * SunOS note: getdomainname will return an empty string if
			 * this machine isn't on NIS.
			 */
			(void) getdomainname(domain, sizeof(domain) - 1);
#endif
#ifdef DOMAIN_NAME
			(void) Strncpy(domain, DOMAIN_NAME);
#endif
			if (index(host, '.') == NULL) {
				/* If the hostname has periods we'll assume that the
				 * it includes the domain name already.  Some gethostname()s
				 * return the whole host name, others just the machine name.
				 * If we have just the machine name and we successfully
				 * found out the domain name (from above), we'll append
				 * the domain to the machine to get a full hostname.
				 */
				if (domain[0]) {
					if (domain[0] != '.')
						(void) _Strncat(host, ".", size);
					(void) _Strncat(host, domain, size);
				} else {
					fprintf(stderr,
"WARNING: could not determine full host name (have: '%s').\n\
The program should be re-compiled with DOMAIN_NAME defined to be the\n\
domain name, i.e. -DDOMAIN_NAME=\\\"unl.edu\\\"\n\n",
						host);
				}
			}
		}
		verbose = oldv;
	}
	return r;
#endif
}	/* getlocalhostname */




/*
 * show status on remote machine
 */
int rmtstatus(int argc, char **argv)
{
	string str;

	if (argc > 1) {
		(void) sprintf(str, "STAT %s" , argv[1]);
		(void) verbose_command(str);
	} else (void) verbose_command("STAT");
	return NOERR;
}	/* rmtstatus */




/*
 * create an empty file on remote machine.
 */
int create(int argc, char **argv)
{
	string			str;
	FILE			*ftemp;

	if (argc < 2)
		argv = re_makeargv("(remote-file) ", &argc);
	if (argc < 2) {
		return USAGE;
	}
	(void) tmp_name(str);
	ftemp = fopen(str, "w");
	/* (void) fputc('x', ftemp); */
	(void) fclose(ftemp);
	creating = 1;
	(void) sendrequest("STOR", str, argv[1]);
	creating = 0;
	(void) unlink(str);
	return NOERR;
}	/* create */




/* show version info */
/*ARGSUSED*/
int show_version(int argc, char **argv)
{
	char	*DStrs[80];
	int		nDStrs = 0, i, j;

	(void) printf("%-30s %s\n", "NcFTP Version:", version);
	(void) printf("%-30s %s\n", "Author:",
		"Mike Gleason, NCEMRSoft (mgleason@cse.unl.edu).");

/* Now entering CPP hell... */
#ifdef __DATE__
	(void) printf("%-30s %s\n", "Compile Date:", __DATE__);
#endif
	(void) printf("%-30s %s (%s)\n", "Operating System:",
#ifdef System
	System,
#else
#	ifdef unix
	"UNIX",
#	else
	"??",
#	endif
#endif
#ifdef SYSV
		"SYSV");
#else
#	ifdef BSD
			"BSD");
#	else
			"neither BSD nor SYSV?");
#	endif
#endif

	/* Show which CPP symbols were used in compilation. */
#ifdef __GNUC__
	DStrs[nDStrs++] = "__GNUC__";
#endif
#ifdef RINDEX
	DStrs[nDStrs++] = "RINDEX";
#endif
#ifdef CURSES
	DStrs[nDStrs++] = "CURSES";
#endif
#ifdef NO_CURSES_H
	DStrs[nDStrs++] = "NO_CURSES_H";
#endif
#ifdef HERROR
	DStrs[nDStrs++] = "HERROR";
#endif
#ifdef U_WAIT
	DStrs[nDStrs++] = "U_WAIT";
#endif
#if defined(NO_CONST) || defined(const)
	DStrs[nDStrs++] = "NO_CONST";
#endif
#ifdef NO_FORMATTING
	DStrs[nDStrs++] = "NO_FORMATTING";
#endif
#ifdef DONT_TIMESTAMP
	DStrs[nDStrs++] = "DONT_TIMESTAMP";
#endif
#ifdef GETPASS
	DStrs[nDStrs++] = "GETPASS";
#endif
#ifdef HAS_GETCWD
	DStrs[nDStrs++] = "HAS_GETCWD";
#endif
#ifdef GETCWDSIZET
	DStrs[nDStrs++] = "GETCWDSIZET";
#endif
#ifdef HAS_DOMAINNAME
	DStrs[nDStrs++] = "HAS_DOMAINNAME";
#endif
#ifdef DOMAIN_NAME
	DStrs[nDStrs++] = "DOMAIN_NAME";
#endif
#ifdef Solaris
	DStrs[nDStrs++] = "Solaris";
#endif
#ifdef USE_GETPWUID
	DStrs[nDStrs++] = "USE_GETPWUID";
#endif
#ifdef HOSTNAME
	DStrs[nDStrs++] = "HOSTNAME";
#endif
#ifdef SYSDIRH
	DStrs[nDStrs++] = "SYSDIRH";
#endif
#ifdef SYSSELECTH
	DStrs[nDStrs++] = "SYSSELECTH";
#endif
#ifdef TERMH
	DStrs[nDStrs++] = "TERMH";
#endif
#ifdef NO_UNISTDH 
	DStrs[nDStrs++] = "NO_UNISTDH";
#endif
#ifdef NO_STDLIBH
	DStrs[nDStrs++] = "NO_STDLIBH";
#endif
#ifdef SYSLOG 
	DStrs[nDStrs++] = "SYSLOG";
#endif
#ifdef BAD_INETADDR
	DStrs[nDStrs++] = "BAD_INETADDR";
#endif
#ifdef SGTTYB
	DStrs[nDStrs++] = "SGTTYB";
#endif
#ifdef TERMIOS
	DStrs[nDStrs++] = "TERMIOS";
#endif
#ifdef STRICT_PROTOS
	DStrs[nDStrs++] = "STRICT_PROTOS";
#endif
#ifdef dFTP_PORT
	DStrs[nDStrs++] = "dFTP_PORT";
#endif
#ifdef BROKEN_MEMCPY
	DStrs[nDStrs++] = "BROKEN_MEMCPY";
#endif
#ifdef READLINE
	DStrs[nDStrs++] = "READLINE";
#endif
#ifdef GETLINE 
	DStrs[nDStrs++] = "GETLINE";
#endif
#ifdef _POSIX_SOURCE
	DStrs[nDStrs++] = "_POSIX_SOURCE";
#endif
#ifdef _XOPEN_SOURCE
	DStrs[nDStrs++] = "_XOPEN_SOURCE";
#endif
#ifdef NO_TIPS
	DStrs[nDStrs++] = "NO_TIPS";
#endif
#ifdef GZCAT
	DStrs[nDStrs++] = "GZCAT";
#endif
#ifdef LINGER
	DStrs[nDStrs++] = "LINGER";
#endif
#ifdef TRY_NOREPLY
	DStrs[nDStrs++] = "TRY_NOREPLY";
#endif
#ifdef NO_UTIMEH 
	DStrs[nDStrs++] = "NO_UTIMEH";
#endif
#ifdef DB_ERRS
	DStrs[nDStrs++] = "DB_ERRS";
#endif
#ifdef NO_VARARGS 
	DStrs[nDStrs++] = "NO_VARARGS";
#endif
#ifdef NO_STDARGH
	DStrs[nDStrs++] = "NO_STDARGH";
#endif
#ifdef NO_MKTIME
	DStrs[nDStrs++] = "NO_MKTIME";
#endif
#ifdef NO_STRSTR
	DStrs[nDStrs++] = "NO_STRSTR";
#endif
#ifdef NO_STRFTIME
	DStrs[nDStrs++] = "NO_STRFTIME";
#endif
#ifdef NO_RENAME
	DStrs[nDStrs++] = "NO_RENAME";
#endif
#ifdef TRY_ABOR
	DStrs[nDStrs++] = "TRY_ABOR";
#endif
#ifdef GATEWAY
	DStrs[nDStrs++] = "GATEWAY";
#endif
#ifdef SOCKS
	DStrs[nDStrs++] = "SOCKS";
#endif
#ifdef NET_ERRNO_H
	DStrs[nDStrs++] = "NET_ERRNO_H";
#endif


/* DONE with #ifdefs for now! */

	(void) printf ("\nCompile Options:\n");
	for (i=j=0; i<nDStrs; i++) {
		if (j == 0)
			(void) printf("    ");
		(void) printf("%-15s", DStrs[i]);
		if (++j == 4) {
			j = 0;
			(void) putchar('\n');
		}
	}
	if (j != 0)
		(void) putchar('\n');

#ifdef MK
	(void) printf("\nMK: %s\n", MK);
#endif /* MK */

	(void) printf("\nDefaults:\n");
	(void) printf("\
    Xfer Buf Size: %8d   Debug: %d   MPrompt: %d   Verbosity: %d\n\
    Prompt: %s   Pager: %s  ZCat: %s\n\
    Logname: %s   Logging: %d   Type: %s   Cmd Len: %d\n\
    Recv Line Len: %d   #Macros: %d   Macbuf: %d  Auto-Binary: %d\n\
    Recent File: %s   Recent On: %d   nRecents: %d\n\
    Redial Delay: %d  Anon Open: %d  New Mail Message: \"%s\"\n",
		MAX_XFER_BUFSIZE, dDEBUG, dMPROMPT, dVERBOSE,
		dPROMPT, dPAGER, ZCAT,
		dLOGNAME, dLOGGING, dTYPESTR, CMDLINELEN,
		RECEIVEDLINELEN, MAXMACROS, MACBUFLEN, dAUTOBINARY,
		dRECENTF, dRECENT_ON, dMAXRECENTS,
		dREDIALDELAY, dANONOPEN, NEWMAILMESSAGE
	);
#ifdef GATEWAY
	(void) printf("\
    Gateway Login: %s\n", dGATEWAY_LOGIN);
#endif
	return NOERR;
}	/* show_version */



void PurgeLineBuffer(void)
{
	register struct lslist *a, *b;
		 
	for (a = lshead; a != NULL; ) {
		b = a->next;
		if (a->string)
			free(a->string);    /* free string */
		Free(a);         /* free node */
		a = b;
	}
	lshead = lstail = NULL;
}	/* PurgeLineBuffer */




/*ARGSUSED*/
int ShowLineBuffer(int argc, char **argv)
{
	register struct lslist *a = lshead;
	int pagemode;
	FILE *fp;
	Sig_t oldintp;

	if (a == NULL)
		return CMDERR;
	pagemode= (**argv) == 'p' && pager[0] == '|';
	if (pagemode) {
		fp = popen(pager + 1, "w");
		if (!fp) {
			PERROR("ShowLineBuffer", pager + 1);
			return CMDERR;
		}
	} else
		fp = stdout;
	oldintp = Signal(SIGPIPE, SIG_IGN);
	while (a) {
		if (a->string)
			(void) fprintf(fp, "%s\n", a->string);
		a = a->next;
	}
	if (pagemode)
		(void) pclose(fp);
	if (oldintp)
		(void) Signal(SIGPIPE, oldintp);
	return NOERR;
}	/* ShowLineBuffer */




#if LIBMALLOC != LIBC_MALLOC
/*ARGSUSED*/
int MallocStatusCmd(int argc, char **argv)
{
#if (LIBMALLOC == FAST_MALLOC)
	struct mallinfo mi;

	mi = mallinfo();
	printf("\
total space in arena:               %d\n\
number of ordinary blocks:          %d\n\
number of small blocks:             %d\n\
number of holding blocks:           %d\n\
space in holding block headers:     %d\n\
space in small blocks in use:       %d\n\
space in free small blocks:         %d\n\
space in ordinary blocks in use:    %d\n\
space in free ordinary blocks:      %d\n\
cost of enabling keep option:       %d\n",
		mi.arena,
		mi.ordblks,
		mi.smblks,
		mi.hblks,
		mi.hblkhd,
		mi.usmblks,
		mi.fsmblks,
		mi.uordblks,
		mi.fordblks,
		mi.keepcost
	);
#else
#if (LIBMALLOC == DEBUG_MALLOC)
	printf("malloc_chain_check: %d\n\n", malloc_chain_check(0));
	if (argc > 1)
		malloc_dump(1);
	printf("malloc_inuse: %lu\n", malloc_inuse(NULL));
#else
	printf("Nothing to report.\n");
#endif	/* (LIBMALLOC == DEBUG_MALLOC) */
#endif	/* (LIBMALLOC == FAST_MALLOC) */

	return (0);
}	/* MallocStatusCmd */
#endif	/* LIBMALLOC */




/*ARGSUSED*/
int unimpl(int argc, char **argv)
{
	if (!parsing_rc)
		(void) printf("%s: command not supported. (and probably won't ever be).\n", argv[0]);
	return (NOERR);
}	/* unimpl */

int setpassive(int argc, char **argv)
{
	passivemode = !passivemode;
	printf("Passive mode %s.\n", (passivemode ? "ON" : "OFF"));
	return NOERR;
}

int setrestrict(int argc, char **argv)
{
	restricted_data_ports = !restricted_data_ports;
	printf("Data port range restrictions %s.\n",
	       (restricted_data_ports ? "ON" : "OFF"));
	return NOERR;
}

/* eof cmds.c */

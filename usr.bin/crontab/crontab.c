#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/usr.bin/crontab/crontab.c,v 1.1.1.1 1993/06/12 14:53:53 rgrimes Exp $";
#endif

/* Revision 1.5  87/05/02  17:33:22  paul
 * pokecron?  (RCS file has the rest of the log)
 * 
 * Revision 1.5  87/05/02  17:33:22  paul
 * baseline for mod.sources release
 * 
 * Revision 1.4  87/03/31  13:11:48  paul
 * I won't say that rs@mirror gave me this idea but crontab uses getopt() now
 * 
 * Revision 1.3  87/03/30  23:43:48  paul
 * another suggestion from rs@mirror:
 *   use getpwuid(getuid)->pw_name instead of getenv("USER")
 *   this is a boost to security...
 * 
 * Revision 1.2  87/02/11  17:40:12  paul
 * changed command syntax to allow append and replace instead of append as
 * default and no replace at all.
 * 
 * Revision 1.1  87/01/26  23:49:06  paul
 * Initial revision
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00131
 * --------------------         -----   ----------------------
 *
 * 06 Apr 93	Adam Glass	Fixes so it compiles quitely
 *
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#define	MAIN_PROGRAM


#include "cron.h"
#include <pwd.h>
#include <errno.h>
#include <sys/file.h>
#if defined(BSD)
# include <sys/time.h>
#endif  /*BSD*/

/* extern	char	*sprintf(); */


static int	Pid;
static char	User[MAX_UNAME], RealUser[MAX_UNAME];
static char	Filename[MAX_FNAME];
static FILE	*NewCrontab;
static int	CheckErrorCount;
static enum	{opt_unknown, opt_list, opt_delete, opt_replace}
		Option;

extern void	log_it();

#if DEBUGGING
static char	*Options[] = {"???", "list", "delete", "replace"};
#endif

void
usage()
{
	fprintf(stderr, "usage:  %s [-u user] ...\n", ProgramName);
	fprintf(stderr, " ... -l         (list user's crontab)\n");
	fprintf(stderr, " ... -d         (delete user's crontab)\n");
	fprintf(stderr, " ... -r file    (replace user's crontab)\n");
	exit(ERROR_EXIT);
}


main(argc, argv)
	int	argc;
	char	*argv[];
{
        void parse_args(), set_cron_uid(), set_cron_cwd(),
             list_cmd(), delete_cmd(), replace_cmd();

	Pid = getpid();
	ProgramName = argv[0];
#if defined(BSD)
	setlinebuf(stderr);
#endif
	parse_args(argc, argv);		/* sets many globals, opens a file */
	set_cron_uid();
	set_cron_cwd();
	if (!allowed(User)) {
		fprintf(stderr,
			"You (%s) are not allowed to use this program (%s)\n",
			User, ProgramName);
		fprintf(stderr, "See crontab(1) for more information\n");
		log_it(RealUser, Pid, "AUTH", "crontab command not allowed");
		exit(ERROR_EXIT);
	}
	switch (Option)
	{
	case opt_list:		list_cmd();
				break;
	case opt_delete:	delete_cmd();
				break;
	case opt_replace:	replace_cmd();
				break;
	}
}
	

 void
parse_args(argc, argv)
	int	argc;
	char	*argv[];
{
	void		usage();
	char		*getenv(), *strcpy();
	int		getuid();
	struct passwd	*getpwnam();
	extern int	getopt(), optind;
	extern char	*optarg;

	struct passwd	*pw;
	int		argch;

	if (!(pw = getpwuid(getuid())))
	{
		fprintf(stderr, "%s: your UID isn't in the passwd file.\n",
			ProgramName);
		fprintf(stderr, "bailing out.\n");
		exit(ERROR_EXIT);
	}
	strcpy(User, pw->pw_name);
	strcpy(RealUser, User);
	Filename[0] = '\0';
	Option = opt_unknown;
	while (EOF != (argch = getopt(argc, argv, "u:ldr:x:")))
	{
		switch (argch)
		{
		case 'x':
			if (!set_debug_flags(optarg))
				usage();
			break;
		case 'u':
			if (getuid() != ROOT_UID)
			{
				fprintf(stderr,
					"must be privileged to use -u\n");
				exit(ERROR_EXIT);
			}
			if ((struct passwd *)NULL == getpwnam(optarg))
			{
				fprintf(stderr, "%s:  user `%s' unknown\n",
					ProgramName, optarg);
				exit(ERROR_EXIT);
			}
			(void) strcpy(User, optarg);
			break;
		case 'l':
			if (Option != opt_unknown)
				usage();
			Option = opt_list;
			break;
		case 'd':
			if (Option != opt_unknown)
				usage();
			Option = opt_delete;
			break;
		case 'r':
			if (Option != opt_unknown)
				usage();
			Option = opt_replace;
			(void) strcpy(Filename, optarg);
			break;
		default:
			usage();
		}
	}

	endpwent();

	if (Option == opt_unknown || argv[optind] != NULL)
		usage();

	if (Option == opt_replace) {
		if (!Filename[0]) {
			/* getopt(3) says this can't be true
			 * but I'm paranoid today.
			 */
			fprintf(stderr, "filename must be given for -a or -r\n");
			usage();
		}
		/* we have to open the file here because we're going to
		 * chdir(2) into /var/cron before we get around to
		 * reading the file.
		 */
		if (!strcmp(Filename, "-")) {
			NewCrontab = stdin;
		} else {
			if (!(NewCrontab = fopen(Filename, "r"))) {
				perror(Filename);
				exit(ERROR_EXIT);
			}
		}
	}

	Debug(DMISC, ("user=%s, file=%s, option=%s\n",
					User, Filename, Options[(int)Option]))
}


 void
list_cmd()
{
	extern	errno;
	char	n[MAX_FNAME];
	FILE	*f;
	int	ch;

	log_it(RealUser, Pid, "LIST", User);
	(void) sprintf(n, CRON_TAB(User));
	if (!(f = fopen(n, "r")))
	{
		if (errno == ENOENT)
			fprintf(stderr, "no crontab for %s\n", User);
		else
			perror(n);
		exit(ERROR_EXIT);
	}

	/* file is open. copy to stdout, close.
	 */
	Set_LineNum(1)
	while (EOF != (ch = get_char(f)))
		putchar(ch);
	fclose(f);
}


 void
delete_cmd()
{
	extern	errno;
	int	unlink();
	void	poke_daemon();
	char	n[MAX_FNAME];

	log_it(RealUser, Pid, "DELETE", User);
	(void) sprintf(n, CRON_TAB(User));
	if (unlink(n))
	{
		if (errno == ENOENT)
			fprintf(stderr, "no crontab for %s\n", User);
		else
			perror(n);
		exit(ERROR_EXIT);
	}
	poke_daemon();
}


 void
check_error(msg)
	char	*msg;
{
	CheckErrorCount += 1;
	fprintf(stderr, "\"%s\", line %d: %s\n", Filename, LineNumber, msg);
}


 void
replace_cmd()
{
	entry	*load_entry();
	int	load_env();
	int	unlink();
	void	free_entry();
	void	check_error();
	void	poke_daemon();
	extern	errno;

	char	n[MAX_FNAME], envstr[MAX_ENVSTR], tn[MAX_FNAME];
	FILE	*tmp;
	int	ch;
	entry	*e;
	int	status;
	time_t	now = time(NULL);

	(void) sprintf(n, "tmp.%d", Pid);
	(void) sprintf(tn, CRON_TAB(n));
	if (!(tmp = fopen(tn, "w"))) {
		perror(tn);
		exit(ERROR_EXIT);
	}

	/* write a signature at the top of the file.  for brian.
	 */
	fprintf(tmp, "# (%s installed on %-24.24s)\n", Filename, ctime(&now));
	fprintf(tmp, "# (Cron version -- %s)\n", rcsid);

	/* copy the crontab to the tmp
	 */
	Set_LineNum(1)
	while (EOF != (ch = get_char(NewCrontab)))
		putc(ch, tmp);
	fclose(NewCrontab);
	fflush(tmp);  rewind(tmp);

	if (ferror(tmp)) {
		fprintf(stderr, "%s: error while writing new crontab to %s\n",
			ProgramName, tn);
		fclose(tmp);  unlink(tn);
		exit(ERROR_EXIT);
	}

	/* check the syntax of the file being installed.
	 */

	/* BUG: was reporting errors after the EOF if there were any errors
	 * in the file proper -- kludged it by stopping after first error.
	 *		vix 31mar87
	 */
	CheckErrorCount = 0;
	while (!CheckErrorCount && (status = load_env(envstr, tmp)) >= OK)
	{
		if (status == FALSE)
		{
			if (NULL != (e = load_entry(NewCrontab, check_error)))
				free((char *) e);
		}
	}

	if (CheckErrorCount != 0)
	{
		fprintf(stderr, "errors in crontab file, can't install.\n");
		fclose(tmp);  unlink(tn);
		exit(ERROR_EXIT);
	}

	if (fchown(fileno(tmp), ROOT_UID, -1) < OK)
	{
		perror("chown");
		fclose(tmp);  unlink(tn);
		exit(ERROR_EXIT);
	}

	if (fchmod(fileno(tmp), 0600) < OK)
	{
		perror("chown");
		fclose(tmp);  unlink(tn);
		exit(ERROR_EXIT);
	}

	if (fclose(tmp) == EOF) {
		perror("fclose");
		unlink(tn);
		exit(ERROR_EXIT);
	}

	(void) sprintf(n, CRON_TAB(User));
	if (rename(tn, n))
	{
		fprintf(stderr, "%s: error renaming %s to %s\n",
			ProgramName, tn, n);
		perror("rename");
		unlink(tn);
		exit(ERROR_EXIT);
	}
	log_it(RealUser, Pid, "REPLACE", User);

	poke_daemon();
}


 void
poke_daemon()
{
#if defined(BSD)
	struct timeval tvs[2];
	struct timezone tz;

	(void) gettimeofday(&tvs[0], &tz);
	tvs[1] = tvs[0];
	if (utimes(SPOOL_DIR, tvs) < OK)
	{
		fprintf(stderr, "crontab: can't update mtime on spooldir\n");
		perror(SPOOL_DIR);
		return;
	}
#endif  /*BSD*/
#if defined(ATT)
	if (utime(SPOOL_DIR, NULL) < OK)
	{
		fprintf(stderr, "crontab: can't update mtime on spooldir\n");
		perror(SPOOL_DIR);
		return;
	}
#endif  /*ATT*/
}

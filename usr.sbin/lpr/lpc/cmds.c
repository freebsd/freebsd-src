/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*
static char sccsid[] = "@(#)cmds.c	8.2 (Berkeley) 4/28/95";
*/
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * lpc -- line printer control program -- commands:
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "lp.h"
#include "lp.local.h"
#include "lpc.h"
#include "extern.h"
#include "pathnames.h"

static void	 abortpr(struct printer *_pp, int _dis);
static int	 doarg(char *_job);
static int	 doselect(struct dirent *_d);
static void	 putmsg(struct printer *_pp, int _argc, char **_argv);
static int	 sortq(const void *_a, const void *_b);
static void	 startpr(struct printer *_pp, int _chgenable);
static int	 touch(struct jobqueue *_jq);
static void	 unlinkf(char *_name);
static void	 upstat(struct printer *_pp, const char *_msg);
static void	 wrapup_clean(int _laststatus);

/*
 * generic framework for commands which operate on all or a specified
 * set of printers
 */
enum	qsel_val {			/* how a given ptr was selected */
	QSEL_UNKNOWN = -1,		/* ... not selected yet */
	QSEL_BYNAME = 0,		/* ... user specifed it by name */
	QSEL_ALL = 1			/* ... user wants "all" printers */
					/*     (with more to come)    */
};

static enum qsel_val generic_qselect;	/* indicates how ptr was selected */
static int generic_initerr;		/* result of initrtn processing */
static char *generic_nullarg;
static void (*generic_wrapup)(int _last_status);   /* perform rtn wrap-up */

void
generic(void (*specificrtn)(struct printer *_pp),
    void (*initrtn)(int _argc, char *_argv[]), int argc, char *argv[])
{
	int cmdstatus, more, targc;
	struct printer myprinter, *pp;
	char **targv;

	if (argc == 1) {
		printf("Usage: %s {all | printer ...}\n", argv[0]);
		return;
	}

	/*
	 * The initialization routine for a command might set a generic
	 * "wrapup" routine, which should be called after processing all
	 * the printers in the command.  This might print summary info.
	 *
	 * Note that the initialization routine may also parse (and
	 * nullify) some of the parameters given on the command, leaving
	 * only the parameters which have to do with printer names.
	 */
	pp = &myprinter;
	generic_wrapup = NULL;
	generic_qselect = QSEL_UNKNOWN;
	cmdstatus = 0;
	/* this just needs to be a distinct value of type 'char *' */
	if (generic_nullarg == NULL)
		generic_nullarg = strdup("");

	/* call initialization routine, if there is one for this cmd */
	if (initrtn != NULL) {
		generic_initerr = 0;
		(*initrtn)(argc, argv);
		if (generic_initerr)
			return;
		/* skip any initial arguments null-ified by initrtn */
		targc = argc;
		targv = argv;
		while (--targc) {
			if (targv[1] != generic_nullarg)
				break;
			++targv;
		}
		if (targv != argv) {
			targv[0] = argv[0];	/* copy the command-name */
			argv = targv;
			argc = targc + 1;
		}
	}

	if (argc == 2 && strcmp(argv[1], "all") == 0) {
		generic_qselect = QSEL_ALL;
		more = firstprinter(pp, &cmdstatus);
		if (cmdstatus)
			goto looperr;
		while (more) {
			(*specificrtn)(pp);
			do {
				more = nextprinter(pp, &cmdstatus);
looperr:
				switch (cmdstatus) {
				case PCAPERR_TCOPEN:
					printf("warning: %s: unresolved "
					       "tc= reference(s) ",
					       pp->printer);
				case PCAPERR_SUCCESS:
					break;
				default:
					fatal(pp, "%s", pcaperr(cmdstatus));
				}
			} while (more && cmdstatus);
		}
		goto wrapup;
	}

	generic_qselect = QSEL_BYNAME;		/* specifically-named ptrs */
	while (--argc) {
		++argv;
		if (*argv == generic_nullarg)
			continue;
		init_printer(pp);
		cmdstatus = getprintcap(*argv, pp);
		switch (cmdstatus) {
		default:
			fatal(pp, "%s", pcaperr(cmdstatus));
		case PCAPERR_NOTFOUND:
			printf("unknown printer %s\n", *argv);
			continue;
		case PCAPERR_TCOPEN:
			printf("warning: %s: unresolved tc= reference(s)\n",
			       *argv);
			break;
		case PCAPERR_SUCCESS:
			break;
		}
		(*specificrtn)(pp);
	}

wrapup:
	if (generic_wrapup) {
		(*generic_wrapup)(cmdstatus);
	}

}

/*
 * kill an existing daemon and disable printing.
 */
void
doabort(struct printer *pp)
{
	abortpr(pp, 1);
}

static void
abortpr(struct printer *pp, int dis)
{
	register FILE *fp;
	struct stat stbuf;
	int pid, fd;
	char lf[MAXPATHLEN];

	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);

	/*
	 * Turn on the owner execute bit of the lock file to disable printing.
	 */
	if (dis) {
		seteuid(euid);
		if (stat(lf, &stbuf) >= 0) {
			if (chmod(lf, stbuf.st_mode | LFM_PRINT_DIS) < 0)
				printf("\tcannot disable printing: %s\n",
				       strerror(errno));
			else {
				upstat(pp, "printing disabled\n");
				printf("\tprinting disabled\n");
			}
		} else if (errno == ENOENT) {
			if ((fd = open(lf, O_WRONLY|O_CREAT, 
				       LOCK_FILE_MODE | LFM_PRINT_DIS)) < 0)
				printf("\tcannot create lock file: %s\n",
				       strerror(errno));
			else {
				(void) close(fd);
				upstat(pp, "printing disabled\n");
				printf("\tprinting disabled\n");
				printf("\tno daemon to abort\n");
			}
			goto out;
		} else {
			printf("\tcannot stat lock file\n");
			goto out;
		}
	}
	/*
	 * Kill the current daemon to stop printing now.
	 */
	if ((fp = fopen(lf, "r")) == NULL) {
		printf("\tcannot open lock file\n");
		goto out;
	}
	if (!getline(fp) || flock(fileno(fp), LOCK_SH|LOCK_NB) == 0) {
		(void) fclose(fp);	/* unlocks as well */
		printf("\tno daemon to abort\n");
		goto out;
	}
	(void) fclose(fp);
	if (kill(pid = atoi(line), SIGTERM) < 0) {
		if (errno == ESRCH)
			printf("\tno daemon to abort\n");
		else
			printf("\tWarning: daemon (pid %d) not killed\n", pid);
	} else
		printf("\tdaemon (pid %d) killed\n", pid);
out:
	seteuid(uid);
}

/*
 * Write a message into the status file.
 */
static void
upstat(struct printer *pp, const char *msg)
{
	register int fd;
	char statfile[MAXPATHLEN];

	status_file_name(pp, statfile, sizeof statfile);
	umask(0);
	fd = open(statfile, O_WRONLY|O_CREAT|O_EXLOCK, STAT_FILE_MODE);
	if (fd < 0) {
		printf("\tcannot create status file: %s\n", strerror(errno));
		return;
	}
	(void) ftruncate(fd, 0);
	if (msg == (char *)NULL)
		(void) write(fd, "\n", 1);
	else
		(void) write(fd, msg, strlen(msg));
	(void) close(fd);
}

/*
 * "global" variables for all the routines related to 'clean' and 'tclean'
 */
static time_t	 cln_now;		/* current time */
static double	 cln_minage;		/* minimum age before file is removed */
static long	 cln_sizecnt;		/* amount of space freed up */
static int 	 cln_debug;		/* print extra debugging msgs */
static int	 cln_filecnt;		/* number of files destroyed */
static int	 cln_foundcore;		/* found a core file! */
static int	 cln_queuecnt;		/* number of queues checked */
static int 	 cln_testonly;		/* remove-files vs just-print-info */

static int
doselect(struct dirent *d)
{
	int c = d->d_name[0];

	if ((c == 't' || c == 'c' || c == 'd') && d->d_name[1] == 'f')
		return 1;
	if (c == 'c') {
		if (!strcmp(d->d_name, "core"))
			cln_foundcore = 1;
	}
	if (c == 'e') {
		if (!strncmp(d->d_name, "errs.", 5))
			return 1;
	}
	return 0;
}

/*
 * Comparison routine for scandir. Sort by job number and machine, then
 * by `cf', `tf', or `df', then by the sequence letter A-Z, a-z.
 */
static int
sortq(const void *a, const void *b)
{
	const struct dirent **d1, **d2;
	int c1, c2;

	d1 = (const struct dirent **)a;
	d2 = (const struct dirent **)b;
	if ((c1 = strcmp((*d1)->d_name + 3, (*d2)->d_name + 3)))
		return(c1);
	c1 = (*d1)->d_name[0];
	c2 = (*d2)->d_name[0];
	if (c1 == c2)
		return((*d1)->d_name[2] - (*d2)->d_name[2]);
	if (c1 == 'c')
		return(-1);
	if (c1 == 'd' || c2 == 'c')
		return(1);
	return(-1);
}

/*
 * Remove all spool files and temporaries from the spooling area.
 * Or, perhaps:
 * Remove incomplete jobs from spooling area.
 */

void
init_clean(int argc, char *argv[])
{

	/* init some fields before 'clean' is called for each queue */
	cln_queuecnt = 0;
	cln_now = time(NULL);
	cln_minage = 3600.0;		/* only delete files >1h old */
	cln_filecnt = 0;
	cln_sizecnt = 0;
	cln_debug = 0;
	cln_testonly = 0;
	generic_wrapup = &wrapup_clean;

	/* see if there are any options specified before the ptr list */
	while (--argc) {
		++argv;
		if (**argv != '-')
			break;
		if (strcmp(*argv, "-d") == 0) {
			/* just an example of an option... */
			cln_debug = 1;
			*argv = generic_nullarg;	/* "erase" it */
		} else {
			printf("Invalid option '%s'\n", *argv);
			generic_initerr = 1;
		}
	}

	return;
}

void
init_tclean(int argc, char *argv[])
{

	/* only difference between 'clean' and 'tclean' is one value */
	/* (...and the fact that 'clean' is priv and 'tclean' is not) */
	init_clean(argc, argv);
	cln_testonly = 1;

	return;
}

void
clean_q(struct printer *pp)
{
	char *cp, *cp1, *lp;
	struct dirent **queue;
	size_t linerem;
	int didhead, i, n, nitems, rmcp;

	cln_queuecnt++;

	didhead = 0;
	if (generic_qselect == QSEL_BYNAME) {
		printf("%s:\n", pp->printer);
		didhead = 1;
	}

	lp = line;
	cp = pp->spool_dir;
	while (lp < &line[sizeof(line) - 1]) {
		if ((*lp++ = *cp++) == 0)
			break;
	}
	lp[-1] = '/';
	linerem = sizeof(line) - (lp - line);

	cln_foundcore = 0;
	seteuid(euid);
	nitems = scandir(pp->spool_dir, &queue, doselect, sortq);
	seteuid(uid);
	if (nitems < 0) {
		if (!didhead) {
			printf("%s:\n", pp->printer);
			didhead = 1;
		}
		printf("\tcannot examine spool directory\n");
		return;
	}
	if (cln_foundcore) {
		if (!didhead) {
			printf("%s:\n", pp->printer);
			didhead = 1;
		}
		printf("\t** found a core file in %s !\n", pp->spool_dir);
	}
	if (nitems == 0)
		return;
	if (!didhead)
		printf("%s:\n", pp->printer);
	i = 0;
	do {
		cp = queue[i]->d_name;
		rmcp = 0;
		if (*cp == 'c') {
			/*
			 * A control file.  Look for matching data-files.
			 */
			/* XXX
			 *  Note the logic here assumes that the hostname
			 *  part of cf-filenames match the hostname part
			 *  in df-filenames, and that is not necessarily
			 *  true (eg: for multi-homed hosts).  This needs
			 *  some further thought...
			 */
			n = 0;
			while (i + 1 < nitems) {
				cp1 = queue[i + 1]->d_name;
				if (*cp1 != 'd' || strcmp(cp + 3, cp1 + 3))
					break;
				i++;
				n++;
			}
			if (n == 0) {
				rmcp = 1;
			}
		} else if (*cp == 'e') {
			/*
			 * Must be an errrs or email temp file.
			 */
			rmcp = 1;
		} else {
			/*
			 * Must be a df with no cf (otherwise, it would have
			 * been skipped above) or a tf file (which can always
			 * be removed if it's old enough).
			 */
			rmcp = 1;
		}
		if (rmcp) {
			if (strlen(cp) >= linerem) {
				printf("\t** internal error: 'line' overflow!\n");
				printf("\t**   spooldir = %s\n", pp->spool_dir);
				printf("\t**   cp = %s\n", cp);
				return;
			}
			strlcpy(lp, cp, linerem);
			unlinkf(line);
		}
     	} while (++i < nitems);
}

static void
wrapup_clean(int laststatus __unused)
{

	printf("Checked %d queues, and ", cln_queuecnt);
	if (cln_filecnt < 1) {
		printf("no cruft was found\n");
		return;
	}
	if (cln_testonly) {
		printf("would have ");
	}
	printf("removed %d files (%ld bytes).\n", cln_filecnt, cln_sizecnt);	
}
 
static void
unlinkf(char *name)
{
	struct stat stbuf;
	double agemod, agestat;
	int res;
	char linkbuf[BUFSIZ];

	/*
	 * We have to use lstat() instead of stat(), in case this is a df*
	 * "file" which is really a symlink due to 'lpr -s' processing.  In
	 * that case, we need to check the last-mod time of the symlink, and
	 * not the file that the symlink is pointed at.
	 */
	seteuid(euid);
	res = lstat(name, &stbuf);
	seteuid(uid);
	if (res < 0) {
		printf("\terror return from stat(%s):\n", name);
		printf("\t      %s\n", strerror(errno));
		return;
	}

	agemod = difftime(cln_now, stbuf.st_mtime);
	agestat = difftime(cln_now,  stbuf.st_ctime);
	if (cln_debug) {
		/* this debugging-aid probably is not needed any more... */
		printf("\t\t  modify age=%g secs, stat age=%g secs\n",
		    agemod, agestat);
	}
	if ((agemod <= cln_minage) && (agestat <= cln_minage))
		return;

	/*
	 * if this file is a symlink, then find out the target of the
	 * symlink before unlink-ing the file itself
	 */
	if (S_ISLNK(stbuf.st_mode)) {
		seteuid(euid);
		res = readlink(name, linkbuf, sizeof(linkbuf));
		seteuid(uid);
		if (res < 0) {
			printf("\terror return from readlink(%s):\n", name);
			printf("\t      %s\n", strerror(errno));
			return;
		}
		if (res == sizeof(linkbuf))
			res--;
		linkbuf[res] = '\0';
	}

	cln_filecnt++;
	cln_sizecnt += stbuf.st_size;

	if (cln_testonly) {
		printf("\twould remove %s\n", name);
		if (S_ISLNK(stbuf.st_mode)) {
			printf("\t    (which is a symlink to %s)\n", linkbuf);
		}
	} else {
		seteuid(euid);
		res = unlink(name);
		seteuid(uid);
		if (res < 0)
			printf("\tcannot remove %s (!)\n", name);
		else
			printf("\tremoved %s\n", name);
		/* XXX
		 *  Note that for a df* file, this code should also check to see
		 *  if it is a symlink to some other file, and if the original
		 *  lpr command included '-r' ("remove file").  Of course, this
		 *  code would not be removing the df* file unless there was no
		 *  matching cf* file, and without the cf* file it is currently
		 *  impossible to determine if '-r' had been specified...
		 *
		 *  As a result of this quandry, we may be leaving behind a
		 *  user's file that was supposed to have been removed after
		 *  being printed.  This may effect services such as CAP or
		 *  samba, if they were configured to use 'lpr -r', and if
		 *  datafiles are not being properly removed.
		*/
		if (S_ISLNK(stbuf.st_mode)) {
			printf("\t    (which was a symlink to %s)\n", linkbuf);
		}
	}
}

/*
 * Enable queuing to the printer (allow lpr's).
 */
void
enable(struct printer *pp)
{
	struct stat stbuf;
	char lf[MAXPATHLEN];

	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);

	/*
	 * Turn off the group execute bit of the lock file to enable queuing.
	 */
	seteuid(euid);
	if (stat(lf, &stbuf) >= 0) {
		if (chmod(lf, stbuf.st_mode & ~LFM_QUEUE_DIS) < 0)
			printf("\tcannot enable queuing\n");
		else
			printf("\tqueuing enabled\n");
	}
	seteuid(uid);
}

/*
 * Disable queuing.
 */
void
disable(struct printer *pp)
{
	register int fd;
	struct stat stbuf;
	char lf[MAXPATHLEN];
	
	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);
	/*
	 * Turn on the group execute bit of the lock file to disable queuing.
	 */
	seteuid(euid);
	if (stat(lf, &stbuf) >= 0) {
		if (chmod(lf, stbuf.st_mode | LFM_QUEUE_DIS) < 0)
			printf("\tcannot disable queuing: %s\n", 
			       strerror(errno));
		else
			printf("\tqueuing disabled\n");
	} else if (errno == ENOENT) {
		if ((fd = open(lf, O_WRONLY|O_CREAT, 
			       LOCK_FILE_MODE | LFM_QUEUE_DIS)) < 0)
			printf("\tcannot create lock file: %s\n", 
			       strerror(errno));
		else {
			(void) close(fd);
			printf("\tqueuing disabled\n");
		}
	} else
		printf("\tcannot stat lock file\n");
	seteuid(uid);
}

/*
 * Disable queuing and printing and put a message into the status file
 * (reason for being down).
 */
void
down(int argc, char *argv[])
{
        int cmdstatus, more;
	struct printer myprinter, *pp = &myprinter;

	if (argc == 1) {
		printf("Usage: down {all | printer} [message ...]\n");
		return;
	}
	if (!strcmp(argv[1], "all")) {
		more = firstprinter(pp, &cmdstatus);
		if (cmdstatus)
			goto looperr;
		while (more) {
			putmsg(pp, argc - 2, argv + 2);
			do {
				more = nextprinter(pp, &cmdstatus);
looperr:
				switch (cmdstatus) {
				case PCAPERR_TCOPEN:
					printf("warning: %s: unresolved "
					       "tc= reference(s) ",
					       pp->printer);
				case PCAPERR_SUCCESS:
					break;
				default:
					fatal(pp, "%s", pcaperr(cmdstatus));
				}
			} while (more && cmdstatus);
		}
		return;
	}
	init_printer(pp);
	cmdstatus = getprintcap(argv[1], pp);
	switch (cmdstatus) {
	default:
		fatal(pp, "%s", pcaperr(cmdstatus));
	case PCAPERR_NOTFOUND:
		printf("unknown printer %s\n", argv[1]);
		return;
	case PCAPERR_TCOPEN:
		printf("warning: %s: unresolved tc= reference(s)", argv[1]);
		break;
	case PCAPERR_SUCCESS:
		break;
	}
	putmsg(pp, argc - 2, argv + 2);
}

static void
putmsg(struct printer *pp, int argc, char **argv)
{
	register int fd;
	register char *cp1, *cp2;
	char buf[1024];
	char file[MAXPATHLEN];
	struct stat stbuf;

	printf("%s:\n", pp->printer);
	/*
	 * Turn on the group execute bit of the lock file to disable queuing;
	 * turn on the owner execute bit of the lock file to disable printing.
	 */
	lock_file_name(pp, file, sizeof file);
	seteuid(euid);
	if (stat(file, &stbuf) >= 0) {
		if (chmod(file, stbuf.st_mode|LFM_PRINT_DIS|LFM_QUEUE_DIS) < 0)
			printf("\tcannot disable queuing: %s\n", 
			       strerror(errno));
		else
			printf("\tprinter and queuing disabled\n");
	} else if (errno == ENOENT) {
		if ((fd = open(file, O_WRONLY|O_CREAT, 
			       LOCK_FILE_MODE|LFM_PRINT_DIS|LFM_QUEUE_DIS)) < 0)
			printf("\tcannot create lock file: %s\n", 
			       strerror(errno));
		else {
			(void) close(fd);
			printf("\tprinter and queuing disabled\n");
		}
		seteuid(uid);
		return;
	} else
		printf("\tcannot stat lock file\n");
	/*
	 * Write the message into the status file.
	 */
	status_file_name(pp, file, sizeof file);
	fd = open(file, O_WRONLY|O_CREAT|O_EXLOCK, STAT_FILE_MODE);
	if (fd < 0) {
		printf("\tcannot create status file: %s\n", strerror(errno));
		seteuid(uid);
		return;
	}
	seteuid(uid);
	(void) ftruncate(fd, 0);
	if (argc <= 0) {
		(void) write(fd, "\n", 1);
		(void) close(fd);
		return;
	}
	cp1 = buf;
	while (--argc >= 0) {
		cp2 = *argv++;
		while ((size_t)(cp1 - buf) < sizeof(buf) && (*cp1++ = *cp2++))
			;
		cp1[-1] = ' ';
	}
	cp1[-1] = '\n';
	*cp1 = '\0';
	(void) write(fd, buf, strlen(buf));
	(void) close(fd);
}

/*
 * Exit lpc
 */
void
quit(int argc __unused, char *argv[] __unused)
{
	exit(0);
}

/*
 * Kill and restart the daemon.
 */
void
restart(struct printer *pp)
{
	abortpr(pp, 0);
	startpr(pp, 0);
}

/*
 * Enable printing on the specified printer and startup the daemon.
 */
void
startcmd(struct printer *pp)
{
	startpr(pp, 1);
}

static void
startpr(struct printer *pp, int chgenable)
{
	struct stat stbuf;
	char lf[MAXPATHLEN];

	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);

	/*
	 * For chgenable==1 ('start'), turn off the LFM_PRINT_DIS bit of the
	 * lock file to re-enable printing.  For chgenable==2 ('up'), also
	 * turn off the LFM_QUEUE_DIS bit to re-enable queueing.
	 */
	seteuid(euid);
	if (chgenable && stat(lf, &stbuf) >= 0) {
		mode_t bits = (chgenable == 2 ? 0 : LFM_QUEUE_DIS);
		if (chmod(lf, stbuf.st_mode & (LOCK_FILE_MODE | bits)) < 0)
			printf("\tcannot enable printing\n");
		else
			printf("\tprinting enabled\n");
	}
	if (!startdaemon(pp))
		printf("\tcouldn't start daemon\n");
	else
		printf("\tdaemon started\n");
	seteuid(uid);
}

/*
 * Print the status of the printer queue.
 */
void
status(struct printer *pp)
{
	struct stat stbuf;
	register int fd, i;
	register struct dirent *dp;
	DIR *dirp;
	char file[MAXPATHLEN];

	printf("%s:\n", pp->printer);
	lock_file_name(pp, file, sizeof file);
	if (stat(file, &stbuf) >= 0) {
		printf("\tqueuing is %s\n",
		       ((stbuf.st_mode & LFM_QUEUE_DIS) ? "disabled"
			: "enabled"));
		printf("\tprinting is %s\n",
		       ((stbuf.st_mode & LFM_PRINT_DIS) ? "disabled"
			: "enabled"));
	} else {
		printf("\tqueuing is enabled\n");
		printf("\tprinting is enabled\n");
	}
	if ((dirp = opendir(pp->spool_dir)) == NULL) {
		printf("\tcannot examine spool directory\n");
		return;
	}
	i = 0;
	while ((dp = readdir(dirp)) != NULL) {
		if (*dp->d_name == 'c' && dp->d_name[1] == 'f')
			i++;
	}
	closedir(dirp);
	if (i == 0)
		printf("\tno entries in spool area\n");
	else if (i == 1)
		printf("\t1 entry in spool area\n");
	else
		printf("\t%d entries in spool area\n", i);
	fd = open(file, O_RDONLY);
	if (fd < 0 || flock(fd, LOCK_SH|LOCK_NB) == 0) {
		(void) close(fd);	/* unlocks as well */
		printf("\tprinter idle\n");
		return;
	}
	(void) close(fd);
	/* print out the contents of the status file, if it exists */
	status_file_name(pp, file, sizeof file);
	fd = open(file, O_RDONLY|O_SHLOCK);
	if (fd >= 0) {
		(void) fstat(fd, &stbuf);
		if (stbuf.st_size > 0) {
			putchar('\t');
			while ((i = read(fd, line, sizeof(line))) > 0)
				(void) fwrite(line, 1, i, stdout);
		}
		(void) close(fd);	/* unlocks as well */
	}
}

/*
 * Stop the specified daemon after completing the current job and disable
 * printing.
 */
void
stop(struct printer *pp)
{
	register int fd;
	struct stat stbuf;
	char lf[MAXPATHLEN];

	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);

	/*
	 * Turn on the owner execute bit of the lock file to disable printing.
	 */
	seteuid(euid);
	if (stat(lf, &stbuf) >= 0) {
		if (chmod(lf, stbuf.st_mode | LFM_PRINT_DIS) < 0)
			printf("\tcannot disable printing: %s\n",
			       strerror(errno));
		else {
			upstat(pp, "printing disabled\n");
			printf("\tprinting disabled\n");
		}
	} else if (errno == ENOENT) {
		if ((fd = open(lf, O_WRONLY|O_CREAT, 
			       LOCK_FILE_MODE | LFM_PRINT_DIS)) < 0)
			printf("\tcannot create lock file: %s\n",
			       strerror(errno));
		else {
			(void) close(fd);
			upstat(pp, "printing disabled\n");
			printf("\tprinting disabled\n");
		}
	} else
		printf("\tcannot stat lock file\n");
	seteuid(uid);
}

struct	jobqueue **queue;
int	nitems;
time_t	mtime;

/*
 * Put the specified jobs at the top of printer queue.
 */
void
topq(int argc, char *argv[])
{
	register int i;
	struct stat stbuf;
	int cmdstatus, changed;
	struct printer myprinter, *pp = &myprinter;

	if (argc < 3) {
		printf("Usage: topq printer [jobnum ...] [user ...]\n");
		return;
	}

	--argc;
	++argv;
	init_printer(pp);
	cmdstatus = getprintcap(*argv, pp);
	switch(cmdstatus) {
	default:
		fatal(pp, "%s", pcaperr(cmdstatus));
	case PCAPERR_NOTFOUND:
		printf("unknown printer %s\n", *argv);
		return;
	case PCAPERR_TCOPEN:
		printf("warning: %s: unresolved tc= reference(s)", *argv);
		break;
	case PCAPERR_SUCCESS:
		break;
	}
	printf("%s:\n", pp->printer);

	seteuid(euid);
	if (chdir(pp->spool_dir) < 0) {
		printf("\tcannot chdir to %s\n", pp->spool_dir);
		goto out;
	}
	seteuid(uid);
	nitems = getq(pp, &queue);
	if (nitems == 0)
		return;
	changed = 0;
	mtime = queue[0]->job_time;
	for (i = argc; --i; ) {
		if (doarg(argv[i]) == 0) {
			printf("\tjob %s is not in the queue\n", argv[i]);
			continue;
		} else
			changed++;
	}
	for (i = 0; i < nitems; i++)
		free(queue[i]);
	free(queue);
	if (!changed) {
		printf("\tqueue order unchanged\n");
		return;
	}
	/*
	 * Turn on the public execute bit of the lock file to
	 * get lpd to rebuild the queue after the current job.
	 */
	seteuid(euid);
	if (changed && stat(pp->lock_file, &stbuf) >= 0)
		(void) chmod(pp->lock_file, stbuf.st_mode | LFM_RESET_QUE);

out:
	seteuid(uid);
} 

/*
 * Reposition the job by changing the modification time of
 * the control file.
 */
static int
touch(struct jobqueue *jq)
{
	struct timeval tvp[2];
	int ret;

	tvp[0].tv_sec = tvp[1].tv_sec = --mtime;
	tvp[0].tv_usec = tvp[1].tv_usec = 0;
	seteuid(euid);
	ret = utimes(jq->job_cfname, tvp);
	seteuid(uid);
	return (ret);
}

/*
 * Checks if specified job name is in the printer's queue.
 * Returns:  negative (-1) if argument name is not in the queue.
 */
static int
doarg(char *job)
{
	register struct jobqueue **qq;
	register int jobnum, n;
	register char *cp, *machine;
	int cnt = 0;
	FILE *fp;

	/*
	 * Look for a job item consisting of system name, colon, number 
	 * (example: ucbarpa:114)  
	 */
	if ((cp = strchr(job, ':')) != NULL) {
		machine = job;
		*cp++ = '\0';
		job = cp;
	} else
		machine = NULL;

	/*
	 * Check for job specified by number (example: 112 or 235ucbarpa).
	 */
	if (isdigit(*job)) {
		jobnum = 0;
		do
			jobnum = jobnum * 10 + (*job++ - '0');
		while (isdigit(*job));
		for (qq = queue + nitems; --qq >= queue; ) {
			n = 0;
			for (cp = (*qq)->job_cfname+3; isdigit(*cp); )
				n = n * 10 + (*cp++ - '0');
			if (jobnum != n)
				continue;
			if (*job && strcmp(job, cp) != 0)
				continue;
			if (machine != NULL && strcmp(machine, cp) != 0)
				continue;
			if (touch(*qq) == 0) {
				printf("\tmoved %s\n", (*qq)->job_cfname);
				cnt++;
			}
		}
		return(cnt);
	}
	/*
	 * Process item consisting of owner's name (example: henry).
	 */
	for (qq = queue + nitems; --qq >= queue; ) {
		seteuid(euid);
		fp = fopen((*qq)->job_cfname, "r");
		seteuid(uid);
		if (fp == NULL)
			continue;
		while (getline(fp) > 0)
			if (line[0] == 'P')
				break;
		(void) fclose(fp);
		if (line[0] != 'P' || strcmp(job, line+1) != 0)
			continue;
		if (touch(*qq) == 0) {
			printf("\tmoved %s\n", (*qq)->job_cfname);
			cnt++;
		}
	}
	return(cnt);
}

/*
 * Enable everything and start printer (undo `down').
 */
void
up(struct printer *pp)
{
	startpr(pp, 2);
}

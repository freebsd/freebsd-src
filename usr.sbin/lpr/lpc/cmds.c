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
  "$FreeBSD: src/usr.sbin/lpr/lpc/cmds.c,v 1.14 2000/01/24 23:30:38 dillon Exp $";
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

static void	abortpr __P((struct printer *, int));
static int	doarg __P((char *));
static int	doselect __P((struct dirent *));
static void	putmsg __P((struct printer *, int, char **));
static int	sortq __P((const void *, const void *));
static void	startpr __P((struct printer *, int));
static int	touch __P((struct queue *));
static void	unlinkf __P((char *));
static void	upstat __P((struct printer *, char *));

/*
 * generic framework for commands which operate on all or a specified
 * set of printers
 */
void
generic(doit, argc, argv)
	void (*doit) __P((struct printer *));
	int argc;
	char *argv[];
{
	int status, more;
	struct printer myprinter, *pp = &myprinter;

	if (argc == 1) {
		printf("Usage: %s {all | printer ...}\n", argv[0]);
		return;
	}
	if (argc == 2 && strcmp(argv[1], "all") == 0) {
		more = firstprinter(pp, &status);
		if (status)
			goto looperr;
		while (more) {
			(*doit)(pp);
			do {
				more = nextprinter(pp, &status);
looperr:
				switch (status) {
				case PCAPERR_TCOPEN:
					printf("warning: %s: unresolved "
					       "tc= reference(s) ",
					       pp->printer);
				case PCAPERR_SUCCESS:
					break;
				default:
					fatal(pp, pcaperr(status));
				}
			} while (more && status);
		}
		return;
	}
	while (--argc) {
		++argv;
		init_printer(pp);
		status = getprintcap(*argv, pp);
		switch(status) {
		default:
			fatal(pp, pcaperr(status));
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
		(*doit)(pp);
	}
}

/*
 * kill an existing daemon and disable printing.
 */
void
doabort(pp)
	struct printer *pp;
{
	abortpr(pp, 1);
}

static void
abortpr(pp, dis)
	struct printer *pp;
	int dis;
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
upstat(pp, msg)
	struct printer *pp;
	char *msg;
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

static int
doselect(d)
	struct dirent *d;
{
	int c = d->d_name[0];

	if ((c == 't' || c == 'c' || c == 'd') && d->d_name[1] == 'f')
		return(1);
	return(0);
}

/*
 * Comparison routine for scandir. Sort by job number and machine, then
 * by `cf', `tf', or `df', then by the sequence letter A-Z, a-z.
 */
static int
sortq(a, b)
	const void *a, *b;
{
	struct dirent **d1, **d2;
	int c1, c2;

	d1 = (struct dirent **)a;
	d2 = (struct dirent **)b;
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
clean(pp)
	struct printer *pp;
{
	register int i, n;
	register char *cp, *cp1, *lp;
	struct dirent **queue;
	int nitems;

	printf("%s:\n", pp->printer);

	lp = line;
	cp = pp->spool_dir;
	while (lp < &line[sizeof(line) - 1]) {
		if ((*lp++ = *cp++) == 0)
			break;
	}
	lp[-1] = '/';

	seteuid(euid);
	nitems = scandir(pp->spool_dir, &queue, doselect, sortq);
	seteuid(uid);
	if (nitems < 0) {
		printf("\tcannot examine spool directory\n");
		return;
	}
	if (nitems == 0)
		return;
	i = 0;
	do {
		cp = queue[i]->d_name;
		if (*cp == 'c') {
			n = 0;
			while (i + 1 < nitems) {
				cp1 = queue[i + 1]->d_name;
				if (*cp1 != 'd' || strcmp(cp + 3, cp1 + 3))
					break;
				i++;
				n++;
			}
			if (n == 0) {
				strncpy(lp, cp, sizeof(line) - strlen(line) - 1);
				line[sizeof(line) - 1] = '\0';
				unlinkf(line);
			}
		} else {
			/*
			 * Must be a df with no cf (otherwise, it would have
			 * been skipped above) or a tf file (which can always
			 * be removed).
			 */
			strncpy(lp, cp, sizeof(line) - strlen(line) - 1);
			line[sizeof(line) - 1] = '\0';
			unlinkf(line);
		}
     	} while (++i < nitems);
}
 
static void
unlinkf(name)
	char	*name;
{
	seteuid(euid);
	if (unlink(name) < 0)
		printf("\tcannot remove %s\n", name);
	else
		printf("\tremoved %s\n", name);
	seteuid(uid);
}

/*
 * Enable queuing to the printer (allow lpr's).
 */
void
enable(pp)
	struct printer *pp;
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
disable(pp)
	struct printer *pp;
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
down(argc, argv)
	int argc;
	char *argv[];
{
        int status, more;
	struct printer myprinter, *pp = &myprinter;

	if (argc == 1) {
		printf("Usage: down {all | printer} [message ...]\n");
		return;
	}
	if (!strcmp(argv[1], "all")) {
		more = firstprinter(pp, &status);
		if (status)
			goto looperr;
		while (more) {
			putmsg(pp, argc - 2, argv + 2);
			do {
				more = nextprinter(pp, &status);
looperr:
				switch (status) {
				case PCAPERR_TCOPEN:
					printf("warning: %s: unresolved "
					       "tc= reference(s) ",
					       pp->printer);
				case PCAPERR_SUCCESS:
					break;
				default:
					fatal(pp, pcaperr(status));
				}
			} while (more && status);
		}
		return;
	}
	init_printer(pp);
	status = getprintcap(argv[1], pp);
	switch(status) {
	default:
		fatal(pp, pcaperr(status));
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
putmsg(pp, argc, argv)
	struct printer *pp;
	int argc;
	char **argv;
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
		while ((cp1 - buf) < sizeof(buf) && (*cp1++ = *cp2++))
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
quit(argc, argv)
	int argc;
	char *argv[];
{
	exit(0);
}

/*
 * Kill and restart the daemon.
 */
void
restart(pp)
	struct printer *pp;
{
	abortpr(pp, 0);
	startpr(pp, 0);
}

/*
 * Enable printing on the specified printer and startup the daemon.
 */
void
startcmd(pp)
	struct printer *pp;
{
	startpr(pp, 1);
}

static void
startpr(pp, enable)
	struct printer *pp;
	int enable;
{
	struct stat stbuf;
	char lf[MAXPATHLEN];

	lock_file_name(pp, lf, sizeof lf);
	printf("%s:\n", pp->printer);

	/*
	 * For enable==1 ('start'), turn off the LFM_PRINT_DIS bit of the
	 * lock file to re-enable printing.  For enable==2 ('up'), also
	 * turn off the LFM_QUEUE_DIS bit to re-enable queueing.
	 */
	seteuid(euid);
	if (enable && stat(lf, &stbuf) >= 0) {
		mode_t bits = (enable == 2 ? 0 : LFM_QUEUE_DIS);
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
status(pp)
	struct printer *pp;
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
stop(pp)
	struct printer *pp;
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

struct	queue **queue;
int	nitems;
time_t	mtime;

/*
 * Put the specified jobs at the top of printer queue.
 */
void
topq(argc, argv)
	int argc;
	char *argv[];
{
	register int i;
	struct stat stbuf;
	int status, changed;
	struct printer myprinter, *pp = &myprinter;

	if (argc < 3) {
		printf("Usage: topq printer [jobnum ...] [user ...]\n");
		return;
	}

	--argc;
	++argv;
	init_printer(pp);
	status = getprintcap(*argv, pp);
	switch(status) {
	default:
		fatal(pp, pcaperr(status));
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
	mtime = queue[0]->q_time;
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
touch(q)
	struct queue *q;
{
	struct timeval tvp[2];
	int ret;

	tvp[0].tv_sec = tvp[1].tv_sec = --mtime;
	tvp[0].tv_usec = tvp[1].tv_usec = 0;
	seteuid(euid);
	ret = utimes(q->q_name, tvp);
	seteuid(uid);
	return (ret);
}

/*
 * Checks if specified job name is in the printer's queue.
 * Returns:  negative (-1) if argument name is not in the queue.
 */
static int
doarg(job)
	char *job;
{
	register struct queue **qq;
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
			for (cp = (*qq)->q_name+3; isdigit(*cp); )
				n = n * 10 + (*cp++ - '0');
			if (jobnum != n)
				continue;
			if (*job && strcmp(job, cp) != 0)
				continue;
			if (machine != NULL && strcmp(machine, cp) != 0)
				continue;
			if (touch(*qq) == 0) {
				printf("\tmoved %s\n", (*qq)->q_name);
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
		fp = fopen((*qq)->q_name, "r");
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
			printf("\tmoved %s\n", (*qq)->q_name);
			cnt++;
		}
	}
	return(cnt);
}

/*
 * Enable everything and start printer (undo `down').
 */
void
up(pp)
	struct printer *pp;
{
	startpr(pp, 2);
}

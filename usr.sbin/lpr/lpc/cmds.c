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
static char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cmds.c	8.2 (Berkeley) 4/28/95";
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

extern uid_t	uid, euid;

static void	abortpr __P((int));
static void	cleanpr __P((void));
static void	disablepr __P((void));
static int	doarg __P((char *));
static int	doselect __P((struct dirent *));
static void	enablepr __P((void));
static void	prstat __P((void));
static void	putmsg __P((int, char **));
static int	sortq __P((const void *, const void *));
static void	startpr __P((int));
static void	stoppr __P((void));
static int	touch __P((struct queue *));
static void	unlinkf __P((char *));
static void	upstat __P((char *));

/*
 * kill an existing daemon and disable printing.
 */
void
doabort(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: abort {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':' &&
			    (cp1 - prbuf) < sizeof(prbuf))
				*cp1++ = c;
			*cp1 = '\0';
			abortpr(1);
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");
		abortpr(1);
	}
}

static void
abortpr(dis)
	int dis;
{
	register FILE *fp;
	struct stat stbuf;
	int pid, fd;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	printf("%s:\n", printer);

	/*
	 * Turn on the owner execute bit of the lock file to disable printing.
	 */
	if (dis) {
		seteuid(euid);
		if (stat(line, &stbuf) >= 0) {
			if (chmod(line, (stbuf.st_mode & 0777) | 0100) < 0)
				printf("\tcannot disable printing\n");
			else {
				upstat("printing disabled\n");
				printf("\tprinting disabled\n");
			}
		} else if (errno == ENOENT) {
			if ((fd = open(line, O_WRONLY|O_CREAT, 0760)) < 0)
				printf("\tcannot create lock file\n");
			else {
				(void) close(fd);
				upstat("printing disabled\n");
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
	if ((fp = fopen(line, "r")) == NULL) {
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
upstat(msg)
	char *msg;
{
	register int fd;
	char statfile[BUFSIZ];

	if (cgetstr(bp, "st", &ST) == -1)
		ST = DEFSTAT;
	(void) sprintf(statfile, "%s/%s", SD, ST);
	umask(0);
	fd = open(statfile, O_WRONLY|O_CREAT, 0664);
	if (fd < 0 || flock(fd, LOCK_EX) < 0) {
		printf("\tcannot create status file\n");
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
 * Remove all spool files and temporaries from the spooling area.
 */
void
clean(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: clean {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			cleanpr();
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		cleanpr();
	}
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
 * Remove incomplete jobs from spooling area.
 */
static void
cleanpr()
{
	register int i, n;
	register char *cp, *cp1, *lp;
	struct dirent **queue;
	int nitems;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	printf("%s:\n", printer);

	for (lp = line, cp = SD; (lp - line) < sizeof(line) && (*lp++ = *cp++);)
		;
	lp[-1] = '/';

	seteuid(euid);
	nitems = scandir(SD, &queue, doselect, sortq);
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
enable(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: enable {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			enablepr();
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		enablepr();
	}
}

static void
enablepr()
{
	struct stat stbuf;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	printf("%s:\n", printer);

	/*
	 * Turn off the group execute bit of the lock file to enable queuing.
	 */
	seteuid(euid);
	if (stat(line, &stbuf) >= 0) {
		if (chmod(line, stbuf.st_mode & 0767) < 0)
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
disable(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: disable {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			disablepr();
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		disablepr();
	}
}

static void
disablepr()
{
	register int fd;
	struct stat stbuf;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	printf("%s:\n", printer);
	/*
	 * Turn on the group execute bit of the lock file to disable queuing.
	 */
	seteuid(euid);
	if (stat(line, &stbuf) >= 0) {
		if (chmod(line, (stbuf.st_mode & 0777) | 010) < 0)
			printf("\tcannot disable queuing\n");
		else
			printf("\tqueuing disabled\n");
	} else if (errno == ENOENT) {
		if ((fd = open(line, O_WRONLY|O_CREAT, 0670)) < 0)
			printf("\tcannot create lock file\n");
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
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: down {all | printer} [message ...]\n");
		return;
	}
	if (!strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			putmsg(argc - 2, argv + 2);
		}
		return;
	}
	printer = argv[1];
	if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
		printf("cannot open printer description file\n");
		return;
	} else if (status == -1) {
		printf("unknown printer %s\n", printer);
		return;
	} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

	putmsg(argc - 2, argv + 2);
}

static void
putmsg(argc, argv)
	int argc;
	char **argv;
{
	register int fd;
	register char *cp1, *cp2;
	char buf[1024];
	struct stat stbuf;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	if (cgetstr(bp, "st", &ST) == -1)
		ST = DEFSTAT;
	printf("%s:\n", printer);
	/*
	 * Turn on the group execute bit of the lock file to disable queuing and
	 * turn on the owner execute bit of the lock file to disable printing.
	 */
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	seteuid(euid);
	if (stat(line, &stbuf) >= 0) {
		if (chmod(line, (stbuf.st_mode & 0777) | 0110) < 0)
			printf("\tcannot disable queuing\n");
		else
			printf("\tprinter and queuing disabled\n");
	} else if (errno == ENOENT) {
		if ((fd = open(line, O_WRONLY|O_CREAT, 0770)) < 0)
			printf("\tcannot create lock file\n");
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
	(void) snprintf(line, sizeof(line), "%s/%s", SD, ST);
	fd = open(line, O_WRONLY|O_CREAT, 0664);
	if (fd < 0 || flock(fd, LOCK_EX) < 0) {
		printf("\tcannot create status file\n");
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
		while ((*cp1++ = *cp2++))
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
restart(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: restart {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			abortpr(0);
			startpr(0);
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		abortpr(0);
		startpr(0);
	}
}

/*
 * Enable printing on the specified printer and startup the daemon.
 */
void
startcmd(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: start {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			startpr(1);
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		startpr(1);
	}
}

static void
startpr(enable)
	int enable;
{
	struct stat stbuf;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	printf("%s:\n", printer);

	/*
	 * Turn off the owner execute bit of the lock file to enable printing.
	 */
	seteuid(euid);
	if (enable && stat(line, &stbuf) >= 0) {
		if (chmod(line, stbuf.st_mode & (enable==2 ? 0666 : 0677)) < 0)
			printf("\tcannot enable printing\n");
		else
			printf("\tprinting enabled\n");
	}
	if (!startdaemon(printer))
		printf("\tcouldn't start daemon\n");
	else
		printf("\tdaemon started\n");
	seteuid(uid);
}

/*
 * Print the status of each queue listed or all the queues.
 */
void
status(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			prstat();
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		prstat();
	}
}

/*
 * Print the status of the printer queue.
 */
static void
prstat()
{
	struct stat stbuf;
	register int fd, i;
	register struct dirent *dp;
	DIR *dirp;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	if (cgetstr(bp, "st", &ST) == -1)
		ST = DEFSTAT;
	printf("%s:\n", printer);
	(void) sprintf(line, "%s/%s", SD, LO);
	if (stat(line, &stbuf) >= 0) {
		printf("\tqueuing is %s\n",
			(stbuf.st_mode & 010) ? "disabled" : "enabled");
		printf("\tprinting is %s\n",
			(stbuf.st_mode & 0100) ? "disabled" : "enabled");
	} else {
		printf("\tqueuing is enabled\n");
		printf("\tprinting is enabled\n");
	}
	if ((dirp = opendir(SD)) == NULL) {
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
		printf("\tno entries\n");
	else if (i == 1)
		printf("\t1 entry in spool area\n");
	else
		printf("\t%d entries in spool area\n", i);
	fd = open(line, O_RDONLY);
	if (fd < 0 || flock(fd, LOCK_SH|LOCK_NB) == 0) {
		(void) close(fd);	/* unlocks as well */
		printf("\tprinter idle\n");
		return;
	}
	(void) close(fd);
	/* print out the contents of the status file, if it exists */
	(void) sprintf(line, "%s/%s", SD, ST);
	fd = open(line, O_RDONLY);
	if (fd >= 0) {
		(void) flock(fd, LOCK_SH);
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
stop(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: stop {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			stoppr();
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		stoppr();
	}
}

static void
stoppr()
{
	register int fd;
	struct stat stbuf;

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	(void) snprintf(line, sizeof(line), "%s/%s", SD, LO);
	printf("%s:\n", printer);

	/*
	 * Turn on the owner execute bit of the lock file to disable printing.
	 */
	seteuid(euid);
	if (stat(line, &stbuf) >= 0) {
		if (chmod(line, (stbuf.st_mode & 0777) | 0100) < 0)
			printf("\tcannot disable printing\n");
		else {
			upstat("printing disabled\n");
			printf("\tprinting disabled\n");
		}
	} else if (errno == ENOENT) {
		if ((fd = open(line, O_WRONLY|O_CREAT, 0760)) < 0)
			printf("\tcannot create lock file\n");
		else {
			(void) close(fd);
			upstat("printing disabled\n");
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

	if (argc < 3) {
		printf("Usage: topq printer [jobnum ...] [user ...]\n");
		return;
	}

	--argc;
	printer = *++argv;
	status = cgetent(&bp, printcapdb, printer);
	if (status == -2) {
		printf("cannot open printer description file\n");
		return;
	} else if (status == -1) {
		printf("%s: unknown printer\n", printer);
		return;
	} else if (status == -3)
		fatal("potential reference loop detected in printcap file");

	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	printf("%s:\n", printer);

	seteuid(euid);
	if (chdir(SD) < 0) {
		printf("\tcannot chdir to %s\n", SD);
		goto out;
	}
	seteuid(uid);
	nitems = getq(&queue);
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
	if (changed && stat(LO, &stbuf) >= 0)
		(void) chmod(LO, (stbuf.st_mode & 0777) | 01);

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
up(argc, argv)
	int argc;
	char *argv[];
{
	register int c, status;
	register char *cp1, *cp2;
	char prbuf[100];

	if (argc == 1) {
		printf("Usage: up {all | printer ...}\n");
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "all")) {
		printer = prbuf;
		while (cgetnext(&bp, printcapdb) > 0) {
			cp1 = prbuf;
			cp2 = bp;
			while ((c = *cp2++) && c != '|' && c != ':')
				*cp1++ = c;
			*cp1 = '\0';
			startpr(2);
		}
		return;
	}
	while (--argc) {
		printer = *++argv;
		if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
			printf("cannot open printer description file\n");
			continue;
		} else if (status == -1) {
			printf("unknown printer %s\n", printer);
			continue;
		} else if (status == -3)
			fatal("potential reference loop detected in printcap file");

		startpr(2);
	}
}

/*
 * atrun.c - run jobs queued by at; run with root privileges.
 * Copyright (c) 1993 by Thomas Koenig
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* System Headers */

#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

#include <paths.h>

/* Local headers */

#define MAIN
#include "privs.h"
#include "pathnames.h"
#include "atrun.h"

/* File scope variables */

static char *namep;
static char rcsid[] = "$Id: atrun.c,v 1.1 1994/01/05 01:03:00 nate Exp $";

/* Local functions */
static void
perr(a)
	const char *a;
{
	syslog(LOG_ERR, "%s: %m", a);
	exit(EXIT_FAILURE);
}

static int
write_string(fd, a)
	int fd;
	const char *a;
{
	return write(fd, a, strlen(a));
}

static void
run_file(filename, uid, gid)
	const char *filename;
	uid_t uid;
	gid_t gid;
{
	/*
	 * Run a file by by spawning off a process which redirects I/O,
	 * spawns a subshell, then waits for it to complete and spawns another
	 * process to send mail to the user.
	 */
	pid_t pid;
	int fd_out, fd_in;
	int queue;
	char mailbuf[9];
	char *mailname = NULL;
	FILE *stream;
	int send_mail = 0;
	struct stat buf;
	off_t size;
	struct passwd *pentry;
	int fflags;

	pid = fork();
	if (pid == -1)
		perr("Cannot fork");
	else if (pid > 0)
		return;

	/*
	 * Let's see who we mail to.  Hopefully, we can read it from the
	 * command file; if not, send it to the owner, or, failing that, to
	 * root.
	 */

	PRIV_START

	    stream = fopen(filename, "r");

	PRIV_END

	if (stream == NULL)
		perr("Cannot open input file");

	if ((fd_in = dup(fileno(stream))) < 0)
		perr("Error duplicating input file descriptor");

	if ((fflags = fcntl(fd_in, F_GETFD)) < 0)
		perr("Error in fcntl");

	fcntl(fd_in, F_SETFD, fflags & ~FD_CLOEXEC);

	if (fscanf(stream, "#! /bin/sh\n# mail %8s %d", mailbuf, &send_mail) == 2) {
		mailname = mailbuf;
	} else {
		pentry = getpwuid(uid);
		if (pentry == NULL)
			mailname = "root";
		else
			mailname = pentry->pw_name;
	}
	fclose(stream);
	if (chdir(_PATH_ATSPOOL) < 0)
		perr("Cannot chdir to " _PATH_ATSPOOL);

	/*
	 * Create a file to hold the output of the job we are  about to
	 * run. Write the mail header.
	 */
	if ((fd_out = open(filename,
		    O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0)
		perr("Cannot create output file");

	write_string(fd_out, "Subject: Output from your job ");
	write_string(fd_out, filename);
	write_string(fd_out, "\n\n");
	fstat(fd_out, &buf);
	size = buf.st_size;

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	pid = fork();
	if (pid < 0)
		perr("Error in fork");
	else if (pid == 0) {
		char *nul = NULL;
		char **nenvp = &nul;

		/*
		 * Set up things for the child; we want standard input from
		 * the input file, and standard output and error sent to
		 * our output file.
		 */

		if (lseek(fd_in, (off_t) 0, SEEK_SET) < 0)
			perr("Error in lseek");

		if (dup(fd_in) != STDIN_FILENO)
			perr("Error in I/O redirection");

		if (dup(fd_out) != STDOUT_FILENO)
			perr("Error in I/O redirection");

		if (dup(fd_out) != STDERR_FILENO)
			perr("Error in I/O redirection");

		close(fd_in);
		close(fd_out);
		if (chdir(_PATH_ATJOBS) < 0)
			perr("Cannot chdir to " _PATH_ATJOBS);

		queue = *filename;

		PRIV_START

		    if (queue > 'b')
			nice(queue - 'b');

		if (setgid(gid) < 0)
			perr("Cannot change group");

		if (setuid(uid) < 0)
			perr("Cannot set user id");

		chdir("/");

		if (execle("/bin/sh", "sh", (char *) NULL, nenvp) != 0)
			perr("Exec failed");

		PRIV_END
	}
	/* We're the parent.  Let's wait. */
	close(fd_in);
	close(fd_out);
	waitpid(pid, (int *) NULL, 0);

	stat(filename, &buf);
	if ((buf.st_size != size) || send_mail) {
		/* Fork off a child for sending mail */
		pid = fork();
		if (pid < 0)
			perr("Fork failed");
		else if (pid == 0) {
			if (open(filename, O_RDONLY) != STDIN_FILENO)
				perr("Cannot reopen output file");

			execl(_PATH_SENDMAIL, _PATH_SENDMAIL, mailname,
			    (char *) NULL);
			perr("Exec failed");
		}
		waitpid(pid, (int *) NULL, 0);
	}
	unlink(filename);
	exit(EXIT_SUCCESS);
}

/* Global functions */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	/*
	 * Browse through  _PATH_ATJOBS, checking all the jobfiles wether
	 * they should be executed and or deleted. The queue is coded into
	 * the first byte of the job filename, the date (in minutes since
	 * Eon) as a hex number in the following eight bytes, followed by
	 * a dot and a serial number.  A file which has not been executed
	 * yet is denoted by its execute - bit set.  For those files which
	 * are to be executed, run_file() is called, which forks off a
	 * child which takes care of I/O redirection, forks off another
	 * child for execution and yet another one, optionally, for sending
	 * mail.  Files which already have run are removed during the
	 * next invocation.
	 */
	DIR *spool;
	struct dirent *dirent;
	struct stat buf;
	int older;
	unsigned long ctm;
	char queue;

	/*
	 * We don't need root privileges all the time; running under uid
	 * and gid daemon is fine.
	 */

	RELINQUISH_PRIVS_ROOT(0) /* it's setuid root */
	openlog("atrun", LOG_PID, LOG_CRON);

	namep = argv[0];
	if (chdir(_PATH_ATJOBS) != 0)
		perr("Cannot change to " _PATH_ATJOBS);

	/*
	 * Main loop. Open spool directory for reading and look over all
	 * the files in there. If the filename indicates that the job
	 * should be run and the x bit is set, fork off a child which sets
	 * its user and group id to that of the files and exec a /bin/sh
	 * which executes the shell script. Unlink older files if they
	 * should no longer be run.  For deletion, their r bit has to be
	 * turned on.
	 */
	if ((spool = opendir(".")) == NULL)
		perr("Cannot read " _PATH_ATJOBS);

	while ((dirent = readdir(spool)) != NULL) {
		double la;

		if (stat(dirent->d_name, &buf) != 0)
			perr("Cannot stat in " _PATH_ATJOBS);

		/* We don't want directories */
		if (!S_ISREG(buf.st_mode))
			continue;

		if (sscanf(dirent->d_name, "%c%8lx", &queue, &ctm) != 2)
			continue;

		if ((queue == 'b') && ((getloadavg(&la, 1) != 1) ||
		    (la > ATRUN_MAXLOAD)))
			continue;

		older = (time_t) ctm *60 <= time(NULL);

		/* The file is executable and old enough */
		if (older && (S_IXUSR & buf.st_mode)) {
			/*
			 * Now we know we want to run the file, we can turn
			 * off the execute bit
			 */

			PRIV_START

			    if (chmod(dirent->d_name, S_IRUSR) != 0)
				perr("Cannot change file permissions");

			PRIV_END

			run_file(dirent->d_name, buf.st_uid, buf.st_gid);
		}
		/* Delete older files */
		if (older && !(S_IXUSR & buf.st_mode) &&
		    (S_IRUSR & buf.st_mode))
			unlink(dirent->d_name);
	}
	closelog();
	exit(EXIT_SUCCESS);
}

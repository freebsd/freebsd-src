/*-
 * Copyright (c) 2010  Peter Pentchev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef __printflike
#ifdef __GNUC__
#define __printflike(fmtarg, firstvararg)	\
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

#define DEFAULT_SPOOLDIR	"/var/spool/dma"

static int	 verbose = 0;
static char	 copybuf[BUFSIZ];

static int	 dma_migrate(int, const char *);

static int	 open_locked(const char *, int, ...);
static void	 cleanup_file(int, char *);

static void	 usage(int);
static void	 version(void);
static void	 debug(const char *, ...) __printflike(1, 2);

int
main(int argc, char **argv)
{
	const char *spooldir;
	int hflag, Vflag, errs, fd, res;
	int ch;
	DIR *d;
	struct dirent *e;
	struct stat sb;

        srandom((unsigned long)((time(NULL) ^ getpid()) + ((uintptr_t)argv)));

	hflag = Vflag = 0;
	spooldir = DEFAULT_SPOOLDIR;
	while (ch = getopt(argc, argv, "d:hVv"), ch != -1)
		switch (ch) {
			case 'd':
				spooldir = optarg;
				break;

			case 'h':
				hflag = 1;
				break;

			case 'V':
				Vflag = 1;
				break;

			case 'v':
				verbose = 1;
				break;

			case '?':
			default:
				usage(1);
				/* NOTREACHED */
		}
	if (Vflag)
		version();
	if (hflag)
		usage(0);
	if (hflag || Vflag)
		exit(0);

	argc -= optind;
	argv += optind;

	/* Let's roll! */
	if (chdir(spooldir) == -1)
		err(1, "Could not change into spool directory %s", spooldir);
	if (d = opendir("."), d == NULL)
		err(1, "Could not read spool directory %s", spooldir);
	errs = 0;
	while (e = readdir(d), e != NULL) {
		/* Do we care about this entry? */
		debug("Read a directory entry: %s\n", e->d_name);
		if (strncmp(e->d_name, "tmp_", 4) == 0 ||
		    e->d_name[0] == 'M' || e->d_name[0] == 'Q' ||
		    (e->d_type != DT_REG && e->d_type != DT_UNKNOWN))
			continue;
		if (e->d_type == DT_UNKNOWN)
			if (stat(e->d_name, &sb) == -1 || !S_ISREG(sb.st_mode))
				continue;
		debug("- want to process it\n");

		/* Try to lock it - skip it if dma is delivering the message */
		if (fd = open_locked(e->d_name, O_RDONLY|O_NDELAY), fd == -1) {
			debug("- seems to be locked, skipping\n");
			continue;
		}

		/* Okay, convert it to the M/Q schema */
		res = dma_migrate(fd, e->d_name);
		close(fd);
		if (res == -1)
			errs++;
	}
	if (errs)
		debug("Finished, %d conversion errors\n", errs);
	else
		debug("Everything seems to be all right\n");
	return (errs && 1);
}

static int
dma_migrate(int fd, const char *fname)
{
	const char *id;
	char *mname, *qname, *tempname, *sender, *recp, *line, *recpline;
	int mfd, qfd, tempfd;
	struct stat sb;
	FILE *fp, *qfp, *mfp;
	size_t sz, len;
	static regex_t *qidreg = NULL;

	mfd = tempfd = qfd = -1;
	mname = qname = sender = recp = line = NULL;
	fp = qfp = NULL;

	if (fstat(fd, &sb) == -1) {
		warn("Could not fstat(%s)", fname);
		return (-1);
	}
	/*
	 * Let's just blithely assume that the queue ID *is* the filename,
	 * since that's the way dma did things so far.
	 * Well, okay, let's check it.
	 */
	if (qidreg == NULL) {
		regex_t *nreg;

		if ((nreg = malloc(sizeof(*qidreg))) == NULL) {
			warn("Could not allocate memory for a regex");
			return (-1);
		}
		if (regcomp(nreg, "^[a-fA-F0-9]\\+\\.[a-fA-F0-9]\\+$", 0)
		    != 0) {
			warnx("Could not compile a dma queue ID regex");
			free(nreg);
			return (-1);
		}
		qidreg = nreg;
	}
	if (regexec(qidreg, fname, 0, NULL, 0) != 0) {
		warnx("The name '%s' is not a valid dma queue ID", fname);
		return (-1);
	}
	id = fname;
	debug("  - queue ID %s\n", id);
	if (asprintf(&mname, "M%s", id) == -1 ||
	    asprintf(&tempname, "tmp_%s", id) == -1 ||
	    asprintf(&qname, "Q%s", id) == -1 ||
	    mname == NULL || tempname == NULL || qname == NULL)
		goto fail;

	/* Create the message placeholder early to avoid races */
	mfd = open_locked(mname, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (mfd == -1) {
		warn("Could not create temporary file %s", mname);
		goto fail;
	}
	if (stat(qname, &sb) != -1 || errno != ENOENT ||
	    stat(tempname, &sb) != -1 || errno != ENOENT) {
		warnx("Some of the queue files for %s already exist", fname);
		goto fail;
	}
	debug("  - mfd %d names %s, %s, %s\n", mfd, mname, tempname, qname);

	fp = fdopen(fd, "r");
	if (fp == NULL) {
		warn("Could not reopen the descriptor for %s", fname);
		goto fail;
	}

	/* Parse the header of the old-format message file */
	/* ...sender... */
	if (getline(&sender, &sz, fp) == -1) {
		warn("Could not read the initial line from %s", fname);
		goto fail;
	}
	sz = strlen(sender);
	while (sz > 0 && (sender[sz - 1] == '\n' || sender[sz - 1] == '\r'))
		sender[--sz] = '\0';
	if (sz == 0) {
		warnx("Empty sender line in %s", fname);
		goto fail;
	}
	debug("  - sender %s\n", sender);
	/* ...recipient(s)... */
	len = strlen(fname);
	recpline = NULL;
	while (1) {
		if (getline(&line, &sz, fp) == -1) {
			warn("Could not read a recipient line from %s", fname);
			goto fail;
		}
		sz = strlen(line);
		while (sz > 0 &&
		    (line[sz - 1] == '\n' || line[sz - 1] == '\r'))
			line[--sz] = '\0';
		if (sz == 0) {
			free(line);
			line = NULL;
			break;
		}
		if (recp == NULL &&
		    strncmp(line, fname, len) == 0 && line[len] == ' ') {
			recp = line + len + 1;
			recpline = line;
		} else {
			free(line);
		}
		line = NULL;
	}
	if (recp == NULL) {
		warnx("Could not find its own recipient line in %s", fname);
		goto fail;
	}
	/* ..phew, finished with the header. */

	tempfd = open_locked(tempname, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (tempfd == -1) {
		warn("Could not create a queue file for %s", fname);
		goto fail;
	}
	qfp = fdopen(tempfd, "w");
	if (qfp == NULL) {
		warn("Could not fdopen(%s) for %s", tempname, fname);
		goto fail;
	}
	mfp = fdopen(mfd, "w");
	if (mfp == NULL) {
		warn("Could not fdopen(%s) for %s", mname, fname);
		goto fail;
	}
	fprintf(qfp, "ID: %s\nSender: %s\nRecipient: %s\n", id, sender, recp);
	fflush(qfp);
	fsync(tempfd);

	/* Copy the message file over to mname */
	while ((sz = fread(copybuf, 1, sizeof(copybuf), fp)) > 0)
		if (fwrite(copybuf, 1, sz, mfp) != sz) {
			warn("Could not copy the message from %s to %s",
			    fname, mname);
			goto fail;
		}
	if (ferror(fp)) {
		warn("Could not read the full message from %s", fname);
		goto fail;
	}
	fflush(mfp);
	fsync(mfd);

	if (rename(tempname, qname) == -1) {
		warn("Could not rename the queue file for %s", fname);
		goto fail;
	}
	qfd = tempfd;
	tempfd = -1;
	if (unlink(fname) == -1) {
		warn("Could not remove the old converted file %s", fname);
		goto fail;
	}

	fclose(fp);
	fclose(qfp);
	free(sender);
	free(line);
	free(recpline);
	free(mname);
	free(qname);
	free(tempname);
	return (0);

fail:
	if (fp != NULL)
		fclose(fp);
	if (qfp != NULL)
		fclose(qfp);
	if (sender != NULL)
		free(sender);
	if (line != NULL)
		free(line);
	if (recpline != NULL)
		free(recpline);
	cleanup_file(mfd, mname);
	cleanup_file(qfd, qname);
	cleanup_file(tempfd, tempname);
	return (-1);
}

static void
cleanup_file(int fd, char *fname)
{
	if (fd != -1) {
		close(fd);
		unlink(fname);
	}
	if (fname != NULL)
		free(fname);
}

static void
usage(int ferr)
{
	const char *s =
	    "Usage:\tdma-migrate [-hVv] [-d spooldir]\n"
	    "\t-d\tspecify the spool directory (" DEFAULT_SPOOLDIR ")\n"
	    "\t-h\tdisplay program usage information and exit\n"
	    "\t-V\tdisplay program version information and exit\n"
	    "\t-v\tverbose operation - display diagnostic messages";

	if (ferr)
		errx(1, "%s", s);
	puts(s);
}

static void
version(void)
{
	printf("dma-migrate 0.01 (dma 0.0.2010.06.17)\n");
}

static void
debug(const char *fmt, ...)
{
	va_list v;

	if (verbose < 1)
		return;
	va_start(v, fmt);
	vfprintf(stderr, fmt, v);
	va_end(v);
}

static int
open_locked(const char *fname, int flags, ...)
{
	int mode = 0;
#ifndef O_EXLOCK
	int fd, save_errno;
#endif

	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}

#ifndef O_EXLOCK
	fd = open(fname, flags, mode);
	if (fd < 0)
		return(fd);
	if (flock(fd, LOCK_EX|((flags & O_NONBLOCK)? LOCK_NB: 0)) < 0) {
		save_errno = errno;
		close(fd);
		errno = save_errno;
		return(-1);
	}
	return(fd);
#else
	return(open(fname, flags|O_EXLOCK, mode));
#endif
}

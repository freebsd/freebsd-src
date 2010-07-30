/*-
 * Copyright (c) 2009 Joerg Sonnenberger <joerg@NetBSD.org>
 * Copyright (c) 2007-2008 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *
 * $FreeBSD$
 *
 * This file would be much shorter if we didn't care about command-line
 * compatibility with Info-ZIP's UnZip, which requires us to duplicate
 * parts of libarchive in order to gain more detailed control of its
 * behaviour for the purpose of implementing the -n, -o, -L and -a
 * options.
 */

#include <sys/queue.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

/* command-line options */
static int		 a_opt;		/* convert EOL */
static int		 C_opt;		/* match case-insensitively */
static int		 c_opt;		/* extract to stdout */
static const char	*d_arg;		/* directory */
static int		 f_opt;		/* update existing files only */
static int		 j_opt;		/* junk directories */
static int		 L_opt;		/* lowercase names */
static int		 n_opt;		/* never overwrite */
static int		 o_opt;		/* always overwrite */
static int		 p_opt;		/* extract to stdout, quiet */
static int		 q_opt;		/* quiet */
static int		 t_opt;		/* test */
static int		 u_opt;		/* update */
static int		 v_opt;		/* verbose/list */

/* time when unzip started */
static time_t		 now;

/* debug flag */
static int		 unzip_debug;

/* running on tty? */
static int		 tty;

/* convenience macro */
/* XXX should differentiate between ARCHIVE_{WARN,FAIL,RETRY} */
#define ac(call)						\
	do {							\
		int acret = (call);				\
		if (acret != ARCHIVE_OK)			\
			errorx("%s", archive_error_string(a));	\
	} while (0)

/*
 * Indicates that last info() did not end with EOL.  This helps error() et
 * al. avoid printing an error message on the same line as an incomplete
 * informational message.
 */
static int noeol;

/* fatal error message + errno */
static void
error(const char *fmt, ...)
{
	va_list ap;

	if (noeol)
		fprintf(stdout, "\n");
	fflush(stdout);
	fprintf(stderr, "unzip: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	exit(1);
}

/* fatal error message, no errno */
static void
errorx(const char *fmt, ...)
{
	va_list ap;

	if (noeol)
		fprintf(stdout, "\n");
	fflush(stdout);
	fprintf(stderr, "unzip: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

#if 0
/* non-fatal error message + errno */
static void
warning(const char *fmt, ...)
{
	va_list ap;

	if (noeol)
		fprintf(stdout, "\n");
	fflush(stdout);
	fprintf(stderr, "unzip: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errno));
}
#endif

/* non-fatal error message, no errno */
static void
warningx(const char *fmt, ...)
{
	va_list ap;

	if (noeol)
		fprintf(stdout, "\n");
	fflush(stdout);
	fprintf(stderr, "unzip: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

/* informational message (if not -q) */
static void
info(const char *fmt, ...)
{
	va_list ap;

	if (q_opt && !unzip_debug)
		return;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);

	if (*fmt == '\0')
		noeol = 1;
	else
		noeol = fmt[strlen(fmt) - 1] != '\n';
}

/* debug message (if unzip_debug) */
static void
debug(const char *fmt, ...)
{
	va_list ap;

	if (!unzip_debug)
		return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);

	if (*fmt == '\0')
		noeol = 1;
	else
		noeol = fmt[strlen(fmt) - 1] != '\n';
}

/* duplicate a path name, possibly converting to lower case */
static char *
pathdup(const char *path)
{
	char *str;
	size_t i, len;

	len = strlen(path);
	while (len && path[len - 1] == '/')
		len--;
	if ((str = malloc(len + 1)) == NULL) {
		errno = ENOMEM;
		error("malloc()");
	}
	if (L_opt) {
		for (i = 0; i < len; ++i)
			str[i] = tolower((unsigned char)path[i]);
	} else {
		memcpy(str, path, len);
	}
	str[len] = '\0';

	return (str);
}

/* concatenate two path names */
static char *
pathcat(const char *prefix, const char *path)
{
	char *str;
	size_t prelen, len;

	prelen = prefix ? strlen(prefix) + 1 : 0;
	len = strlen(path) + 1;
	if ((str = malloc(prelen + len)) == NULL) {
		errno = ENOMEM;
		error("malloc()");
	}
	if (prefix) {
		memcpy(str, prefix, prelen);	/* includes zero */
		str[prelen - 1] = '/';		/* splat zero */
	}
	memcpy(str + prelen, path, len);	/* includes zero */

	return (str);
}

/*
 * Pattern lists for include / exclude processing
 */
struct pattern {
	STAILQ_ENTRY(pattern) link;
	char pattern[];
};

STAILQ_HEAD(pattern_list, pattern);
static struct pattern_list include = STAILQ_HEAD_INITIALIZER(include);
static struct pattern_list exclude = STAILQ_HEAD_INITIALIZER(exclude);

/*
 * Add an entry to a pattern list
 */
static void
add_pattern(struct pattern_list *list, const char *pattern)
{
	struct pattern *entry;
	size_t len;

	debug("adding pattern '%s'\n", pattern);
	len = strlen(pattern);
	if ((entry = malloc(sizeof *entry + len + 1)) == NULL) {
		errno = ENOMEM;
		error("malloc()");
	}
	memcpy(entry->pattern, pattern, len + 1);
	STAILQ_INSERT_TAIL(list, entry, link);
}

/*
 * Match a string against a list of patterns
 */
static int
match_pattern(struct pattern_list *list, const char *str)
{
	struct pattern *entry;

	STAILQ_FOREACH(entry, list, link) {
		if (fnmatch(entry->pattern, str, C_opt ? FNM_CASEFOLD : 0) == 0)
			return (1);
	}
	return (0);
}

/*
 * Verify that a given pathname is in the include list and not in the
 * exclude list.
 */
static int
accept_pathname(const char *pathname)
{

	if (!STAILQ_EMPTY(&include) && !match_pattern(&include, pathname))
		return (0);
	if (!STAILQ_EMPTY(&exclude) && match_pattern(&exclude, pathname))
		return (0);
	return (1);
}

/*
 * Create the specified directory with the specified mode, taking certain
 * precautions on they way.
 */
static void
make_dir(const char *path, int mode)
{
	struct stat sb;

	if (lstat(path, &sb) == 0) {
		if (S_ISDIR(sb.st_mode))
			return;
		/*
		 * Normally, we should either ask the user about removing
		 * the non-directory of the same name as a directory we
		 * wish to create, or respect the -n or -o command-line
		 * options.  However, this may lead to a later failure or
		 * even compromise (if this non-directory happens to be a
		 * symlink to somewhere unsafe), so we don't.
		 */

		/*
		 * Don't check unlink() result; failure will cause mkdir()
		 * to fail later, which we will catch.
		 */
		(void)unlink(path);
	}
	if (mkdir(path, mode) != 0 && errno != EEXIST)
		error("mkdir('%s')", path);
}

/*
 * Ensure that all directories leading up to (but not including) the
 * specified path exist.
 *
 * XXX inefficient + modifies the file in-place
 */
static void
make_parent(char *path)
{
	struct stat sb;
	char *sep;

	sep = strrchr(path, '/');
	if (sep == NULL || sep == path)
		return;
	*sep = '\0';
	if (lstat(path, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			*sep = '/';
			return;
		}
		unlink(path);
	}
	make_parent(path);
	mkdir(path, 0755);
	*sep = '/';

#if 0
	for (sep = path; (sep = strchr(sep, '/')) != NULL; sep++) {
		/* root in case of absolute d_arg */
		if (sep == path)
			continue;
		*sep = '\0';
		make_dir(path, 0755);
		*sep = '/';
	}
#endif
}

/*
 * Extract a directory.
 */
static void
extract_dir(struct archive *a, struct archive_entry *e, const char *path)
{
	int mode;

	mode = archive_entry_mode(e) & 0777;
	if (mode == 0)
		mode = 0755;

	/*
	 * Some zipfiles contain directories with weird permissions such
	 * as 0644 or 0444.  This can cause strange issues such as being
	 * unable to extract files into the directory we just created, or
	 * the user being unable to remove the directory later without
	 * first manually changing its permissions.  Therefore, we whack
	 * the permissions into shape, assuming that the user wants full
	 * access and that anyone who gets read access also gets execute
	 * access.
	 */
	mode |= 0700;
	if (mode & 0040)
		mode |= 0010;
	if (mode & 0004)
		mode |= 0001;

	info("d %s\n", path);
	make_dir(path, mode);
	ac(archive_read_data_skip(a));
}

static unsigned char buffer[8192];
static char spinner[] = { '|', '/', '-', '\\' };

static int
handle_existing_file(char **path)
{
	size_t alen;
	ssize_t len;
	char buf[4];

	for (;;) {
		fprintf(stderr,
		    "replace %s? [y]es, [n]o, [A]ll, [N]one, [r]ename: ",
		    *path);
		if (fgets(buf, sizeof(buf), stdin) == 0) {
			clearerr(stdin);
			printf("NULL\n(EOF or read error, "
			    "treating as \"[N]one\"...)\n");
			n_opt = 1;
			return -1;
		}
		switch (*buf) {
		case 'A':
			o_opt = 1;
			/* FALLTHROUGH */
		case 'y':
		case 'Y':
			(void)unlink(*path);
			return 1;
		case 'N':
			n_opt = 1;			
			/* FALLTHROUGH */
		case 'n':
			return -1;
		case 'r':
		case 'R':
			printf("New name: ");
			fflush(stdout);
			free(*path);
			*path = NULL;
			alen = 0;
			len = getdelim(path, &alen, '\n', stdin);
			if ((*path)[len - 1] == '\n')
				(*path)[len - 1] = '\0';
			return 0;
		default:
			break;
		}
	}
}

/*
 * Extract a regular file.
 */
static void
extract_file(struct archive *a, struct archive_entry *e, char **path)
{
	int mode;
	time_t mtime;
	struct stat sb;
	struct timeval tv[2];
	int cr, fd, text, warn, check;
	ssize_t len;
	unsigned char *p, *q, *end;

	mode = archive_entry_mode(e) & 0777;
	if (mode == 0)
		mode = 0644;
	mtime = archive_entry_mtime(e);

	/* look for existing file of same name */
recheck:
	if (lstat(*path, &sb) == 0) {
		if (u_opt || f_opt) {
			/* check if up-to-date */
			if (S_ISREG(sb.st_mode) && sb.st_mtime >= mtime)
				return;
			(void)unlink(*path);
		} else if (o_opt) {
			/* overwrite */
			(void)unlink(*path);
		} else if (n_opt) {
			/* do not overwrite */
			return;
		} else {
			check = handle_existing_file(path);
			if (check == 0)
				goto recheck;
			if (check == -1)
				return; /* do not overwrite */
		}
	} else {
		if (f_opt)
			return;
	}

	if ((fd = open(*path, O_RDWR|O_CREAT|O_TRUNC, mode)) < 0)
		error("open('%s')", *path);

	/* loop over file contents and write to disk */
	info(" extracting: %s", *path);
	text = a_opt;
	warn = 0;
	cr = 0;
	for (int n = 0; ; n++) {
		if (tty && (n % 4) == 0)
			info(" %c\b\b", spinner[(n / 4) % sizeof spinner]);

		len = archive_read_data(a, buffer, sizeof buffer);

		if (len < 0)
			ac(len);

		/* left over CR from previous buffer */
		if (a_opt && cr) {
			if (len == 0 || buffer[0] != '\n')
				if (write(fd, "\r", 1) != 1)
					error("write('%s')", *path);
			cr = 0;
		}

		/* EOF */
		if (len == 0)
			break;
		end = buffer + len;

		/*
		 * Detect whether this is a text file.  The correct way to
		 * do this is to check the least significant bit of the
		 * "internal file attributes" field of the corresponding
		 * file header in the central directory, but libarchive
		 * does not read the central directory, so we have to
		 * guess by looking for non-ASCII characters in the
		 * buffer.  Hopefully we won't guess wrong.  If we do
		 * guess wrong, we print a warning message later.
		 */
		if (a_opt && n == 0) {
			for (p = buffer; p < end; ++p) {
				if (!isascii((unsigned char)*p)) {
					text = 0;
					break;
				}
			}
		}

		/* simple case */
		if (!a_opt || !text) {
			if (write(fd, buffer, len) != len)
				error("write('%s')", *path);
			continue;
		}

		/* hard case: convert \r\n to \n (sigh...) */
		for (p = buffer; p < end; p = q + 1) {
			for (q = p; q < end; q++) {
				if (!warn && !isascii(*q)) {
					warningx("%s may be corrupted due"
					    " to weak text file detection"
					    " heuristic", *path);
					warn = 1;
				}
				if (q[0] != '\r')
					continue;
				if (&q[1] == end) {
					cr = 1;
					break;
				}
				if (q[1] == '\n')
					break;
			}
			if (write(fd, p, q - p) != q - p)
				error("write('%s')", *path);
		}
	}
	if (tty)
		info("  \b\b");
	if (text)
		info(" (text)");
	info("\n");

	/* set access and modification time */
	tv[0].tv_sec = now;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = mtime;
	tv[1].tv_usec = 0;
	if (futimes(fd, tv) != 0)
		error("utimes('%s')", *path);
	if (close(fd) != 0)
		error("close('%s')", *path);
}

/*
 * Extract a zipfile entry: first perform some sanity checks to ensure
 * that it is either a directory or a regular file and that the path is
 * not absolute and does not try to break out of the current directory;
 * then call either extract_dir() or extract_file() as appropriate.
 *
 * This is complicated a bit by the various ways in which we need to
 * manipulate the path name.  Case conversion (if requested by the -L
 * option) happens first, but the include / exclude patterns are applied
 * to the full converted path name, before the directory part of the path
 * is removed in accordance with the -j option.  Sanity checks are
 * intentionally done earlier than they need to be, so the user will get a
 * warning about insecure paths even for files or directories which
 * wouldn't be extracted anyway.
 */
static void
extract(struct archive *a, struct archive_entry *e)
{
	char *pathname, *realpathname;
	mode_t filetype;
	char *p, *q;

	pathname = pathdup(archive_entry_pathname(e));
	filetype = archive_entry_filetype(e);

	/* sanity checks */
	if (pathname[0] == '/' ||
	    strncmp(pathname, "../", 3) == 0 ||
	    strstr(pathname, "/../") != NULL) {
		warningx("skipping insecure entry '%s'", pathname);
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* I don't think this can happen in a zipfile.. */
	if (!S_ISDIR(filetype) && !S_ISREG(filetype)) {
		warningx("skipping non-regular entry '%s'", pathname);
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* skip directories in -j case */
	if (S_ISDIR(filetype) && j_opt) {
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* apply include / exclude patterns */
	if (!accept_pathname(pathname)) {
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* apply -j and -d */
	if (j_opt) {
		for (p = q = pathname; *p; ++p)
			if (*p == '/')
				q = p + 1;
		realpathname = pathcat(d_arg, q);
	} else {
		realpathname = pathcat(d_arg, pathname);
	}

	/* ensure that parent directory exists */
	make_parent(realpathname);

	if (S_ISDIR(filetype))
		extract_dir(a, e, realpathname);
	else
		extract_file(a, e, &realpathname);

	free(realpathname);
	free(pathname);
}

static void
extract_stdout(struct archive *a, struct archive_entry *e)
{
	char *pathname;
	mode_t filetype;
	int cr, text, warn;
	ssize_t len;
	unsigned char *p, *q, *end;

	pathname = pathdup(archive_entry_pathname(e));
	filetype = archive_entry_filetype(e);

	/* I don't think this can happen in a zipfile.. */
	if (!S_ISDIR(filetype) && !S_ISREG(filetype)) {
		warningx("skipping non-regular entry '%s'", pathname);
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* skip directories in -j case */
	if (S_ISDIR(filetype)) {
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	/* apply include / exclude patterns */
	if (!accept_pathname(pathname)) {
		ac(archive_read_data_skip(a));
		free(pathname);
		return;
	}

	if (c_opt)
		info("x %s\n", pathname);

	text = a_opt;
	warn = 0;
	cr = 0;
	for (int n = 0; ; n++) {
		len = archive_read_data(a, buffer, sizeof buffer);

		if (len < 0)
			ac(len);

		/* left over CR from previous buffer */
		if (a_opt && cr) {
			if (len == 0 || buffer[0] != '\n') {
				if (fwrite("\r", 1, 1, stderr) != 1)
					error("write('%s')", pathname);
			}
			cr = 0;
		}

		/* EOF */
		if (len == 0)
			break;
		end = buffer + len;

		/*
		 * Detect whether this is a text file.  The correct way to
		 * do this is to check the least significant bit of the
		 * "internal file attributes" field of the corresponding
		 * file header in the central directory, but libarchive
		 * does not read the central directory, so we have to
		 * guess by looking for non-ASCII characters in the
		 * buffer.  Hopefully we won't guess wrong.  If we do
		 * guess wrong, we print a warning message later.
		 */
		if (a_opt && n == 0) {
			for (p = buffer; p < end; ++p) {
				if (!isascii((unsigned char)*p)) {
					text = 0;
					break;
				}
			}
		}

		/* simple case */
		if (!a_opt || !text) {
			if (fwrite(buffer, 1, len, stdout) != (size_t)len)
				error("write('%s')", pathname);
			continue;
		}

		/* hard case: convert \r\n to \n (sigh...) */
		for (p = buffer; p < end; p = q + 1) {
			for (q = p; q < end; q++) {
				if (!warn && !isascii(*q)) {
					warningx("%s may be corrupted due"
					    " to weak text file detection"
					    " heuristic", pathname);
					warn = 1;
				}
				if (q[0] != '\r')
					continue;
				if (&q[1] == end) {
					cr = 1;
					break;
				}
				if (q[1] == '\n')
					break;
			}
			if (fwrite(p, 1, q - p, stdout) != (size_t)(q - p))
				error("write('%s')", pathname);
		}
	}

	free(pathname);
}

/*
 * Print the name of an entry to stdout.
 */
static void
list(struct archive *a, struct archive_entry *e)
{
	char buf[20];
	time_t mtime;

	mtime = archive_entry_mtime(e);
	strftime(buf, sizeof(buf), "%m-%d-%g %R", localtime(&mtime));

	if (v_opt == 1) {
		printf(" %8ju  %s   %s\n",
		    (uintmax_t)archive_entry_size(e),
		    buf, archive_entry_pathname(e));
	} else if (v_opt == 2) {
		printf("%8ju  Stored  %7ju   0%%  %s  %08x  %s\n",
		    (uintmax_t)archive_entry_size(e),
		    (uintmax_t)archive_entry_size(e),
		    buf,
		    0U,
		    archive_entry_pathname(e));
	}
	ac(archive_read_data_skip(a));
}

/*
 * Extract to memory to check CRC
 */
static int
test(struct archive *a, struct archive_entry *e)
{
	ssize_t len;
	int error_count;

	error_count = 0;
	if (S_ISDIR(archive_entry_filetype(e)))
		return 0;

	info("    testing: %s\t", archive_entry_pathname(e));
	while ((len = archive_read_data(a, buffer, sizeof buffer)) > 0)
		/* nothing */;
	if (len < 0) {
		info(" %s\n", archive_error_string(a));
		++error_count;
	} else {
		info(" OK\n");
	}

	/* shouldn't be necessary, but it doesn't hurt */
	ac(archive_read_data_skip(a));

	return error_count;
}


/*
 * Main loop: open the zipfile, iterate over its contents and decide what
 * to do with each entry.
 */
static void
unzip(const char *fn)
{
	struct archive *a;
	struct archive_entry *e;
	int fd, ret;
	uintmax_t total_size, file_count, error_count;

	if ((fd = open(fn, O_RDONLY)) < 0)
		error("%s", fn);

	if ((a = archive_read_new()) == NULL)
		error("archive_read_new failed");

	ac(archive_read_support_format_zip(a));
	ac(archive_read_open_fd(a, fd, 8192));

	if (!p_opt && !q_opt)
		printf("Archive:  %s\n", fn);
	if (v_opt == 1) {
		printf("  Length     Date   Time    Name\n");
		printf(" --------    ----   ----    ----\n");
	} else if (v_opt == 2) {
		printf(" Length   Method    Size  Ratio   Date   Time   CRC-32    Name\n");
		printf("--------  ------  ------- -----   ----   ----   ------    ----\n");
	}

	total_size = 0;
	file_count = 0;
	error_count = 0;
	for (;;) {
		ret = archive_read_next_header(a, &e);
		if (ret == ARCHIVE_EOF)
			break;
		ac(ret);
		if (t_opt)
			error_count += test(a, e);
		else if (v_opt)
			list(a, e);
		else if (p_opt || c_opt)
			extract_stdout(a, e);
		else
			extract(a, e);

		total_size += archive_entry_size(e);
		++file_count;
	}

	if (v_opt == 1) {
		printf(" --------                   -------\n");
		printf(" %8ju                   %ju file%s\n",
		    total_size, file_count, file_count != 1 ? "s" : "");
	} else if (v_opt == 2) {
		printf("--------          -------  ---                            -------\n");
		printf("%8ju          %7ju   0%%                            %ju file%s\n",
		    total_size, total_size, file_count,
		    file_count != 1 ? "s" : "");
	}

	ac(archive_read_close(a));
	(void)archive_read_finish(a);

	if (close(fd) != 0)
		error("%s", fn);

	if (t_opt) {
		if (error_count > 0) {
			errorx("%d checksum error(s) found.", error_count);
		}
		else {
			printf("No errors detected in compressed data of %s.\n",
			       fn);
		}
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: unzip [-aCcfjLlnopqtuv] [-d dir] [-x pattern] zipfile\n");
	exit(1);
}

static int
getopts(int argc, char *argv[])
{
	int opt;

	optreset = optind = 1;
	while ((opt = getopt(argc, argv, "aCcd:fjLlnopqtuvx:")) != -1)
		switch (opt) {
		case 'a':
			a_opt = 1;
			break;
		case 'C':
			C_opt = 1;
			break;
		case 'c':
			c_opt = 1;
			break;
		case 'd':
			d_arg = optarg;
			break;
		case 'f':
			f_opt = 1;
			break;
		case 'j':
			j_opt = 1;
			break;
		case 'L':
			L_opt = 1;
			break;
		case 'l':
			if (v_opt == 0)
				v_opt = 1;
			break;
		case 'n':
			n_opt = 1;
			break;
		case 'o':
			o_opt = 1;
			q_opt = 1;
			break;
		case 'p':
			p_opt = 1;
			break;
		case 'q':
			q_opt = 1;
			break;
		case 't':
			t_opt = 1;
			break;
		case 'u':
			u_opt = 1;
			break;
		case 'v':
			v_opt = 2;
			break;
		case 'x':
			add_pattern(&exclude, optarg);
			break;
		default:
			usage();
		}

	return (optind);
}

int
main(int argc, char *argv[])
{
	const char *zipfile;
	int nopts;

	if (isatty(STDOUT_FILENO))
		tty = 1;

	if (getenv("UNZIP_DEBUG") != NULL)
		unzip_debug = 1;
	for (int i = 0; i < argc; ++i)
		debug("%s%c", argv[i], (i < argc - 1) ? ' ' : '\n');

	/*
	 * Info-ZIP's unzip(1) expects certain options to come before the
	 * zipfile name, and others to come after - though it does not
	 * enforce this.  For simplicity, we accept *all* options both
	 * before and after the zipfile name.
	 */
	nopts = getopts(argc, argv);

	if (argc <= nopts)
		usage();
	zipfile = argv[nopts++];

	while (nopts < argc && *argv[nopts] != '-')
		add_pattern(&include, argv[nopts++]);

	nopts--; /* fake argv[0] */
	nopts += getopts(argc - nopts, argv + nopts);

	if (n_opt + o_opt + u_opt > 1)
		errorx("-n, -o and -u are contradictory");

	time(&now);

	unzip(zipfile);

	exit(0);
}

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009, 2010 Joerg Sonnenberger <joerg@NetBSD.org>
 * Copyright (c) 2007-2008 Dag-Erling Smørgrav
 * All rights reserved.
 */

#include "bsdunzip_platform.h"

#include "la_queue.h"
#include "la_getline.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if ((!defined(HAVE_UTIMENSAT) && defined(HAVE_LUTIMES)) || \
    (!defined(HAVE_FUTIMENS) && defined(HAVE_FUTIMES)))
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#endif
#ifdef HAVE_GETOPT_OPTRESET
#include <getopt.h>
#endif

#include "bsdunzip.h"
#include "passphrase.h"
#include "err.h"

/* command-line options */
static int		 a_opt;		/* convert EOL */
static int		 C_opt;		/* match case-insensitively */
static int		 c_opt;		/* extract to stdout */
static const char	*d_arg;		/* directory */
static int		 f_opt;		/* update existing files only */
static const char	*O_arg;		/* encoding */
static int		 j_opt;		/* junk directories */
static int		 L_opt;		/* lowercase names */
static int		 n_opt;		/* never overwrite */
static int		 o_opt;		/* always overwrite */
static int		 p_opt;		/* extract to stdout, quiet */
static const char	*P_arg;		/* passphrase */
static int		 q_opt;		/* quiet */
static int		 t_opt;		/* test */
static int		 u_opt;		/* update */
static int		 v_opt;		/* verbose/list */
static const char	*y_str = "";	/* 4 digit year */
static int		 Z1_opt;	/* zipinfo mode list files only */
static int		 version_opt;	/* version string */

/* debug flag */
static int		 unzip_debug;

/* zipinfo mode */
static int		 zipinfo_mode;

/* running on tty? */
static int		 tty;

/* processing exclude list */
static int		 unzip_exclude_mode = 0;

int bsdunzip_optind;

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

/* for an interactive passphrase input */
static char *passphrase_buf;

/* fatal error message + errno */
static void __LA_NORETURN
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
	exit(EXIT_FAILURE);
}

/* fatal error message, no errno */
static void __LA_NORETURN
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
	exit(EXIT_FAILURE);
}

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

	if (path == NULL || path[0] == '\0')
		return (NULL);

	len = strlen(path);
	while (len && path[len - 1] == '/')
		len--;
	if ((str = malloc(len + 1)) == NULL) {
		errno = ENOMEM;
		error("malloc()");
	}
	if (L_opt) {
		for (i = 0; i < len; ++i)
			str[i] = (char)tolower((unsigned char)path[i]);
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
#ifdef HAVE_FNMATCH
		if (fnmatch(entry->pattern, str, C_opt ? FNM_CASEFOLD : 0) == 0)
			return (1);
#else
#error "Unsupported platform: fnmatch() is required"
#endif
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
	if (mkdir(path, (mode_t)mode) != 0 && errno != EEXIST)
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

	/*
	 * Dropbox likes to create '/' directory entries, just ignore
	 * such junk.
	 */
	if (*path == '\0')
		return;

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

	info("   creating: %s/\n", path);
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
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			goto stdin_err;
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
			len = getline(path, &alen, stdin);
			if (len < 1)
				goto stdin_err;
			if ((*path)[len - 1] == '\n')
				(*path)[len - 1] = '\0';
			return 0;
		default:
			break;
		}
	}
stdin_err:
	clearerr(stdin);
	printf("NULL\n(EOF or read error, "
		"treating as \"[N]one\"...)\n");
	n_opt = 1;
	return -1;
}

/*
 * Detect binary files by a combination of character white list and
 * black list. NUL bytes and other control codes without use in text files
 * result directly in switching the file to binary mode. Otherwise, at least
 * one white-listed byte has to be found.
 *
 * Black-listed: 0..6, 14..25, 28..31
 * 0xf3ffc07f = 11110011111111111100000001111111b
 * White-listed: 9..10, 13, >= 32
 * 0x00002600 = 00000000000000000010011000000000b
 *
 * See the proginfo/txtvsbin.txt in the zip sources for a detailed discussion.
 */
#define BYTE_IS_BINARY(x)	((x) < 32 && (0xf3ffc07fU & (1U << (x))))
#define	BYTE_IS_TEXT(x)		((x) >= 32 || (0x00002600U & (1U << (x))))

static int
check_binary(const unsigned char *buf, size_t len)
{
	int rv;
	for (rv = 1; len--; ++buf) {
		if (BYTE_IS_BINARY(*buf))
			return 1;
		if (BYTE_IS_TEXT(*buf))
			rv = 0;
	}

	return rv;
}

/*
 * Extract to a file descriptor
 */
static int
extract2fd(struct archive *a, char *pathname, int fd)
{
	int cr, text, warn;
	ssize_t len;
	unsigned char *p, *q, *end;

	text = a_opt;
	warn = 0;
	cr = 0;

	/* loop over file contents and write to fd */
	for (int n = 0; ; n++) {
		if (fd != STDOUT_FILENO)
			if (tty && (n % 4) == 0)
				info(" %c\b\b", spinner[(n / 4) % sizeof spinner]);

		len = archive_read_data(a, buffer, sizeof buffer);

		if (len < 0)
			ac(len);

		/* left over CR from previous buffer */
		if (a_opt && cr) {
			if (len == 0 || buffer[0] != '\n')
				if (write(fd, "\r", 1) != 1)
					error("write('%s')", pathname);
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
		 * does not provide access to this field, so we have to
		 * guess by looking for non-ASCII characters in the
		 * buffer.  Hopefully we won't guess wrong.  If we do
		 * guess wrong, we print a warning message later.
		 */
		if (a_opt && n == 0) {
			if (check_binary(buffer, len))
				text = 0;
		}

		/* simple case */
		if (!a_opt || !text) {
			if (write(fd, buffer, len) != len)
				error("write('%s')", pathname);
			continue;
		}

		/* hard case: convert \r\n to \n (sigh...) */
		for (p = buffer; p < end; p = q + 1) {
			for (q = p; q < end; q++) {
				if (!warn && BYTE_IS_BINARY(*q)) {
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
			if (write(fd, p, q - p) != q - p)
				error("write('%s')", pathname);
		}
	}

	return text;
}

/*
 * Extract a regular file.
 */
static void
extract_file(struct archive *a, struct archive_entry *e, char **path)
{
	int mode;
	struct timespec mtime;
	struct stat sb;
	int fd, check, text;
	const char *linkname;
#if defined(HAVE_UTIMENSAT) || defined(HAVE_FUTIMENS)
	struct timespec ts[2];
#endif
#if ((!defined(HAVE_UTIMENSAT) && defined(HAVE_LUTIMES)) || \
    (!defined(HAVE_FUTIMENS) && defined(HAVE_FUTIMES)))
	struct timeval times[2];
#endif

	mode = archive_entry_mode(e) & 0777;
	if (mode == 0)
		mode = 0644;
	mtime.tv_sec = archive_entry_mtime(e);
	mtime.tv_nsec = archive_entry_mtime_nsec(e);

	/* look for existing file of same name */
recheck:
	if (lstat(*path, &sb) == 0) {
		if (u_opt || f_opt) {
			/* check if up-to-date */
			if (S_ISREG(sb.st_mode) && (
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
			    sb.st_mtimespec.tv_sec > mtime.tv_sec ||
			    (sb.st_mtimespec.tv_sec == mtime.tv_sec &&
			    sb.st_mtimespec.tv_nsec >= mtime.tv_nsec)
#elif HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
			    sb.st_mtim.tv_sec > mtime.tv_sec ||
			    (sb.st_mtim.tv_sec == mtime.tv_sec &&
			    sb.st_mtim.tv_nsec >= mtime.tv_nsec)
#elif HAVE_STRUCT_STAT_ST_MTIME_N
			    sb.st_mtime > mtime.tv_sec ||
			    (sb.st_mtime == mtime.tv_sec &&
			    sb.st_mtime_n => mtime.tv_nsec)
#elif HAVE_STRUCT_STAT_ST_MTIME_USEC
			    sb.st_mtime > mtime.tv_sec ||
			    (sb.st_mtime == mtime.tv_sec &&
			    sb.st_mtime_usec => mtime.tv_nsec / 1000)
#else
			    sb.st_mtime > mtime.tv_sec
#endif
			    ))
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

#if defined(HAVE_UTIMENSAT) || defined(HAVE_FUTIMENS)
	ts[0].tv_sec = 0;
	ts[0].tv_nsec = UTIME_NOW;
	ts[1] = mtime;
#endif
#if ((!defined(HAVE_UTIMENSAT) && defined(HAVE_LUTIMES)) || \
    (!defined(HAVE_FUTIMENS) && defined(HAVE_FUTIMES)))
	times[0].tv_sec = 0;
	times[0].tv_usec = -1;
	times[1].tv_sec = mtime.tv_sec;
	times[1].tv_usec = mtime.tv_nsec / 1000;
#endif

	/* process symlinks */
	linkname = archive_entry_symlink(e);
	if (linkname != NULL) {
		if (symlink(linkname, *path) != 0)
			error("symlink('%s')", *path);
		info(" extracting: %s -> %s\n", *path, linkname);
#ifdef HAVE_LCHMOD
		if (lchmod(*path, (mode_t)mode) != 0)
			warning("Cannot set mode for '%s'", *path);
#endif
		/* set access and modification time */
#if defined(HAVE_UTIMENSAT)
		if (utimensat(AT_FDCWD, *path, ts, AT_SYMLINK_NOFOLLOW) != 0)
			warning("utimensat('%s')", *path);
#elif defined(HAVE_LUTIMES)
		gettimeofday(&times[0], NULL);
		if (lutimes(*path, times) != 0)
			warning("lutimes('%s')", *path);
#endif
		return;
	}

	if ((fd = open(*path, O_RDWR|O_CREAT|O_TRUNC, mode)) < 0)
		error("open('%s')", *path);

	info(" extracting: %s", *path);

	text = extract2fd(a, *path, fd);

	if (tty)
		info("  \b\b");
	if (text)
		info(" (text)");
	info("\n");

	/* set access and modification time */
#if defined(HAVE_FUTIMENS)
	if (futimens(fd, ts) != 0)
		error("futimens('%s')", *path);
#elif defined(HAVE_FUTIMES)
	gettimeofday(&times[0], NULL);
	if (futimes(fd, times) != 0)
		error("futimes('%s')", *path);
#endif
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

	if ((pathname = pathdup(archive_entry_pathname(e))) == NULL) {
		warningx("skipping empty or unreadable filename entry");
		ac(archive_read_data_skip(a));
		return;
	}
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
	if (!S_ISDIR(filetype) && !S_ISREG(filetype) && !S_ISLNK(filetype)) {
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

	if ((pathname = pathdup(archive_entry_pathname(e))) == NULL) {
		warningx("skipping empty or unreadable filename entry");
		ac(archive_read_data_skip(a));
		return;
	}
	filetype = archive_entry_filetype(e);

	/* I don't think this can happen in a zipfile.. */
	if (!S_ISDIR(filetype) && !S_ISREG(filetype) && !S_ISLNK(filetype)) {
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

	(void)extract2fd(a, pathname, STDOUT_FILENO);

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
	struct tm *tm;
	const char *pathname;

	mtime = archive_entry_mtime(e);
	tm = localtime(&mtime);
	if (*y_str)
		strftime(buf, sizeof(buf), "%m-%d-%G %R", tm);
	else
		strftime(buf, sizeof(buf), "%m-%d-%g %R", tm);

	pathname = archive_entry_pathname(e);
	if (!pathname)
		pathname = "";
	if (!zipinfo_mode) {
		if (v_opt == 1) {
			printf(" %8ju  %s   %s\n",
			    (uintmax_t)archive_entry_size(e),
			    buf, pathname);
		} else if (v_opt == 2) {
			printf("%8ju  Stored  %7ju   0%%  %s  %08x  %s\n",
			    (uintmax_t)archive_entry_size(e),
			    (uintmax_t)archive_entry_size(e),
			    buf,
			    0U,
			    pathname);
		}
	} else {
		if (Z1_opt)
			printf("%s\n", pathname);
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
 * Callback function for reading passphrase.
 * Originally from cpio.c and passphrase.c, libarchive.
 */
#define PPBUFF_SIZE 1024
static const char *
passphrase_callback(struct archive *a, void *_client_data)
{
	char *p;

	(void)a; /* UNUSED */
	(void)_client_data; /* UNUSED */

	if (passphrase_buf == NULL) {
		passphrase_buf = malloc(PPBUFF_SIZE);
		if (passphrase_buf == NULL) {
			errno = ENOMEM;
			error("malloc()");
		}
	}

	p = lafe_readpassphrase("\nEnter password: ", passphrase_buf,
		PPBUFF_SIZE);

	if (p == NULL && errno != EINTR)
		error("Error reading password");

	return p;
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
	int ret;
	uintmax_t total_size, file_count, error_count;

	if ((a = archive_read_new()) == NULL)
		error("archive_read_new failed");

	ac(archive_read_support_format_zip(a));

	if (O_arg)
		ac(archive_read_set_format_option(a, "zip", "hdrcharset", O_arg));

	if (P_arg)
		archive_read_add_passphrase(a, P_arg);
	else
		archive_read_set_passphrase_callback(a, NULL,
			&passphrase_callback);

	ac(archive_read_open_filename(a, fn, 8192));

	if (!zipinfo_mode) {
		if (!p_opt && !q_opt)
			printf("Archive:  %s\n", fn);
		if (v_opt == 1) {
			printf("  Length     %sDate   Time    Name\n", y_str);
			printf(" --------    %s----   ----    ----\n", y_str);
		} else if (v_opt == 2) {
			printf(" Length   Method    Size  Ratio   %sDate   Time   CRC-32    Name\n", y_str);
			printf("--------  ------  ------- -----   %s----   ----   ------    ----\n", y_str);
		}
	}

	total_size = 0;
	file_count = 0;
	error_count = 0;
	for (;;) {
		ret = archive_read_next_header(a, &e);
		if (ret == ARCHIVE_EOF)
			break;
		ac(ret);
		if (!zipinfo_mode) {
			if (t_opt)
				error_count += test(a, e);
			else if (v_opt)
				list(a, e);
			else if (p_opt || c_opt)
				extract_stdout(a, e);
			else
				extract(a, e);
		} else {
			if (Z1_opt)
				list(a, e);
		}

		total_size += archive_entry_size(e);
		++file_count;
	}

	if (zipinfo_mode) {
		if (v_opt == 1) {
			printf(" --------                   %s-------\n", y_str);
			printf(" %8ju                   %s%ju file%s\n",
			    total_size, y_str, file_count, file_count != 1 ? "s" : "");
		} else if (v_opt == 2) {
			printf("--------          -------  ---                            %s-------\n", y_str);
			printf("%8ju          %7ju   0%%                            %s%ju file%s\n",
			    total_size, total_size, y_str, file_count,
			    file_count != 1 ? "s" : "");
		}
	}

	ac(archive_read_free(a));

	if (passphrase_buf != NULL) {
		memset(passphrase_buf, 0, PPBUFF_SIZE);
		free(passphrase_buf);
	}

	if (t_opt) {
		if (error_count > 0) {
			errorx("%ju checksum error(s) found.", error_count);
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

	fprintf(stderr,
"Usage: unzip [-aCcfjLlnopqtuvyZ1] [{-O|-I} encoding] [-d dir] [-x pattern] [-P password] zipfile\n"
"             [member ...]\n");
	exit(EXIT_FAILURE);
}

static void
version(void)
{
        printf("bsdunzip %s - %s \n",
            BSDUNZIP_VERSION_STRING,
            archive_version_details());
        exit(0);
}

static int
getopts(int argc, char *argv[])
{
	struct bsdunzip *bsdunzip, bsdunzip_storage;
	int opt;
	bsdunzip_optind = 1;

	bsdunzip = &bsdunzip_storage;
	memset(bsdunzip, 0, sizeof(*bsdunzip));

        bsdunzip->argv = argv;
        bsdunzip->argc = argc;

	while ((opt = bsdunzip_getopt(bsdunzip)) != -1) {
		unzip_exclude_mode = 0;
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
			d_arg = bsdunzip->argument;
			break;
		case 'f':
			f_opt = 1;
			break;
		case 'I':
		case 'O':
			O_arg = bsdunzip->argument;
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
		case 'P':
			P_arg = bsdunzip->argument;
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
			add_pattern(&exclude, bsdunzip->argument);
			unzip_exclude_mode = 1;
			break;
		case 'y':
			y_str = "  ";
			break;
		case 'Z':
			zipinfo_mode = 1;
			if (bsdunzip->argument != NULL &&
			    strcmp(bsdunzip->argument, "1") == 0) {
				Z1_opt = 1;
			}
			break;
		case OPTION_VERSION:
			version_opt = 1;
			break;
		case OPTION_NONE:
			break;
		default:
			usage();
		}
		if (opt == OPTION_NONE)
			break;
	}
	return (bsdunzip_optind);
}

int
main(int argc, char *argv[])
{
	const char *zipfile;
	int nopts;

	lafe_setprogname(*argv, "bsdunzip");

#if HAVE_SETLOCALE
	if (setlocale(LC_ALL, "") == NULL)
		lafe_warnc(0, "Failed to set default locale");
#endif

	if (isatty(STDOUT_FILENO))
		tty = 1;

	if (getenv("UNZIP_DEBUG") != NULL)
		unzip_debug = 1;
	for (int i = 0; i < argc; ++i)
		debug("%s%c", argv[i], (i < argc - 1) ? ' ' : '\n');

#ifdef __GLIBC__
	/* Prevent GNU getopt(3) from rearranging options. */
	setenv("POSIXLY_CORRECT", "", 1);
#endif
	/*
	 * Info-ZIP's unzip(1) expects certain options to come before the
	 * zipfile name, and others to come after - though it does not
	 * enforce this.  For simplicity, we accept *all* options both
	 * before and after the zipfile name.
	 */
	nopts = getopts(argc, argv);

	if (version_opt == 1)
		version();

	/*
	 * When more of the zipinfo mode options are implemented, this
	 * will need to change.
	 */
	if (zipinfo_mode && !Z1_opt) {
		printf("Zipinfo mode needs additional options\n");
		exit(EXIT_FAILURE);
	}

	if (argc <= nopts)
		usage();
	zipfile = argv[nopts++];

	if (strcmp(zipfile, "-") == 0)
		zipfile = NULL; /* STDIN */

	unzip_exclude_mode = 0;

	while (nopts < argc && *argv[nopts] != '-')
		add_pattern(&include, argv[nopts++]);

	nopts--; /* fake argv[0] */
	nopts += getopts(argc - nopts, argv + nopts);

	/*
	 * For compatibility with Info-ZIP's unzip(1) we need to treat
	 * non-option arguments following an -x after the zipfile as
	 * exclude list members.
	 */
	if (unzip_exclude_mode) {
		while (nopts < argc && *argv[nopts] != '-')
			add_pattern(&exclude, argv[nopts++]);
		nopts--; /* fake argv[0] */
		nopts += getopts(argc - nopts, argv + nopts);
	}

	/* There may be residual arguments if we encountered -- */
	while (nopts < argc)
		add_pattern(&include, argv[nopts++]);

	if (n_opt + o_opt + u_opt > 1)
		errorx("-n, -o and -u are contradictory");

	unzip(zipfile);

	exit(EXIT_SUCCESS);
}

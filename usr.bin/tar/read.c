/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "bsdtar.h"

static void	list_item_verbose(struct bsdtar *, FILE *,
		    struct archive_entry *);
static void	read_archive(struct bsdtar *bsdtar, char mode);

void
tar_mode_t(struct bsdtar *bsdtar)
{
	read_archive(bsdtar, 't');
}

void
tar_mode_x(struct bsdtar *bsdtar)
{
	read_archive(bsdtar, 'x');
}

/*
 * Handle 'x' and 't' modes.
 */
static void
read_archive(struct bsdtar *bsdtar, char mode)
{
	FILE			 *out;
	struct archive		 *a;
	struct archive_entry	 *entry;
	const struct stat	 *st;
	int			  r;

	while (*bsdtar->argv) {
		include(bsdtar, *bsdtar->argv);
		bsdtar->argv++;
	}

	if (bsdtar->names_from_file != NULL)
		include_from_file(bsdtar, bsdtar->names_from_file);

	a = archive_read_new();
	if (bsdtar->compress_program != NULL)
		archive_read_support_compression_program(a, bsdtar->compress_program);
	else
		archive_read_support_compression_all(a);
	archive_read_support_format_all(a);
	if (archive_read_open_file(a, bsdtar->filename,
	    bsdtar->bytes_per_block != 0 ? bsdtar->bytes_per_block :
	    DEFAULT_BYTES_PER_BLOCK))
		bsdtar_errc(bsdtar, 1, 0, "Error opening archive: %s",
		    archive_error_string(a));

	do_chdir(bsdtar);
	for (;;) {
		/* Support --fast-read option */
		if (bsdtar->option_fast_read &&
		    unmatched_inclusions(bsdtar) == 0)
			break;

		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r < ARCHIVE_OK)
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
		if (r <= ARCHIVE_WARN)
			bsdtar->return_value = 1;
		if (r == ARCHIVE_RETRY) {
			/* Retryable error: try again */
			bsdtar_warnc(bsdtar, 0, "Retrying...");
			continue;
		}
		if (r == ARCHIVE_FATAL)
			break;

		/*
		 * Exclude entries that are too old.
		 */
		st = archive_entry_stat(entry);
		if (bsdtar->newer_ctime_sec > 0) {
			if (st->st_ctime < bsdtar->newer_ctime_sec)
				continue; /* Too old, skip it. */
			if (st->st_ctime == bsdtar->newer_ctime_sec
			    && ARCHIVE_STAT_CTIME_NANOS(st)
			    <= bsdtar->newer_ctime_nsec)
				continue; /* Too old, skip it. */
		}
		if (bsdtar->newer_mtime_sec > 0) {
			if (st->st_mtime < bsdtar->newer_mtime_sec)
				continue; /* Too old, skip it. */
			if (st->st_mtime == bsdtar->newer_mtime_sec
			    && ARCHIVE_STAT_MTIME_NANOS(st)
			    <= bsdtar->newer_mtime_nsec)
				continue; /* Too old, skip it. */
		}

		/*
		 * Note that pattern exclusions are checked before
		 * pathname rewrites are handled.  This gives more
		 * control over exclusions, since rewrites always lose
		 * information.  (For example, consider a rewrite
		 * s/foo[0-9]/foo/.  If we check exclusions after the
		 * rewrite, there would be no way to exclude foo1/bar
		 * while allowing foo2/bar.)
		 */
		if (excluded(bsdtar, archive_entry_pathname(entry)))
			continue; /* Excluded by a pattern test. */

		/*
		 * Modify the pathname as requested by the user.  We
		 * do this for -t as well to give users a way to
		 * preview the effects of their rewrites.  We also do
		 * this before extraction security checks (including
		 * leading '/' removal).  Note that some rewrite
		 * failures prevent extraction.
		 */
		if (edit_pathname(bsdtar, entry))
			continue; /* Excluded by a rewrite failure. */

		if (mode == 't') {
			/* Perversely, gtar uses -O to mean "send to stderr"
			 * when used with -t. */
			out = bsdtar->option_stdout ? stderr : stdout;

			if (bsdtar->verbose < 2)
				safe_fprintf(out, "%s",
				    archive_entry_pathname(entry));
			else
				list_item_verbose(bsdtar, out, entry);
			fflush(out);
			r = archive_read_data_skip(a);
			if (r == ARCHIVE_WARN) {
				fprintf(out, "\n");
				bsdtar_warnc(bsdtar, 0, "%s",
				    archive_error_string(a));
			}
			if (r == ARCHIVE_RETRY) {
				fprintf(out, "\n");
				bsdtar_warnc(bsdtar, 0, "%s",
				    archive_error_string(a));
			}
			if (r == ARCHIVE_FATAL) {
				fprintf(out, "\n");
				bsdtar_warnc(bsdtar, 0, "%s",
				    archive_error_string(a));
				bsdtar->return_value = 1;
				break;
			}
			fprintf(out, "\n");
		} else {
			if (bsdtar->option_interactive &&
			    !yes("extract '%s'", archive_entry_pathname(entry)))
				continue;

			/*
			 * Format here is from SUSv2, including the
			 * deferred '\n'.
			 */
			if (bsdtar->verbose) {
				safe_fprintf(stderr, "x %s",
				    archive_entry_pathname(entry));
				fflush(stderr);
			}
			if (bsdtar->option_stdout)
				r = archive_read_data_into_fd(a, 1);
			else
				r = archive_read_extract(a, entry,
				    bsdtar->extract_flags);
			if (r != ARCHIVE_OK) {
				if (!bsdtar->verbose)
					safe_fprintf(stderr, "%s",
					    archive_entry_pathname(entry));
				safe_fprintf(stderr, ": %s",
				    archive_error_string(a));
				if (!bsdtar->verbose)
					fprintf(stderr, "\n");
				bsdtar->return_value = 1;
			}
			if (bsdtar->verbose)
				fprintf(stderr, "\n");
			if (r == ARCHIVE_FATAL)
				break;
		}
	}

	if (bsdtar->verbose > 2)
		fprintf(stdout, "Archive Format: %s,  Compression: %s\n",
		    archive_format_name(a), archive_compression_name(a));

	archive_read_finish(a);
}


/*
 * Display information about the current file.
 *
 * The format here roughly duplicates the output of 'ls -l'.
 * This is based on SUSv2, where 'tar tv' is documented as
 * listing additional information in an "unspecified format,"
 * and 'pax -l' is documented as using the same format as 'ls -l'.
 */
static void
list_item_verbose(struct bsdtar *bsdtar, FILE *out, struct archive_entry *entry)
{
	const struct stat	*st;
	char			 tmp[100];
	size_t			 w;
	const char		*p;
	const char		*fmt;
	time_t			 tim;
	static time_t		 now;

	st = archive_entry_stat(entry);

	/*
	 * We avoid collecting the entire list in memory at once by
	 * listing things as we see them.  However, that also means we can't
	 * just pre-compute the field widths.  Instead, we start with guesses
	 * and just widen them as necessary.  These numbers are completely
	 * arbitrary.
	 */
	if (!bsdtar->u_width) {
		bsdtar->u_width = 6;
		bsdtar->gs_width = 13;
	}
	if (!now)
		time(&now);
	fprintf(out, "%s %d ",
	    archive_entry_strmode(entry),
	    (int)(st->st_nlink));

	/* Use uname if it's present, else uid. */
	p = archive_entry_uname(entry);
	if ((p == NULL) || (*p == '\0')) {
		sprintf(tmp, "%lu ", (unsigned long)st->st_uid);
		p = tmp;
	}
	w = strlen(p);
	if (w > bsdtar->u_width)
		bsdtar->u_width = w;
	fprintf(out, "%-*s ", (int)bsdtar->u_width, p);

	/* Use gname if it's present, else gid. */
	p = archive_entry_gname(entry);
	if (p != NULL && p[0] != '\0') {
		fprintf(out, "%s", p);
		w = strlen(p);
	} else {
		sprintf(tmp, "%lu", (unsigned long)st->st_gid);
		w = strlen(tmp);
		fprintf(out, "%s", tmp);
	}

	/*
	 * Print device number or file size, right-aligned so as to make
	 * total width of group and devnum/filesize fields be gs_width.
	 * If gs_width is too small, grow it.
	 */
	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
		sprintf(tmp, "%lu,%lu",
		    (unsigned long)major(st->st_rdev),
		    (unsigned long)minor(st->st_rdev)); /* ls(1) also casts here. */
	} else {
		/*
		 * Note the use of platform-dependent macros to format
		 * the filesize here.  We need the format string and the
		 * corresponding type for the cast.
		 */
		sprintf(tmp, BSDTAR_FILESIZE_PRINTF,
		    (BSDTAR_FILESIZE_TYPE)st->st_size);
	}
	if (w + strlen(tmp) >= bsdtar->gs_width)
		bsdtar->gs_width = w+strlen(tmp)+1;
	fprintf(out, "%*s", (int)(bsdtar->gs_width - w), tmp);

	/* Format the time using 'ls -l' conventions. */
	tim = (time_t)st->st_mtime;
	if (abs(tim - now) > (365/2)*86400)
		fmt = bsdtar->day_first ? "%e %b  %Y" : "%b %e  %Y";
	else
		fmt = bsdtar->day_first ? "%e %b %R" : "%b %e %R";
	strftime(tmp, sizeof(tmp), fmt, localtime(&tim));
	fprintf(out, " %s ", tmp);
	safe_fprintf(out, "%s", archive_entry_pathname(entry));

	/* Extra information for links. */
	if (archive_entry_hardlink(entry)) /* Hard link */
		safe_fprintf(out, " link to %s",
		    archive_entry_hardlink(entry));
	else if (S_ISLNK(st->st_mode)) /* Symbolic link */
		safe_fprintf(out, " -> %s", archive_entry_symlink(entry));
}

/*-
 * Copyright (c) 2003-2004 Tim Kientzle
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <archive.h>
#include <archive_entry.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bsdtar.h"

static void	cleanup_security(struct bsdtar *);
static void	list_item_verbose(struct bsdtar *, FILE *,
		    struct archive_entry *);
static void	read_archive(struct bsdtar *bsdtar, char mode);
static int	security_problem(struct bsdtar *, struct archive_entry *);

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
	int			  r;

	while (*bsdtar->argv) {
		include(bsdtar, *bsdtar->argv);
		bsdtar->argv++;
	}

	if (bsdtar->names_from_file != NULL)
		include_from_file(bsdtar, bsdtar->names_from_file);

	a = archive_read_new();
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
		if (r == ARCHIVE_WARN)
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
		if (r == ARCHIVE_FATAL) {
			bsdtar->return_value = 1;
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
			break;
		}
		if (r == ARCHIVE_RETRY) {
			/* Retryable error: try again */
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
			bsdtar_warnc(bsdtar, 0, "Retrying...");
			continue;
		}

		if (excluded(bsdtar, archive_entry_pathname(entry)))
			continue;

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
				break;
			}
			fprintf(out, "\n");
		} else {
			if (bsdtar->option_interactive &&
			    !yes("extract '%s'", archive_entry_pathname(entry)))
				continue;

			if (security_problem(bsdtar, entry))
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
			if (bsdtar->option_stdout) {
				/* TODO: Catch/recover any errors here. */
				archive_read_data_into_fd(a, 1);
			} else if (archive_read_extract(a, entry,
				       bsdtar->extract_flags)) {
				if (!bsdtar->verbose)
					safe_fprintf(stderr, "%s",
					    archive_entry_pathname(entry));
				safe_fprintf(stderr, ": %s",
				    archive_error_string(a));
				if (!bsdtar->verbose)
					fprintf(stderr, "\n");
				/*
				 * TODO: Decide how to handle
				 * extraction error... <sigh>
				 */
				bsdtar->return_value = 1;
			}
			if (bsdtar->verbose)
				fprintf(stderr, "\n");
		}
	}

	if (bsdtar->verbose > 2)
		fprintf(stdout, "Archive Format: %s,  Compression: %s\n",
		    archive_format_name(a), archive_compression_name(a));

	archive_read_finish(a);
	cleanup_security(bsdtar);
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
	bsdtar_strmode(entry, tmp);
	fprintf(out, "%s %d ", tmp, st->st_nlink);

	/* Use uname if it's present, else uid. */
	p = archive_entry_uname(entry);
	if ((p == NULL) || (*p == '\0')) {
		sprintf(tmp, "%d ", st->st_uid);
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
		sprintf(tmp, "%d", st->st_gid);
		w = strlen(tmp);
		fprintf(out, "%s", tmp);
	}

	/*
	 * Print device number or file size, right-aligned so as to make
	 * total width of group and devnum/filesize fields be gs_width.
	 * If gs_width is too small, grow it.
	 */
	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
		sprintf(tmp, "%d,%u",
		    major(st->st_rdev),
		    (unsigned)minor(st->st_rdev)); /* ls(1) also casts here. */
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

struct security {
	char	*path;
	size_t	 path_size;
};

/*
 * Check for a variety of security issues.  Fix what we can here,
 * generate warnings as appropriate, return non-zero to prevent
 * this entry from being extracted.
 */
static int
security_problem(struct bsdtar *bsdtar, struct archive_entry *entry)
{
	struct stat st;
	const char *name, *pn;
	char *p;
	int r;

	/* -P option forces us to just accept all pathnames. */
	if (bsdtar->option_absolute_paths)
		return (0);

	/* Strip leading '/'. */
	name = archive_entry_pathname(entry);
	if (name[0] == '/') {
		/* Generate a warning the first time this happens. */
		if (!bsdtar->warned_lead_slash) {
			bsdtar_warnc(bsdtar, 0,
			    "Removing leading '/' from member names");
			bsdtar->warned_lead_slash = 1;
		}
		while (name[0] == '/')
			name++;
		archive_entry_set_pathname(entry, name);
	}

	/* Reject any archive entry with '..' as a path element. */
	pn = name;
	while (pn != NULL && pn[0] != '\0') {
		if (pn[0] == '.' && pn[1] == '.' &&
		    (pn[2] == '\0' || pn[2] == '/')) {
			bsdtar_warnc(bsdtar, 0,
			    "Skipping pathname containing ..");
			bsdtar->return_value = 1;
			return (1);
		}
		pn = strchr(pn, '/');
		if (pn != NULL)
			pn++;
	}

	/*
	 * Gaurd against symlink tricks.  Reject any archive entry whose
	 * destination would be altered by a symlink.
	 */
	/* XXX TODO: Make this faster by comparing current path to
	 * prefix of last successful check to avoid duplicate lstat()
	 * calls. XXX */
	pn = name;
	if (bsdtar->security == NULL) {
		bsdtar->security = malloc(sizeof(*bsdtar->security));
		if (bsdtar->security == NULL)
			bsdtar_errc(bsdtar, 1, errno, "No Memory");
		bsdtar->security->path_size = MAXPATHLEN + 1;
		bsdtar->security->path = malloc(bsdtar->security->path_size);
		if (bsdtar->security->path == NULL)
			bsdtar_errc(bsdtar, 1, errno, "No Memory");
	}
	if (strlen(name) >= bsdtar->security->path_size) {
		free(bsdtar->security->path);
		while (strlen(name) >= bsdtar->security->path_size)
			bsdtar->security->path_size *= 2;
		bsdtar->security->path = malloc(bsdtar->security->path_size);
		if (bsdtar->security->path == NULL)
			bsdtar_errc(bsdtar, 1, errno, "No Memory");
	}
	p = bsdtar->security->path;
	while (pn != NULL && pn[0] != '\0') {
		*p++ = *pn++;
		while (*pn != '\0' && *pn != '/')
			*p++ = *pn++;
		p[0] = '\0';
		r = lstat(bsdtar->security->path, &st);
		if (r != 0) {
			if (errno == ENOENT)
				break;
		} else if (S_ISLNK(st.st_mode)) {
			if (pn[0] == '\0') {
				/*
				 * Last element is symlink; remove it
				 * so we can overwrite it with the
				 * item being extracted.
				 */
				if (!S_ISLNK(archive_entry_mode(entry))) {
					/*
					 * Warn only if the symlink is being
					 * replaced with a non-symlink.
					 */
					bsdtar_warnc(bsdtar, 0,
					    "Removing symlink %s",
					    bsdtar->security->path);
				}
				if (unlink(bsdtar->security->path))
					bsdtar_errc(bsdtar, 1, errno,
					    "Unlink failed");
				/* Symlink gone.  No more problem! */
				return (0);
			} else if (bsdtar->option_unlink_first) {
				/* User asked us to remove problems. */
				if (unlink(bsdtar->security->path))
					bsdtar_errc(bsdtar, 1, errno,
					    "Unlink failed");
			} else {
				bsdtar_warnc(bsdtar, 0,
				    "Cannot extract %s through symlink %s",
				    name, bsdtar->security->path);
				bsdtar->return_value = 1;
				return (1);
			}
		}
	}

	return (0);
}

static void
cleanup_security(struct bsdtar *bsdtar)
{
	if (bsdtar->security != NULL) {
		free(bsdtar->security->path);
		free(bsdtar->security);
	}
}

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
static void	list_item_verbose(struct bsdtar *, struct archive_entry *);
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
void
read_archive(struct bsdtar *bsdtar, char mode)
{
	struct archive		 *a;
	struct archive_entry	 *entry;
	int			  format;
	int			  r;

        while (*bsdtar->argv) {
		include(bsdtar, *bsdtar->argv);
		bsdtar->argv++;
        }

	format = -1;

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_all(a);
	if (archive_read_open_file(a, bsdtar->filename, bsdtar->bytes_per_block))
		bsdtar_errc(1, 0, "Error opening archive: %s",
		    archive_error_string(a));

	if (bsdtar->verbose > 2)
		fprintf(stdout, "Compression: %s\n",
		    archive_compression_name(a));

	if (bsdtar->start_dir != NULL && chdir(bsdtar->start_dir))
		bsdtar_errc(1, errno, "chdir(%s) failed", bsdtar->start_dir);

	for (;;) {
		/* Support --fast-read option */
		if (bsdtar->option_fast_read &&
		    unmatched_inclusions(bsdtar) == 0)
			break;

		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r == ARCHIVE_WARN)
			bsdtar_warnc(0, "%s", archive_error_string(a));
		if (r == ARCHIVE_FATAL) {
			bsdtar_warnc(0, "%s", archive_error_string(a));
			break;
		}
		if (r == ARCHIVE_RETRY) {
			/* Retryable error: try again */
			bsdtar_warnc(0, "%s", archive_error_string(a));
			bsdtar_warnc(0, "Retrying...");
			continue;
		}

		if (bsdtar->verbose > 2 && format != archive_format(a)) {
			format = archive_format(a);
			fprintf(stdout, "Archive Format: %s\n",
			    archive_format_name(a));
		}

		if (excluded(bsdtar, archive_entry_pathname(entry)))
			continue;

		if (mode == 't') {
			if (bsdtar->verbose < 2)
				safe_fprintf(stdout, "%s",
				    archive_entry_pathname(entry));
			else
				list_item_verbose(bsdtar, entry);
			fflush(stdout);
			switch (archive_read_data_skip(a)) {
			case ARCHIVE_OK:
				break;
			case ARCHIVE_WARN:
			case ARCHIVE_RETRY:
				fprintf(stdout, "\n");
				bsdtar_warnc(0, "%s", archive_error_string(a));
				break;
			case ARCHIVE_FATAL:
				fprintf(stdout, "\n");
				bsdtar_errc(1, 0, "%s",
				    archive_error_string(a));
				break;
			}
			fprintf(stdout, "\n");
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
			}
			if (bsdtar->verbose)
				fprintf(stderr, "\n");
		}
	}
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
list_item_verbose(struct bsdtar *bsdtar, struct archive_entry *entry)
{
	FILE			*out = stdout;
	const struct stat	*st;
	char			 tmp[100];
	size_t			 w;
	const char		*p;
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
	w = 0;
	p = archive_entry_uname(entry);
	if (p && *p) {
		sprintf(tmp, "%s ", p);
	} else {
		sprintf(tmp, "%d ", st->st_uid);
	}
	w = strlen(tmp);
	if (w > bsdtar->u_width)
		bsdtar->u_width = w;
	fprintf(out, "%-*s", (int)bsdtar->u_width, tmp);

	/* Use gname if it's present, else gid. */
	w = 0;
	p = archive_entry_gname(entry);
	if (p && *p) {
		fprintf(out, "%s", p);
		w += strlen(p);
	} else {
		sprintf(tmp, "%d", st->st_gid);
		w += strlen(tmp);
		fprintf(out, "%s", tmp);
	}

	/*
	 * Print device number or file size, right-aligned so as to make
	 * total width of group and devnum/filesize fields be gs_width.
	 * If gs_width is too small, grow it.
	 */
	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
		sprintf(tmp, "%u,%u", major(st->st_rdev), minor(st->st_rdev));
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
	if (tim < now - 6*30*24*60*60  || tim > now + 6*30*24*60*60)
		strftime(tmp, sizeof(tmp), "%b %e %Y", localtime(&tim));
	else
		strftime(tmp, sizeof(tmp), "%b %e %R", localtime(&tim));
	safe_fprintf(out, " %s %s", tmp, archive_entry_pathname(entry));

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
int
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
		/* XXX gtar generates a warning the first time this happens. */
		name++;
		archive_entry_set_pathname(entry, name);
	}

	/* Reject any archive entry with '..' as a path element. */
	pn = name;
	while (pn != NULL && pn[0] != '\0') {
		if (pn[0] == '.' && pn[1] == '.' &&
		    (pn[2] == '\0' || pn[2] == '/')) {
			bsdtar_warnc(0,"Skipping pathname containing ..");
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
		bsdtar->security->path_size = MAXPATHLEN + 1;
		bsdtar->security->path = malloc(bsdtar->security->path_size);
	}
	if (strlen(name) >= bsdtar->security->path_size) {
		free(bsdtar->security->path);
		while (strlen(name) >= bsdtar->security->path_size)
			bsdtar->security->path_size *= 2;
		bsdtar->security->path = malloc(bsdtar->security->path_size);
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
			if (*pn == '\0') {
				/* Last element is symlink; just remove it. */
				unlink(bsdtar->security->path);
			} else if (bsdtar->option_unlink_first) {
				/* User asked us to remove problems. */
				unlink(bsdtar->security->path);
			} else {
				bsdtar_warnc(0,
				    "Cannot extract %s through symlink %s",
				    name, bsdtar->security->path);
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
		if (bsdtar->security->path != NULL)
			free(bsdtar->security->path);
		free(bsdtar->security);
	}
}

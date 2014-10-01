/*-
 * Copyright (c) 2011 Nathan Whitehorn
 * Copyright (c) 2014 Devin Teske <dteske@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <archive.h>
#include <ctype.h>
#include <dialog.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Data to process */
static char *distdir = NULL;
struct file_node {
	char	*path;
	char	*name;
	int	length;
	struct file_node *next;
};
static struct file_node *dists = NULL;

/* Function prototypes */
static int	count_files(const char *file);
static int	extract_files(int nfiles, struct file_node *files);

#if __FreeBSD_version <= 1000008 /* r232154: bump for libarchive update */
#define archive_read_support_filter_all(x) \
	archive_read_support_compression_all(x)
#endif

#define _errx(...) (end_dialog(), errx(__VA_ARGS__))

int
main(void)
{
	char *chrootdir;
	char *distributions;
	int ndists = 0;
	int retval;
	size_t file_node_size = sizeof(struct file_node);
	size_t span;
	struct file_node *dist = dists;
	char error[PATH_MAX + 512];

	if ((distributions = getenv("DISTRIBUTIONS")) == NULL)
		errx(EXIT_FAILURE, "DISTRIBUTIONS variable is not set");
	if ((distdir = getenv("BSDINSTALL_DISTDIR")) == NULL)
		distdir = __DECONST(char *, "");

	/* Initialize dialog(3) */
	init_dialog(stdin, stdout);
	dialog_vars.backtitle = __DECONST(char *, "FreeBSD Installer");
	dlg_put_backtitle();

	dialog_msgbox("",
	    "Checking distribution archives.\nPlease wait...", 4, 35, FALSE);

	/*
	 * Parse $DISTRIBUTIONS into linked-list
	 */
	while (*distributions != '\0') {
		span = strcspn(distributions, "\t\n\v\f\r ");
		if (span < 1) { /* currently on whitespace */
			distributions++;
			continue;
		}
		ndists++;

		/* Allocate a new struct for the distribution */
		if (dist == NULL) {
			if ((dist = calloc(1, file_node_size)) == NULL)
				_errx(EXIT_FAILURE, "Out of memory!");
			dists = dist;
		} else {
			dist->next = calloc(1, file_node_size);
			if (dist->next == NULL)
				_errx(EXIT_FAILURE, "Out of memory!");
			dist = dist->next;
		}

		/* Set path */
		if ((dist->path = malloc(span + 1)) == NULL)
			_errx(EXIT_FAILURE, "Out of memory!");
		snprintf(dist->path, span + 1, "%s", distributions);
		dist->path[span] = '\0';

		/* Set display name */
		dist->name = strrchr(dist->path, '/');
		if (dist->name == NULL)
			dist->name = dist->path;

		/* Set initial length in files (-1 == error) */
		dist->length = count_files(dist->path);
		if (dist->length < 0) {
			end_dialog();
			return (EXIT_FAILURE);
		}

		distributions += span;
	}

	/* Optionally chdir(2) into $BSDINSTALL_CHROOT */
	chrootdir = getenv("BSDINSTALL_CHROOT");
	if (chrootdir != NULL && chdir(chrootdir) != 0) {
		snprintf(error, sizeof(error),
		    "Could not change to directory %s: %s\n",
		    chrootdir, strerror(errno));
		dialog_msgbox("Error", error, 0, 0, TRUE);
		end_dialog();
		return (EXIT_FAILURE);
	}

	retval = extract_files(ndists, dists);

	end_dialog();

	while ((dist = dists) != NULL) {
		dists = dist->next;
		if (dist->path != NULL)
			free(dist->path);
		free(dist);
	}

	return (retval);
}

/*
 * Returns number of files in archive file. Parses $BSDINSTALL_DISTDIR/MANIFEST
 * if it exists, otherwise uses archive(3) to read the archive file.
 */
static int
count_files(const char *file)
{
	static FILE *manifest = NULL;
	char *p;
	int file_count;
	int retval;
	size_t span;
	struct archive *archive;
	struct archive_entry *entry;
	char line[512];
	char path[PATH_MAX];
	char errormsg[PATH_MAX + 512];

	if (manifest == NULL) {
		snprintf(path, sizeof(path), "%s/MANIFEST", distdir);
		manifest = fopen(path, "r");
	}

	if (manifest != NULL) {
		rewind(manifest);
		while (fgets(line, sizeof(line), manifest) != NULL) {
			p = &line[0];
			span = strcspn(p, "\t") ;
			if (span < 1 || strncmp(p, file, span) != 0)
				continue;

			/*
			 * We're at the right manifest line. The file count is
			 * in the third element
			 */
			span = strcspn(p += span + (*p != '\0' ? 1 : 0), "\t");
			span = strcspn(p += span + (*p != '\0' ? 1 : 0), "\t");
			if (span > 0) {
				file_count = (int)strtol(p, (char **)NULL, 10);
				if (file_count == 0 && errno == EINVAL)
					continue;
				return (file_count);
			}
		}
	}

	/*
	 * Either no manifest, or manifest didn't mention this archive.
	 * Use archive(3) to read the archive, counting files within.
	 */
	if ((archive = archive_read_new()) == NULL) {
		snprintf(errormsg, sizeof(errormsg),
		    "Error: %s\n", archive_error_string(NULL));
		dialog_msgbox("Extract Error", errormsg, 0, 0, TRUE);
		return (-1);
	}
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	snprintf(path, sizeof(path), "%s/%s", distdir, file);
	retval = archive_read_open_filename(archive, path, 4096);
	if (retval != ARCHIVE_OK) {
		snprintf(errormsg, sizeof(errormsg),
		    "Error while extracting %s: %s\n", file,
		    archive_error_string(archive));
		dialog_msgbox("Extract Error", errormsg, 0, 0, TRUE);
		return (-1);
	}

	file_count = 0;
	while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
		file_count++;
	archive_read_free(archive);

	return (file_count);
}

static int
extract_files(int nfiles, struct file_node *files)
{
	int archive_file;
	int archive_files[nfiles];
	int current_files = 0;
	int i;
	int last_progress;
	int progress = 0;
	int retval;
	int total_files = 0;
	struct archive *archive;
	struct archive_entry *entry;
	struct file_node *file;
	char status[8];
	static char title[] = "Archive Extraction";
	static char pprompt[] = "Extracting distribution files...\n";
	char path[PATH_MAX];
	char errormsg[PATH_MAX + 512];
	const char *items[nfiles*2];

	/* Make the transfer list for dialog */
	i = 0;
	for (file = files; file != NULL; file = file->next) {
		items[i*2] = file->name;
		items[i*2 + 1] = "Pending";
		archive_files[i] = file->length;

		total_files += file->length;
		i++;
	}

	i = 0;
	for (file = files; file != NULL; file = file->next) {
		if ((archive = archive_read_new()) == NULL) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error: %s\n", archive_error_string(NULL));
			dialog_msgbox("Extract Error", errormsg, 0, 0, TRUE);
			return (EXIT_FAILURE);
		}
		archive_read_support_format_all(archive);
		archive_read_support_filter_all(archive);
		snprintf(path, sizeof(path), "%s/%s", distdir, file->path);
		retval = archive_read_open_filename(archive, path, 4096);
		if (retval != 0) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error opening %s: %s\n", file->name,
			    archive_error_string(archive));
			dialog_msgbox("Extract Error", errormsg, 0, 0, TRUE);
			return (EXIT_FAILURE);
		}

		items[i*2 + 1] = "In Progress";
		archive_file = 0;

		dialog_mixedgauge(title, pprompt, 0, 0, progress, nfiles,
		    __DECONST(char **, items));

		while ((retval = archive_read_next_header(archive, &entry)) ==
		    ARCHIVE_OK) {
			last_progress = progress;
			progress = (current_files*100)/total_files; 

			snprintf(status, sizeof(status), "-%d",
			    (archive_file*100)/archive_files[i]);
			items[i*2 + 1] = status;

			if (progress > last_progress)
				dialog_mixedgauge(title, pprompt, 0, 0,
				    progress, nfiles,
				    __DECONST(char **, items));

			retval = archive_read_extract(archive, entry,
			    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_OWNER |
			    ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_XATTR | ARCHIVE_EXTRACT_FFLAGS);

			if (retval != ARCHIVE_OK)
				break;

			archive_file++;
			current_files++;
		}

		items[i*2 + 1] = "Done";

		if (retval != ARCHIVE_EOF) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error while extracting %s: %s\n", items[i*2],
			    archive_error_string(archive));
			items[i*2 + 1] = "Failed";
			dialog_msgbox("Extract Error", errormsg, 0, 0, TRUE);
			return (retval);
		}

		progress = (current_files*100)/total_files; 
		dialog_mixedgauge(title, pprompt, 0, 0, progress, nfiles,
		    __DECONST(char **, items));

		archive_read_free(archive);
		i++;
	}

	return (EXIT_SUCCESS);
}

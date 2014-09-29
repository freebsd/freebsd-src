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

static int	count_files(const char *file);
static int	extract_files(int nfiles, const char **files);

int
main(void)
{
	const char **dists;
	char *diststring;
	int i;
	int ndists = 0;
	int retval;
	char error[PATH_MAX + 512];

	if (getenv("DISTRIBUTIONS") == NULL)
		errx(EXIT_FAILURE, "DISTRIBUTIONS variable is not set");

	diststring = strdup(getenv("DISTRIBUTIONS"));
	for (i = 0; diststring[i] != 0; i++)
		if (isspace(diststring[i]) && !isspace(diststring[i+1]))
			ndists++;
	ndists++; /* Last one */

	dists = calloc(ndists, sizeof(const char *));
	if (dists == NULL) {
		free(diststring);
		errx(EXIT_FAILURE, "Out of memory!");
	}

	for (i = 0; i < ndists; i++)
		dists[i] = strsep(&diststring, " \t");

	init_dialog(stdin, stdout);
	dialog_vars.backtitle = __DECONST(char *, "FreeBSD Installer");
	dlg_put_backtitle();

	if (chdir(getenv("BSDINSTALL_CHROOT")) != 0) {
		snprintf(error, sizeof(error),
		    "Could could change to directory %s: %s\n",
		    getenv("BSDINSTALL_DISTDIR"), strerror(errno));
		dialog_msgbox("Error", error, 0, 0, TRUE);
		end_dialog();
		return (EXIT_FAILURE);
	}

	retval = extract_files(ndists, dists);

	end_dialog();

	free(diststring);
	free(dists);

	return (retval);
}

static int
count_files(const char *file)
{
	static FILE *manifest = NULL;
	char *tok1;
	char *tok2;
	int file_count;
	int retval;
	struct archive *archive;
	struct archive_entry *entry;
	char line[512];
	char path[PATH_MAX];
	char errormsg[PATH_MAX + 512];

	if (manifest == NULL) {
		snprintf(path, sizeof(path), "%s/MANIFEST",
		    getenv("BSDINSTALL_DISTDIR"));
		manifest = fopen(path, "r");
	}

	if (manifest != NULL) {
		rewind(manifest);
		while (fgets(line, sizeof(line), manifest) != NULL) {
			tok2 = line;
			tok1 = strsep(&tok2, "\t");
			if (tok1 == NULL || strcmp(tok1, file) != 0)
				continue;

			/*
			 * We're at the right manifest line. The file count is
			 * in the third element
			 */
			tok1 = strsep(&tok2, "\t");
			tok1 = strsep(&tok2, "\t");
			if (tok1 != NULL)
				return atoi(tok1);
		}
	}

	/* Either we didn't have a manifest, or this archive wasn't there */
	archive = archive_read_new();
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	snprintf(path, sizeof(path), "%s/%s", getenv("BSDINSTALL_DISTDIR"),
	    file);
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
extract_files(int nfiles, const char **files)
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
	char status[8];
	char path[PATH_MAX];
	char errormsg[PATH_MAX + 512];
	const char *items[nfiles*2];

	/* Make the transfer list for dialog */
	for (i = 0; i < nfiles; i++) {
		items[i*2] = strrchr(files[i], '/');
		if (items[i*2] != NULL)
			items[i*2]++;
		else
			items[i*2] = files[i];
		items[i*2 + 1] = "Pending";
	}

	dialog_msgbox("",
	    "Checking distribution archives.\nPlease wait...", 0, 0, FALSE);

	/* Count all the files */
	for (i = 0; i < nfiles; i++) {
		archive_files[i] = count_files(files[i]);
		if (archive_files[i] < 0)
			return (-1);
		total_files += archive_files[i];
	}

	for (i = 0; i < nfiles; i++) {
		archive = archive_read_new();
		archive_read_support_format_all(archive);
		archive_read_support_filter_all(archive);
		snprintf(path, sizeof(path), "%s/%s",
		    getenv("BSDINSTALL_DISTDIR"), files[i]);
		retval = archive_read_open_filename(archive, path, 4096);

		items[i*2 + 1] = "In Progress";
		archive_file = 0;

		while ((retval = archive_read_next_header(archive, &entry)) ==
		    ARCHIVE_OK) {
			last_progress = progress;
			progress = (current_files*100)/total_files; 

			snprintf(status, sizeof(status), "-%d",
			    (archive_file*100)/archive_files[i]);
			items[i*2 + 1] = status;

			if (progress > last_progress)
				dialog_mixedgauge("Archive Extraction",
				    "Extracting distribution files...", 0, 0,
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
			dialog_msgbox("Extract Error", errormsg, 0, 0,
			    TRUE);
			return (retval);
		}

		archive_read_free(archive);
	}

	return (EXIT_SUCCESS);
}

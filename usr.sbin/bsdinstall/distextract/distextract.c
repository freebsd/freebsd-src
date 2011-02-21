/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <archive.h>
#include <dialog.h>

static int extract_files(int nfiles, const char **files);

int
main(void)
{
	char *diststring = strdup(getenv("DISTRIBUTIONS"));
	const char **dists;
	int i, retval, ndists = 0;
	for (i = 0; diststring[i] != 0; i++)
		if (isspace(diststring[i]) && !isspace(diststring[i+1]))
			ndists++;
	ndists++; /* Last one */

	dists = calloc(ndists, sizeof(const char *));
	if (dists == NULL) {
		fprintf(stderr, "Out of memory!\n");
		return (1);
	}

	for (i = 0; i < ndists; i++)
		dists[i] = strsep(&diststring, " \t");

	init_dialog(stdin, stdout);
	dialog_vars.backtitle = __DECONST(char *, "FreeBSD Installer");
	dlg_put_backtitle();

	if (chdir(getenv("BSDINSTALL_CHROOT")) != 0) {
		char error[512];
		sprintf(error, "Could could change to directory %s: %s\n",
		    getenv("BSDINSTALL_DISTDIR"), strerror(errno));
		dialog_msgbox("Error", error, 0, 0, TRUE);
		end_dialog();
		return (1);
	}

	retval = extract_files(ndists, dists);

	end_dialog();

	free(diststring);
	free(dists);

	return (retval);
}

static int
extract_files(int nfiles, const char **files)
{
	const char *items[nfiles*2];
	char path[PATH_MAX];
	int archive_files[nfiles];
	int total_files, current_files, archive_file;
	struct archive *archive;
	struct archive_entry *entry;
	char errormsg[512];
	char status[8];
	int i, err, progress, last_progress;

	err = 0;
	progress = 0;
	
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

	/* Open all the archives */
	total_files = 0;
	for (i = 0; i < nfiles; i++) {
		archive = archive_read_new();
		archive_read_support_format_all(archive);
		archive_read_support_compression_all(archive);
		sprintf(path, "%s/%s", getenv("BSDINSTALL_DISTDIR"), files[i]);
		err = archive_read_open_filename(archive, path, 4096);
		if (err != ARCHIVE_OK) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error while extracting %s: %s\n", items[i*2],
			    archive_error_string(archive));
			items[i*2 + 1] = "Failed";
			dialog_msgbox("Extract Error", errormsg, 0, 0,
			    TRUE);
			return (err);
		}
		archive_files[i] = 0;
		while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
			archive_files[i]++;
		total_files += archive_files[i];
		archive_read_free(archive);
	}

	current_files = 0;

	for (i = 0; i < nfiles; i++) {
		archive = archive_read_new();
		archive_read_support_format_all(archive);
		archive_read_support_compression_all(archive);
		sprintf(path, "%s/%s", getenv("BSDINSTALL_DISTDIR"), files[i]);
		err = archive_read_open_filename(archive, path, 4096);

		items[i*2 + 1] = "In Progress";
		archive_file = 0;

		while ((err = archive_read_next_header(archive, &entry)) ==
		    ARCHIVE_OK) {
			last_progress = progress;
			progress = (current_files*100)/total_files; 

			sprintf(status, "-%d",
			    (archive_file*100)/archive_files[i]);
			items[i*2 + 1] = status;

			if (progress > last_progress)
				dialog_mixedgauge("Archive Extraction",
				    "Extracting distribution files...", 0, 0,
				    progress, nfiles,
				    __DECONST(char **, items));

			err = archive_read_extract(archive, entry,
			    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_OWNER |
			    ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_XATTR | ARCHIVE_EXTRACT_FFLAGS);

			if (err != ARCHIVE_OK)
				break;

			archive_file++;
			current_files++;
		}

		items[i*2 + 1] = "Done";

		if (err != ARCHIVE_EOF) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error while extracting %s: %s\n", items[i*2],
			    archive_error_string(archive));
			items[i*2 + 1] = "Failed";
			dialog_msgbox("Extract Error", errormsg, 0, 0,
			    TRUE);
			return (err);
		}

		archive_read_free(archive);
	}

	return (0);
}

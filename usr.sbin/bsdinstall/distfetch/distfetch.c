/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/param.h>

#include <bsddialog.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <fetch.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "opt_osname.h"

static int fetch_files(int nfiles, char **urls);

int
main(void)
{
	char *diststring;
	char **urls;
	int i;
	int ndists = 0;
	int nfetched;
	char error[PATH_MAX + 512];
	struct bsddialog_conf conf;

	if (getenv("DISTRIBUTIONS") == NULL)
		errx(EXIT_FAILURE, "DISTRIBUTIONS variable is not set");

	diststring = strdup(getenv("DISTRIBUTIONS"));
	for (i = 0; diststring[i] != 0; i++)
		if (isspace(diststring[i]) && !isspace(diststring[i+1]))
			ndists++;
	ndists++; /* Last one */

	urls = calloc(ndists, sizeof(const char *));
	if (urls == NULL) {
		free(diststring);
		errx(EXIT_FAILURE, "Error: distfetch URLs out of memory!");
	}

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		free(diststring);
		errx(EXIT_FAILURE, "Error libbsddialog: %s\n",
		    bsddialog_geterror());
	}
	bsddialog_initconf(&conf);
	bsddialog_backtitle(&conf, OSNAME " Installer");

	for (i = 0; i < ndists; i++) {
		urls[i] = malloc(PATH_MAX);
		snprintf(urls[i], PATH_MAX, "%s/%s",
		    getenv("BSDINSTALL_DISTSITE"), strsep(&diststring, " \t"));
	}

	if (chdir(getenv("BSDINSTALL_DISTDIR")) != 0) {
		snprintf(error, sizeof(error),
		    "Could not change to directory %s: %s\n",
		    getenv("BSDINSTALL_DISTDIR"), strerror(errno));
		conf.title = "Error";
		bsddialog_msgbox(&conf, error, 0, 0);
		bsddialog_end();
		return (EXIT_FAILURE);
	}

	nfetched = fetch_files(ndists, urls);

	bsddialog_end();

	free(diststring);
	for (i = 0; i < ndists; i++) 
		free(urls[i]);
	free(urls);

	return ((nfetched == ndists) ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int
fetch_files(int nfiles, char **urls)
{
	FILE *fetch_out;
	FILE *file_out;
	const char **minilabel;
	int *miniperc;
	int perc;
	int i;
	int last_progress;
	int nsuccess = 0; /* Number of files successfully downloaded */
	int progress = 0;
	size_t chunk;
	off_t current_bytes;
	off_t fsize;
	off_t total_bytes;
	float file_perc;
	float mainperc_file;
	struct url_stat ustat;
	char errormsg[PATH_MAX + 512];
	uint8_t block[4096];
	struct bsddialog_conf errconf;
	struct bsddialog_conf mgconf;

	/* Make the transfer list for mixedgauge */
	minilabel = calloc(sizeof(char *), nfiles);
	miniperc = calloc(sizeof(int), nfiles);
	if (minilabel == NULL || miniperc == NULL)
		errx(EXIT_FAILURE, "Error: distfetch minibars out of memory!");

	for (i = 0; i < nfiles; i++) {
		minilabel[i] = strrchr(urls[i], '/');
		if (minilabel[i] != NULL)
			minilabel[i]++;
		else
			minilabel[i] = urls[i];
		miniperc[i] = BSDDIALOG_MG_PENDING;
	}

	bsddialog_initconf(&errconf);
	bsddialog_infobox(&errconf, "Connecting to server.\nPlease wait...",
	    0, 0);

	/* Try to stat all the files */
	total_bytes = 0;
	for (i = 0; i < nfiles; i++) {
		if (fetchStatURL(urls[i], &ustat, "") == 0 && ustat.size > 0) {
			total_bytes += ustat.size;
		} else {
			total_bytes = 0;
			break;
		}
	}

	errconf.title = "Fetch Error";
	errconf.clear = true;
	bsddialog_initconf(&mgconf);
	mgconf.title = "Fetching Distribution";
	mgconf.auto_minwidth = 40;

	mainperc_file = 100.0 / nfiles;
	current_bytes = 0;
	for (i = 0; i < nfiles; i++) {
		fetchLastErrCode = 0;
		fetch_out = fetchXGetURL(urls[i], &ustat, "");
		if (fetch_out == NULL) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error (URL) while fetching %s: %s\n", urls[i],
			    fetchLastErrString);
			miniperc[2] = BSDDIALOG_MG_FAILED;
			bsddialog_msgbox(&errconf, errormsg, 0, 0);
			total_bytes = 0;
			continue;
		}

		miniperc[i] = BSDDIALOG_MG_INPROGRESS;
		fsize = 0;
		file_out = fopen(minilabel[i], "w+");
		if (file_out == NULL) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error (fopen) while fetching %s: %s\n",
			    urls[i], strerror(errno));
			miniperc[i] = BSDDIALOG_MG_FAILED;
			bsddialog_msgbox(&errconf, errormsg, 0, 0);
			fclose(fetch_out);
			total_bytes = 0;
			continue;
		}

		while ((chunk = fread(block, 1, sizeof(block), fetch_out))
		    > 0) {
			if (fwrite(block, 1, chunk, file_out) < chunk)
				break;

			current_bytes += chunk;
			fsize += chunk;

			last_progress = progress;
			if (total_bytes > 0) {
				progress = (current_bytes * 100) / total_bytes;
			} else {
				file_perc = ustat.size > 0 ?
				    (fsize * 100) / ustat.size : 0;
				progress = (i * mainperc_file) +
				    ((file_perc * mainperc_file) / 100);
			}

			if (ustat.size > 0) {
				perc = (fsize * 100) / ustat.size;
				miniperc[i] = perc;
			}

			if (progress > last_progress) {
				bsddialog_mixedgauge(&mgconf,
				    "\nFetching distribution files...\n",
				    0, 0, progress, nfiles, minilabel,
				    miniperc);
			}
		}

		if (ustat.size > 0 && fsize < ustat.size) {
			if (fetchLastErrCode == 0)
				snprintf(errormsg, sizeof(errormsg),
				    "Error (undone) while fetching %s: %s\n",
				    urls[i], strerror(errno));
			else
				snprintf(errormsg, sizeof(errormsg),
				    "Error (libfetch) while fetching %s: %s\n",
				    urls[i], fetchLastErrString);
			miniperc[i] = BSDDIALOG_MG_FAILED;
			bsddialog_msgbox(&errconf, errormsg, 0, 0);
			total_bytes = 0;
		} else {
			miniperc[i] = BSDDIALOG_MG_DONE;
			nsuccess++;
		}

		fclose(fetch_out);
		fclose(file_out);
	}

	bsddialog_mixedgauge(&mgconf, "\nFetching distribution completed\n",
	    0, 0, progress, nfiles, minilabel, miniperc);

	free(minilabel);
	free(miniperc);
	return (nsuccess);
}

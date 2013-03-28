/*-
 * Copyright (c) 2012-2013 Baptiste Daroussin <bapt@FreeBSD.org>
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
#include <sys/wait.h>

#include <archive.h>
#include <archive_entry.h>
#include <err.h>
#include <errno.h>
#include <fetch.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dns_utils.h"
#include "config.h"

static int
extract_pkg_static(int fd, char *p, int sz)
{
	struct archive *a;
	struct archive_entry *ae;
	char *end;
	int ret, r;

	ret = -1;
	a = archive_read_new();
	if (a == NULL) {
		warn("archive_read_new");
		return (ret);
	}
	archive_read_support_filter_all(a);
	archive_read_support_format_tar(a);

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		goto cleanup;
	}

	if (archive_read_open_fd(a, fd, 4096) != ARCHIVE_OK) {
		warnx("archive_read_open_fd: %s", archive_error_string(a));
		goto cleanup;
	}

	ae = NULL;
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		end = strrchr(archive_entry_pathname(ae), '/');
		if (end == NULL)
			continue;

		if (strcmp(end, "/pkg-static") == 0) {
			r = archive_read_extract(a, ae,
			    ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
			    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR);
			strlcpy(p, archive_entry_pathname(ae), sz);
			break;
		}
	}

	if (r == ARCHIVE_OK)
		ret = 0;
	else
		warnx("fail to extract pkg-static");

cleanup:
	archive_read_free(a);
	return (ret);

}

static int
install_pkg_static(char *path, char *pkgpath)
{
	int pstat;
	pid_t pid;

	switch ((pid = fork())) {
	case -1:
		return (-1);
	case 0:
		execl(path, "pkg-static", "add", pkgpath, (char *)NULL);
		_exit(1);
	default:
		break;
	}

	while (waitpid(pid, &pstat, 0) == -1)
		if (errno != EINTR)
			return (-1);

	if (WEXITSTATUS(pstat))
		return (WEXITSTATUS(pstat));
	else if (WIFSIGNALED(pstat))
		return (128 & (WTERMSIG(pstat)));
	return (pstat);
}

static int
bootstrap_pkg(void)
{
	struct url *u;
	FILE *remote;
	FILE *config;
	char *site;
	struct dns_srvinfo *mirrors, *current;
	/* To store _https._tcp. + hostname + \0 */
	char zone[MAXHOSTNAMELEN + 13];
	char url[MAXPATHLEN];
	char conf[MAXPATHLEN];
	char tmppkg[MAXPATHLEN];
	const char *packagesite, *mirror_type;
	char buf[10240];
	char pkgstatic[MAXPATHLEN];
	int fd, retry, ret, max_retry;
	struct url_stat st;
	off_t done, r;
	time_t now;
	time_t last;

	done = 0;
	last = 0;
	max_retry = 3;
	ret = -1;
	remote = NULL;
	config = NULL;
	current = mirrors = NULL;

	printf("Bootstrapping pkg please wait\n");

	if (config_string(PACKAGESITE, &packagesite) != 0) {
		warnx("No PACKAGESITE defined");
		return (-1);
	}
	if (config_string(MIRROR_TYPE, &mirror_type) != 0) {
		warnx("No MIRROR_TYPE defined");
		return (-1);
	}
	snprintf(url, MAXPATHLEN, "%s/Latest/pkg.txz", packagesite);

	snprintf(tmppkg, MAXPATHLEN, "%s/pkg.txz.XXXXXX",
	    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP);

	if ((fd = mkstemp(tmppkg)) == -1) {
		warn("mkstemp()");
		return (-1);
	}

	retry = max_retry;

	u = fetchParseURL(url);
	while (remote == NULL) {
		if (retry == max_retry) {
			if (strcmp(u->scheme, "file") != 0 &&
			    strcasecmp(mirror_type, "srv") == 0) {
				snprintf(zone, sizeof(zone),
				    "_%s._tcp.%s", u->scheme, u->host);
				mirrors = dns_getsrvinfo(zone);
				current = mirrors;
			}
		}

		if (mirrors != NULL)
			strlcpy(u->host, current->host, sizeof(u->host));

		remote = fetchXGet(u, &st, "");
		if (remote == NULL) {
			--retry;
			if (retry <= 0)
				goto fetchfail;
			if (mirrors == NULL) {
				sleep(1);
			} else {
				current = current->next;
				if (current == NULL)
					current = mirrors;
			}
		}
	}

	if (remote == NULL)
		goto fetchfail;

	while (done < st.size) {
		if ((r = fread(buf, 1, sizeof(buf), remote)) < 1)
			break;

		if (write(fd, buf, r) != r) {
			warn("write()");
			goto cleanup;
		}

		done += r;
		now = time(NULL);
		if (now > last || done == st.size)
			last = now;
	}

	if (ferror(remote))
		goto fetchfail;

	if ((ret = extract_pkg_static(fd, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, tmppkg);

	snprintf(conf, MAXPATHLEN, "%s/etc/pkg.conf",
	    getenv("LOCALBASE") ? getenv("LOCALBASE") : _LOCALBASE);

	if (access(conf, R_OK) == -1) {
		site = strrchr(url, '/');
		if (site == NULL)
			goto cleanup;
		site[0] = '\0';
		site = strrchr(url, '/');
		if (site == NULL)
			goto cleanup;
		site[0] = '\0';

		config = fopen(conf, "w+");
		if (config == NULL)
			goto cleanup;
		fprintf(config, "packagesite: %s\n", url);
		fclose(config);
	}

	goto cleanup;

fetchfail:
	warnx("Error fetching %s: %s", url, fetchLastErrString);
	fprintf(stderr, "A pre-built version of pkg could not be found for your system.\n");
	fprintf(stderr, "Consider changing PACKAGESITE or installing it from ports: 'ports-mgmt/pkg'.\n");

cleanup:
	if (remote != NULL)
		fclose(remote);
	close(fd);
	unlink(tmppkg);

	return (ret);
}

static const char confirmation_message[] =
"The package management tool is not yet installed on your system.\n"
"Do you want to fetch and install it now? [y/N]: ";

static int
pkg_query_yes_no(void)
{
	int ret, c;

	c = getchar();

	if (c == 'y' || c == 'Y')
		ret = 1;
	else
		ret = 0;

	while (c != '\n' && c != EOF)
		c = getchar();

	return (ret);
}

int
main(__unused int argc, char *argv[])
{
	char pkgpath[MAXPATHLEN];
	bool yes = false;

	snprintf(pkgpath, MAXPATHLEN, "%s/sbin/pkg",
	    getenv("LOCALBASE") ? getenv("LOCALBASE") : _LOCALBASE);

	if (access(pkgpath, X_OK) == -1) {
		/* 
		 * To allow 'pkg -N' to be used as a reliable test for whether
		 * a system is configured to use pkg, don't bootstrap pkg
		 * when that argument is given as argv[1].
		 */
		if (argv[1] != NULL && strcmp(argv[1], "-N") == 0)
			errx(EXIT_FAILURE, "pkg is not installed");

		/*
		 * Do not ask for confirmation if either of stdin or stdout is
		 * not tty. Check the environment to see if user has answer
		 * tucked in there already.
		 */
		config_init();
		config_bool(ASSUME_ALWAYS_YES, &yes);
		if (!yes) {
			printf("%s", confirmation_message);
			if (!isatty(fileno(stdin)))
				exit(EXIT_FAILURE);

			if (pkg_query_yes_no() == 0)
				exit(EXIT_FAILURE);
		}
		if (bootstrap_pkg() != 0)
			exit(EXIT_FAILURE);
		config_finish();
	}

	execv(pkgpath, argv);

	/* NOT REACHED */
	return (EXIT_FAILURE);
}

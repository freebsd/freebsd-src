/*-
 * Copyright (c) 2002 M. Warner Losh.
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

/*
 * DEVD control daemon.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "devd.h"

#define CF "/etc/devd.conf"

extern FILE *yyin;
extern int lineno;

int dflag;
int romeo_must_die = 0;
struct file_list_head dirlist = TAILQ_HEAD_INITIALIZER(dirlist);

static void event_loop(void);
static void parse(void);
static void parse_config_file(const char *fn);
static void parse_files_in_dir(const char *dirname);
static void reset_config(void);
static void usage(void);

void
add_directory(const char *dir)
{
	struct file_list *elm;

	elm = malloc(sizeof(*elm));
	elm->path = strdup(dir);
	TAILQ_INSERT_TAIL(&dirlist, elm, fl_link);
}

static void
reset_config(void)
{
	struct file_list *flp;

	TAILQ_FOREACH(flp, &dirlist, fl_link) {
		free(flp->path);
		free(flp);
	}
}

static void
parse_config_file(const char *fn)
{
	if (dflag)
		printf("Parsing %s\n", fn);
	yyin = fopen(fn, "r");
	if (yyin == NULL)
		err(1, "Cannot open config file %s", fn);
	if (yyparse() != 0)
		errx(1, "Cannot parse %s at line %d", fn, lineno);
	fclose(yyin);
}

static void
parse_files_in_dir(const char *dirname)
{
	DIR *dirp;
	struct dirent *dp;
	char path[PATH_MAX];

	if (dflag)
		printf("Parsing files in %s\n", dirname);
	dirp = opendir(dirname);
	if (dirp == NULL)
		return;
	readdir(dirp);		/* Skip . */
	readdir(dirp);		/* Skip .. */
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name + dp->d_namlen - 5, ".conf") == 0) {
			snprintf(path, sizeof(path), "%s/%s",
			    dirname, dp->d_name);
			parse_config_file(path);
		}
	}
}

static void
parse(void)
{
	struct file_list *flp;

	parse_config_file(CF);
	TAILQ_FOREACH(flp, &dirlist, fl_link) {
		parse_files_in_dir(flp->path);
	}
}

static void
process_event(const char *buffer)
{
	char type;
	char cmd[1024];
	char *sp;

	// Ignore unknown devices for now.
	if (*buffer == '?')
		return;
	type = *buffer++;
	sp = strchr(buffer, ' ');
	if (sp == NULL)
		return;	/* Can't happen? */
	*sp = '\0';
	snprintf(cmd, sizeof(cmd), "/etc/devd-generic %s %s", buffer,
	    type == '+' ? "start" : "stop");
	if (dflag)
		printf("Trying '%s'\n", cmd);
	system(cmd);
}

static void
event_loop(void)
{
	int rv;
	int fd;
	char buffer[1024 + 1];	/* XXX */

	fd = open("/dev/devctl", O_RDONLY);	 /* XXX */
	if (fd == -1)
		err(1, "Can't open devctl");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
		err(1, "Can't set close-on-exec flag");
	while (1) {
		if (romeo_must_die)
			break;
		rv = read(fd, buffer, sizeof(buffer) - 1);
		if (rv > 0) {
			buffer[rv] = '\0';
			while (buffer[--rv] == '\n')
				buffer[rv] = '\0';
			process_event(buffer);
		} else if (rv < 0) {
			if (errno != EINTR)
				break;
		} else {
			/* EOF */
			break;
		}
	}
	close(fd);
}

static void
gensighand(int foo __unused)
{
	romeo_must_die++;
}

static void
usage()
{
	fprintf(stderr, "usage: %s [-d]", getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			dflag++;
			break;
		default:
			usage();
		}
	}

	reset_config();
	parse();
	if (!dflag)
		daemon(0, 0);
	event_loop();
	signal(SIGHUP, gensighand);
	signal(SIGINT, gensighand);
	signal(SIGTERM, gensighand);
	return (0);
}

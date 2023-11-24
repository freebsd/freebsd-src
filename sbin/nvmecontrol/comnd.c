/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2019 Netflix, Inc
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

#include <sys/param.h>
#include <sys/ioccom.h>

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <libutil.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "comnd.h"

static struct cmd top;

static void
print_tree(const struct cmd *f)
{

	if (f->parent != NULL)
		print_tree(f->parent);
	if (f->name != NULL)
		fprintf(stderr, " %s", f->name);
}

static void
print_usage(const struct cmd *f)
{

	fprintf(stderr, "    %s", getprogname());
	print_tree(f->parent);
	fprintf(stderr, " %-15s - %s\n", f->name, f->descr);
}

static void
gen_usage(const struct cmd *t)
{
	struct cmd *walker;

	fprintf(stderr, "usage:\n");
	SLIST_FOREACH(walker, &t->subcmd, link) {
		print_usage(walker);
	}
	exit(EX_USAGE);
}

int
cmd_dispatch(int argc, char *argv[], const struct cmd *t)
{
	struct cmd *walker;

	if (t == NULL)
		t = &top;

	if (argv[1] == NULL) {
		gen_usage(t);
		return (1);
	}
	SLIST_FOREACH(walker, &t->subcmd, link) {
		if (strcmp(argv[1], walker->name) == 0) {
			walker->fn(walker, argc-1, &argv[1]);
			return (0);
		}
	}
	fprintf(stderr, "Unknown command: %s\n", argv[1]);
	gen_usage(t);
	return (1);
}

static void
arg_suffix(char *buf, size_t len, arg_type at)
{
	switch (at) {
	case arg_none:
		break;
	case arg_string:
		strlcat(buf, "=<STRING>", len);
		break;
	case arg_path:
		strlcat(buf, "=<FILE>", len);
		break;
	default:
		strlcat(buf, "=<NUM>", len);
		break;
	}
}

void
arg_help(int argc __unused, char * const *argv, const struct cmd *f)
{
	int i;
	char buf[31];
	const struct opts *opts = f->opts;
	const struct args *args = f->args;

	// XXX walk up the cmd list...
	if (argv[optind])
		fprintf(stderr, "Unknown argument: %s\n", argv[optind]);
	fprintf(stderr, "Usage:\n    %s", getprogname());
	print_tree(f);
	if (opts)
		fprintf(stderr, " <args>");
	if (args) {
		while (args->descr != NULL) {
			fprintf(stderr, " %s", args->descr);
			args++;
		}
	}
	fprintf(stderr, "\n\n%s\n", f->descr);
	if (opts != NULL) {
		fprintf(stderr, "Options:\n");
		for (i = 0; opts[i].long_arg != NULL; i++) {
			*buf = '\0';
			if (isprint(opts[i].short_arg)) {
				snprintf(buf, sizeof(buf), " -%c, ", opts[i].short_arg);
			} else {
				strlcpy(buf, "    ", sizeof(buf));
			}
			strlcat(buf, "--", sizeof(buf));
			strlcat(buf, opts[i].long_arg, sizeof(buf));
			arg_suffix(buf, sizeof(buf), opts[i].at);
			fprintf(stderr, "%-30.30s - %s\n", buf, opts[i].descr);
		}
	}
	exit(EX_USAGE);
}

static int
find_long(struct option *lopts, int ch)
{
	int i;

	for (i = 0; lopts[i].val != ch && lopts[i].name != NULL; i++)
		continue;
	return (i);
}

int
arg_parse(int argc, char * const * argv, const struct cmd *f)
{
	int i, n, idx, ch;
	uint64_t v;
	struct option *lopts;
	char *shortopts, *p;
	const struct opts *opts = f->opts;
	const struct args *args = f->args;

	if (opts == NULL)
		n = 0;
	else
		for (n = 0; opts[n].long_arg != NULL;)
			n++;
	lopts = malloc((n + 2) * sizeof(struct option));
	if (lopts == NULL)
		err(EX_OSERR, "option memory");
	p = shortopts = malloc((2 * n + 3) * sizeof(char));
	if (shortopts == NULL)
		err(EX_OSERR, "shortopts memory");
	idx = 0;
	for (i = 0; i < n; i++) {
		lopts[i].name = opts[i].long_arg;
		lopts[i].has_arg = opts[i].at == arg_none ? no_argument : required_argument;
		lopts[i].flag = NULL;
		lopts[i].val = opts[i].short_arg;
		if (isprint(opts[i].short_arg)) {
			*p++ = opts[i].short_arg;
			if (lopts[i].has_arg)
				*p++ = ':';
		}
	}
	lopts[n].name = "help";
	lopts[n].has_arg = no_argument;
	lopts[n].flag = NULL;
	lopts[n].val = '?';
	*p++ = '?';
	*p++ = '\0';
	memset(lopts + n + 1, 0, sizeof(struct option));
	while ((ch = getopt_long(argc, argv, shortopts, lopts, &idx)) != -1) {
		/*
		 * If ch != 0, we've found a short option, and we have to
		 * look it up lopts table. Otherwise idx is valid.
		 */
		if (ch != 0)
			idx = find_long(lopts, ch);
		if (idx == n)
			arg_help(argc, argv, f);
		switch (opts[idx].at) {
		case arg_none:
			*(bool *)opts[idx].ptr = true;
			break;
		case arg_string:
		case arg_path:
			*(const char **)opts[idx].ptr = optarg;
			break;
		case arg_uint8:
			v = strtoul(optarg, NULL, 0);
			if (v > 0xff)
				goto bad_arg;
			*(uint8_t *)opts[idx].ptr = v;
			break;
		case arg_uint16:
			v = strtoul(optarg, NULL, 0);
			if (v > 0xffff)
				goto bad_arg;
			*(uint16_t *)opts[idx].ptr = v;
			break;
		case arg_uint32:
			v = strtoul(optarg, NULL, 0);
			if (v > 0xffffffffu)
				goto bad_arg;
			*(uint32_t *)opts[idx].ptr = v;
			break;
		case arg_uint64:
			v = strtoul(optarg, NULL, 0);
			if (v > 0xffffffffffffffffull)
				goto bad_arg;
			*(uint64_t *)opts[idx].ptr = v;
			break;
		case arg_size:
			if (expand_number(optarg, &v) < 0)
				goto bad_arg;
			*(uint64_t *)opts[idx].ptr = v;
			break;
		}
	}
	if (args) {
		while (args->descr) {
			if (optind >= argc) {
				fprintf(stderr, "Missing arg %s\n", args->descr);
				arg_help(argc, argv, f);
				free(lopts);
				free(shortopts);
				return (1);
			}
			*(char **)args->ptr = argv[optind++];
			args++;
		}
	}
	free(lopts);
	free(shortopts);
	return (0);
bad_arg:
	fprintf(stderr, "Bad value to --%s: %s\n", opts[idx].long_arg, optarg);
	free(lopts);
	free(shortopts);
	exit(EX_USAGE);
}

/*
 * Loads all the .so's from the specified directory.
 */
void
cmd_load_dir(const char *dir, cmd_load_cb_t cb, void *argp)
{
	DIR *d;
	struct dirent *dent;
	char *path = NULL;
	void *h;

	d = opendir(dir);
	if (d == NULL)
		return;
	for (dent = readdir(d); dent != NULL; dent = readdir(d)) {
		if (strcmp(".so", dent->d_name + dent->d_namlen - 3) != 0)
			continue;
		asprintf(&path, "%s/%s", dir, dent->d_name);
		if (path == NULL)
			err(EX_OSERR, "Can't malloc for path, giving up.");
		if ((h = dlopen(path, RTLD_NOW | RTLD_GLOBAL)) == NULL)
			warnx("Can't load %s: %s", path, dlerror());
		else {
			if (cb != NULL)
				cb(argp, h);
		}
		free(path);
		path = NULL;
	}
	closedir(d);
}

void
cmd_register(struct cmd *up, struct cmd *cmd)
{
	struct cmd *walker, *last;

	if (up == NULL)
		up = &top;
	SLIST_INIT(&cmd->subcmd);
	cmd->parent = up;
	last = NULL;
	SLIST_FOREACH(walker, &up->subcmd, link) {
		if (strcmp(walker->name, cmd->name) > 0)
			break;
		last = walker;
	}
	if (last == NULL) {
		SLIST_INSERT_HEAD(&up->subcmd, cmd, link);
	} else {
		SLIST_INSERT_AFTER(last, cmd, link);
	}
}

void
cmd_init(void)
{

}

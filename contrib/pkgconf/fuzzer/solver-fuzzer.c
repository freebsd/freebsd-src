/*
 * solver-fuzzer.c
 * dependency solver fuzzing harness
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

#include "alloc-inject.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* bound the number of injection rounds per input to keep executions cheap */
#define ALLOC_FAIL_MAX 4096

/* the fuzzer input is split on NUL bytes into up to this many .pc files, named
 * a.pc, b.pc, ... so that Requires/Conflicts/Provides between them resolve.
 */
#define UNIVERSE_MAX 4
#define SOLVE_MAXDEPTH 10

static const char universe_names[UNIVERSE_MAX] = { 'a', 'b', 'c', 'd' };

static const char *
environ_lookup_handler(const pkgconf_client_t *client, const char *key)
{
	(void) client;
	(void) key;

	return NULL;
}

static int
write_all(int fd, const uint8_t *data, size_t size)
{
	while (size > 0)
	{
		ssize_t n = write(fd, data, size);

		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}

		data += n;
		size -= n;
	}

	return 0;
}

static void
write_pkg(const char *dir, char name, const uint8_t *data, size_t size)
{
	char path[PKGCONF_ITEM_SIZE];
	int fd;

	snprintf(path, sizeof path, "%s/%c.pc", dir, name);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return;

	write_all(fd, data, size);
	close(fd);
}

static void
write_universe(const char *dir, const uint8_t *data, size_t size)
{
	size_t start = 0, idx = 0;

	for (size_t i = 0; i < size && idx < UNIVERSE_MAX; i++)
	{
		if (data[i] == 0x00)
		{
			write_pkg(dir, universe_names[idx++], data + start, i - start);
			start = i + 1;
		}
	}

	if (idx < UNIVERSE_MAX)
		write_pkg(dir, universe_names[idx], data + start, size - start);
}

static void
cleanup_universe(const char *dir)
{
	char path[PKGCONF_ITEM_SIZE];

	for (size_t i = 0; i < UNIVERSE_MAX; i++)
	{
		snprintf(path, sizeof path, "%s/%c.pc", dir, universe_names[i]);
		unlink(path);
	}

	rmdir(dir);
}

static void
run_solve(const pkgconf_cross_personality_t *pers, const char *dir)
{
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, environ_lookup_handler);
	if (client == NULL)
		return;

	pkgconf_path_add(dir, &client->dir_list, false);

	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};
	pkgconf_list_t pkgq = PKGCONF_LIST_INITIALIZER;

	pkgconf_queue_push(&pkgq, "a");

	if (pkgconf_queue_solve(client, &pkgq, &world, SOLVE_MAXDEPTH))
	{
		pkgconf_list_t cflags = PKGCONF_LIST_INITIALIZER;
		pkgconf_list_t libs = PKGCONF_LIST_INITIALIZER;
		pkgconf_buffer_t render = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_pkg_cflags(client, &world, &cflags, SOLVE_MAXDEPTH);
		pkgconf_pkg_libs(client, &world, &libs, SOLVE_MAXDEPTH);

		pkgconf_fragment_render_buf(&cflags, &render, true, NULL, ' ');
		pkgconf_fragment_render_buf(&libs, &render, true, NULL, ' ');

		pkgconf_buffer_finalize(&render);
		pkgconf_fragment_free(&cflags);
		pkgconf_fragment_free(&libs);
	}

	pkgconf_solution_free(client, &world);
	pkgconf_queue_free(&pkgq);
	pkgconf_client_free(client);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0)
		return 0;

	char dir[] = "/tmp/pkgconf-fuzz-univ-XXXXXX";
	if (mkdtemp(dir) == NULL)
		return 0;

	write_universe(dir, data, size);

	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();

	/* baseline run with all allocations succeeding */
	run_solve(pers, dir);

	/* then fail each allocation site reachable by this input, one at a time */
	for (unsigned long i = 1; i <= ALLOC_FAIL_MAX; i++)
	{
		alloc_inject_arm(i);
		run_solve(pers, dir);
		alloc_inject_disarm();

		if (!alloc_inject_fired())
			break;
	}

	pkgconf_cross_personality_deinit(pers);
	cleanup_universe(dir);

	return 0;
}

/*
 * parser-fuzzer.c
 * parser fuzzing harness
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

static const char *
environ_lookup_handler(const pkgconf_client_t *client, const char *key)
{
	(void) client;
	(void) key;

	return NULL;
}

static void
run_once(pkgconf_client_t *client, const char *path)
{
	pkgconf_pkg_t *pkg = pkgconf_pkg_new_from_path(client, path, 0);
	if (pkg == NULL)
		return;

	pkgconf_list_t cflags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t libs = PKGCONF_LIST_INITIALIZER;
	pkgconf_buffer_t render = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_pkg_verify_graph(client, pkg, 2);

	pkgconf_pkg_cflags(client, pkg, &cflags, 2);
	pkgconf_pkg_libs(client, pkg, &libs, 2);

	pkgconf_fragment_render_buf(&cflags, &render, true, NULL, ' ');
	pkgconf_fragment_render_buf(&libs, &render, true, NULL, ' ');

	pkgconf_buffer_finalize(&render);
	pkgconf_fragment_free(&cflags);
	pkgconf_fragment_free(&libs);
	pkgconf_pkg_free(client, pkg);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0)
		return 0;

	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, environ_lookup_handler);
	if (client == NULL)
		return 0;

	char path[] = "/tmp/pkgconf-fuzz-XXXXXX.pc";
	int fd = mkstemps(path, 3);  // keep ".pc"
	if (fd < 0)
	{
		pkgconf_client_free(client);
		return 0;
	}

	if (write_all(fd, data, size) != 0)
	{
		close(fd);
		unlink(path);
		pkgconf_client_free(client);
		return 0;
	}

	close(fd);

	/* baseline run with all allocations succeeding */
	run_once(client, path);

	/* then fail each allocation site reachable by this input, one at a time */
	for (unsigned long i = 1; i <= ALLOC_FAIL_MAX; i++)
	{
		alloc_inject_arm(i);
		run_once(client, path);
		alloc_inject_disarm();

		/* this input made fewer than i allocations; no point going further */
		if (!alloc_inject_fired())
			break;
	}

	unlink(path);
	pkgconf_client_free(client);
	pkgconf_cross_personality_deinit(pers);

	return 0;
}

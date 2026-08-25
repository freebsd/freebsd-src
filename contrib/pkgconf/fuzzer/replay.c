/*
 * replay.c
 * Standalone driver that replays corpus files through a libFuzzer
 * LLVMFuzzerTestOneInput entry point, without libFuzzer.
 *
 * This lets the fuzz targets (and the allocator fault injection they drive via
 * alloc-inject) run deterministically over their seed corpus under an ordinary
 * compiler -- in particular a coverage build -- so their OOM-path coverage is
 * captured by codecov, which the libFuzzer/clang fuzz job does not provide.
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
#include <sys/stat.h>
#include <dirent.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void
replay_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return;

	if (fseek(f, 0, SEEK_END) != 0)
	{
		fclose(f);
		return;
	}

	long len = ftell(f);
	rewind(f);

	if (len < 0)
	{
		fclose(f);
		return;
	}

	uint8_t *buf = malloc((size_t) len + 1);
	if (buf == NULL)
	{
		fclose(f);
		return;
	}

	size_t got = fread(buf, 1, (size_t) len, f);
	fclose(f);

	LLVMFuzzerTestOneInput(buf, got);
	free(buf);
}

static void
replay_path(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return;

	if (!S_ISDIR(st.st_mode))
	{
		replay_file(path);
		return;
	}

	DIR *dir = opendir(path);
	if (dir == NULL)
		return;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (ent->d_name[0] == '.')
			continue;

		char child[4096];
		snprintf(child, sizeof child, "%s/%s", path, ent->d_name);
		replay_path(child);
	}

	closedir(dir);
}

int
main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
		replay_path(argv[i]);

	return 0;
}

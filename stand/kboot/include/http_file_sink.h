/*
 * Copyright (c) 2026 Aryan Arora
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/param.h>

#include "http.h"

struct http_file_sink {
	sink_t sink;
	const char *dest_dir;
	char output_path[MAXPATHLEN];
	int fd;
};

void http_file_sink_init(struct http_file_sink *fs, const char *dest_dir);

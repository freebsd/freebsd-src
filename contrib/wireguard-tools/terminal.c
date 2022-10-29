// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "ctype.h"
#include "terminal.h"

static bool color_mode(void)
{
	static int mode = -1;
	const char *var;

	if (mode != -1)
		return mode;
	var = getenv("WG_COLOR_MODE");
	if (var && !strcmp(var, "always"))
		mode = true;
	else if (var && !strcmp(var, "never"))
		mode = false;
	else
		mode = isatty(fileno(stdout));
	return mode;
}

static void filter_ansi(const char *fmt, va_list args)
{
	char *str = NULL;
	size_t len, i, j;

	if (color_mode()) {
		vfprintf(stdout, fmt, args);
		return;
	}

	len = vasprintf(&str, fmt, args);

	if (len >= 2) {
		for (i = 0; i < len - 2; ++i) {
			if (str[i] == '\x1b' && str[i + 1] == '[') {
				str[i] = str[i + 1] = '\0';
				for (j = i + 2; j < len; ++j) {
					if (char_is_alpha(str[j]))
						break;
					str[j] = '\0';
				}
				str[j] = '\0';
			}
		}
	}
	for (i = 0; i < len; i = j) {
		fputs(&str[i], stdout);
		for (j = i + strlen(&str[i]); j < len; ++j) {
			if (str[j] != '\0')
				break;
		}
	}

	free(str);
}

void terminal_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	filter_ansi(fmt, args);
	va_end(args);
}

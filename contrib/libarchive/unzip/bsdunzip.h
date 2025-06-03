/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023, Martin Matuska
 * All rights reserved.
 */

#ifndef BSDUNZIP_H_INCLUDED
#define BSDUNZIP_H_INCLUDED

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#else
/* Not having a config.h of some sort is a serious problem. */
#include "config.h"
#endif

#include <archive.h>
#include <archive_entry.h>

struct bsdunzip {
	/* Option parser state */
	int		  getopt_state;
	char		 *getopt_word;

	/* Miscellaneous state information */
	int		  argc;
	char		**argv;
	const char	 *argument;
};

enum {
	OPTION_NONE,
	OPTION_VERSION
};

int bsdunzip_getopt(struct bsdunzip *);

extern int bsdunzip_optind;

#endif

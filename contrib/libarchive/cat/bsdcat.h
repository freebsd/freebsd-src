/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014, Mike Kazantsev
 * All rights reserved.
 */

#ifndef BSDCAT_H_INCLUDED
#define BSDCAT_H_INCLUDED

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#else
/* Not having a config.h of some sort is a serious problem. */
#include "config.h"
#endif

struct bsdcat {
	/* Option parser state */
	int		  getopt_state;
	char		 *getopt_word;

	/* Miscellaneous state information */
	int		  argc;
	char		**argv;
	const char	 *argument;
};

enum {
	OPTION_VERSION
};

int bsdcat_getopt(struct bsdcat *);

#endif

/*-
 * Copyright (c) 2002, 2003 Greg Lehey
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
 * This software is provided by the author ``as is'' and any express
 * or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  In no event shall the author be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <err.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "asf.h"

/*
 * Get the linker file list from kldstat(8) output.
 * The "run" flag tells if kldstat(8) should run now.
 * Of course, kldstat(1) can run in a live system only, but its output
 * can be saved beforehand and fed to this function later via stdin.
 */
void
asf_prog(int run)
{
	char	buf[LINE_MAX];
	char   *token[MAXTOKEN];
	char   *endp;
	FILE   *kldstat;
	caddr_t	base;
	int	tokens;

	if (run) {
		if ((kldstat = popen("kldstat", "r")) == NULL)
			err(2, "can't start kldstat");
	} else
		kldstat = stdin;

	while (fgets(buf, sizeof(buf), kldstat)) {
		/* Skip header line and main kernel file */
		if (buf[0] == 'I' || strstr(buf, KERNFILE))
			continue;
		tokens = tokenize(buf, token, MAXTOKEN);
		if (tokens < 4)
			continue;
		base = (caddr_t)(uintptr_t)strtoumax(token[2], &endp, 16);
		if (endp == NULL || *endp != '\0')
			continue;
		kfile_add(token[4], base);
	}
}

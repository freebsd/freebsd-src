/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * $Id: pathnames.c,v 8.5 1997/05/21 19:52:28 halley Exp $
 */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

int
main(int argc, char *argv[], char *envp[]) {
	char *arg;

	argc--, argv++;
	while (argc-- && (arg = *argv++) != NULL)
		if (!strcasecmp("_PATH_XFER", arg))
			puts(_PATH_XFER);
		else if (!strcasecmp("_PATH_PIDFILE", arg))
			puts(_PATH_PIDFILE);
		else if (!strcasecmp("_PATH_NAMED", arg))
			puts(_PATH_NAMED);
		else
			exit(1);
	exit(0);
}

/*
 * Copyright (c) 2005, 2006 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Id: t-memstat.c,v 1.6 2006/03/27 22:34:47 ca Exp $")

/*
**  Simple test program for memstat
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

extern char *optarg;
extern int optind;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int r, r2, i, l, slp, sz;
	long v;
	char *resource;

	l = 1;
	sz = slp = 0;
	resource = NULL;
	while ((r = getopt(argc, argv, "l:m:r:s:")) != -1)
	{
		switch ((char) r)
		{
		  case 'l':
			l = strtol(optarg, NULL, 0);
			break;

		  case 'm':
			sz = strtol(optarg, NULL, 0);
			break;

		  case 'r':
			resource = strdup(optarg);
			break;

		  case 's':
			slp = strtol(optarg, NULL, 0);
			break;

		  default:
			break;
		}
	}

	r = sm_memstat_open();
	r2 = -1;
	for (i = 0; i < l; i++)
	{
		char *mem;

		r2 = sm_memstat_get(resource, &v);
		if (slp > 0 && i + 1 < l && 0 == r)
		{
			printf("open=%d, memstat=%d, %s=%ld\n", r, r2,
				resource != NULL ? resource : "default-value",
				v);
			sleep(slp);
			if (sz > 0)
			{
				/*
				**  Just allocate some memory to test the
				**  values that are returned.
				**  Note: this is a memory leak, but that
				**  doesn't matter here.
				*/

				mem = malloc(sz);
				if (NULL == mem)
					printf("malloc(%d) failed\n", sz);
			}
		}
	}
	printf("open=%d, memstat=%d, %s=%ld\n", r, r2,
		resource != NULL ? resource : "default-value", v);
	r = sm_memstat_close();
	return r;
}

/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Id: t-qic.c,v 1.10 2013-11-22 20:51:43 ca Exp $")

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sm/sendmail.h>
#include <sm/ixlen.h>
#include <sm/test.h>

#if _FFR_8BITENVADDR
extern bool SmTestVerbose;

static int
tstrncaseeq(s1, s2, len)
	char *s1;
	char *s2;
	size_t len;
{
	return SM_STRNCASEEQ(s1, s2, len);
}

static void
usage(prg)
	const char *prg;
{
	fprintf(stderr, "usage: %s [options]\n", prg);
	fprintf(stderr, "options:\n");
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int o, len;
#define MAXL	1024
	char s1[MAXL], s2[MAXL];

	while ((o = getopt(argc, argv, "h")) != -1)
	{
		switch ((char) o)
		{
		  default:
			usage(argv[0]);
			exit(1);
		}
	}

	sm_test_begin(argc, argv, "test strncaseeq");

	while (fscanf(stdin, "%d:%s\n", &len, s1) == 2 &&
		fscanf(stdin, "%d:%s\n", &o,s2) == 2)
	{
		SM_TEST(tstrncaseeq(s1, s2, len) == o);
	}

	return sm_test_end();
}
#else /* _FFR_8BITENVADDR */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	return 0;
}
#endif /* _FFR_8BITENVADDR */

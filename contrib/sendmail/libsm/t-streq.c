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

static void
hack(str)
	char *str;
{
	char c;

	/* replace just one \x char */
	while ((c = *str++) != '\0')
	{
		if (c != '\\')
			continue;
		c = *str;
		switch (c)
		{
		  case 'n': c ='\n'; break;
		  case 't': c ='\t'; break;
		  case 'r': c ='\r'; break;
		  /* case 'X': c ='\X'; break; */
		  default: c ='\0'; break;
		}
		*(str - 1) = c;
		*str = '\0';
		break;
	}
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
		int r;

		hack(s1);
		hack(s2);
		SM_TEST(tstrncaseeq(s1, s2, len) == o);
		if ((r = tstrncaseeq(s1, s2, len)) != o)
			fprintf(stderr, "\"%s\"\n\"%s\"\n%d!=%d\n", s1, s2, o, r);
	}

	return sm_test_end();
}

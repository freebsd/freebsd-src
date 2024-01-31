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
static int Verbose = 0;

static void
chkilenx(str, len)
	const char *str;
	int len;
{
	int xlen;

	xlen = ilenx(str);
	SM_TEST(len == xlen);
	if (len != xlen)
		fprintf(stderr, "str=\"%s\", len=%d, expected=%d\n",
			str, xlen, len);
}

static void
chkilen(str)
	char *str;
{
	char *obp;
	int outlen, leni, lenx, ilen;
	char line_in[1024];
	XLENDECL

	lenx = strlen(str);
	sm_strlcpy(line_in, str, sizeof(line_in));
	obp = quote_internal_chars(str, NULL, &outlen, NULL);
	leni = strlen(obp);

	for (ilen = 0; *obp != '\0'; obp++, ilen++)
	{
		XLEN(*obp);
	}
	if (Verbose)
		fprintf(stderr, "str=\"%s\", ilen=%d, xlen=%d\n",
			str, ilen, xlen);
	SM_TEST(ilen == leni);
	if (ilen != leni)
		fprintf(stderr, "str=\"%s\", ilen=%d, leni=%d\n",
			str, ilen, leni);
	SM_TEST(xlen == lenx);
	if (xlen != lenx)
		fprintf(stderr, "str=\"%s\", xlen=%d, lenx=%d\n",
			str, xlen, lenx);
}

static void
chkxleni(str, len)
	const char *str;
	int len;
{
	int ilen;

	ilen = xleni(str);
	SM_TEST(len == ilen);
	if (len != ilen)
		fprintf(stderr, "str=\"%s\", len=%d, expected=%d\n",
			str, ilen, len);
}


static void
usage(prg)
	const char *prg;
{
	fprintf(stderr, "usage: %s [options]\n", prg);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "-x    xleni\n");
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int o, len;
	bool x, both;
	char line[1024];

	x = both = false;
	while ((o = getopt(argc, argv, "bxV")) != -1)
	{
		switch ((char) o)
		{
		  case 'b':
			both = true;
			break;

		  case 'x':
			x = true;
			break;

		  case 'V':
			Verbose++;
			break;

		  default:
			usage(argv[0]);
			exit(1);
		}
	}

	sm_test_begin(argc, argv, "test ilenx");

	if (both)
	{
		while (fscanf(stdin, "%s\n", line) == 1)
			chkilen(line);
		return sm_test_end();
	}
	while (fscanf(stdin, "%d:%s\n", &len, line) == 2)
	{
		if (x)
			chkxleni(line, len);
		else
			chkilenx(line, len);
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

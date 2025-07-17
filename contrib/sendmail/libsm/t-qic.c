/*
 * Copyright (c) 2006, 2023 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Id: t-qic.c,v 1.10 2013-11-22 20:51:43 ca Exp $")

#include <stdio.h>
#include <sm/sendmail.h>
#include <sm/assert.h>
#include <sm/heap.h>
#include <sm/string.h>
#include <sm/test.h>

extern bool SmTestVerbose;

void
show_diff(s1, s2)
	const char *s1;
	const char *s2;
{
	int i;

	for (i = 0; s1[i] != '\0' && s2[i] != '\0'; i++)
	{
		if (s1[i] != s2[i])
		{
			fprintf(stderr, "i=%d, s1[]=%u, s2[]=%u\n",
				i, (unsigned char) s1[i],
				(unsigned char) s2[i]);
			return;
		}
	}
	if (s1[i] != s2[i])
	{
		fprintf(stderr, "i=%d, s1[]=%u, s2[]=%u\n",
			i, (unsigned char) s1[i], (unsigned char) s2[i]);
	}
}

char *quote_unquote __P((char *, char *, int, int, int));

char *
quote_unquote(in, out, outlen, exp, mode)
	char *in;
	char *out;
	int outlen;
	int exp;
	int mode;
{
	char *obp, *bp;
	char line_back[1024];
	char line_in[1024];
	int cmp;

	sm_strlcpy(line_in, in, sizeof(line_in));
	obp = quote_internal_chars(in, out, &outlen, NULL);
	bp = str2prt(line_in);
	if (0 == mode)
		dequote_internal_chars(obp, line_back, sizeof(line_back));
	else if (1 == mode)
		dequote_internal_chars(obp, line_back, strlen(obp));
	else if (2 == mode)
		dequote_internal_chars(obp, line_back, strlen(obp) + 1);
	cmp = strcmp(line_in, line_back);
	SM_TEST(exp == cmp);
	if (cmp != exp && !SmTestVerbose)
	{
		fprintf(stderr, "in: %s\n", bp);
		bp = str2prt(line_back);
		fprintf(stderr, "out:%s\n", bp);
		fprintf(stderr, "cmp=%d\n", cmp);
		show_diff(in, line_back);
	}
	if (SmTestVerbose)
	{
		fprintf(stderr, "%s -> ", bp);
		bp = str2prt(obp);
		fprintf(stderr, "%s\n", bp);
		fprintf(stderr, "cmp=%d\n", cmp);
	}
	return obp;
}

struct sm_qic_S
{
	char		*qic_in;
	char		*qic_out;
	int		 qic_exp;
};

typedef struct sm_qic_S sm_qic_T;


int
main(argc, argv)
	int argc;
	char *argv[];
{
	char line_in[1024], line[256], line_out[32], *obp;
	int i, los, cmp, mode;
	sm_qic_T inout[] = {
		  { "", "",	0 }
		, { "\t", "\t",	0 }
		, { "\tuser", "\tuser",	0 }
		, { "abcdef", "abcdef",	0 }
		, { "01234567890123456789", "01234567890123456789",	0 }
		, { "\\", "\\",	0 }
		, { "\\A", "\\A",	0 }
		, { "01234567890123456789\001", "01234567890123456789\001",
			0 }
		, { "012345\2067890123456789", "012345\377\2067890123456789",
			0 }
		, { "\377", "\377\377",	0 }
		, { "\240", "\240",	0 }
		, { "\220", "\377\220",	0 }
		, { "\240\220", "\240\377\220",	0 }
		, { "\377\377", "\377\377\377\377",	0 }
		, { "\377a\377b", "\377\377a\377\377b",	0 }
		, { "\376a\377b", "\376a\377\377b",	0 }
		, { "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240",
		    "\377\200\377\201\377\202\377\203\377\204\377\205\377\206\377\207\377\210\377\211\377\212\377\213\377\214\377\215\377\216\377\217\377\220\377\221\377\222\377\223\377\224\377\225\377\226\377\227\377\230\377\231\377\232\377\233\377\234\377\235\377\236\377\237\240",
			0 }
		, { NULL, NULL,	0 }
	};

	sm_test_begin(argc, argv, "test meta quoting");
	mode = 0;
	if (argc > 1)
		mode = atoi(argv[1]);
	for (i = 0; i < sizeof(line_out); i++)
		line_out[i] = '\0';
	for (i = 0; i < sizeof(line_in); i++)
		line_in[i] = '\0';
	for (i = 0; i < sizeof(line_in) / 2; i++)
	{
		char ch;

		ch = 0200 + i;
		if ('\0' == ch)
			ch = '0';
		line_in[i] = ch;
	}
	los = sizeof(line_out) / 2;
	obp = quote_unquote(line_in, line_out, los, 0, mode);
	if (obp != line_out)
		SM_FREE(obp);

	for (i = 0; i < sizeof(line_in); i++)
		line_in[i] = '\0';
	for (i = 0; i < sizeof(line_in) / 2; i++)
	{
		char ch;

		ch = 0200 + i;
		if ('\0' == ch)
			ch = '0';
		line_in[i] = ch;
	}
	los = sizeof(line_in);
	obp = quote_unquote(line_in, line_in, los, 0, mode);
	if (obp != line_in)
		SM_FREE(obp);

	for (i = 0; inout[i].qic_in != NULL; i++)
	{
		los = sizeof(line_out) / 2;
		obp = quote_unquote(inout[i].qic_in, line_out, los,
				inout[i].qic_exp, mode);
		cmp = strcmp(inout[i].qic_out, obp);
		SM_TEST(inout[i].qic_exp == cmp);
		if (inout[i].qic_exp != cmp && !SmTestVerbose)
		{
			char *bp;

			bp = str2prt(obp);
			fprintf(stderr, "got: %s\n", bp);
			bp = str2prt(inout[i].qic_out);
			fprintf(stderr, "exp:%s\n", bp);
			fprintf(stderr, "cmp=%d\n", cmp);
			show_diff(inout[i].qic_in, inout[i].qic_out);
		}
		if (obp != line_out)
			SM_FREE(obp);
	}

	/* use same buffer for in and out */
	for (i = 0; inout[i].qic_in != NULL; i++)
	{
		bool same;

		same = strcmp(inout[i].qic_in, inout[i].qic_out) == 0;
		los = sm_strlcpy(line, inout[i].qic_in, sizeof(line));
		SM_TEST(los + 1 < sizeof(line));
		++los;
		obp = quote_unquote(line, line, los, inout[i].qic_exp, mode);
		cmp = strcmp(inout[i].qic_out, obp);
		SM_TEST(inout[i].qic_exp == cmp);
		if (inout[i].qic_exp != cmp && !SmTestVerbose)
		{
			char *bp;

			bp = str2prt(obp);
			fprintf(stderr, "got: %s\n", bp);
			bp = str2prt(inout[i].qic_out);
			fprintf(stderr, "exp:%s\n", bp);
			fprintf(stderr, "cmp=%d\n", cmp);
			show_diff(inout[i].qic_in, inout[i].qic_out);
		}
		if (obp != line)
		{
			SM_TEST(!same);
			if (same)
				show_diff(obp, inout[i].qic_out);
			SM_FREE(obp);
		}
	}

	/* use NULL buffer for out */
	for (i = 0; inout[i].qic_in != NULL; i++)
	{
		los = 0;
		obp = quote_unquote(inout[i].qic_in, NULL, los,
				inout[i].qic_exp, mode);
		SM_TEST(obp != NULL);
		cmp = strcmp(inout[i].qic_out, obp);
		SM_TEST(inout[i].qic_exp == cmp);
		if (inout[i].qic_exp != cmp && !SmTestVerbose)
		{
			char *bp;

			bp = str2prt(obp);
			fprintf(stderr, "got: %s\n", bp);
			bp = str2prt(inout[i].qic_out);
			fprintf(stderr, "exp:%s\n", bp);
			fprintf(stderr, "cmp=%d\n", cmp);
			show_diff(inout[i].qic_in, inout[i].qic_out);
		}
	}

	los = -1;
	obp = quote_internal_chars(NULL, NULL, &los, NULL);
	SM_TEST(NULL == obp);
	SM_TEST(-1 == los);

	sm_strlcpy(line_in, "nothing", sizeof(line_in));
	los = -123;
	obp = quote_internal_chars(line_in, NULL, &los, NULL);
	SM_TEST(NULL != obp);
	SM_TEST(los > 0);

	return sm_test_end();
}

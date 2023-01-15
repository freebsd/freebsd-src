/*
 * Copyright (c) 2006 Proofpoint, Inc. and its suppliers.
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
	char *obp;
	int i, cmp;
	sm_qic_T inout[] = {
		  { "", "",	0 }
		, { "abcdef", "abcdef",	0 }
		, { "01234567890123456789", "01234567890123456789",	0 }
		, { "\\", "\\\\",	0 }
		, { "\\001", "\\\\001",	0 }
		, { "01234567890123456789\\001", "01234567890123456789\\\\001",
			0 }
		, { NULL, NULL,	0 }
	};

	sm_test_begin(argc, argv, "test meta quoting");
	for (i = 0; inout[i].qic_in != NULL; i++)
	{
		obp = str2prt(inout[i].qic_in);
		cmp = strcmp(inout[i].qic_out, obp);
		SM_TEST(inout[i].qic_exp == cmp);
		if (inout[i].qic_exp != cmp && SmTestVerbose)
		{
			fprintf(stderr, "in: %s\n", inout[i].qic_in);
			fprintf(stderr, "got: %s\n", obp);
			fprintf(stderr, "exp: %s\n", inout[i].qic_out);
			fprintf(stderr, "cmp=%d\n", cmp);
		}
	}

	return sm_test_end();
}

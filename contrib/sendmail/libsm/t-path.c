/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Id: t-path.c,v 1.8 2001/09/11 04:04:49 gshapiro Exp $")

#include <string.h>
#include <sm/path.h>
#include <sm/test.h>

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *r;

	sm_test_begin(argc, argv, "test path handling");

	SM_TEST(sm_path_isdevnull(SM_PATH_DEVNULL));
	r = "/dev/null";
	SM_TEST(sm_path_isdevnull(r));
	r = "/nev/dull";
	SM_TEST(!sm_path_isdevnull(r));
	r = "nul";
	SM_TEST(!sm_path_isdevnull(r));

	return sm_test_end();
}

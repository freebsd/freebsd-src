/* 	$OpenBSD: test_argv.c,v 1.1 2021/03/19 04:23:50 djm Exp $ */
/*
 * Regress test for misc argv handling functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "log.h"
#include "misc.h"

void test_argv(void);

static void
free_argv(char **av, int ac)
{
	int i;

	for (i = 0; i < ac; i++)
		free(av[i]);
	free(av);
}

void
test_argv(void)
{
	char **av = NULL;
	int ac = 0;

#define RESET_ARGV() \
	do { \
		free_argv(av, ac); \
		av = NULL; \
		ac = -1; \
	} while (0)

	TEST_START("empty args");
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 0);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_PTR_EQ(av[0], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("    ", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 0);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_PTR_EQ(av[0], NULL);
	TEST_DONE();

	TEST_START("trivial args");
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("leamas", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "leamas");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("smiley leamas", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 2);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "smiley");
	ASSERT_STRING_EQ(av[1], "leamas");
	ASSERT_PTR_EQ(av[2], NULL);
	TEST_DONE();

	TEST_START("quoted");
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("\"smiley\"", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "smiley");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("leamas \" smiley \"", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 2);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "leamas");
	ASSERT_STRING_EQ(av[1], " smiley ");
	ASSERT_PTR_EQ(av[2], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("\"smiley leamas\"", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "smiley leamas");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("smiley\" leamas\" liz", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 2);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "smiley leamas");
	ASSERT_STRING_EQ(av[1], "liz");
	ASSERT_PTR_EQ(av[2], NULL);
	TEST_DONE();

	TEST_START("escaped");
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("\\\"smiley\\'", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "\"smiley'");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("'\\'smiley\\\"'", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "'smiley\"");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("smiley\\'s leamas\\'", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 2);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "smiley's");
	ASSERT_STRING_EQ(av[1], "leamas'");
	ASSERT_PTR_EQ(av[2], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("leamas\\\\smiley", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 1);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "leamas\\smiley");
	ASSERT_PTR_EQ(av[1], NULL);
	RESET_ARGV();
	ASSERT_INT_EQ(argv_split("leamas\\\\ \\\\smiley", &ac, &av), 0);
	ASSERT_INT_EQ(ac, 2);
	ASSERT_PTR_NE(av, NULL);
	ASSERT_STRING_EQ(av[0], "leamas\\");
	ASSERT_STRING_EQ(av[1], "\\smiley");
	ASSERT_PTR_EQ(av[2], NULL);
	TEST_DONE();

	/* XXX test char *argv_assemble(int argc, char **argv) */
}

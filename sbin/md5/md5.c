/*
 * Derived from:
 *
 * MDDRIVER.C - test driver for MD2, MD4 and MD5
 */

/*
 *  Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
 *  rights reserved.
 *
 *  RSA Data Security, Inc. makes no representations concerning either
 *  the merchantability of this software or the suitability of this
 *  software for any particular purpose. It is provided "as is"
 *  without express or implied warranty of any kind.
 *
 *  These notices must be retained in any copies of any part of this
 *  documentation and/or software.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 10000
#define TEST_BLOCK_COUNT 100000

int qflag;
int rflag;
int sflag;

static void MDString(const char *);
static void MDTimeTrial(void);
static void MDTestSuite(void);
static void MDFilter(int);
static void usage(void);

/* Main driver.

Arguments (may be any combination):
  -sstring - digests string
  -t       - runs time trial
  -x       - runs test script
  filename - digests file
  (none)   - digests standard input
 */
int
main(int argc, char *argv[])
{
	int     ch;
	char   *p;
	char	buf[33];

	while ((ch = getopt(argc, argv, "pqrs:tx")) != -1)
		switch (ch) {
		case 'p':
			MDFilter(1);
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			MDString(optarg);
			break;
		case 't':
			MDTimeTrial();
			break;
		case 'x':
			MDTestSuite();
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		do {
			p = MD5File(*argv, buf);
			if (!p)
				warn("%s", *argv);
			else
				if (qflag)
					printf("%s\n", p);
				else if (rflag)
					printf("%s %s\n", p, *argv);
				else
					printf("MD5 (%s) = %s\n", *argv, p);
		} while (*++argv);
	} else if (!sflag && (optind == 1 || qflag || rflag))
		MDFilter(0);

	return (0);
}
/*
 * Digests a string and prints the result.
 */
static void
MDString(const char *string)
{
	size_t len = strlen(string);
	char buf[33];

	if (qflag)
		printf("%s\n", MD5Data(string, len, buf));
	else if (rflag)
		printf("%s \"%s\"\n", MD5Data(string, len, buf), string);
	else
		printf("MD5 (\"%s\") = %s\n", string, MD5Data(string, len, buf));
}
/*
 * Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks.
 */
static void
MDTimeTrial(void)
{
	MD5_CTX context;
	time_t  endTime, startTime;
	unsigned char block[TEST_BLOCK_LEN];
	unsigned int i;
	char   *p, buf[33];

	printf
	    ("MD5 time trial. Digesting %d %d-byte blocks ...",
	    TEST_BLOCK_COUNT, TEST_BLOCK_LEN);
	fflush(stdout);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block[i] = (unsigned char) (i & 0xff);

	/* Start timer */
	time(&startTime);

	/* Digest blocks */
	MD5Init(&context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		MD5Update(&context, block, TEST_BLOCK_LEN);
	p = MD5End(&context,buf);

	/* Stop timer */
	time(&endTime);

	printf(" done\n");
	printf("Digest = %s", p);
	printf("\nTime = %ld seconds\n", (long) (endTime - startTime));
	/* Be careful that endTime-startTime is not zero. (Bug fix from Ric
	 * Anderson, ric@Artisoft.COM.) */
	printf
	    ("Speed = %ld bytes/second\n",
	    (long) TEST_BLOCK_LEN * (long) TEST_BLOCK_COUNT / ((endTime - startTime) != 0 ? (long)(endTime - startTime) : 1));
}
/*
 * Digests a reference suite of strings and prints the results.
 */
static void
MDTestSuite(void)
{

	printf("MD5 test suite:\n");

	MDString("");
	MDString("a");
	MDString("abc");
	MDString("message digest");
	MDString("abcdefghijklmnopqrstuvwxyz");
	MDString
	    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	MDString
	    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
}

/*
 * Digests the standard input and prints the result.
 */
static void
MDFilter(int tee)
{
	MD5_CTX context;
	unsigned int len;
	unsigned char buffer[BUFSIZ];
	char buf[33];

	MD5Init(&context);
	while ((len = fread(buffer, 1, BUFSIZ, stdin))) {
		if (tee && len != fwrite(buffer, 1, len, stdout))
			err(1, "stdout");
		MD5Update(&context, buffer, len);
	}
	printf("%s\n", MD5End(&context,buf));
}

static void
usage(void)
{

	fprintf(stderr, "usage: md5 [-pqrtx] [-s string] [files ...]\n");
	exit(1);
}

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
#include <ripemd.h>
#include <sha.h>
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

typedef void (DIGEST_Init)(void *);
typedef void (DIGEST_Update)(void *, const unsigned char *, size_t);
typedef char *(DIGEST_End)(void *, char *);

typedef struct Algorithm_t {
	const char *progname;
	const char *name;
	DIGEST_Init *Init;
	DIGEST_Update *Update;
	DIGEST_End *End;
	char *(*Data)(const unsigned char *, unsigned int, char *);
	char *(*File)(const char *, char *);
} Algorithm_t;

static void MD5_Update(MD5_CTX *, const unsigned char *, size_t);
static void MDString(Algorithm_t *, const char *);
static void MDTimeTrial(Algorithm_t *);
static void MDTestSuite(Algorithm_t *);
static void MDFilter(Algorithm_t *, int);
static void usage(Algorithm_t *);

typedef union {
	MD5_CTX md5;
	SHA1_CTX sha1;
	RIPEMD160_CTX ripemd160;
} DIGEST_CTX;

/* max(MD5_DIGEST_LENGTH, SHA_DIGEST_LENGTH, RIPEMD160_DIGEST_LENGTH)*2+1 */
#define HEX_DIGEST_LENGTH 41

/* algorithm function table */

struct Algorithm_t Algorithm[] = {
	{ "md5", "MD5", (DIGEST_Init*)&MD5Init,
		(DIGEST_Update*)&MD5_Update, (DIGEST_End*)&MD5End,
		&MD5Data, &MD5File },
	{ "sha1", "SHA1", (DIGEST_Init*)&SHA1_Init,
		(DIGEST_Update*)&SHA1_Update, (DIGEST_End*)&SHA1_End,
		&SHA1_Data, &SHA1_File },
	{ "rmd160", "RMD160",
		(DIGEST_Init*)&RIPEMD160_Init, (DIGEST_Update*)&RIPEMD160_Update,
		(DIGEST_End*)&RIPEMD160_End, &RIPEMD160_Data, &RIPEMD160_File }
};

static void
MD5_Update(MD5_CTX *c, const unsigned char *data, size_t len)
{
	MD5Update(c, data, len);
}

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
	char	buf[HEX_DIGEST_LENGTH];
 	unsigned	digest;
 	const char	*progname;
 
 	if ((progname = strrchr(argv[0], '/')) == NULL)
 		progname = argv[0];
 	else
 		progname++;
 
 	for (digest = 0; digest < sizeof(Algorithm)/sizeof(*Algorithm); digest++)
 		if (strcasecmp(Algorithm[digest].progname, progname) == 0)
 			break;
 
 	if (digest == sizeof(Algorithm)/sizeof(*Algorithm))
 		digest = 0;

	while ((ch = getopt(argc, argv, "pqrs:tx")) != -1)
		switch (ch) {
		case 'p':
			MDFilter(&Algorithm[digest], 1);
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			MDString(&Algorithm[digest], optarg);
			break;
		case 't':
			MDTimeTrial(&Algorithm[digest]);
			break;
		case 'x':
			MDTestSuite(&Algorithm[digest]);
			break;
		default:
			usage(&Algorithm[digest]);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		do {
			p = Algorithm[digest].File(*argv, buf);
			if (!p)
				warn("%s", *argv);
			else
				if (qflag)
					printf("%s\n", p);
				else if (rflag)
					printf("%s %s\n", p, *argv);
				else
					printf("%s (%s) = %s\n", Algorithm[digest].name, *argv, p);
		} while (*++argv);
	} else if (!sflag && (optind == 1 || qflag || rflag))
		MDFilter(&Algorithm[digest], 0);

	return (0);
}
/*
 * Digests a string and prints the result.
 */
static void
MDString(Algorithm_t *alg, const char *string)
{
	size_t len = strlen(string);
	char buf[HEX_DIGEST_LENGTH];

	if (qflag)
		printf("%s\n", alg->Data(string, len, buf));
	else if (rflag)
		printf("%s \"%s\"\n", alg->Data(string, len, buf), string);
	else
		printf("%s (\"%s\") = %s\n", alg->name, string, alg->Data(string, len, buf));
}
/*
 * Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks.
 */
static void
MDTimeTrial(Algorithm_t *alg)
{
	DIGEST_CTX context;
	time_t  endTime, startTime;
	unsigned char block[TEST_BLOCK_LEN];
	unsigned int i;
	char   *p, buf[HEX_DIGEST_LENGTH];

	printf
	    ("%s time trial. Digesting %d %d-byte blocks ...",
	    alg->name, TEST_BLOCK_COUNT, TEST_BLOCK_LEN);
	fflush(stdout);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block[i] = (unsigned char) (i & 0xff);

	/* Start timer */
	time(&startTime);

	/* Digest blocks */
	alg->Init(&context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		alg->Update(&context, block, TEST_BLOCK_LEN);
	p = alg->End(&context, buf);

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
MDTestSuite(Algorithm_t *alg)
{

	printf("%s test suite:\n", alg->name);

	MDString(alg, "");
	MDString(alg, "a");
	MDString(alg, "abc");
	MDString(alg, "message digest");
	MDString(alg, "abcdefghijklmnopqrstuvwxyz");
	MDString
	    (alg, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	MDString
	    (alg, "1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
}

/*
 * Digests the standard input and prints the result.
 */
static void
MDFilter(Algorithm_t *alg, int tee)
{
	DIGEST_CTX context;
	unsigned int len;
	unsigned char buffer[BUFSIZ];
	char buf[HEX_DIGEST_LENGTH];

	alg->Init(&context);
	while ((len = fread(buffer, 1, BUFSIZ, stdin))) {
		if (tee && len != fwrite(buffer, 1, len, stdout))
			err(1, "stdout");
		alg->Update(&context, buffer, len);
	}
	printf("%s\n", alg->End(&context, buf));
}

static void
usage(Algorithm_t *alg)
{

	fprintf(stderr, "usage: %s [-pqrtx] [-s string] [files ...]\n", alg->progname);
	exit(1);
}

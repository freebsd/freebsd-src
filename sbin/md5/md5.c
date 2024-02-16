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

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <md5.h>
#include <osreldate.h>
#include <ripemd.h>
#include <sha.h>
#include <sha224.h>
#include <sha256.h>
#include <sha384.h>
#include <sha512.h>
#include <sha512t.h>
#include <skein.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_CAPSICUM
#include <sys/capsicum.h>
#include <capsicum_helpers.h>
#include <libcasper.h>
#include <casper/cap_fileargs.h>
#endif

/*
 * Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 10000
#define TEST_BLOCK_COUNT 100000
#define MDTESTCOUNT 8

static char *progname;

static bool cflag;
static bool pflag;
static bool qflag;
static bool sflag;
static bool wflag;
static bool strict;
static bool skip;
static bool ignoreMissing;
static char* checkAgainst;
static int checksFailed;
static bool failed;
static int endl = '\n';

typedef void (DIGEST_Init)(void *);
typedef void (DIGEST_Update)(void *, const unsigned char *, size_t);
typedef char *(DIGEST_End)(void *, char *);

extern const char *MD5TestOutput[MDTESTCOUNT];
extern const char *SHA1_TestOutput[MDTESTCOUNT];
extern const char *SHA224_TestOutput[MDTESTCOUNT];
extern const char *SHA256_TestOutput[MDTESTCOUNT];
extern const char *SHA384_TestOutput[MDTESTCOUNT];
extern const char *SHA512_TestOutput[MDTESTCOUNT];
extern const char *SHA512t224_TestOutput[MDTESTCOUNT];
extern const char *SHA512t256_TestOutput[MDTESTCOUNT];
extern const char *RIPEMD160_TestOutput[MDTESTCOUNT];
extern const char *SKEIN256_TestOutput[MDTESTCOUNT];
extern const char *SKEIN512_TestOutput[MDTESTCOUNT];
extern const char *SKEIN1024_TestOutput[MDTESTCOUNT];

typedef struct Algorithm_t {
	const char *progname;
	const char *perlname;
	const char *name;
	const char *(*TestOutput)[MDTESTCOUNT];
	DIGEST_Init *Init;
	DIGEST_Update *Update;
	DIGEST_End *End;
	char *(*Data)(const void *, unsigned int, char *);
} Algorithm_t;

static void MD5_Update(MD5_CTX *, const unsigned char *, size_t);
static char *MDInput(const Algorithm_t *, FILE *, char *, bool);
static void MDOutput(const Algorithm_t *, char *, const char *);
static void MDTimeTrial(const Algorithm_t *);
static void MDTestSuite(const Algorithm_t *);
static void usage(const Algorithm_t *);
static void version(void);

typedef union {
	MD5_CTX md5;
	SHA1_CTX sha1;
	SHA224_CTX sha224;
	SHA256_CTX sha256;
	SHA384_CTX sha384;
	SHA512_CTX sha512;
	RIPEMD160_CTX ripemd160;
	SKEIN256_CTX skein256;
	SKEIN512_CTX skein512;
	SKEIN1024_CTX skein1024;
} DIGEST_CTX;

/* max(MD5_DIGEST_LENGTH, SHA_DIGEST_LENGTH,
	SHA256_DIGEST_LENGTH, SHA512_DIGEST_LENGTH,
	RIPEMD160_DIGEST_LENGTH, SKEIN1024_DIGEST_LENGTH)*2+1 */
#define HEX_DIGEST_LENGTH 257

/* algorithm function table */

static const struct Algorithm_t Algorithm[] = {
	{ "md5", NULL, "MD5",
		&MD5TestOutput, (DIGEST_Init*)&MD5Init,
		(DIGEST_Update*)&MD5_Update, (DIGEST_End*)&MD5End,
		&MD5Data },
	{ "sha1", "1", "SHA1",
		&SHA1_TestOutput, (DIGEST_Init*)&SHA1_Init,
		(DIGEST_Update*)&SHA1_Update, (DIGEST_End*)&SHA1_End,
		&SHA1_Data },
	{ "sha224", "224", "SHA224",
		&SHA224_TestOutput, (DIGEST_Init*)&SHA224_Init,
		(DIGEST_Update*)&SHA224_Update, (DIGEST_End*)&SHA224_End,
		&SHA224_Data },
	{ "sha256", "256", "SHA256",
		&SHA256_TestOutput, (DIGEST_Init*)&SHA256_Init,
		(DIGEST_Update*)&SHA256_Update, (DIGEST_End*)&SHA256_End,
		&SHA256_Data },
	{ "sha384", "384", "SHA384",
		&SHA384_TestOutput, (DIGEST_Init*)&SHA384_Init,
		(DIGEST_Update*)&SHA384_Update, (DIGEST_End*)&SHA384_End,
		&SHA384_Data },
	{ "sha512", "512", "SHA512",
		&SHA512_TestOutput, (DIGEST_Init*)&SHA512_Init,
		(DIGEST_Update*)&SHA512_Update, (DIGEST_End*)&SHA512_End,
		&SHA512_Data },
	{ "sha512t224", "512224", "SHA512t224",
		&SHA512t224_TestOutput, (DIGEST_Init*)&SHA512_224_Init,
		(DIGEST_Update*)&SHA512_224_Update, (DIGEST_End*)&SHA512_224_End,
		&SHA512_224_Data },
	{ "sha512t256", "512256", "SHA512t256",
		&SHA512t256_TestOutput, (DIGEST_Init*)&SHA512_256_Init,
		(DIGEST_Update*)&SHA512_256_Update, (DIGEST_End*)&SHA512_256_End,
		&SHA512_256_Data },
	{ "rmd160", NULL, "RMD160",
		&RIPEMD160_TestOutput,
		(DIGEST_Init*)&RIPEMD160_Init, (DIGEST_Update*)&RIPEMD160_Update,
		(DIGEST_End*)&RIPEMD160_End, &RIPEMD160_Data },
	{ "skein256", NULL, "Skein256",
		&SKEIN256_TestOutput,
		(DIGEST_Init*)&SKEIN256_Init, (DIGEST_Update*)&SKEIN256_Update,
		(DIGEST_End*)&SKEIN256_End, &SKEIN256_Data },
	{ "skein512", NULL, "Skein512",
		&SKEIN512_TestOutput,
		(DIGEST_Init*)&SKEIN512_Init, (DIGEST_Update*)&SKEIN512_Update,
		(DIGEST_End*)&SKEIN512_End, &SKEIN512_Data },
	{ "skein1024", NULL, "Skein1024",
		&SKEIN1024_TestOutput,
		(DIGEST_Init*)&SKEIN1024_Init, (DIGEST_Update*)&SKEIN1024_Update,
		(DIGEST_End*)&SKEIN1024_End, &SKEIN1024_Data },
	{ }
};

static int digest = -1;
static unsigned int malformed;

static enum mode {
	mode_bsd,
	mode_gnu,
	mode_perl,
} mode = mode_bsd;

static enum input_mode {
	input_binary	 = '*',
	input_text	 = ' ',
	input_universal	 = 'U',
	input_bits	 = '^',
} input_mode = input_binary;

static enum output_mode {
	output_bare,
	output_tagged,
	output_reverse,
	output_gnu,
} output_mode = output_tagged;

enum optval {
	opt_end = -1,
	/* ensure we don't collide with shortopts */
	opt_dummy = CHAR_MAX,
	/* BSD options */
	opt_check,
	opt_passthrough,
	opt_quiet,
	opt_reverse,
	opt_string,
	opt_time_trial,
	opt_self_test,
	/* GNU options */
	opt_binary,
	opt_help,
	opt_ignore_missing,
	opt_status,
	opt_strict,
	opt_tag,
	opt_text,
	opt_warn,
	opt_version,
	opt_zero,
	/* Perl options */
	opt_algorithm,
	opt_bits,
	opt_universal,
};

static const struct option bsd_longopts[] = {
	{ "check",		required_argument,	0, opt_check },
	{ "passthrough",	no_argument,		0, opt_passthrough },
	{ "quiet",		no_argument,		0, opt_quiet },
	{ "reverse",		no_argument,		0, opt_reverse },
	{ "string",		required_argument,	0, opt_string },
	{ "time-trial",		no_argument,		0, opt_time_trial },
	{ "self-test",		no_argument,		0, opt_self_test },
	{ }
};
static const char *bsd_shortopts = "bc:pqrs:tx";

static const struct option gnu_longopts[] = {
	{ "binary",		no_argument,		0, opt_binary },
	{ "check",		no_argument,		0, opt_check },
	{ "help",		no_argument,		0, opt_help },
	{ "ignore-missing",	no_argument,		0, opt_ignore_missing },
	{ "quiet",		no_argument,		0, opt_quiet },
	{ "status",		no_argument,		0, opt_status },
	{ "strict",		no_argument,		0, opt_strict },
	{ "tag",		no_argument,		0, opt_tag },
	{ "text",		no_argument,		0, opt_text },
	{ "version",		no_argument,		0, opt_version },
	{ "warn",		no_argument,		0, opt_warn },
	{ "zero",		no_argument,		0, opt_zero },
	{ }
};
static const char *gnu_shortopts = "bctwz";

static const struct option perl_longopts[] = {
	{ "algorithm",		required_argument,	0, opt_algorithm },
	{ "check",		required_argument,	0, opt_check },
	{ "help",		no_argument,		0, opt_help },
	{ "ignore-missing",	no_argument,		0, opt_ignore_missing },
	{ "quiet",		no_argument,		0, opt_quiet },
	{ "status",		no_argument,		0, opt_status },
	{ "strict",		no_argument,		0, opt_strict },
	{ "tag",		no_argument,		0, opt_tag },
	{ "text",		no_argument,		0, opt_text },
	{ "UNIVERSAL",		no_argument,		0, opt_universal },
	{ "version",		no_argument,		0, opt_version },
	{ "warn",		no_argument,		0, opt_warn },
	{ "01",			no_argument,		0, opt_bits },
	{ }
};
static const char *perl_shortopts = "0a:bchqstUvw";

static void
MD5_Update(MD5_CTX *c, const unsigned char *data, size_t len)
{
	MD5Update(c, data, len);
}

struct chksumrec {
	char	*filename;
	char	*chksum;
	struct	chksumrec	*next;
};

static struct chksumrec *head = NULL;
static struct chksumrec **next = &head;
static unsigned int numrecs;

#define PADDING	7	/* extra padding for "SHA512t256 (...) = ...\n" style */
#define CHKFILELINELEN	(HEX_DIGEST_LENGTH + MAXPATHLEN + PADDING)

static void
gnu_check(const char *checksumsfile)
{
	FILE	*inp;
	char	*linebuf = NULL;
	size_t	linecap;
	ssize_t	linelen;
	int	lineno;
	char	*filename;
	char	*hashstr;
	struct chksumrec	*rec;
	const char	*digestname;
	size_t	digestnamelen;
	size_t	hashstrlen;
	struct stat st;

	if (strcmp(checksumsfile, "-") == 0)
		inp = stdin;
	else if ((inp = fopen(checksumsfile, "r")) == NULL)
		err(1, "%s", checksumsfile);
	digestname = Algorithm[digest].name;
	digestnamelen = strlen(digestname);
	hashstrlen = strlen(*(Algorithm[digest].TestOutput[0]));
	lineno = 0;
	linecap = CHKFILELINELEN;
	while ((linelen = getline(&linebuf, &linecap, inp)) > 0) {
		lineno++;
		while (linelen > 0 && linebuf[linelen - 1] == '\n')
			linelen--;
		linebuf[linelen] = '\0';
		filename = linebuf + digestnamelen + 2;
		hashstr = linebuf + linelen - hashstrlen;
		/*
		 * supported formats:
		 * BSD: <DigestName> (<Filename>): <Digest>
		 * GNU: <Digest> [ *U^]<Filename>
		 */
		if ((size_t)linelen >= digestnamelen + hashstrlen + 6 &&
		    strncmp(linebuf, digestname, digestnamelen) == 0 &&
		    strncmp(filename - 2, " (", 2) == 0 &&
		    strncmp(hashstr - 4, ") = ", 4) == 0 &&
		    strspn(hashstr, "0123456789ABCDEFabcdef") == hashstrlen) {
			*(hashstr - 4) = '\0';
		} else if ((size_t)linelen >= hashstrlen + 3 &&
		    strspn(linebuf, "0123456789ABCDEFabcdef") == hashstrlen &&
		    linebuf[hashstrlen] == ' ') {
			linebuf[hashstrlen] = '\0';
			hashstr = linebuf;
			filename = linebuf + hashstrlen + 1;
		} else {
			if (wflag) {
				warnx("%s: %d: improperly formatted "
				    "%s checksum line",
				    checksumsfile, lineno,
				    mode == mode_perl ? "SHA" : digestname);
			}
			malformed++;
			continue;
		}
		rec = malloc(sizeof(*rec));
		if (rec == NULL)
			errx(1, "malloc failed");

		if (*filename == '*' ||
		    *filename == ' ' ||
		    *filename == 'U' ||
		    *filename == '^') {
			if (lstat(filename, &st) != 0)
				filename++;
		}

		rec->chksum = strdup(hashstr);
		rec->filename = strdup(filename);
		if (rec->chksum == NULL || rec->filename == NULL)
			errx(1, "malloc failed");
		rec->next = NULL;
		*next = rec;
		next = &rec->next;
		numrecs++;
	}
	if (inp != stdin)
		fclose(inp);
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
#ifdef HAVE_CAPSICUM
	cap_rights_t	rights;
	fileargs_t	*fa = NULL;
#endif
	const struct option *longopts;
	const char *shortopts;
	FILE   *f;
	int	i, opt;
	char   *p, *string = NULL;
	char	buf[HEX_DIGEST_LENGTH];
	size_t	len;
	struct chksumrec	*rec;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	/*
	 * GNU coreutils has a number of programs named *sum. These produce
	 * similar results to the BSD version, but in a different format,
	 * similar to BSD's -r flag. We install links to this program with
	 * ending 'sum' to provide this compatibility. Check here to see if the
	 * name of the program ends in 'sum', set the flag and drop the 'sum' so
	 * the digest lookup works. Also, make -t a nop when running in this mode
	 * since that means 'text file' there (though it's a nop in coreutils
	 * on unix-like systems). The -c flag conflicts, so it's just disabled
	 * in this mode (though in the future it might be implemented).
	 *
	 * We also strive to be compatible with the shasum script which is
	 * included in Perl.  It is roughly equivalent to the GNU offering
	 * but uses a command-line argument to select the algorithm, and
	 * supports only SHA-1 and SHA-2.
	 */
	len = strlen(progname);
	if (strcmp(progname, "shasum") == 0) {
		mode = mode_perl;
		input_mode = input_text;
		output_mode = output_gnu;
		digest = 1;
		longopts = perl_longopts;
		shortopts = perl_shortopts;
	} else if (len > 3 && strcmp(progname + len - 3, "sum") == 0) {
		len -= 3;
		mode = mode_gnu;
		input_mode = input_text;
		/*
		 * The historical behavior in GNU emulation mode is
		 * output_reverse, however this not true to the original
		 * and the flag that was used to force the correct output
		 * was -b, which means something else (input_binary) in
		 * GNU land.  Switch to the correct behavior.
		 */
		output_mode = output_gnu;
		longopts = gnu_longopts;
		shortopts = gnu_shortopts;
	} else {
		mode = mode_bsd;
		input_mode = input_binary;
		output_mode = output_tagged;
		longopts = bsd_longopts;
		shortopts = bsd_shortopts;
	}

	if (digest < 0) {
		for (digest = 0; Algorithm[digest].progname != NULL; digest++)
			if (strncasecmp(Algorithm[digest].progname, progname, len) == 0)
				break;

		if (Algorithm[digest].progname == NULL)
			digest = 0;
	}

	failed = false;
	checkAgainst = NULL;
	checksFailed = 0;
	skip = false;
	while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != opt_end)
		switch (opt) {
		case opt_bits:
		case '0':
			input_mode = input_bits;
			break;
		case opt_algorithm:
		case 'a':
			for (i = 0; Algorithm[i].progname != NULL; i++) {
				if (Algorithm[i].perlname != NULL &&
				    strcasecmp(Algorithm[i].perlname, optarg) == 0) {
					digest = i;
					break;
				}
			}
			if (Algorithm[i].progname == NULL)
				usage(&Algorithm[digest]);
			break;
		case opt_binary:
		case 'b':
			/* in BSD mode, -b is now a no-op */
			if (mode != mode_bsd)
				input_mode = input_binary;
			break;
		case opt_check:
		case 'c':
			cflag = true;
			if (mode == mode_bsd)
				checkAgainst = optarg;
			break;
		case opt_passthrough:
		case 'p':
			pflag = true;
			break;
		case opt_quiet:
		case 'q':
			output_mode = output_bare;
			qflag = true;
			break;
		case opt_reverse:
		case 'r':
			if (!qflag)
				output_mode = output_reverse;
			break;
		case opt_status:
			sflag = true;
			break;
		case opt_strict:
			strict = 1;
			break;
		case 's':
			if (mode == mode_perl) {
				sflag = true;
				break;
			}
			/* fall through */
		case opt_string:
			output_mode = output_bare;
			string = optarg;
			break;
		case opt_tag:
			output_mode = output_tagged;
			break;
		case opt_time_trial:
		case opt_text:
		case 't':
			if (mode == mode_bsd) {
				MDTimeTrial(&Algorithm[digest]);
				skip = true;
			} else {
				input_mode = input_text;
			}
			break;
		case opt_universal:
		case 'U':
			input_mode = input_universal;
			break;
		case opt_version:
			version();
			break;
		case opt_warn:
		case 'w':
			wflag = true;
			break;
		case opt_self_test:
		case 'x':
			MDTestSuite(&Algorithm[digest]);
			skip = true;
			break;
		case opt_zero:
		case 'z':
			endl = '\0';
			break;
		case opt_ignore_missing:
			ignoreMissing = true;
			break;
		default:
			usage(&Algorithm[digest]);
		}
	argc -= optind;
	argv += optind;

#ifdef HAVE_CAPSICUM
	if (caph_limit_stdout() < 0 || caph_limit_stderr() < 0)
		err(1, "unable to limit rights for stdio");
#endif

	if (cflag && mode != mode_bsd) {
		/*
		 * Read digest files into a linked list, then replace argv
		 * with an array of the filenames from that list.
		 */
		if (argc < 1)
			usage(&Algorithm[digest]);
		while (argc--)
			gnu_check(*argv++);
		argc = 0;
		argv = calloc(sizeof(char *), numrecs + 1);
		for (rec = head; rec != NULL; rec = rec->next) {
			argv[argc] = rec->filename;
			argc++;
		}
		argv[argc] = NULL;
		rec = head;
	}

#ifdef HAVE_CAPSICUM
	fa = fileargs_init(argc, argv, O_RDONLY, 0,
	    cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_FCNTL), FA_OPEN | FA_LSTAT);
	if (fa == NULL)
		err(1, "Unable to initialize casper");
	if (caph_enter_casper() < 0)
		err(1, "Unable to enter capability mode");
#endif

	if (*argv && !pflag && string == NULL) {
		do {
			const char *filename = *argv;
			const char *filemode = "rb";

			if (strcmp(filename, "-") == 0) {
				f = stdin;
			} else {
#ifdef HAVE_CAPSICUM
				f = fileargs_fopen(fa, filename, filemode);
#else
				f = fopen(filename, filemode);
#endif
			}
			if (f == NULL) {
				if (errno != ENOENT || !(cflag && ignoreMissing)) {
					warn("%s", filename);
					failed = true;
				}
				if (cflag && mode != mode_bsd)
					rec = rec->next;
				continue;
			}
#ifdef HAVE_CAPSICUM
			if (caph_rights_limit(fileno(f), &rights) < 0)
				err(1, "capsicum");
#endif
			if (cflag && mode != mode_bsd) {
				checkAgainst = rec->chksum;
				rec = rec->next;
			}
			p = MDInput(&Algorithm[digest], f, buf, false);
			if (f != stdin)
				(void)fclose(f);
			MDOutput(&Algorithm[digest], p, filename);
		} while (*++argv);
	} else if (!cflag && string == NULL && !skip) {
#ifdef HAVE_CAPSICUM
		if (caph_limit_stdin() < 0)
			err(1, "capsicum");
#endif
		if (mode == mode_bsd)
			output_mode = output_bare;
		p = MDInput(&Algorithm[digest], stdin, buf, pflag);
		MDOutput(&Algorithm[digest], p, "-");
	} else if (string != NULL) {
		len = strlen(string);
		p = Algorithm[digest].Data(string, len, buf);
		MDOutput(&Algorithm[digest], p, string);
	}
	if (cflag && mode != mode_bsd) {
		if (!sflag && malformed > 1)
			warnx("WARNING: %d lines are improperly formatted", malformed);
		else if (!sflag && malformed > 0)
			warnx("WARNING: %d line is improperly formatted", malformed);
		if (!sflag && checksFailed > 1)
			warnx("WARNING: %d computed checksums did NOT match", checksFailed);
		else if (!sflag && checksFailed > 0)
			warnx("WARNING: %d computed checksum did NOT match", checksFailed);
		if (checksFailed != 0 || (strict && malformed > 0))
			return (1);
	}
#ifdef HAVE_CAPSICUM
	fileargs_free(fa);
#endif
	if (failed)
		return (1);
	if (checksFailed > 0)
		return (2);

	return (0);
}

/*
 * Common input handling
 */
static char *
MDInput(const Algorithm_t *alg, FILE *f, char *buf, bool tee)
{
	char block[4096];
	DIGEST_CTX context;
	char *end, *p, *q;
	size_t len;
	int bits;
	uint8_t byte;
	bool cr = false;

	alg->Init(&context);
	while ((len = fread(block, 1, sizeof(block), f)) > 0) {
		switch (input_mode) {
		case input_binary:
		case input_text:
			if (tee && fwrite(block, 1, len, stdout) != len)
				err(1, "stdout");
			alg->Update(&context, block, len);
			break;
		case input_universal:
			end = block + len;
			for (p = q = block; p < end; p = q) {
				if (cr) {
					if (*p == '\n')
						p++;
					if (tee && putchar('\n') == EOF)
						err(1, "stdout");
					alg->Update(&context, "\n", 1);
					cr = false;
				}
				for (q = p; q < end && *q != '\r'; q++)
					/* nothing */;
				if (q > p) {
					if (tee &&
					    fwrite(p, 1, q - p, stdout) !=
					    (size_t)(q - p))
						err(1, "stdout");
					alg->Update(&context, p, q - p);
				}
				if (q < end && *q == '\r') {
					cr = true;
					q++;
				}
			}
			break;
		case input_bits:
			end = block + len;
			bits = byte = 0;
			for (p = block; p < end; p++) {
				if (*p == '0' || *p == '1') {
					byte <<= 1;
					byte |= *p - '0';
					if (++bits == 8) {
						if (tee && putchar(byte) == EOF)
							err(1, "stdout");
						alg->Update(&context, &byte, 1);
						bits = byte = 0;
					}
				}
			}
			break;
		}
	}
	if (ferror(f)) {
		alg->End(&context, buf);
		return (NULL);
	}
	if (cr) {
		if (tee && putchar('\n') == EOF)
			err(1, "stdout");
		alg->Update(&context, "\n", 1);
	}
	if (input_mode == input_bits && bits != 0)
		errx(1, "input length was not a multiple of 8");
	return (alg->End(&context, buf));
}

/*
 * Common output handling
 */
static void
MDOutput(const Algorithm_t *alg, char *p, const char *name)
{
	bool checkfailed = false;

	if (p == NULL) {
		warn("%s", name);
		failed = true;
	} else if (cflag && mode != mode_bsd) {
		checkfailed = strcasecmp(checkAgainst, p) != 0;
		if (!sflag && (!qflag || checkfailed))
			printf("%s: %s%c", name, checkfailed ? "FAILED" : "OK",
			    endl);
	} else {
		switch (output_mode) {
		case output_bare:
			printf("%s", p);
			break;
		case output_gnu:
			printf("%s %c%s", p, input_mode, name);
			break;
		case output_reverse:
			printf("%s %s", p, name);
			break;
		case output_tagged:
			if (mode == mode_perl &&
			    strncmp(alg->name, "SHA512t", 7) == 0) {
				printf("%.6s/%s", alg->name, alg->name + 7);
			} else {
				printf("%s", alg->name);
			}
			printf(" (%s) = %s", name, p);
			break;
		}
		if (checkAgainst) {
			checkfailed = strcasecmp(checkAgainst, p) != 0;
			if (!qflag && checkfailed)
				printf(" [ Failed ]");
		}
		printf("%c", endl);
	}
	if (checkfailed)
		checksFailed++;
}

/*
 * Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks.
 */
static void
MDTimeTrial(const Algorithm_t *alg)
{
	DIGEST_CTX context;
	struct rusage before, after;
	struct timeval total;
	float seconds;
	unsigned char block[TEST_BLOCK_LEN];
	unsigned int i;
	char *p, buf[HEX_DIGEST_LENGTH];

	printf("%s time trial. Digesting %d %d-byte blocks ...",
	    alg->name, TEST_BLOCK_COUNT, TEST_BLOCK_LEN);
	fflush(stdout);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block[i] = (unsigned char) (i & 0xff);

	/* Start timer */
	getrusage(RUSAGE_SELF, &before);

	/* Digest blocks */
	alg->Init(&context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		alg->Update(&context, block, TEST_BLOCK_LEN);
	p = alg->End(&context, buf);

	/* Stop timer */
	getrusage(RUSAGE_SELF, &after);
	timersub(&after.ru_utime, &before.ru_utime, &total);
	seconds = total.tv_sec + (float) total.tv_usec / 1000000;

	printf(" done\n");
	printf("Digest = %s", p);
	printf("\nTime = %f seconds\n", seconds);
	printf("Speed = %f MiB/second\n", (float) TEST_BLOCK_LEN *
		(float) TEST_BLOCK_COUNT / seconds / (1 << 20));
}
/*
 * Digests a reference suite of strings and prints the results.
 */

static const char *MDTestInput[MDTESTCOUNT] = {
	"",
	"a",
	"abc",
	"message digest",
	"abcdefghijklmnopqrstuvwxyz",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	"MD5 has not yet (2001-09-03) been broken, but sufficient attacks have been made \
that its security is in some doubt"
};

const char *MD5TestOutput[MDTESTCOUNT] = {
	"d41d8cd98f00b204e9800998ecf8427e",
	"0cc175b9c0f1b6a831c399e269772661",
	"900150983cd24fb0d6963f7d28e17f72",
	"f96b697d7cb7938d525a2f31aaf161d0",
	"c3fcd3d76192e4007dfb496cca67e13b",
	"d174ab98d277d9f5a5611c2c9f419d9f",
	"57edf4a22be3c955ac49da2e2107b67a",
	"b50663f41d44d92171cb9976bc118538"
};

const char *SHA1_TestOutput[MDTESTCOUNT] = {
	"da39a3ee5e6b4b0d3255bfef95601890afd80709",
	"86f7e437faa5a7fce15d1ddcb9eaeaea377667b8",
	"a9993e364706816aba3e25717850c26c9cd0d89d",
	"c12252ceda8be8994d5fa0290a47231c1d16aae3",
	"32d10c7b8cf96570ca04ce37f2a19d84240d3a89",
	"761c457bf73b14d27e9e9265c46f4b4dda11f940",
	"50abf5706a150990a08b2c5ea40fa0e585554732",
	"18eca4333979c4181199b7b4fab8786d16cf2846"
};

const char *SHA224_TestOutput[MDTESTCOUNT] = {
	"d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
	"abd37534c7d9a2efb9465de931cd7055ffdb8879563ae98078d6d6d5",
	"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
	"2cb21c83ae2f004de7e81c3c7019cbcb65b71ab656b22d6d0c39b8eb",
	"45a5f72c39c5cff2522eb3429799e49e5f44b356ef926bcf390dccc2",
	"bff72b4fcb7d75e5632900ac5f90d219e05e97a7bde72e740db393d9",
	"b50aecbe4e9bb0b57bc5f3ae760a8e01db24f203fb3cdcd13148046e",
	"5ae55f3779c8a1204210d7ed7689f661fbe140f96f272ab79e19d470"
};

const char *SHA256_TestOutput[MDTESTCOUNT] = {
	"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
	"ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb",
	"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
	"f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650",
	"71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73",
	"db4bfcbd4da0cd85a60c3c37d3fbd8805c77f15fc6b1fdfe614ee0a7c8fdb4c0",
	"f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e",
	"e6eae09f10ad4122a0e2a4075761d185a272ebd9f5aa489e998ff2f09cbfdd9f"
};

const char *SHA384_TestOutput[MDTESTCOUNT] = {
	"38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
	"54a59b9f22b0b80880d8427e548b7c23abd873486e1f035dce9cd697e85175033caa88e6d57bc35efae0b5afd3145f31",
	"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
	"473ed35167ec1f5d8e550368a3db39be54639f828868e9454c239fc8b52e3c61dbd0d8b4de1390c256dcbb5d5fd99cd5",
	"feb67349df3db6f5924815d6c3dc133f091809213731fe5c7b5f4999e463479ff2877f5f2936fa63bb43784b12f3ebb4",
	"1761336e3f7cbfe51deb137f026f89e01a448e3b1fafa64039c1464ee8732f11a5341a6f41e0c202294736ed64db1a84",
	"b12932b0627d1c060942f5447764155655bd4da0c9afa6dd9b9ef53129af1b8fb0195996d2de9ca0df9d821ffee67026",
	"99428d401bf4abcd4ee0695248c9858b7503853acfae21a9cffa7855f46d1395ef38596fcd06d5a8c32d41a839cc5dfb"
};

const char *SHA512_TestOutput[MDTESTCOUNT] = {
	"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
	"1f40fc92da241694750979ee6cf582f2d5d7d28e18335de05abc54d0560e0f5302860c652bf08d560252aa5e74210546f369fbbbce8c12cfc7957b2652fe9a75",
	"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
	"107dbf389d9e9f71a3a95f6c055b9251bc5268c2be16d6c13492ea45b0199f3309e16455ab1e96118e8a905d5597b72038ddb372a89826046de66687bb420e7c",
	"4dbff86cc2ca1bae1e16468a05cb9881c97f1753bce3619034898faa1aabe429955a1bf8ec483d7421fe3c1646613a59ed5441fb0f321389f77f48a879c7b1f1",
	"1e07be23c26a86ea37ea810c8ec7809352515a970e9253c26f536cfc7a9996c45c8370583e0a78fa4a90041d71a4ceab7423f19c71b9d5a3e01249f0bebd5894",
	"72ec1ef1124a45b047e8b7c75a932195135bb61de24ec0d1914042246e0aec3a2354e093d76f3048b456764346900cb130d2a4fd5dd16abb5e30bcb850dee843",
	"e8a835195e039708b13d9131e025f4441dbdc521ce625f245a436dcd762f54bf5cb298d96235e6c6a304e087ec8189b9512cbdf6427737ea82793460c367b9c3"
};

const char *SHA512t224_TestOutput[MDTESTCOUNT] = {
	"6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4",
	"d5cdb9ccc769a5121d4175f2bfdd13d6310e0d3d361ea75d82108327",
	"4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa",
	"ad1a4db188fe57064f4f24609d2a83cd0afb9b398eb2fcaeaae2c564",
	"ff83148aa07ec30655c1b40aff86141c0215fe2a54f767d3f38743d8",
	"a8b4b9174b99ffc67d6f49be9981587b96441051e16e6dd036b140d3",
	"ae988faaa47e401a45f704d1272d99702458fea2ddc6582827556dd2",
	"b3c3b945249b0c8c94aba76ea887bcaad5401665a1fbeb384af4d06b"
};

const char *SHA512t256_TestOutput[MDTESTCOUNT] = {
	"c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a",
	"455e518824bc0601f9fb858ff5c37d417d67c2f8e0df2babe4808858aea830f8",
	"53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23",
	"0cf471fd17ed69d990daf3433c89b16d63dec1bb9cb42a6094604ee5d7b4e9fb",
	"fc3189443f9c268f626aea08a756abe7b726b05f701cb08222312ccfd6710a26",
	"cdf1cc0effe26ecc0c13758f7b4a48e000615df241284185c39eb05d355bb9c8",
	"2c9fdbc0c90bdd87612ee8455474f9044850241dc105b1e8b94b8ddf5fac9148",
	"dd095fc859b336c30a52548b3dc59fcc0d1be8616ebcf3368fad23107db2d736"
};

const char *RIPEMD160_TestOutput[MDTESTCOUNT] = {
	"9c1185a5c5e9fc54612808977ee8f548b2258d31",
	"0bdc9d2d256b3ee9daae347be6f4dc835a467ffe",
	"8eb208f7e05d987a9b044a8e98c6b087f15a0bfc",
	"5d0689ef49d2fae572b881b123a85ffa21595f36",
	"f71c27109c692c1b56bbdceb5b9d2865b3708dbc",
	"b0e20b6e3116640286ed3a87a5713079b21f5189",
	"9b752e45573d4b39f4dbd3323cab82bf63326bfb",
	"5feb69c6bf7c29d95715ad55f57d8ac5b2b7dd32"
};

const char *SKEIN256_TestOutput[MDTESTCOUNT] = {
	"c8877087da56e072870daa843f176e9453115929094c3a40c463a196c29bf7ba",
	"7fba44ff1a31d71a0c1f82e6e82fb5e9ac6c92a39c9185b9951fed82d82fe635",
	"258bdec343b9fde1639221a5ae0144a96e552e5288753c5fec76c05fc2fc1870",
	"4d2ce0062b5eb3a4db95bc1117dd8aa014f6cd50fdc8e64f31f7d41f9231e488",
	"46d8440685461b00e3ddb891b2ecc6855287d2bd8834a95fb1c1708b00ea5e82",
	"7c5eb606389556b33d34eb2536459528dc0af97adbcd0ce273aeb650f598d4b2",
	"4def7a7e5464a140ae9c3a80279fbebce4bd00f9faad819ab7e001512f67a10d",
	"d9c017dbe355f318d036469eb9b5fbe129fc2b5786a9dc6746a516eab6fe0126"
};

const char *SKEIN512_TestOutput[MDTESTCOUNT] = {
	"bc5b4c50925519c290cc634277ae3d6257212395cba733bbad37a4af0fa06af41fca7903d06564fea7a2d3730dbdb80c1f85562dfcc070334ea4d1d9e72cba7a",
	"b1cd8d33f61b3737adfd59bb13ad82f4a9548e92f22956a8976cca3fdb7fee4fe91698146c4197cec85d38b83c5d93bdba92c01fd9a53870d0c7f967bc62bdce",
	"8f5dd9ec798152668e35129496b029a960c9a9b88662f7f9482f110b31f9f93893ecfb25c009baad9e46737197d5630379816a886aa05526d3a70df272d96e75",
	"15b73c158ffb875fed4d72801ded0794c720b121c0c78edf45f900937e6933d9e21a3a984206933d504b5dbb2368000411477ee1b204c986068df77886542fcc",
	"23793ad900ef12f9165c8080da6fdfd2c8354a2929b8aadf83aa82a3c6470342f57cf8c035ec0d97429b626c4d94f28632c8f5134fd367dca5cf293d2ec13f8c",
	"0c6bed927e022f5ddcf81877d42e5f75798a9f8fd3ede3d83baac0a2f364b082e036c11af35fe478745459dd8f5c0b73efe3c56ba5bb2009208d5a29cc6e469c",
	"2ca9fcffb3456f297d1b5f407014ecb856f0baac8eb540f534b1f187196f21e88f31103128c2f03fcc9857d7a58eb66f9525e2302d88833ee069295537a434ce",
	"1131f2aaa0e97126c9314f9f968cc827259bbfabced2943bb8c9274448998fb3b78738b4580dd500c76105fd3c03e465e1414f2c29664286b1f79d3e51128125"
};

const char *SKEIN1024_TestOutput[MDTESTCOUNT] = {
	"0fff9563bb3279289227ac77d319b6fff8d7e9f09da1247b72a0a265cd6d2a62645ad547ed8193db48cff847c06494a03f55666d3b47eb4c20456c9373c86297d630d5578ebd34cb40991578f9f52b18003efa35d3da6553ff35db91b81ab890bec1b189b7f52cb2a783ebb7d823d725b0b4a71f6824e88f68f982eefc6d19c6",
	"6ab4c4ba9814a3d976ec8bffa7fcc638ceba0544a97b3c98411323ffd2dc936315d13dc93c13c4e88cda6f5bac6f2558b2d8694d3b6143e40d644ae43ca940685cb37f809d3d0550c56cba8036dee729a4f8fb960732e59e64d57f7f7710f8670963cdcdc95b41daab4855fcf8b6762a64b173ee61343a2c7689af1d293eba97",
	"35a599a0f91abcdb4cb73c19b8cb8d947742d82c309137a7caed29e8e0a2ca7a9ff9a90c34c1908cc7e7fd99bb15032fb86e76df21b72628399b5f7c3cc209d7bb31c99cd4e19465622a049afbb87c03b5ce3888d17e6e667279ec0aa9b3e2712624c01b5f5bbe1a564220bdcf6990af0c2539019f313fdd7406cca3892a1f1f",
	"ea891f5268acd0fac97467fc1aa89d1ce8681a9992a42540e53babee861483110c2d16f49e73bac27653ff173003e40cfb08516cd34262e6af95a5d8645c9c1abb3e813604d508b8511b30f9a5c1b352aa0791c7d2f27b2706dccea54bc7de6555b5202351751c3299f97c09cf89c40f67187e2521c0fad82b30edbb224f0458",
	"f23d95c2a25fbcd0e797cd058fec39d3c52d2b5afd7a9af1df934e63257d1d3dcf3246e7329c0f1104c1e51e3d22e300507b0c3b9f985bb1f645ef49835080536becf83788e17fed09c9982ba65c3cb7ffe6a5f745b911c506962adf226e435c42f6f6bc08d288f9c810e807e3216ef444f3db22744441deefa4900982a1371f",
	"cf3889e8a8d11bfd3938055d7d061437962bc5eac8ae83b1b71c94be201b8cf657fdbfc38674997a008c0c903f56a23feb3ae30e012377f1cfa080a9ca7fe8b96138662653fb3335c7d06595bf8baf65e215307532094cfdfa056bd8052ab792a3944a2adaa47b30335b8badb8fe9eb94fe329cdca04e58bbc530f0af709f469",
	"cf21a613620e6c119eca31fdfaad449a8e02f95ca256c21d2a105f8e4157048f9fe1e897893ea18b64e0e37cb07d5ac947f27ba544caf7cbc1ad094e675aed77a366270f7eb7f46543bccfa61c526fd628408058ed00ed566ac35a9761d002e629c4fb0d430b2f4ad016fcc49c44d2981c4002da0eecc42144160e2eaea4855a",
	"e6799b78db54085a2be7ff4c8007f147fa88d326abab30be0560b953396d8802feee9a15419b48a467574e9283be15685ca8a079ee52b27166b64dd70b124b1d4e4f6aca37224c3f2685e67e67baef9f94b905698adc794a09672aba977a61b20966912acdb08c21a2c37001785355dc884751a21f848ab36e590331ff938138"
};

static void
MDTestSuite(const Algorithm_t *alg)
{
	int i;
	char buffer[HEX_DIGEST_LENGTH];

	printf("%s test suite:\n", alg->name);
	for (i = 0; i < MDTESTCOUNT; i++) {
		(*alg->Data)(MDTestInput[i], strlen(MDTestInput[i]), buffer);
		printf("%s (\"%s\") = %s", alg->name, MDTestInput[i], buffer);
		if (strcmp(buffer, (*alg->TestOutput)[i]) == 0) {
			printf(" - verified correct\n");
		} else {
			printf(" - INCORRECT RESULT!\n");
			failed = true;
		}
	}
}

static void
usage(const Algorithm_t *alg)
{

	switch (mode) {
	case mode_gnu:
		fprintf(stderr, "usage: %ssum [-bctwz] [files ...]\n", alg->progname);
		break;
	case mode_perl:
		fprintf(stderr, "usage: shasum [-0bchqstUvw] [-a alg] [files ...]\n");
		break;
	default:
		fprintf(stderr, "usage: %s [-pqrtx] [-c string] [-s string] [files ...]\n",
		    alg->progname);
	}
	exit(1);
}

static void
version(void)
{
	if (mode == mode_gnu)
		printf("%s (FreeBSD) ", progname);
	printf("%d.%d\n",
	    __FreeBSD_version / 100000,
	    (__FreeBSD_version / 1000) % 100);
	exit(0);
}

/* $FreeBSD$ */

/*
 * This program repeatedly encrypts and decrypts a buffer with the built-in
 * iv and key, using hardware crypto.  At the end, it computes the data rate
 * achieved.  invoke with the number of times to encrypt and the buffer size.
 *
 * For a test of how fast a crypto card is, use something like:
 *	cryptotest -z 1024
 * This will run a series of tests using the available crypto/cipher
 * algorithms over a variety of buffer sizes.  The 1024 says to do 1024
 * iterations.  Extra arguments can be used to specify one or more buffer
 * sizes to use in doing tests.
 *
 * To fork multiple processes all doing the same work, specify -t X on the
 * command line to get X "threads" running simultaneously.  No effort is made
 * synchronize the threads or otherwise maximize load.
 *
 * If the kernel crypto code is built with CRYPTO_TIMING and you run as root,
 * then you can specify the -p option to get a "profile" of the time spent
 * processing crypto operations.  At present this data is only meaningful for
 * symmetric operations.  To get meaningful numbers you must run on an idle
 * machine.
 *
 * Expect ~400 Mb/s for a Broadcom 582x for 16K buffers on a reasonable CPU.
 * Hifn 7811 parts top out at ~110 Mb/s.
 *
 * This code originally came from openbsd; give them all the credit.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <crypto/cryptodev.h>

#define	CHUNK	64	/* how much to display */
#define	N(a)		(sizeof (a) / sizeof (a[0]))
#define	streq(a,b)	(strcasecmp(a,b) == 0)

void	hexdump(char *, int);

int	cryptodev_fd;
int	fd;
struct	session_op session;
struct	crypt_op cryptop;
char	iv[8] = "00000000";
int	verbose = 0;

struct alg {
	const char* name;
	int	blocksize;
	int	minkeylen;
	int	maxkeylen;
	int	code;
} algorithms[] = {
#ifdef CRYPTO_NULL_CBC
	{ "null",	8,	1,	256,	CRYPTO_NULL_CBC },
#endif
	{ "des",	8,	8,	8,	CRYPTO_DES_CBC },
	{ "3des",	8,	24,	24,	CRYPTO_3DES_CBC },
	{ "blf",	8,	5,	56,	CRYPTO_BLF_CBC },
	{ "cast",	8,	5,	16,	CRYPTO_CAST_CBC },
	{ "skj",	8,	10,	10,	CRYPTO_SKIPJACK_CBC },
	{ "aes",	16,	8,	32,	CRYPTO_RIJNDAEL128_CBC },
#ifdef notdef
	{ "arc4",	8,	1,	32,	CRYPTO_ARC4 },
#endif
};

static void
usage(const char* cmd)
{
	printf("usage: %s [-z] [-s] [-v] [-a algorithm] [count] [size ...]\n",
		cmd);
	printf("where algorithm is one of:\n");
	printf("    des 3des (default) blowfish cast skipjack\n");
	printf("    rijndael arc4\n");
	printf("count is the number of encrypt/decrypt ops to do\n");
	printf("size is the number of bytes of text to encrypt+decrypt\n");
	exit(-1);
}

static struct alg*
getalgbycode(int cipher)
{
	int i;

	for (i = 0; i < N(algorithms); i++)
		if (cipher == algorithms[i].code)
			return &algorithms[i];
	return NULL;
}

static struct alg*
getalgbyname(const char* name)
{
	int i;

	for (i = 0; i < N(algorithms); i++)
		if (streq(name, algorithms[i].name))
			return &algorithms[i];
	return NULL;
}

static void
runtest(struct alg *alg, int count, int size, int cmd, struct timeval *tv)
{
	int i;
	struct timeval start, stop, dt;
	char *cleartext, *ciphertext;

	if (ioctl(cryptodev_fd,CRIOGET,&fd) == -1)
		err(1, "CRIOGET failed");

	session.mac = 0;
	session.keylen = (alg->minkeylen + alg->maxkeylen)/2;
	session.key = (char *) malloc(session.keylen);
	if (session.key == NULL)
		err(1, "malloc (key)");
	for (i = 0; i < session.keylen; i++)
		session.key[i] = '0' + (i%10);
	session.cipher = alg->code;
	if (ioctl(fd, cmd, &session) == -1) {
		if (cmd == CIOCGSESSION) {
			close(fd);
			/* hardware doesn't support algorithm; skip it */
			return;
		}
		printf("cipher %s keylen %u\n", alg->name, session.keylen);
		err(1, "CIOCGSESSION failed");
	}

	if ((cleartext = (char *)malloc(size)) == NULL)
		err(1, "malloc (cleartext)");
	if ((ciphertext = (char *)malloc(size)) == NULL)
		err(1, "malloc (ciphertext)");
	for (i = 0; i < size; i++)
		cleartext[i] = 'a' + i%26;

	if (verbose) {
		printf("session = 0x%x\n", session.ses);
		printf("count = %d, size = %d\n", count, size);
		cryptop.ses = session.ses;
		printf("iv:");
		hexdump(iv, sizeof iv);
		printf("cleartext:");
		hexdump(cleartext, MIN(size, CHUNK));
	}

	gettimeofday(&start, NULL);
	for (i = 0; i < count; i++) {
		cryptop.op = COP_ENCRYPT;
		cryptop.flags = 0;
		cryptop.len = size;
		cryptop.src = cleartext;
		cryptop.dst = ciphertext;
		cryptop.mac = 0;
		cryptop.iv = iv;

		if (ioctl(fd, CIOCCRYPT, &cryptop) == -1)
			err(1, "CIOCCRYPT failed");

		memset(cleartext, 'x', MIN(size, CHUNK));
		cryptop.op = COP_DECRYPT;
		cryptop.flags = 0;
		cryptop.len = size;
		cryptop.src = ciphertext;
		cryptop.dst = cleartext;
		cryptop.mac = 0;
		cryptop.iv = iv;

		if (ioctl(fd, CIOCCRYPT, &cryptop) == -1)
			err(1, "CIOCCRYPT failed");
	}
	gettimeofday(&stop, NULL);
 
	if (ioctl(fd, CIOCFSESSION, &session.ses) == -1)
		perror("CIOCFSESSION");

	if (verbose) {
		printf("cleartext:");
		hexdump(cleartext, MIN(size, CHUNK));
	}
	timersub(&stop, &start, tv);

	free(ciphertext);
	free(cleartext);

	close(fd);
}

static void
resetstats()
{
	struct cryptostats stats;
	size_t slen;

	slen = sizeof (stats);
	if (sysctlbyname("kern.crypto_stats", &stats, &slen, NULL, NULL) < 0) {
		perror("kern.crypto_stats");
		return;
	}
	bzero(&stats.cs_invoke, sizeof (stats.cs_invoke));
	bzero(&stats.cs_done, sizeof (stats.cs_done));
	bzero(&stats.cs_cb, sizeof (stats.cs_cb));
	bzero(&stats.cs_finis, sizeof (stats.cs_finis));
	stats.cs_invoke.min.tv_sec = 10000;
	stats.cs_done.min.tv_sec = 10000;
	stats.cs_cb.min.tv_sec = 10000;
	stats.cs_finis.min.tv_sec = 10000;
	if (sysctlbyname("kern.crypto_stats", NULL, NULL, &stats, sizeof (stats)) < 0)
		perror("kern.cryptostats");
}

static void
printt(const char* tag, struct cryptotstat *ts)
{
	uint64_t avg, min, max;

	if (ts->count == 0)
		return;
	avg = (1000000000LL*ts->acc.tv_sec + ts->acc.tv_nsec) / ts->count;
	min = 1000000000LL*ts->min.tv_sec + ts->min.tv_nsec;
	max = 1000000000LL*ts->max.tv_sec + ts->max.tv_nsec;
	printf("%16.16s: avg %6llu ns : min %6llu ns : max %7llu ns [%u samps]\n",
		tag, avg, min, max, ts->count);
}

static void
runtests(struct alg *alg, int count, int size, int cmd, int threads, int profile)
{
	int i, status;
	double t;
	void *region;
	struct timeval *tvp;
	struct timeval total;
	int otiming;

	if (size % alg->blocksize) {
		if (verbose)
			printf("skipping blocksize %u 'cuz not a multiple of "
				"%s blocksize %u\n",
				size, alg->name, alg->blocksize);
		return;
	}

	region = mmap(NULL, threads * sizeof (struct timeval),
			PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
	if (region == MAP_FAILED) {
		perror("mmap");
		return;
	}
	tvp = (struct timeval *) region;
	if (profile) {
		size_t tlen = sizeof (otiming);
		int timing = 1;

		resetstats();
		if (sysctlbyname("debug.crypto_timing", &otiming, &tlen,
				&timing, sizeof (timing)) < 0)
			perror("debug.crypto_timing");
	}

	if (threads > 1) {
		for (i = 0; i < threads; i++)
			if (fork() == 0) {
				runtest(alg, count, size, cmd, &tvp[i]);
				exit(0);
			}
		while (waitpid(WAIT_MYPGRP, &status, 0) != -1)
			;
	} else
		runtest(alg, count, size, cmd, tvp);

	t = 0;
	for (i = 0; i < threads; i++)
		t += (((double)tvp[i].tv_sec * 1000000 + tvp[i].tv_usec) / 1000000);
	if (t) {
		printf("%6.3lf sec, %7d %6s crypts, %7d bytes, %8.0lf byte/sec, %7.1lf Mb/sec\n",
		    t/threads, 2*count*threads, alg->name, size, (double)2*count*size*threads / t,
		    (double)2*count*size*threads / t * 8 / 1024 / 1024);
	}
	if (profile) {
		struct cryptostats stats;
		size_t slen = sizeof (stats);

		if (sysctlbyname("debug.crypto_timing", NULL, NULL,
				&otiming, sizeof (otiming)) < 0)
			perror("debug.crypto_timing");
		if (sysctlbyname("kern.crypto_stats", &stats, &slen, NULL, NULL) < 0)
			perror("kern.cryptostats");
		if (stats.cs_invoke.count) {
			printt("dispatch->invoke", &stats.cs_invoke);
			printt("invoke->done", &stats.cs_done);
			printt("done->cb", &stats.cs_cb);
			printt("cb->finis", &stats.cs_finis);
		}
	}
	fflush(stdout);
}

int
main(int argc, char **argv)
{
	struct alg *alg = NULL;
	int count = 1;
	int sizes[128], nsizes = 0;
	int cmd = CIOCGSESSION;
	int testall = 0;
	int maxthreads = 1;
	int profile = 0;
	int i, ch;

	while ((ch = getopt(argc, argv, "pzsva:t:")) != -1) {
		switch (ch) {
#ifdef CIOCGSSESSION
		case 's':
			cmd = CIOCGSSESSION;
			break;
#endif
		case 'v':
			verbose++;
			break;
		case 'a':
			alg = getalgbyname(optarg);
			if (alg == NULL) {
				if (streq(optarg, "rijndael"))
					alg = getalgbyname("aes");
				else
					usage(argv[0]);
			}
			break;
		case 't':
			maxthreads = atoi(optarg);
			break;
		case 'z':
			testall = 1;
			break;
		case 'p':
			profile = 1;
			break;
		default:
			usage(argv[0]);
		}
	}
	argc -= optind, argv += optind;
	if (argc > 0)
		count = atoi(argv[0]);
	while (argc > 1) {
		int s = atoi(argv[1]);
		if (nsizes < N(sizes)) {
			sizes[nsizes++] = s;
		} else {
			printf("Too many sizes, ignoring %u\n", s);
		}
		argc--, argv++;
	}
	if (nsizes == 0) {
		sizes[nsizes++] = 8;
		if (testall) {
			while (sizes[nsizes-1] < 8*1024) {
				sizes[nsizes] = sizes[nsizes-1]<<1;
				nsizes++;
			}
		}
	}

	if ((cryptodev_fd = open("/dev/crypto",O_RDWR,0)) < 0)
		err(1, "/dev/crypto");

	if (testall) {
		for (i = 0; i < N(algorithms); i++) {
			int j;
			alg = &algorithms[i];
			for (j = 0; j < nsizes; j++)
				runtests(alg, count, sizes[j], cmd, maxthreads, profile);
		}
	} else {
		if (alg == NULL)
			alg = getalgbycode(CRYPTO_3DES_CBC);
		for (i = 0; i < nsizes; i++)
			runtests(alg, count, sizes[i], cmd, maxthreads, profile);
	}

	return (0);
}

void hexdump(char *p, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (i%16 == 0) {
			if (i != 0) {
				int j;
				char *l = p-16;
				printf("  |");
				for (j = 0; j < 16; j++,l++)
					printf("%c", (((*l)&0xff)>0x1f && ((*l)&0xff)<0x7f) ? (*l)&0xff : '.');
				printf("|");
			}
			printf("\n%04x: ", i);
		}
		printf(" %02x", (int)(*p++)&0xff);
	}
	printf("\n");
}

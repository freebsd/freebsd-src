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
 * Expect ~400 Mb/s for a Broadcom 582x for 16K buffers on a reasonable CPU.
 * Hifn 7811 parts top out at ~110 Mb/s.
 *
 * This code originally came from openbsd; give them all the credit.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <crypto/cryptodev.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

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
	{ "null",	8,	1,	256,	CRYPTO_NULL_CBC },
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
runtest(struct alg *alg, int count, int size, int cmd)
{
	int i;
	struct timeval start, stop, dt;
	char *cleartext, *ciphertext;
	double t;

	if (size % alg->blocksize) {
		if (verbose)
			printf("skipping blocksize %u 'cuz not a multiple of "
				"%s blocksize %u\n",
				size, alg->name, alg->blocksize);
		return;
	}

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
	timersub(&stop, &start, &dt);
	t = (((double)dt.tv_sec * 1000000 + dt.tv_usec) / 1000000);
	printf("%6.3lf sec, %7d %6s crypts, %7d bytes, %8.0lf byte/sec, %7.1lf Mb/sec\n",
	    t, 2*count, alg->name, size, (double)2*count*size / t,
	    (double)2*count*size / t * 8 / 1024 / 1024);

	free(ciphertext);
	free(cleartext);

	close(fd);
}

int
main(int argc, char **argv)
{
	struct alg *alg = NULL;
	int count = 1;
	int sizes[128], nsizes = 0;
	int cmd = CIOCGSESSION;
	int testall = 0;
	int i, ch;

	while ((ch = getopt(argc, argv, "zsva:")) != -1) {
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
		case 'z':
			testall = 1;
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
			while (sizes[nsizes-1] < 32768) {
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
				runtest(alg, count, sizes[j], cmd);
		}
	} else {
		if (alg == NULL)
			alg = getalgbycode(CRYPTO_3DES_CBC);
		for (i = 0; i < nsizes; i++)
			runtest(alg, count, sizes[i], cmd);
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

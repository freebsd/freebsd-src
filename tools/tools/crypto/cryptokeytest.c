/* $FreeBSD$ */
/*
 * The big num stuff is a bit broken at the moment and I've not yet fixed it.
 * The symtom is that odd size big nums will fail.  Test code below (it only
 * uses modexp currently).
 * 
 * --Jason L. Wright
 */
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <crypto/cryptodev.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/err.h>

int	crid = CRYPTO_FLAG_HARDWARE;
int	verbose = 0;

static int
devcrypto(void)
{
	static int fd = -1;

	if (fd < 0) {
		fd = open(_PATH_DEV "crypto", O_RDWR, 0);
		if (fd < 0)
			err(1, _PATH_DEV "crypto");
		if (fcntl(fd, F_SETFD, 1) == -1)
			err(1, "fcntl(F_SETFD) (devcrypto)");
	}
	return fd;
}

static int
crlookup(const char *devname)
{
	struct crypt_find_op find;

	find.crid = -1;
	strlcpy(find.name, devname, sizeof(find.name));
	if (ioctl(devcrypto(), CIOCFINDDEV, &find) == -1)
		err(1, "ioctl(CIOCFINDDEV)");
	return find.crid;
}

static const char *
crfind(int crid)
{
	static struct crypt_find_op find;

	bzero(&find, sizeof(find));
	find.crid = crid;
	if (ioctl(devcrypto(), CIOCFINDDEV, &find) == -1)
		err(1, "ioctl(CIOCFINDDEV)");
	return find.name;
}

/*
 * Convert a little endian byte string in 'p' that is 'plen' bytes long to a
 * BIGNUM.  A new BIGNUM is allocated.  Returns NULL on failure.
 */
static BIGNUM *
le_to_bignum(BIGNUM *res, const void *p, int plen)
{

	res = BN_lebin2bn(p, plen, res);
	if (res == NULL)
		ERR_print_errors_fp(stderr);

	return (res);
}

/*
 * Convert a BIGNUM to a little endian byte string.  Space for BN_num_bytes(n)
 * is allocated.
 * Returns NULL on failure.
 */
static void *
bignum_to_le(const BIGNUM *n)
{
	int blen, error;
	void *rd;

	blen = BN_num_bytes(n);
	if (blen == 0)
		return (NULL);

	rd = malloc(blen);
	if (rd == NULL)
		return (NULL);

	error = BN_bn2lebinpad(n, rd, blen);
	if (error < 0) {
		ERR_print_errors_fp(stderr);
		free(rd);
		return (NULL);
	}

	return (rd);
}

static int
UB_mod_exp(BIGNUM *res, const BIGNUM *a, const BIGNUM *b, const BIGNUM *c)
{
	struct crypt_kop kop;
	void *ale, *ble, *cle;
	static int crypto_fd = -1;

	if (crypto_fd == -1 && ioctl(devcrypto(), CRIOGET, &crypto_fd) == -1)
		err(1, "CRIOGET");

	if ((ale = bignum_to_le(a)) == NULL)
		err(1, "bignum_to_le, a");
	if ((ble = bignum_to_le(b)) == NULL)
		err(1, "bignum_to_le, b");
	if ((cle = bignum_to_le(c)) == NULL)
		err(1, "bignum_to_le, c");

	bzero(&kop, sizeof(kop));
	kop.crk_op = CRK_MOD_EXP;
	kop.crk_iparams = 3;
	kop.crk_oparams = 1;
	kop.crk_crid = crid;
	kop.crk_param[0].crp_p = ale;
	kop.crk_param[0].crp_nbits = BN_num_bytes(a) * 8;
	kop.crk_param[1].crp_p = ble;
	kop.crk_param[1].crp_nbits = BN_num_bytes(b) * 8;
	kop.crk_param[2].crp_p = cle;
	kop.crk_param[2].crp_nbits = BN_num_bytes(c) * 8;
	kop.crk_param[3].crp_p = cle;
	kop.crk_param[3].crp_nbits = BN_num_bytes(c) * 8;

	if (ioctl(crypto_fd, CIOCKEY2, &kop) == -1)
		err(1, "CIOCKEY2");
	if (verbose)
		printf("device = %s\n", crfind(kop.crk_crid));

	explicit_bzero(ale, BN_num_bytes(a));
	free(ale);
	explicit_bzero(ble, BN_num_bytes(b));
	free(ble);

	if (kop.crk_status != 0) {
		printf("error %d\n", kop.crk_status);
		explicit_bzero(cle, BN_num_bytes(c));
		free(cle);
		return (-1);
	} else {
		res = le_to_bignum(res, cle, BN_num_bytes(c));
		explicit_bzero(cle, BN_num_bytes(c));
		free(cle);
		if (res == NULL)
			err(1, "le_to_bignum");
		return (0);
	}
	return (0);
}

static void
show_result(const BIGNUM *a, const BIGNUM *b, const BIGNUM *c,
    const BIGNUM *sw, const BIGNUM *hw)
{
	printf("\n");

	printf("A = ");
	BN_print_fp(stdout, a);
	printf("\n");

	printf("B = ");
	BN_print_fp(stdout, b);
	printf("\n");

	printf("C = ");
	BN_print_fp(stdout, c);
	printf("\n");

	printf("sw= ");
	BN_print_fp(stdout, sw);
	printf("\n");

	printf("hw= ");
	BN_print_fp(stdout, hw);
	printf("\n");

	printf("\n");
}

static void
testit(void)
{
	BIGNUM *a, *b, *c, *r1, *r2;
	BN_CTX *ctx;

	ctx = BN_CTX_new();

	a = BN_new();
	b = BN_new();
	c = BN_new();
	r1 = BN_new();
	r2 = BN_new();

	BN_pseudo_rand(a, 1023, 0, 0);
	BN_pseudo_rand(b, 1023, 0, 0);
	BN_pseudo_rand(c, 1024, 0, 0);

	if (BN_cmp(a, c) > 0) {
		BIGNUM *rem = BN_new();

		BN_mod(rem, a, c, ctx);
		UB_mod_exp(r2, rem, b, c);
		BN_free(rem);
	} else {
		UB_mod_exp(r2, a, b, c);
	}
	BN_mod_exp(r1, a, b, c, ctx);

	if (BN_cmp(r1, r2) != 0) {
		show_result(a, b, c, r1, r2);
	}

	BN_free(r2);
	BN_free(r1);
	BN_free(c);
	BN_free(b);
	BN_free(a);
	BN_CTX_free(ctx);
}

static void
usage(const char* cmd)
{
	printf("usage: %s [-d dev] [-v] [count]\n", cmd);
	printf("count is the number of bignum ops to do\n");
	printf("\n");
	printf("-d use specific device\n");
	printf("-v be verbose\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	int c, i;

	while ((c = getopt(argc, argv, "d:v")) != -1) {
		switch (c) {
		case 'd':
			crid = crlookup(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(argv[0]);
		}
	}
	argc -= optind, argv += optind;

	for (i = 0; i < 1000; i++) {
		fprintf(stderr, "test %d\n", i);
		testit();
	}
	return (0);
}

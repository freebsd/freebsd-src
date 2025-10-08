
# STOP - This is not a general purpose library and is only for use by pkg(7)
# to align with the implementation in pkg(8).
LIB=	pkgecc
INTERNALLIB=

.PATH: $(SRCTOP)/crypto/libecc
SRCS+=	pkg_libecc_rand.c

# curves_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/curves
SRCS+=	aff_pt.c \
	aff_pt_montgomery.c \
	ec_edwards.c \
	ec_montgomery.c \
	ec_params.c \
	ec_shortw.c \
	aff_pt_edwards.c \
	curves.c \
	prj_pt.c

# utils_ec_src
.PATH:	$(SRCTOP)/crypto/libecc/src/utils
SRCS+=	print_curves.c

# fp_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/fp
SRCS+=	fp_add.c \
	fp.c \
	fp_montgomery.c \
	fp_mul.c \
	fp_mul_redc1.c \
	fp_pow.c \
	fp_rand.c \
	fp_sqrt.c

# nn_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/nn
SRCS+=	nn_add.c \
	nn.c \
	nn_div.c \
	nn_logical.c \
	nn_modinv.c \
	nn_mod_pow.c \
	nn_mul.c \
	nn_mul_redc1.c \
	nn_rand.c

# utils_arith_src
SRCS+=	utils.c \
	utils_rand.c \
	print_buf.c \
	print_fp.c \
	print_nn.c

## libsign bits
# hash_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/hash
SRCS+=	hash_algs.c \
	sm3.c \
	streebog.c \
	ripemd160.c \
	belt-hash.c \
	hmac.c \
	bash224.c \
	bash256.c \
	bash384.c \
	bash512.c \
	bash.c \
	sha224.c \
	sha256.c \
	sha3-224.c \
	sha3-256.c \
	sha3-384.c \
	sha3-512.c \
	sha384.c \
	sha3.c \
	sha512-224.c \
	sha512-256.c \
	sha512.c \
	sha512_core.c \
	shake256.c \
	shake.c

# sig_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/sig
SRCS+=	decdsa.c \
	ecdsa.c \
	ecfsdsa.c \
	ecgdsa.c \
	eckcdsa.c \
	ecosdsa.c \
	ecrdsa.c \
	ecsdsa.c \
	eddsa.c \
	fuzzing_ecdsa.c \
	fuzzing_ecgdsa.c \
	fuzzing_ecrdsa.c \
	ecdsa_common.c \
	ecsdsa_common.c \
	sig_algs.c \
	sm2.c \
	bign_common.c \
	bign.c \
	dbign.c \
	bip0340.c

# key_mod_src
SRCS+=	ec_key.c

# utils_sign_src
.PATH:	$(SRCTOP)/crypto/libecc/src/sig
SRCS+=	print_keys.c

# ecdh_mod_src
.PATH:	$(SRCTOP)/crypto/libecc/src/ecdh
SRCS+=	ecccdh.c \
	x25519_448.c

# external_deps
.PATH:	$(SRCTOP)/crypto/libecc/src/external_deps
SRCS+=	print.c

CONFLICTS=	-Dsha256_init=_libecc_sha256_init \
		-Dsha256_update=_libecc_sha256_update \
		-Dsha256_final=_libecc_sha256_final \
		-Dsha512_224_init=_libecc_sha512_224_init \
		-Dsha512_256_init=_libecc_sha512_256_init

CFLAGS=		-I$(SRCTOP)/crypto/libecc/include \
		-fno-builtin \
		-DUSE_WARN_UNUSED_RET \
		-DWITH_STDLIB \
		$(CONFLICTS)

.include <bsd.lib.mk>

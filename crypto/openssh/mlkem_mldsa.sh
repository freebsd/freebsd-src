#!/bin/sh
#       $OpenBSD: mlkem_mldsa.sh,v 1.1 2026/06/14 03:59:34 djm Exp $
#       Placed in the Public Domain.
#

WANT_LIBCRUX_REVISION="origin/jonas/combined-extraction-mldsa"

BASE="libcrux/combined_extraction/generated"
FILES="
	$BASE/eurydice_glue.h
	$BASE/combined_core.h
	$BASE/libcrux_sha3_portable.h
	$BASE/libcrux_mlkem_core.h
	$BASE/libcrux_mldsa_core.h
	$BASE/libcrux_ct_ops.h
	$BASE/libcrux_mldsa_portable.h
	$BASE/libcrux_mldsa44_portable.h
	$BASE/libcrux_mldsa65_portable.h
	$BASE/libcrux_mldsa87_portable.h
	$BASE/libcrux_mlkem768_portable.h
"

START="$PWD"
die() {
	echo "$@" 1>&2
	exit 1
}

set -xeuo pipefail
test -d libcrux || git clone https://github.com/cryspen/libcrux
cd libcrux
test `git diff | wc -l` -ne 0 && die "tree has unstaged changes"
git fetch
git checkout -B extract 1>&2
git reset --hard $WANT_LIBCRUX_REVISION 1>&2
LIBCRUX_REVISION=`git rev-parse HEAD`
set +x

cd $START
(
printf '/*  $Open'; printf 'BSD$ */\n' # Sigh
echo
echo "/* Extracted from libcrux revision $LIBCRUX_REVISION */"
echo
echo '/*'
cat libcrux/LICENSE-MIT | sed 's/^/ * /;s/ *$//'
echo ' */'
echo

LSHIFT="<<"
cat << _EOF
#if !defined(__GNUC__) || (__GNUC__ < 2)
# define __attribute__(x)
#endif
#define KRML_MUSTINLINE inline
#define KRML_NOINLINE __attribute__((noinline, unused))
#define KRML_HOST_EPRINTF(...)
#define KRML_HOST_EXIT(x) fatal_f("internal error")
#define KRML_UNION_CONSTRUCTOR(T)

static inline void
store64_le(uint8_t dst[8], uint64_t src)
{
	dst[0] = src & 0xff;
	dst[1] = (src >> 8) & 0xff;
	dst[2] = (src >> 16) & 0xff;
	dst[3] = (src >> 24) & 0xff;
	dst[4] = (src >> 32) & 0xff;
	dst[5] = (src >> 40) & 0xff;
	dst[6] = (src >> 48) & 0xff;
	dst[7] = (src >> 56) & 0xff;
}

static inline void
store32_le(uint8_t dst[4], uint32_t src)
{
	dst[0] = src & 0xff;
	dst[1] = (src >> 8) & 0xff;
	dst[2] = (src >> 16) & 0xff;
	dst[3] = (src >> 24) & 0xff;
}

static inline void
store16_le(uint8_t dst[2], uint16_t src)
{
	dst[0] = src & 0xff;
	dst[1] = (src >> 8) & 0xff;
}

static inline void
store32_be(uint8_t dst[4], uint32_t src)
{
	dst[0] = (src >> 24) & 0xff;
	dst[1] = (src >> 16) & 0xff;
	dst[2] = (src >> 8) & 0xff;
	dst[3] = src & 0xff;
}

static inline uint64_t
load64_le(uint8_t src[8])
{
	return (uint64_t)(src[0]) |
	    ((uint64_t)(src[1]) $LSHIFT 8) |
	    ((uint64_t)(src[2]) $LSHIFT 16) |
	    ((uint64_t)(src[3]) $LSHIFT 24) |
	    ((uint64_t)(src[4]) $LSHIFT 32) |
	    ((uint64_t)(src[5]) $LSHIFT 40) |
	    ((uint64_t)(src[6]) $LSHIFT 48) |
	    ((uint64_t)(src[7]) $LSHIFT 56);
}

static inline uint32_t
load32_le(uint8_t src[4])
{
	return (uint32_t)(src[0]) |
	    ((uint32_t)(src[1]) $LSHIFT 8) |
	    ((uint32_t)(src[2]) $LSHIFT 16) |
	    ((uint32_t)(src[3]) $LSHIFT 24);
}

static inline uint16_t
load16_le(uint8_t src[4])
{
	return (uint16_t)(src[0]) |
	    ((uint16_t)(src[1]) $LSHIFT 8);
}

#ifdef MISSING_BUILTIN_POPCOUNT
static inline unsigned int
__builtin_popcount(unsigned int num)
{
  const int v[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
  return v[num & 0xf] + v[(num >> 4) & 0xf];
}
#endif

_EOF

for i in $FILES; do
	echo "/* from $i */"
	# Changes to all files:
	#  - remove all includes, we inline everything required.
	#  - cleanup whitespace
	#  - convert C++-style constructors to C-style compound literals
	#  - convert Result constructors to C initializers
	#  - use anonymous unions to avoid "union U" redefinition errors
	sed -e "/#include/d" \
	    -e 's/[	 ]*$//' \
	    $i | \
	case "$i" in
	*/eurydice_glue.h)
		# Replace endian function for consistency.
		perl -0777 -pe 's/(static inline void core_num__u32__to_be_bytes.*\n)([^}]*\n)/\1  store32_be(dst, src);\n/'
		;;
	# Default: pass through.
	*)
		cat
		;;
	esac | \
	perl -0777 -pe 's/ <<\n\s+\(uint32_t\)\(int32_t\)0//g'
	echo
done

cat << _EOF

/* rename some types to be a bit more ergonomic */

/* ML-KEM 768 */
typedef Eurydice_arr_c7 libcrux_mlkem768_keypair_rnd;
typedef Eurydice_arr_ec libcrux_mlkem768_enc_rnd;
typedef libcrux_ml_kem_mlkem768_MlKem768KeyPair libcrux_mlkem768_keypair;
typedef Eurydice_arr_5f libcrux_mlkem768_pk;
typedef Eurydice_arr_7d libcrux_mlkem768_sk;
typedef Eurydice_arr_2b libcrux_mlkem768_ciphertext;
typedef tuple_f4 libcrux_mlkem768_enc_result;
typedef Eurydice_arr_ec libcrux_mlkem768_dec_result;
/* ML-DSA 44 */
typedef Eurydice_arr_ec libcrux_mldsa44_keypair_rnd;
typedef libcrux_ml_dsa_ml_dsa_generic_ml_dsa_44_MLDSA44KeyPair
    libcrux_mldsa44_keypair;
typedef Eurydice_arr_10 libcrux_mldsa44_sk;
typedef Eurydice_arr_02 libcrux_mldsa44_pk;
typedef Eurydice_borrow_slice_u8 libcrux_mldsa44_message;
typedef Eurydice_arr_ec libcrux_mldsa44_sign_rnd;
typedef core_result_Result_48 libcrux_mldsa44_sign_result;
typedef core_result_Result_41 libcrux_mldsa44_verify_result;
typedef Eurydice_arr_85 libcrux_mldsa44_signature;
/* ML-DSA 65 */
typedef Eurydice_arr_ec libcrux_mldsa65_keypair_rnd;
typedef libcrux_ml_dsa_ml_dsa_generic_ml_dsa_65_MLDSA65KeyPair
    libcrux_mldsa65_keypair;
typedef Eurydice_arr_24 libcrux_mldsa65_sk;
typedef Eurydice_arr_29 libcrux_mldsa65_pk;
typedef Eurydice_borrow_slice_u8 libcrux_mldsa65_message;
typedef Eurydice_arr_ec libcrux_mldsa65_sign_rnd;
typedef core_result_Result_8c libcrux_mldsa65_sign_result;
typedef core_result_Result_41 libcrux_mldsa65_verify_result;
typedef Eurydice_arr_0c libcrux_mldsa65_signature;
/* ML-DSA 87 */
typedef Eurydice_arr_ec libcrux_mldsa87_keypair_rnd;
typedef libcrux_ml_dsa_ml_dsa_generic_ml_dsa_87_MLDSA87KeyPair
    libcrux_mldsa87_keypair;
typedef Eurydice_arr_e2 libcrux_mldsa87_sk;
typedef Eurydice_arr_43 libcrux_mldsa87_pk;
typedef Eurydice_borrow_slice_u8 libcrux_mldsa87_message;
typedef Eurydice_arr_ec libcrux_mldsa87_sign_rnd;
typedef core_result_Result_8b libcrux_mldsa87_sign_result;
typedef core_result_Result_41 libcrux_mldsa87_verify_result;
typedef Eurydice_arr_93 libcrux_mldsa87_signature;

#define LIBCRUX_RESULT_OK	core_result_Ok

_EOF

) > libcrux_internal.h_new

# Do some checks on the resultant file

cat > libcrux_internal_check.c << _EOF
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <err.h>
#include "crypto_api.h"
#define fatal_f(x) exit(1)
#include "libcrux_internal.h_new"

#define TEST_MLDSA(L) do { \
	libcrux_mldsa##L##_keypair_rnd kpseed##L = {0}; \
	libcrux_mldsa##L##_keypair kp##L = {0}; \
	libcrux_mldsa##L##_sk sk##L = {0}; \
	libcrux_mldsa##L##_pk pk##L = {0}; \
	const uint8_t msgdata##L[6] = { 0x4a, 0x75, 0x6c, 0x69, 0x75, 0x73 }; \
	libcrux_mldsa##L##_message msg##L = { \
		msgdata##L, sizeof(msgdata##L) \
	}; \
	const uint8_t ctxdata##L[4] = { 0x48, 0x75, 0x67, 0x6f }; \
	libcrux_mldsa##L##_message ctx##L = { \
		ctxdata##L, sizeof(ctxdata##L) \
	}; \
	libcrux_mldsa##L##_sign_rnd signseed##L = {0}; \
	libcrux_mldsa##L##_sign_result signresult##L = {0}; \
	libcrux_mldsa##L##_verify_result verifyresult##L = {0}; \
	libcrux_mldsa##L##_signature sig##L = {0}; \
	\
	if (sizeof(kpseed##L.data) != MLDSA##L##_SEEDBYTES) \
		errx(1, "libcrux_mldsa%d_keypair_rnd bad", L); \
	if (sizeof(kp##L.verification_key.data) != MLDSA##L##_PUBLICKEYBYTES) \
		errx(1, "libcrux_mldsa%d_keypair verification_key bad", L); \
	if (sizeof(kp##L.signing_key.data) != MLDSA##L##_SECRETKEYBYTES) \
		errx(1, "libcrux_mldsa%d_keypair signing_key bad", L); \
	if (sizeof(pk##L.data) != MLDSA##L##_PUBLICKEYBYTES) \
		errx(1, "libcrux_mldsa%d_pk bad", L); \
	if (sizeof(sk##L.data) != MLDSA##L##_SECRETKEYBYTES) \
		errx(1, "libcrux_mldsa%d_sk bad", L); \
	if (sizeof(signresult##L.val.case_Ok.data) != MLDSA##L##_SIGBYTES) \
		errx(1, "libcrux_mldsa%d_sign_result bad", L); \
	if (sizeof(sig##L.data) != MLDSA##L##_SIGBYTES) \
		errx(1, "libcrux_mldsa%d_signature bad", L); \
	\
	kp##L = libcrux_ml_dsa_ml_dsa_##L##_portable_generate_key_pair( \
	    kpseed##L); \
	sk##L = kp##L.signing_key; \
	pk##L = kp##L.verification_key; \
	signresult##L = libcrux_ml_dsa_ml_dsa_##L##_portable_sign(&sk##L, \
	    msg##L, ctx##L, signseed##L); \
	if (signresult##L.tag != LIBCRUX_RESULT_OK) \
		errx(1, "libcrux_ml_dsa_ml_dsa_%d_portable_sign failed", L); \
	sig##L = signresult##L.val.case_Ok; \
	verifyresult##L = libcrux_ml_dsa_ml_dsa_##L##_portable_verify(&pk##L, \
	    msg##L, ctx##L, &sig##L); \
	if (verifyresult##L.tag != LIBCRUX_RESULT_OK) \
		errx(1, "libcrux_ml_dsa_ml_dsa_%d_portable_verify failed", L); \
	sig##L.data[10] ^= 0x10; /* corrupt the signature */ \
	verifyresult##L = libcrux_ml_dsa_ml_dsa_##L##_portable_verify(&pk##L, \
	    msg##L, ctx##L, &sig##L); \
	if (verifyresult##L.tag == LIBCRUX_RESULT_OK) \
		errx(1, "libcrux_ml_dsa_ml_dsa_%d_portable_verify fail2", L); \
} while (0)

int main(void) {
	libcrux_mlkem768_keypair keypair = {0};
	libcrux_mlkem768_pk pk = {0};
	libcrux_mlkem768_sk sk = {0};
	libcrux_mlkem768_ciphertext ct = {0};
	libcrux_mlkem768_enc_result enc_result = {0};
	libcrux_mlkem768_keypair_rnd kp_seed = {0};
	libcrux_mlkem768_enc_rnd enc_seed = {0};
	libcrux_mlkem768_dec_result shared_secret = {0};
	uint8_t shared_key[crypto_kem_mlkem768_BYTES];

	if (sizeof(keypair.pk.data) != crypto_kem_mlkem768_PUBLICKEYBYTES)
		errx(1, "keypair.pk bad");
	if (sizeof(keypair.sk.data) != crypto_kem_mlkem768_SECRETKEYBYTES)
		errx(1, "keypair.sk bad");
	if (sizeof(pk.data) != crypto_kem_mlkem768_PUBLICKEYBYTES)
		errx(1, "pk bad");
	if (sizeof(sk.data) != crypto_kem_mlkem768_SECRETKEYBYTES)
		errx(1, "sk bad");
	if (sizeof(ct.data) != crypto_kem_mlkem768_CIPHERTEXTBYTES)
		errx(1, "ct bad");
	if (sizeof(enc_result.fst.data) != crypto_kem_mlkem768_CIPHERTEXTBYTES)
		errx(1, "enc_result ct bad");
	if (sizeof(enc_result.snd.data) != crypto_kem_mlkem768_BYTES)
		errx(1, "enc_result shared key bad");
	if (sizeof(kp_seed.data) != crypto_kem_mlkem768_KEYPAIRSEEDBYTES)
		errx(1, "keypair rnd bad");
	if (sizeof(enc_seed.data) != crypto_kem_mlkem768_ENCSEEDBYTES)
		errx(1, "keypair rnd bad");

	keypair = libcrux_ml_kem_mlkem768_portable_generate_key_pair(kp_seed);
	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&keypair.pk))
		errx(1, "valid smoke failed");
	enc_result = libcrux_ml_kem_mlkem768_portable_encapsulate(&keypair.pk,
	    enc_seed);
	shared_secret = libcrux_ml_kem_mlkem768_portable_decapsulate(
	    &keypair.sk, &enc_result.fst);
	memcpy(shared_key, shared_secret.data, sizeof(shared_key));
	if (memcmp(shared_key, enc_result.snd.data, sizeof(shared_key)) != 0)
		errx(1, "smoke failed");

	TEST_MLDSA(44);
	TEST_MLDSA(65);
	TEST_MLDSA(87);

	return 0;
}
_EOF
cc -Wall -Wextra -Wno-unused-parameter -I . -o libcrux_internal_check \
	libcrux_internal_check.c
./libcrux_internal_check

mv libcrux_internal.h_new libcrux_internal.h
rm libcrux_internal_check libcrux_internal_check.c
echo 1>&2
echo "libcrux_internal.h OK" 1>&2


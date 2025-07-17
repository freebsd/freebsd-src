#!/bin/sh
#       $OpenBSD: mlkem768.sh,v 1.3 2024/10/27 02:06:01 djm Exp $
#       Placed in the Public Domain.
#

#WANT_LIBCRUX_REVISION="origin/main"
WANT_LIBCRUX_REVISION="84c5d87b3092c59294345aa269ceefe0eb97cc35"

FILES="
	libcrux/libcrux-ml-kem/cg/eurydice_glue.h
	libcrux/libcrux-ml-kem/cg/libcrux_core.h
	libcrux/libcrux-ml-kem/cg/libcrux_ct_ops.h
	libcrux/libcrux-ml-kem/cg/libcrux_sha3_portable.h
	libcrux/libcrux-ml-kem/cg/libcrux_mlkem768_portable.h
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
echo '#if !defined(__GNUC__) || (__GNUC__ < 2)'
echo '# define __attribute__(x)'
echo '#endif'
echo '#define KRML_MUSTINLINE inline'
echo '#define KRML_NOINLINE __attribute__((noinline, unused))'
echo '#define KRML_HOST_EPRINTF(...)'
echo '#define KRML_HOST_EXIT(x) fatal_f("internal error")'
echo

for i in $FILES; do
	echo "/* from $i */"
	# Changes to all files:
	#  - remove all includes, we inline everything required.
	#  - cleanup whitespace
	sed -e "/#include/d" \
	    -e 's/[	 ]*$//' \
	    $i | \
	case "$i" in
	*/libcrux-ml-kem/cg/eurydice_glue.h)
		# Replace endian functions with versions that work.
		perl -0777 -pe 's/(static inline void core_num__u64_9__to_le_bytes.*\n)([^}]*\n)/\1  v = htole64(v);\n\2/' |
		perl -0777 -pe 's/(static inline uint64_t core_num__u64_9__from_le_bytes.*?)return v;/\1return le64toh(v);/s' |
		perl -0777 -pe 's/(static inline uint32_t core_num__u32_8__from_le_bytes.*?)return v;/\1return le32toh(v);/s'
		;;
	# Default: pass through.
	*)
		cat
		;;
	esac
	echo
done

echo
echo '/* rename some types to be a bit more ergonomic */'
echo '#define libcrux_mlkem768_keypair libcrux_ml_kem_mlkem768_MlKem768KeyPair_s'
echo '#define libcrux_mlkem768_pk_valid_result Option_92_s'
echo '#define libcrux_mlkem768_pk libcrux_ml_kem_types_MlKemPublicKey_15_s'
echo '#define libcrux_mlkem768_sk libcrux_ml_kem_types_MlKemPrivateKey_55_s'
echo '#define libcrux_mlkem768_ciphertext libcrux_ml_kem_mlkem768_MlKem768Ciphertext_s'
echo '#define libcrux_mlkem768_enc_result tuple_3c_s'
) > libcrux_mlkem768_sha3.h_new

# Do some checks on the resultant file

cat > libcrux_mlkem768_sha3_check.c << _EOF
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
#include "libcrux_mlkem768_sha3.h_new"
int main(void) {
	struct libcrux_mlkem768_keypair keypair = {0};
	struct libcrux_mlkem768_pk pk = {0};
	struct libcrux_mlkem768_sk sk = {0};
	struct libcrux_mlkem768_ciphertext ct = {0};
	struct libcrux_mlkem768_enc_result enc_result = {0};
	uint8_t kp_seed[64] = {0}, enc_seed[32] = {0};
	uint8_t shared_key[crypto_kem_mlkem768_BYTES];

	if (sizeof(keypair.pk.value) != crypto_kem_mlkem768_PUBLICKEYBYTES)
		errx(1, "keypair.pk bad");
	if (sizeof(keypair.sk.value) != crypto_kem_mlkem768_SECRETKEYBYTES)
		errx(1, "keypair.sk bad");
	if (sizeof(pk.value) != crypto_kem_mlkem768_PUBLICKEYBYTES)
		errx(1, "pk bad");
	if (sizeof(sk.value) != crypto_kem_mlkem768_SECRETKEYBYTES)
		errx(1, "sk bad");
	if (sizeof(ct.value) != crypto_kem_mlkem768_CIPHERTEXTBYTES)
		errx(1, "ct bad");
	if (sizeof(enc_result.fst.value) != crypto_kem_mlkem768_CIPHERTEXTBYTES)
		errx(1, "enc_result ct bad");
	if (sizeof(enc_result.snd) != crypto_kem_mlkem768_BYTES)
		errx(1, "enc_result shared key bad");

	keypair = libcrux_ml_kem_mlkem768_portable_generate_key_pair(kp_seed);
	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&keypair.pk))
		errx(1, "valid smoke failed");
	enc_result = libcrux_ml_kem_mlkem768_portable_encapsulate(&keypair.pk,
	    enc_seed);
	libcrux_ml_kem_mlkem768_portable_decapsulate(&keypair.sk,
	    &enc_result.fst, shared_key);
	if (memcmp(shared_key, enc_result.snd, sizeof(shared_key)) != 0)
		errx(1, "smoke failed");
	return 0;
}
_EOF
cc -Wall -Wextra -Wno-unused-parameter -o libcrux_mlkem768_sha3_check \
	libcrux_mlkem768_sha3_check.c
./libcrux_mlkem768_sha3_check

# Extract PRNG inputs; there's no nice #defines for these
key_pair_rng_len=`sed -e '/^libcrux_ml_kem_mlkem768_portable_kyber_generate_key_pair[(]$/,/[)] {$/!d' < libcrux_mlkem768_sha3.h_new | grep 'uint8_t randomness\[[0-9]*U\][)]' | sed 's/.*randomness\[\([0-9]*\)U\].*/\1/'`
enc_rng_len=`sed -e '/^static inline tuple_3c libcrux_ml_kem_mlkem768_portable_kyber_encapsulate[(]$/,/[)] {$/!d' < libcrux_mlkem768_sha3.h_new | grep 'uint8_t randomness\[[0-9]*U\][)]' | sed 's/.*randomness\[\([0-9]*\)U\].*/\1/'`
test -z "$key_pair_rng_len" && die "couldn't find size of libcrux_ml_kem_mlkem768_portable_kyber_generate_key_pair randomness argument"
test -z "$enc_rng_len" && die "couldn't find size of libcrux_ml_kem_mlkem768_portable_kyber_encapsulate randomness argument"

(
echo "/* defines for PRNG inputs */"
echo "#define LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN $key_pair_rng_len"
echo "#define LIBCRUX_ML_KEM_ENC_PRNG_LEN $enc_rng_len"
) >> libcrux_mlkem768_sha3.h_new

mv libcrux_mlkem768_sha3.h_new libcrux_mlkem768_sha3.h
rm libcrux_mlkem768_sha3_check libcrux_mlkem768_sha3_check.c
echo 1>&2
echo "libcrux_mlkem768_sha3.h OK" 1>&2


#!/bin/sh
#       $OpenBSD: ed25519.sh,v 1.8 2026/09/16 00:59:12 djm Exp $
#       Placed in the Public Domain.

set -eu
SCRIPT_DIR="$PWD"
LIBSODIUM_DIR="$SCRIPT_DIR/libsodium"
OUT="$SCRIPT_DIR/ed25519.c"
NEW="$OUT.new"
CHECK_C="$SCRIPT_DIR/ed25519_check.c"
CHECK="$SCRIPT_DIR/ed25519_check"
CHECK_O="$SCRIPT_DIR/ed25519_check.o"
CORE=src/libsodium/crypto_core/ed25519/ref10
SIGN=src/libsodium/crypto_sign/ed25519/ref10
PRIVATE=src/libsodium/include/sodium/private

die() { echo "ed25519.sh: $*" >&2; exit 1; }
cleanup() { return;rm -f "$NEW" "$CHECK_C" "$CHECK" "$CHECK_O"; }
trap cleanup EXIT HUP INT TERM

test -d "$LIBSODIUM_DIR/.git" || die "$LIBSODIUM_DIR is not a libsodium checkout"
test -z "$(git -C "$LIBSODIUM_DIR" status --short)" || die "libsodium tree has uncommitted changes"
LIBSODIUM_REVISION=$(git -C "$LIBSODIUM_DIR" rev-parse HEAD)

for f in LICENSE "$PRIVATE/ed25519_ref10.h" \
    "$PRIVATE/ed25519_ref10_fe_25_5.h" "$CORE/fe_25_5/constants.h" \
    "$CORE/fe_25_5/fe.h" "$CORE/fe_25_5/base.h" \
    "$CORE/fe_25_5/base2.h" "$CORE/ed25519_ref10.c" \
    "$SIGN/sign_ed25519_ref10.h" "$SIGN/keypair.c" "$SIGN/sign.c" \
    "$SIGN/open.c"; do
	test -f "$LIBSODIUM_DIR/$f" || die "missing libsodium source: $f"
done

(
	printf '/*  $OpenBSD: ed25519.sh,v 1.8 2026/09/16 00:59:12 djm Exp $ */\n\n'
	echo "/* Extracted from libsodium revision $LIBSODIUM_REVISION */"
	echo
	cat "$LIBSODIUM_DIR/LICENSE"
	cat <<'EOF'

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto_api.h"

/* OpenSSH compatibility glue for the small libsodium API subset below. */
#define ACQUIRE_FENCE (void)0
#define COMPILER_ASSERT(x) (void)sizeof(char[(x) ? 1 : -1])

#ifdef WITH_OPENSSL
# include <openssl/sha.h>
typedef SHA512_CTX crypto_hash_sha512_state;
#else
typedef SHA2_CTX crypto_hash_sha512_state;
#endif

static int
crypto_hash_sha512_init(crypto_hash_sha512_state *state)
{
#ifdef WITH_OPENSSL
	SHA512_Init(state);
#else
	SHA512Init(state);
#endif
	return 0;
}

static int
crypto_hash_sha512_update(crypto_hash_sha512_state *state,
    const unsigned char *in, unsigned long long inlen)
{
#ifdef WITH_OPENSSL
	SHA512_Update(state, in, inlen);
#else
	SHA512Update(state, in, inlen);
#endif
	return 0;
}

static int
crypto_hash_sha512_final(crypto_hash_sha512_state *state, unsigned char *out)
{
#ifdef WITH_OPENSSL
	SHA512_Final(out, state);
#else
	SHA512Final(out, state);
#endif
	return 0;
}

static int
sodium_is_zero(const unsigned char *n, size_t nlen)
{
	size_t i;
	volatile unsigned char d = 0;

	for (i = 0; i < nlen; i++)
		d |= n[i];
	return 1 & ((d - 1) >> 8);
}

EOF

	sed -e '/^#include/d' -e '/^# *include/d' \
	    -e 's/^void /static void /' \
	    -e 's/^int /static int /' \
	    -e '/ ge25519_is_on_curve(/d' \
	    -e '/ ge25519_is_on_main_subgroup(/d' \
	    -e '/ ge25519_tobytes(/d' \
	    -e '/ ge25519_scalarmult(/,/);/d' \
	    -e '/ ge25519_clear_cofactor(/d' \
	    -e '/ ge25519_from_uniform(/d' \
	    -e '/ ge25519_from_hash(/d' \
	    -e '/ ristretto255_frombytes(/d' \
	    -e '/ ristretto255_p3_tobytes(/d' \
	    -e '/ ristretto255_from_hash(/d' \
	    -e '/ sc25519_mul(/,/);/d' \
	    -e '/ sc25519_invert(/d' \
	    "$LIBSODIUM_DIR/$PRIVATE/ed25519_ref10.h"
	sed -e '/^#include/d' \
	    "$LIBSODIUM_DIR/$PRIVATE/ed25519_ref10_fe_25_5.h" | \
	perl -0777 -pe '
	    s/^static void\nfe25519_cswap\(.*?^\}\n//ms;
	    s/^static inline void\nfe25519_mul32\(.*?^\}\n//ms;
	'

	sed \
	    -e "/# include \"fe_25_5\/constants.h\"/r $LIBSODIUM_DIR/$CORE/fe_25_5/constants.h" \
	    -e "/# include \"fe_25_5\/fe.h\"/r $LIBSODIUM_DIR/$CORE/fe_25_5/fe.h" \
	    -e "/# include \"fe_25_5\/base.h\"/r $LIBSODIUM_DIR/$CORE/fe_25_5/base.h" \
	    -e "/# include \"fe_25_5\/base2.h\"/r $LIBSODIUM_DIR/$CORE/fe_25_5/base2.h" \
	    -e '/^#include/d' -e '/^# include/d' \
	    -e 's/^void /static void /' -e 's/^int /static int /' \
	    "$LIBSODIUM_DIR/$CORE/ed25519_ref10.c" | \
	sed -e '/^#include/d' -e '/^# include/d' | \
	perl -0777 -pe '
	    s/^static const fe25519 (?:ed25519_sqrtam2|ed25519_A|ed25519_sqrtadm1|ed25519_invsqrtamd|ed25519_onemsqd|ed25519_sqdmone) = \{.*?^\};\n//msg;
	    s/^#define ed25519_A_32 .*\n//m;
	    s/^static inline void\nfe25519_sqmul\(.*?^\}\n//ms;
	    s/^static inline void\nfe25519_cneg\(.*?^\}\n//ms;
	    s/^static inline void\nfe25519_abs\(.*?^\}\n//ms;
	    s/^static void\nfe25519_unchecked_sqrt\(.*?^\}\n//ms;
	    s/^static int\nfe25519_sqrt\(.*?^\}\n//ms;
	    s/^static int\nfe25519_notsquare\(.*?^\}\n//ms;
	    s/^static void\nge25519_p3_to_precomp\(.*?^\}\n//ms;
	    s/^static void\nge25519_cached_0\(.*?^\}\n//ms;
	    s/^static void\nge25519_cmov_cached\(.*?^\}\n//ms;
	    s/^static void\nge25519_cmov8_cached\(.*?^\}\n//ms;
	    s/^(?:static )?void\nge25519_tobytes\(.*?^\}\n//ms;
	    s/^(?:static )?void\nge25519_scalarmult\(.*?^\}\n//ms;
	    s/^static void\nge25519_p3p3_dbl\(.*?^\}\n//ms;
	    s/^static void\nge25519_p3_dbladd\(.*?^\}\n//ms;
	    s/^(?:static )?int\nge25519_is_on_curve\(.*?^\}\n//ms;
	    s/^(?:static )?int\nge25519_is_on_main_subgroup\(.*?^\}\n//ms;
	    s/^static void\nge25519_mul_l\(.*?^\}\n//ms;
	    s/^(?:static )?void\nsc25519_mul\(.*?^\}\n//ms;
	    s/^static inline void\nsc25519_sqmul\(.*?^\}\n//ms;
	    s/^static inline void\nsc25519_sq\(.*?^\}\n//ms;
	    s/^(?:static )?void\nsc25519_invert\(.*?^\}\n//ms;
	    s/^static void\nge25519_mont_to_ed\(.*?^\}\n//ms;
	    s/^static int\nge25519_xmont_to_ymont\(.*?^\}\n//ms;
	    s/^(?:static )?void\nge25519_clear_cofactor\(.*?^\}\n//ms;
	    s/^static void\nge25519_elligator2\(.*?^\}\n//ms;
	    s/^(?:static )?void\nge25519_from_uniform\(.*?^\}\n//ms;
	    s/^static void\nfe25519_reduce64\(.*?^\}\n//ms;
	    s/^(?:static )?void\nge25519_from_hash\(.*?^\}\n//ms;
	    s/^static int\nristretto255_sqrt_ratio_m1\([\s\S]*\z//m;
	'

	sed -e '/^#include/d' -e 's/^void /static void /' \
	    -e 's/^int /static int /' \
	    "$LIBSODIUM_DIR/$SIGN/sign_ed25519_ref10.h"

	sed -e '/^#include/d' -e 's/sodium_memzero/explicit_bzero/g' \
	    -e 's/randombytes_buf/arc4random_buf/g' \
	    -e '/^int$/N; /crypto_sign_ed25519_pk_to_curve25519/,$d' \
	    "$LIBSODIUM_DIR/$SIGN/keypair.c"

	sed -e '/^#include/d' -e 's/sodium_memzero/explicit_bzero/g' \
	    -e 's/^void$/static void/' -e '/^int$/N; /crypto_sign_ed25519(/,$d' \
	    "$LIBSODIUM_DIR/$SIGN/sign.c" | \
	sed -e '/^int$/N; /_crypto_sign_ed25519_detached/ { s/^int$/static int/; }'

	sed -e '/^#include/d' -e '/^int$/N; /crypto_sign_ed25519_open/,$d' \
	    "$LIBSODIUM_DIR/$SIGN/open.c" | \
	sed -e '/^int$/N; /_crypto_sign_ed25519_verify_detached/ { s/^int$/static int/; }'

	echo
) | sed -e 's/[[:space:]]*$//' > "$NEW"

cat > "$CHECK_C" <<'EOF'
#include "ed25519.c.new"

#include <err.h>

static int
hexval(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	err(1, "invalid hex digit");
}

static void
unhex(unsigned char *out, size_t outlen, const char *hex)
{
	size_t i;
	for (i = 0; i < outlen; i++)
		out[i] = (hexval(hex[i * 2]) << 4) | hexval(hex[i * 2 + 1]);
}

int
main(void)
{
	static const char seedhex[] =
	    "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
	static const char pkhex[] =
	    "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
	static const char sighex[] =
	    "e5564300c360ac729086e2cc806e828a84877f1e"
	    "b8e5d974d873e065224901555fb8821590a33bac"
	    "c61e39701cf9b46bd25bf5f0595bbe2465514143"
	    "8e7a100b";
	unsigned char seed[32], pk[32], wantpk[32], sk[64];
	unsigned char sig[64], wantsig[64], msg[1] = { 0 }, randompk[32], randomsk[64];
	unsigned long long siglen = 0;

	unhex(seed, sizeof(seed), seedhex);
	unhex(wantpk, sizeof(wantpk), pkhex);
	unhex(wantsig, sizeof(wantsig), sighex);
	if (crypto_sign_ed25519_seed_keypair(pk, sk, seed) != 0 ||
	    memcmp(pk, wantpk, sizeof(pk)) != 0)
		errx(1, "seed keypair KAT failed");
	if (crypto_sign_ed25519_detached(sig, &siglen, msg, 0, sk) != 0 ||
	    siglen != sizeof(sig) || memcmp(sig, wantsig, sizeof(sig)) != 0)
		errx(1, "signature KAT failed");
	if (crypto_sign_ed25519_verify_detached(sig, msg, 0, pk) != 0)
		errx(1, "verification failed");
	sig[10] ^= 1;
	if (crypto_sign_ed25519_verify_detached(sig, msg, 0, pk) == 0)
		errx(1, "corrupt signature accepted");
	if (crypto_sign_ed25519_keypair(randompk, randomsk) != 0 ||
	    crypto_sign_ed25519_detached(sig, &siglen, msg, sizeof(msg), randomsk) != 0 ||
	    crypto_sign_ed25519_verify_detached(sig, msg, sizeof(msg), randompk) != 0)
		errx(1, "random keypair smoke test failed");
	msg[0] ^= 1;
	if (crypto_sign_ed25519_verify_detached(sig, msg, sizeof(msg), randompk) == 0)
		errx(1, "modified message accepted");
	return 0;
}
EOF

cd "$SCRIPT_DIR"
${CC:-cc} -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -I. -c ed25519_check.c -o ed25519_check.o
globals=$(nm -g ed25519_check.o | \
    grep -E "^([0-9a-fA-F]+)?[[:space:]]+T[[:space:]]" | \
    awk '{ print $3}' | \
    grep -v "^main$")
expected='crypto_sign_ed25519_detached
crypto_sign_ed25519_keypair
crypto_sign_ed25519_seed_keypair
crypto_sign_ed25519_verify_detached'
test "$globals" = "$expected" || {
	echo "unexpected global symbols in ed25519.c:" >&2
	echo "$globals" >&2
	exit 1
}
${CC:-cc} -o ed25519_check ed25519_check.o
./ed25519_check
mv "$NEW" "$OUT"
echo "ed25519.c OK (libsodium $LIBSODIUM_REVISION)" >&2

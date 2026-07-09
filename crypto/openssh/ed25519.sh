#!/bin/sh
#       $OpenBSD: ed25519.sh,v 1.6 2026/06/14 04:16:19 djm Exp $
#       Placed in the Public Domain.
#
AUTHOR="supercop-20221122/crypto_sign/ed25519/ref/implementors"
FILES="
	supercop-20221122/crypto_verify/32/ref/verify.c
	supercop-20221122/crypto_sign/ed25519/ref/fe25519.h
	supercop-20221122/crypto_sign/ed25519/ref/fe25519.c
	supercop-20221122/crypto_sign/ed25519/ref/sc25519.h
	supercop-20221122/crypto_sign/ed25519/ref/sc25519.c
	supercop-20221122/crypto_sign/ed25519/ref/ge25519.h
	supercop-20221122/crypto_sign/ed25519/ref/ge25519.c
	supercop-20221122/crypto_sign/ed25519/ref/keypair.c
	supercop-20221122/crypto_sign/ed25519/ref/sign.c
	supercop-20221122/crypto_sign/ed25519/ref/open.c
"
###
PORTABLE=${PORTABLE:-0}

DATA="supercop-20221122/crypto_sign/ed25519/ref/ge25519_base.data"

set -e
test -z "$1" || cd $1
echo -n '/*  $'
echo 'OpenBSD: $ */'
echo
echo '/*'
echo ' * Public Domain, Authors:'
sed -e '/Alphabetical order:/d' -e 's/^/ * - /' < $AUTHOR
echo ' */'
echo
if [ "$PORTABLE" -ne 0 ]; then
	echo '#include "includes.h"'
	echo
	echo '#ifndef OPENSSL_HAS_ED25519'
	echo
fi
echo '#include <string.h>'
echo
echo '#include "crypto_api.h"'
echo
# Map the types used in this code to the ones in crypto_api.h.  We use #define
# instead of typedef since some systems have existing intXX types and do not
# permit multiple typedefs even if they do not conflict.
for t in int8 uint8 int16 uint16 int32 uint32 int64 uint64; do
	echo "#define $t crypto_${t}"
done
echo
for i in $FILES; do
	echo "/* from $i */"

	case "$i" in
	*/crypto_sign/ed25519/ref/open.c)
	# Include our malleability fix at the start if open.c
	cat << _EOF

/*
 * Local OpenSSH addition: check that S < group order L
 * Where L = 2^{252} + 27742317777372353535851937790883648493
 * This can be variable time as the signature is public.
 */
static inline int sc25519_inrange(const unsigned char *s)
{
  int i;

  for (i = 0; i < 32; i++) {
    if (s[31 - i] > sc25519_m[31 - i]) return -1;
    if (s[31 - i] < sc25519_m[31 - i]) return 0;
  }
  return -1;
}
_EOF
	;;
	esac

	nl=`echo`
	# Changes to all files:
	#  - inline ge25519_base.data where it is included
	#  - expand CRYPTO_NAMESPACE() namespacing define
	#  - remove all includes, we inline everything required.
	#  - make functions not required elsewhere static.
	#  - rename the functions we do use.
	sed \
	    -e "/#include \"ge25519_base.data\"/r $DATA" \
	    -e "/#include/d" \
	    -e "s/^void /static void /g" \
	    -e 's/CRYPTO_NAMESPACE[(]\([a-zA-Z0-9_]*\)[)]/crypto_sign_ed25519_ref_\1/g' \
	    $i | \
	case "$i" in
	*/crypto_verify/32/ref/verify.c)
	    # rename crypto_verify() to the name that the ed25519 code expects.
	    sed -e "/^#include.*/d" \
	        -e "s/crypto_verify/crypto_verify_32/g" \
	        -e "s/^int /static int /g"
	    ;;
	*/crypto_sign/ed25519/ref/sign.c)
	    # rename signing function to the name OpenSSH expects
	    sed -e "s/crypto_sign/crypto_sign_ed25519/g"
	    ;;
	*/crypto_sign/ed25519/ref/keypair.c)
	    # provide an explicit-seed key generation function and rename
	    # it to the name OpenSSH expects
	    sed -e "s/int crypto_sign_keypair(unsigned char \*pk,unsigned char \*sk)/int crypto_sign_ed25519_keypair_from_seed(unsigned char *pk,unsigned char *sk, const unsigned char *seed)/g" \
	        -e "s/randombytes(sk,32);/memcpy(sk, seed, 32);/g"
	    ;;
	*/crypto_sign/ed25519/ref/open.c)
	    # rename verification function to the name OpenSSH expects
	    # Insert malleability checks
	    sed -e "s/crypto_sign_open/crypto_sign_ed25519_open/g" | \
	    perl -0777 -pe 's/(.*if.*ge25519_unpackneg_vartime.*get1,pk.*)/  if (sc25519_inrange(sm+32)) goto badsig;\n\1\n  if (ge25519_isneutral_vartime(&get1)) goto badsig;/'
	    #perl -0777 -pe 's/^(.*ge25519_unpackneg_vartime.*,pk.*)$/  if (sc25519_inrange(sm+32)) goto badsig;\n$1\n  if (ge25519_isneutral_vartime(&get1)) goto badsig;/'
	    ;;
	*/crypto_sign/ed25519/ref/fe25519.*)
	    # avoid a couple of name collisions with other files
	    sed -e "s/reduce_add_sub/fe25519_reduce_add_sub/g" \
	        -e "s/ equal[(]/ fe25519_equal(/g" \
	        -e "s/^int /static int /g"
	    ;;
	*/crypto_sign/ed25519/ref/sc25519.h)
	    # Lots of unused prototypes to remove
	    sed -e "s/^int /static int /g" \
	        -e '/shortsc25519_from16bytes/d' \
	        -e '/sc25519_iszero_vartime/d' \
	        -e '/sc25519_isshort_vartime/d' \
	        -e '/sc25519_lt_vartime/d' \
	        -e '/sc25519_sub_nored/d' \
	        -e '/sc25519_mul_shortsc/d' \
	        -e '/sc25519_from_shortsc/d' \
	        -e '/sc25519_window5/d'
	    ;;
	*/crypto_sign/ed25519/ref/sc25519.c)
	    # Lots of unused code to remove, some name collisions to avoid
	    sed -e "s/reduce_add_sub/sc25519_reduce_add_sub/g" \
	        -e "s/ equal[(]/ sc25519_equal(/g" \
	        -e "s/^int /static int /g" \
	        -e "s/m[[]/sc25519_m[/g" \
	        -e "s/mu[[]/sc25519_mu[/g" \
	        -e '/shortsc25519_from16bytes/,/^}$/d' \
	        -e '/sc25519_iszero_vartime/,/^}$/d' \
	        -e '/sc25519_isshort_vartime/,/^}$/d' \
	        -e '/sc25519_lt_vartime/,/^}$/d' \
	        -e '/sc25519_sub_nored/,/^}$/d' \
	        -e '/sc25519_mul_shortsc/,/^}$/d' \
	        -e '/sc25519_from_shortsc/,/^}$/d' \
	        -e '/sc25519_window5/,/^}$/d'
	    ;;
	*/crypto_sign/ed25519/ref//ge25519.*)
	    sed -e "s/^int /static int /g"
	    ;;
	# Default: pass through.
	*)
	    cat
	    ;;
	esac | \
	sed -e 's/[	 ]*$//'

	# Include implicit-seed keygen function used for ssh-ed25519
	case "$i" in
	*/crypto_sign/ed25519/ref/keypair.c)
	cat << _EOF

int
crypto_sign_ed25519_keypair(unsigned char *pk, unsigned char *sk)
{
  unsigned char seed[32];
  int r;

  randombytes(seed, 32);
  r = crypto_sign_ed25519_keypair_from_seed(pk, sk, seed);
  explicit_bzero(seed, sizeof(seed));
  return r;
}
_EOF
	;;
	esac

done

if [ "$PORTABLE" -ne 0 ]; then
       echo
       echo '#endif /* OPENSSL_HAS_ED25519 */'
fi

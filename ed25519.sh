#!/bin/sh
#       $OpenBSD: ed25519.sh,v 1.2 2024/05/17 02:39:11 jsg Exp $
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

DATA="supercop-20221122/crypto_sign/ed25519/ref/ge25519_base.data"

set -e
cd $1
echo -n '/*  $'
echo 'OpenBSD: $ */'
echo
echo '/*'
echo ' * Public Domain, Authors:'
sed -e '/Alphabetical order:/d' -e 's/^/ * - /' < $AUTHOR
echo ' */'
echo
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
	    # rename key generation function to the name OpenSSH expects
	    sed -e "s/crypto_sign_keypair/crypto_sign_ed25519_keypair/g"
	    ;;
	*/crypto_sign/ed25519/ref/open.c)
	    # rename verification function to the name OpenSSH expects
	    sed -e "s/crypto_sign_open/crypto_sign_ed25519_open/g"
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
done

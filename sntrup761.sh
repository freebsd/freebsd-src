#!/bin/sh
#       $OpenBSD: sntrup761.sh,v 1.9 2024/09/16 05:37:05 djm Exp $
#       Placed in the Public Domain.
#
AUTHOR="supercop-20240808/crypto_kem/sntrup761/ref/implementors"
FILES=" supercop-20240808/cryptoint/crypto_int16.h
	supercop-20240808/cryptoint/crypto_int32.h
	supercop-20240808/cryptoint/crypto_int64.h
	supercop-20240808/crypto_sort/int32/portable4/sort.c
	supercop-20240808/crypto_sort/uint32/useint32/sort.c
	supercop-20240808/crypto_kem/sntrup761/compact/kem.c
"
###

set -euo pipefail
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
echo '#include "crypto_api.h"'
echo
echo '#define crypto_declassify(x, y) do {} while (0)'
echo
# Map the types used in this code to the ones in crypto_api.h.  We use #define
# instead of typedef since some systems have existing intXX types and do not
# permit multiple typedefs even if they do not conflict.
for t in int8 uint8 int16 uint16 int32 uint32 int64 uint64; do
	echo "#define $t crypto_${t}"
done

for x in 16 32 64 ; do
	echo "extern volatile crypto_int$x crypto_int${x}_optblocker;"
done

echo
for i in $FILES; do
	echo "/* from $i */"
	# Changes to all files:
	#  - remove all includes, we inline everything required.
	#  - make functions not required elsewhere static.
	#  - rename the functions we do use.
	#  - remove unnecessary defines and externs.
	sed -e "/#include/d" \
	    -e "s/crypto_kem_/crypto_kem_sntrup761_/g" \
	    -e "s/^void /static void /g" \
	    -e "s/^int16 /static int16 /g" \
	    -e "s/^uint16 /static uint16 /g" \
	    -e "/^extern /d" \
	    -e '/CRYPTO_NAMESPACE/d' \
	    -e "/^#define int32 crypto_int32/d" \
	    -e 's/[	 ]*$//' \
	    $i | \
	case "$i" in
	*/cryptoint/crypto_int16.h)
	    sed -e "s/static void crypto_int16_store/void crypto_int16_store/" \
		-e "s/^[#]define crypto_int16_optblocker.*//" \
	        -e "s/static void crypto_int16_minmax/void crypto_int16_minmax/"
	    ;;
	*/cryptoint/crypto_int32.h)
	# Use int64_t for intermediate values in crypto_int32_minmax to
	# prevent signed 32-bit integer overflow when called by
	# crypto_sort_int32. Original code depends on -fwrapv (we set -ftrapv)
	    sed -e "s/static void crypto_int32_store/void crypto_int32_store/" \
		-e "s/^[#]define crypto_int32_optblocker.*//" \
		-e "s/crypto_int32 crypto_int32_r = crypto_int32_y ^ crypto_int32_x;/crypto_int64 crypto_int32_r = (crypto_int64)crypto_int32_y ^ (crypto_int64)crypto_int32_x;/" \
		-e "s/crypto_int32 crypto_int32_z = crypto_int32_y - crypto_int32_x;/crypto_int64 crypto_int32_z = (crypto_int64)crypto_int32_y - (crypto_int64)crypto_int32_x;/" \
	        -e "s/static void crypto_int32_minmax/void crypto_int32_minmax/"
	    ;;
	*/cryptoint/crypto_int64.h)
	    sed -e "s/static void crypto_int64_store/void crypto_int64_store/" \
		-e "s/^[#]define crypto_int64_optblocker.*//" \
	        -e "s/static void crypto_int64_minmax/void crypto_int64_minmax/"
	    ;;
	*/int32/portable4/sort.c)
	    sed -e "s/void crypto_sort[(]/void crypto_sort_int32(/g"
	    ;;
	*/int32/portable5/sort.c)
	    sed -e "s/crypto_sort_smallindices/crypto_sort_int32_smallindices/"\
	        -e "s/void crypto_sort[(]/void crypto_sort_int32(/g"
	    ;;
	*/uint32/useint32/sort.c)
	    sed -e "s/void crypto_sort/void crypto_sort_uint32/g"
	    ;;
	# Remove unused function to prevent warning.
	*/crypto_kem/sntrup761/ref/int32.c)
	    sed -e '/ int32_div_uint14/,/^}$/d'
	    ;;
	# Remove unused function to prevent warning.
	*/crypto_kem/sntrup761/ref/uint32.c)
	    sed -e '/ uint32_div_uint14/,/^}$/d'
	    ;;
	# Default: pass through.
	*)
	    cat
	    ;;
	esac
	echo
done

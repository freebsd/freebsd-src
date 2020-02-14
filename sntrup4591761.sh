#!/bin/sh
#       $OpenBSD: sntrup4591761.sh,v 1.3 2019/01/30 19:51:15 markus Exp $
#       Placed in the Public Domain.
#
AUTHOR="libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/implementors"
FILES="
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/int32_sort.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/int32_sort.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/small.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/mod3.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/modq.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/params.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/swap.h
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/dec.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/enc.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/keypair.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3_mult.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3_recip.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/randomsmall.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/randomweightw.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_mult.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_recip3.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_round3.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_rounded.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/small.c
	libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/swap.c
"
###

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
echo '#include "crypto_api.h"'
echo
for i in $FILES; do
	echo "/* from $i */"
	b=$(basename $i .c)
	grep \
	   -v '#include' $i | \
	   grep -v "extern crypto_int32 small_random32" |
	   sed -e "s/crypto_kem_/crypto_kem_sntrup4591761_/g" \
		-e "s/smaller_mask/smaller_mask_${b}/g" \
		-e "s/^extern void /static void /" \
		-e "s/^void /static void /"
	echo
done

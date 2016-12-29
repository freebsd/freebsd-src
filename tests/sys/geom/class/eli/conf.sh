#!/bin/sh
# $FreeBSD$

class="eli"
base=`basename $0`

# We need to use linear probing in order to detect the first available md(4)
# device instead of using mdconfig -a -t, because geli(8) attachs md(4) devices
no=0
while [ -c /dev/md$no ]; do
	: $(( no += 1 ))
done

# Execute `func` for each combination of cipher, sectorsize, and hmac algo
# `func` usage should be:
# func <cipher> <aalgo> <secsize>
for_each_geli_config() {
	func=$1

	for cipher in aes-xts:128 aes-xts:256 \
	    aes-cbc:128 aes-cbc:192 aes-cbc:256 \
	    3des-cbc:192 \
	    blowfish-cbc:128 blowfish-cbc:160 blowfish-cbc:192 \
	    blowfish-cbc:224 blowfish-cbc:256 blowfish-cbc:288 \
	    blowfish-cbc:320 blowfish-cbc:352 blowfish-cbc:384 \
	    blowfish-cbc:416 blowfish-cbc:448 \
	    camellia-cbc:128 camellia-cbc:192 camellia-cbc:256; do
		ealgo=${cipher%%:*}
		keylen=${cipher##*:}
		for aalgo in hmac/md5 hmac/sha1 hmac/ripemd160 hmac/sha256 \
		    hmac/sha384 hmac/sha512; do
			for secsize in 512 1024 2048 4096 8192; do
				${func} $cipher $aalgo $secsize
			done
		done
	done
}

# Execute `func` for each combination of cipher, and sectorsize, with no hmac
# `func` usage should be:
# func <cipher> <secsize>
for_each_geli_config_nointegrity() {
	func=$1

	for cipher in aes-xts:128 aes-xts:256 \
	    aes-cbc:128 aes-cbc:192 aes-cbc:256 \
	    3des-cbc:192 \
	    blowfish-cbc:128 blowfish-cbc:160 blowfish-cbc:192 \
	    blowfish-cbc:224 blowfish-cbc:256 blowfish-cbc:288 \
	    blowfish-cbc:320 blowfish-cbc:352 blowfish-cbc:384 \
	    blowfish-cbc:416 blowfish-cbc:448 \
	    camellia-cbc:128 camellia-cbc:192 camellia-cbc:256; do
		ealgo=${cipher%%:*}
		keylen=${cipher##*:}
		for secsize in 512 1024 2048 4096 8192; do
			${func} $cipher $aalgo $secsize
		done
	done
}


geli_test_cleanup()
{
	[ -c /dev/md${no}.eli ] && geli detach md${no}.eli
	mdconfig -d -u $no
}
trap geli_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh

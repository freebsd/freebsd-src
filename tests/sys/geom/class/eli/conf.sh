#!/bin/sh
# $FreeBSD$

class="eli"
base=`basename $0`

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
				bytes=`expr $secsize \* $sectors + 512`b
				md=$(attach_md -t malloc -s $bytes)
				${func} $cipher $aalgo $secsize
				geli detach ${md} 2>/dev/null
				mdconfig -d -u ${md} 2>/dev/null
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
			bytes=`expr $secsize \* $sectors + 512`b
			md=$(attach_md -t malloc -s $bytes)
			${func} $cipher $aalgo $secsize
			geli detach ${md} 2>/dev/null
			mdconfig -d -u ${md} 2>/dev/null
		done
	done
}


geli_test_cleanup()
{
	if [ -f "$TEST_MDS_FILE" ]; then
		while read md; do
			[ -c /dev/${md}.eli ] && \
				geli detach $md.eli 2>/dev/null
			mdconfig -d -u $md 2>/dev/null
		done < $TEST_MDS_FILE
	fi
	rm -f "$TEST_MDS_FILE"
}
trap geli_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh

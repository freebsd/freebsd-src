#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100

echo "1..1380"

i=1
for cipher in aes:0 aes:128 aes:256 \
    aes-xts:0 aes-xts:128 aes-xts:256 \
    aes-cbc:0 aes-cbc:128 aes-cbc:192 aes-cbc:256 \
    3des:0 3des:192 \
    3des-cbc:0 3des-cbc:192 \
    blowfish:0 blowfish:128 blowfish:160 blowfish:192 blowfish:224 \
    blowfish:256 blowfish:288 blowfish:320 blowfish:352 blowfish:384 \
    blowfish:416 blowfish:448 \
    blowfish-cbc:0 blowfish-cbc:128 blowfish-cbc:160 blowfish-cbc:192 blowfish-cbc:224 \
    blowfish-cbc:256 blowfish-cbc:288 blowfish-cbc:320 blowfish-cbc:352 blowfish-cbc:384 \
    blowfish-cbc:416 blowfish-cbc:448 \
    camellia:0 camellia:128 camellia:192 camellia:256 \
    camellia-cbc:0 camellia-cbc:128 camellia-cbc:192 camellia-cbc:256; do
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}
	for aalgo in hmac/md5 hmac/sha1 hmac/ripemd160 hmac/sha256 hmac/sha384 hmac/sha512; do
		for secsize in 512 1024 2048 4096 8192; do
			rnd=`mktemp $base.XXXXXX` || exit 1
			mdconfig -a -t malloc -s `expr $secsize \* $sectors + 512`b -u $no || exit 1

			geli onetime -a $aalgo -e $ealgo -l $keylen -s $secsize md${no} 2>/dev/null

			secs=`diskinfo /dev/md${no}.eli | awk '{print $4}'`

			dd if=/dev/random of=${rnd} bs=${secsize} count=${secs} >/dev/null 2>&1
			dd if=${rnd} of=/dev/md${no}.eli bs=${secsize} count=${secs} 2>/dev/null

			md_rnd=`dd if=${rnd} bs=${secsize} count=${secs} 2>/dev/null | md5`
			md_ddev=`dd if=/dev/md${no}.eli bs=${secsize} count=${secs} 2>/dev/null | md5`

			if [ ${md_rnd} = ${md_ddev} ]; then
				echo "ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			rm -f $rnd
			mdconfig -d -u $no
		done
	done
done

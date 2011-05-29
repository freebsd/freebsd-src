#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sector=`mktemp /tmp/$base.XXXXXX` || exit 1

echo "1..2760"

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
			mdconfig -a -t malloc -s `expr $secsize \* 2 + 512`b -u $no || exit 1

			dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

			geli init -B none -a $aalgo -e $ealgo -l $keylen -P -K $keyfile -s $secsize md${no} 2>/dev/null
			geli attach -p -k $keyfile md${no}

			dd if=/dev/random of=/dev/md${no}.eli bs=${secsize} count=1 >/dev/null 2>&1

			dd if=/dev/md${no}.eli bs=${secsize} count=1 >/dev/null 2>&1
			if [ $? -eq 0 ]; then
				echo "ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			# Corrupt 8 bytes of HMAC.
			dd if=/dev/md${no} of=${sector} bs=512 count=1 >/dev/null 2>&1
			dd if=/dev/random of=${sector} bs=1 count=16 conv=notrunc >/dev/null 2>&1
			dd if=${sector} of=/dev/md${no} bs=512 count=1 >/dev/null 2>&1
			geli attach -p -k $keyfile md${no}

			dd if=/dev/md${no}.eli bs=${secsize} count=1 >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				echo "ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			mdconfig -d -u $no
		done
	done
done

rm -f $keyfile $sector

#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sector=`mktemp /tmp/$base.XXXXXX` || exit 1

echo "1..2640"

i=1
for cipher in aes:0 aes:128 aes:192 aes:256 \
    3des:0 3des:192 \
    blowfish:0 blowfish:128 blowfish:160 blowfish:192 blowfish:224 \
    blowfish:256 blowfish:288 blowfish:320 blowfish:352 blowfish:384 \
    blowfish:416 blowfish:448 \
    camellia:0 camellia:128 camellia:192 camellia:256; do
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}
	for aalgo in hmac/md5 hmac/sha1 hmac/ripemd160 hmac/sha256 hmac/sha384 hmac/sha512; do
		for secsize in 512 1024 2048 4096 8192; do
			#mdconfig -a -t malloc -s `expr $secsize \* 2 + 512`b -u $no || exit 1
			mdconfig -a -t malloc -s $sectors -u $no || exit 1

			dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

			geli init -B none -a $aalgo -e $ealgo -l $keylen -P -K $keyfile -s $secsize md${no} 2>/dev/null
			geli attach -p -k $keyfile md${no}

			dd if=/dev/random of=/dev/md${no}.eli bs=${secsize} count=1 >/dev/null 2>&1

			dd if=/dev/md${no}.eli bs=${secsize} count=1 >/dev/null 2>&1
			if [ $? -eq 0 ]; then
				echo "ok $i - small 1 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - small 1 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			# Copy first small sector to the second small sector.
			# This should be detected as corruption.
			dd if=/dev/md${no} of=${sector} bs=512 count=1 >/dev/null 2>&1
			dd if=${sector} of=/dev/md${no} bs=512 count=1 seek=1 >/dev/null 2>&1
			geli attach -p -k $keyfile md${no}

			dd if=/dev/md${no}.eli of=/dev/null bs=${secsize} count=1 >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				echo "ok $i - small 2 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - small 2 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			ms=`diskinfo /dev/md${no} | awk '{print $3 - 512}'`
			ns=`diskinfo /dev/md${no}.eli | awk '{print $4}'`
			usecsize=`echo "($ms / $ns) - (($ms / $ns) % 512)" | bc`

			dd if=/dev/random of=/dev/md${no}.eli bs=${secsize} count=2 >/dev/null 2>&1

			dd if=/dev/md${no}.eli bs=${secsize} count=2 >/dev/null 2>&1
			if [ $? -eq 0 ]; then
				echo "ok $i - big 1 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - big 1 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			# Copy first big sector to the second big sector.
			# This should be detected as corruption.
			dd if=/dev/md${no} of=${sector} bs=${usecsize} count=1 >/dev/null 2>&1
			dd if=${sector} of=/dev/md${no} bs=${usecsize} count=1 seek=1 >/dev/null 2>&1
			geli attach -p -k $keyfile md${no}

			dd if=/dev/md${no}.eli of=/dev/null bs=${secsize} count=2 >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				echo "ok $i - big 2 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			else
				echo "not ok $i - big 2 aalgo=${aalgo} ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
			fi
			i=$((i+1))

			geli detach md${no}
			mdconfig -d -u $no
		done
	done
done

rm -f $keyfile $sector

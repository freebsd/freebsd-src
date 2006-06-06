#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100

echo "1..180"

i=1
for cipher in aes:0 aes:128 aes:192 aes:256 \
    3des:0 3des:192 \
    blowfish:0 blowfish:128 blowfish:160 blowfish:192 blowfish:224 \
    blowfish:256 blowfish:288 blowfish:320 blowfish:352 blowfish:384 \
    blowfish:416 blowfish:448; do
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}
	for secsize in 512 1024 2048 4096 8192; do
		rnd=`mktemp /tmp/$base.XXXXXX` || exit 1
		mdconfig -a -t malloc -s `expr $secsize \* $sectors`b -u $no || exit 1

		geli onetime -e $ealgo -l $keylen -s $secsize md${no}

		secs=`diskinfo /dev/md${no}.eli | awk '{print $4}'`

		dd if=/dev/random of=${rnd} bs=${secsize} count=${secs} >/dev/null 2>&1
		dd if=${rnd} of=/dev/md${no}.eli bs=${secsize} count=${secs} 2>/dev/null

		md_rnd=`dd if=${rnd} bs=${secsize} count=${secs} 2>/dev/null | md5`
		md_ddev=`dd if=/dev/md${no}.eli bs=${secsize} count=${secs} 2>/dev/null | md5`
		md_edev=`dd if=/dev/md${no} bs=${secsize} count=${secs} 2>/dev/null | md5`

		if [ ${md_rnd} = ${md_ddev} ]; then
			echo "ok $i - ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
		else
			echo "not ok $i - ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
		fi
		i=$((i+1))
		if [ ${md_rnd} != ${md_edev} ]; then
			echo "ok $i - ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
		else
			echo "not ok $i - ealgo=${ealgo} keylen=${keylen} sec=${secsize}"
		fi
		i=$((i+1))

		geli detach md${no}
		rm -f $rnd
		mdconfig -d -u $no
	done
done

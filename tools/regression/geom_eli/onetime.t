#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
rnd=`mktemp /tmp/$base.XXXXXX` || exit 1
mdconfig -a -t malloc -s $sectors -u $no || exit 1

echo "1..36"

i=1
for cipher in aes:0 aes:128 aes:192 aes:256 \
    3des:0 3des:192 \
    blowfish:0 blowfish:128 blowfish:160 blowfish:192 blowfish:224 \
    blowfish:256 blowfish:288 blowfish:320 blowfish:352 blowfish:384 \
    blowfish:416 blowfish:448; do
	algo=${cipher%%:*}
	keylen=${cipher##*:}

	dd if=/dev/random of=${rnd} bs=512 count=${sectors} >/dev/null 2>&1

	geli onetime -a $algo -l $keylen md${no}

	dd if=${rnd} of=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null

	md_rnd=`dd if=${rnd} bs=512 count=${sectors} 2>/dev/null | md5`
	md_ddev=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	md_edev=`dd if=/dev/md${no} bs=512 count=${sectors} 2>/dev/null | md5`

	if [ ${md_rnd} = ${md_ddev} ]; then
		echo "ok $i - ${cipher}"
	else
		echo "not ok $i - ${cipher}"
	fi
	i=$((i+1))
	if [ ${md_rnd} != ${md_edev} ]; then
		echo "ok $i - ${cipher}"
	else
		echo "not ok $i - ${cipher}"
	fi
	i=$((i+1))

	geli detach md${no}
done

mdconfig -d -u $no
rm -f $rnd

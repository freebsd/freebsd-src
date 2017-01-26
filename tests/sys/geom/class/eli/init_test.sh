#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=32
keyfile=`mktemp $base.XXXXXX` || exit 1
rnd=`mktemp $base.XXXXXX` || exit 1

echo "1..200"

do_test() {
	cipher=$1
	secsize=$2
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}

	mdconfig -a -t malloc -s `expr $secsize \* $sectors + 512`b -u $no || exit 1

	geli init -B none -e $ealgo -l $keylen -P -K $keyfile -s $secsize md${no} 2>/dev/null
	geli attach -p -k $keyfile md${no}

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
	mdconfig -d -u $no
}

i=1
dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1
for_each_geli_config_nointegrity do_test

rm -f $rnd
rm -f $keyfile

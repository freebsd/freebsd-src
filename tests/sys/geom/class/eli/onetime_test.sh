#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100

echo "1..200"

do_test() {
	cipher=$1
	secsize=$2
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}

	rnd=`mktemp $base.XXXXXX` || exit 1

	geli onetime -e $ealgo -l $keylen -s $secsize ${md} 2>/dev/null

	secs=`diskinfo /dev/${md}.eli | awk '{print $4}'`

	dd if=/dev/random of=${rnd} bs=${secsize} count=${secs} >/dev/null 2>&1
	dd if=${rnd} of=/dev/${md}.eli bs=${secsize} count=${secs} 2>/dev/null

	md_rnd=`dd if=${rnd} bs=${secsize} count=${secs} 2>/dev/null | md5`
	md_ddev=`dd if=/dev/${md}.eli bs=${secsize} count=${secs} 2>/dev/null | md5`
	md_edev=`dd if=/dev/${md} bs=${secsize} count=${secs} 2>/dev/null | md5`

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

	rm -f $rnd
}

i=1
for_each_geli_config_nointegrity do_test

#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
keyfile=`mktemp $base.XXXXXX` || exit 1
sector=`mktemp $base.XXXXXX` || exit 1

echo "1..2400"

do_test() {
	cipher=$1
	aalgo=$2
	secsize=$3
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}

	mdconfig -a -t malloc -s `expr $secsize \* 2 + 512`b -u $no || exit 1
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

	# Fix the corruption
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
}


i=1
dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

for_each_geli_config do_test

rm -f $keyfile $sector

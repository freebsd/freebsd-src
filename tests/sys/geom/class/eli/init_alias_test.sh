#!/bin/sh
# $FreeBSD$

# Test "geli init"'s various cipher aliases
. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
keyfile=`mktemp $base.XXXXXX` || exit 1
rnd=`mktemp $base.XXXXXX` || exit 1

do_test() {
	ealgo=$1
	keylen=$2
	expected_ealgo=$3
	expected_keylen=$4

	geli init -B none -e $ealgo -l $keylen -P -K $keyfile md${no} 2>/dev/null
	geli attach -p -k $keyfile md${no}
	real_ealgo=`geli list md${no}.eli | awk '/EncryptionAlgorithm/ {print $2}'`
	real_keylen=`geli list md${no}.eli | awk '/KeyLength/ {print $2}'`

	if [ ${real_ealgo} = ${expected_ealgo} ]; then
		echo "ok $i - ${ealgo} aliased to ${real_ealgo}"
	else
		echo "not ok $i - expected ${expected_ealgo} but got ${real_ealgo}"
	fi
	i=$((i+1))

	if [ ${real_keylen} = ${expected_keylen} ]; then
		echo "ok $i - keylen=${keylen} for ealgo=${ealgo} aliases to ${real_keylen}"
	else
		echo "not ok $i - expected ${expected_keylen} but got ${real_keylen}"
	fi
	i=$((i+1))

	geli detach md${no}
}

echo "1..38"
i=1
mdconfig -a -t malloc -s 1024k -u $no || exit 1
dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

for spec in aes:0:AES-XTS:128 aes:128:AES-XTS:128 aes:256:AES-XTS:256 \
	3des:0:3DES-CBC:192 3des:192:3DES-CBC:192 \
	blowfish:0:Blowfish-CBC:128 blowfish:128:Blowfish-CBC:128 \
	blowfish:160:Blowfish-CBC:160 blowfish:192:Blowfish-CBC:192 \
	blowfish:224:Blowfish-CBC:224 blowfish:256:Blowfish-CBC:256 \
	blowfish:288:Blowfish-CBC:288 blowfish:352:Blowfish-CBC:352 \
	blowfish:384:Blowfish-CBC:384 blowfish:416:Blowfish-CBC:416 \
	blowfish:448:Blowfish-CBC:448 \
	camellia:0:CAMELLIA-CBC:128 camellia:128:CAMELLIA-CBC:128 \
	camellia:256:CAMELLIA-CBC:256 ; do

	ealgo=`echo $spec | cut -d : -f 1`
	keylen=`echo $spec | cut -d : -f 2`
	expected_ealgo=`echo $spec | cut -d : -f 3`
	expected_keylen=`echo $spec | cut -d : -f 4`

	do_test $ealgo $keylen $expected_ealgo $expected_keylen
done

rm -f $keyfile

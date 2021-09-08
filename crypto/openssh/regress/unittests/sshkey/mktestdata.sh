#!/bin/sh
# $OpenBSD: mktestdata.sh,v 1.11 2020/06/19 03:48:49 djm Exp $

PW=mekmitasdigoat

rsa_params() {
	_in="$1"
	_outbase="$2"
	set -e
	openssl rsa -noout -text -in $_in | \
	    awk '/^modulus:$/,/^publicExponent:/' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.n
	openssl rsa -noout -text -in $_in | \
	    awk '/^prime1:$/,/^prime2:/' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.p
	openssl rsa -noout -text -in $_in | \
	    awk '/^prime2:$/,/^exponent1:/' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.q
	for x in n p q ; do
		echo "" >> ${_outbase}.$x
		echo ============ ${_outbase}.$x
		cat ${_outbase}.$x
		echo ============
	done
}

dsa_params() {
	_in="$1"
	_outbase="$2"
	set -e
	openssl dsa -noout -text -in $_in | \
	    awk '/^priv:$/,/^pub:/' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.priv
	openssl dsa -noout -text -in $_in | \
	    awk '/^pub:/,/^P:/' | #\
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.pub
	openssl dsa -noout -text -in $_in | \
	    awk '/^G:/,0' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.g
	for x in priv pub g ; do
		echo "" >> ${_outbase}.$x
		echo ============ ${_outbase}.$x
		cat ${_outbase}.$x
		echo ============
	done
}

ecdsa_params() {
	_in="$1"
	_outbase="$2"
	set -e
	openssl ec -noout -text -in $_in | \
	    awk '/^priv:$/,/^pub:/' | \
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.priv
	openssl ec -noout -text -in $_in | \
	    awk '/^pub:/,/^ASN1 OID:/' | #\
	    grep -v '^[a-zA-Z]' | tr -d ' \n:' > ${_outbase}.pub
	openssl ec -noout -text -in $_in | \
	    grep "ASN1 OID:" | \
	    sed 's/.*: //;s/ *$//' | tr -d '\n' > ${_outbase}.curve
	for x in priv pub curve ; do
		echo "" >> ${_outbase}.$x
		echo ============ ${_outbase}.$x
		cat ${_outbase}.$x
		echo ============
	done
}

set -ex

cd testdata

if [ -f ../../../misc/sk-dummy/sk-dummy.so ] ; then
	SK_DUMMY=../../../misc/sk-dummy/sk-dummy.so
elif [ -f ../../../misc/sk-dummy/obj/sk-dummy.so ] ; then
	SK_DUMMY=../../../misc/sk-dummy/obj/sk-dummy.so
else
	echo "Can't find sk-dummy.so" 1>&2
	exit 1
fi

rm -f rsa_1 dsa_1 ecdsa_1 ed25519_1
rm -f rsa_2 dsa_2 ecdsa_2 ed25519_2
rm -f rsa_n dsa_n ecdsa_n # new-format keys
rm -f rsa_1_pw dsa_1_pw ecdsa_1_pw ed25519_1_pw
rm -f rsa_n_pw dsa_n_pw ecdsa_n_pw
rm -f pw *.pub *.bn.* *.param.* *.fp *.fp.bb

ssh-keygen -t rsa -b 1024 -C "RSA test key #1" -N "" -f rsa_1 -m PEM
ssh-keygen -t dsa -b 1024 -C "DSA test key #1" -N "" -f dsa_1 -m PEM
ssh-keygen -t ecdsa -b 256 -C "ECDSA test key #1" -N "" -f ecdsa_1 -m PEM
ssh-keygen -t ed25519 -C "ED25519 test key #1" -N "" -f ed25519_1
ssh-keygen -w "$SK_DUMMY" -t ecdsa-sk -C "ECDSA-SK test key #1" \
    -N "" -f ecdsa_sk1
ssh-keygen -w "$SK_DUMMY" -t ed25519-sk -C "ED25519-SK test key #1" \
    -N "" -f ed25519_sk1


ssh-keygen -t rsa -b 2048 -C "RSA test key #2" -N "" -f rsa_2 -m PEM
ssh-keygen -t dsa -b 1024 -C "DSA test key #2" -N "" -f dsa_2 -m PEM
ssh-keygen -t ecdsa -b 521 -C "ECDSA test key #2" -N "" -f ecdsa_2 -m PEM
ssh-keygen -t ed25519 -C "ED25519 test key #2" -N "" -f ed25519_2
ssh-keygen -w "$SK_DUMMY" -t ecdsa-sk -C "ECDSA-SK test key #2" \
    -N "" -f ecdsa_sk2
ssh-keygen -w "$SK_DUMMY" -t ed25519-sk -C "ED25519-SK test key #2" \
    -N "" -f ed25519_sk2

cp rsa_1 rsa_n
cp dsa_1 dsa_n
cp ecdsa_1 ecdsa_n

ssh-keygen -pf rsa_n -N ""
ssh-keygen -pf dsa_n -N ""
ssh-keygen -pf ecdsa_n -N ""

cp rsa_1 rsa_1_pw
cp dsa_1 dsa_1_pw
cp ecdsa_1 ecdsa_1_pw
cp ed25519_1 ed25519_1_pw
cp ecdsa_sk1 ecdsa_sk1_pw
cp ed25519_sk1 ed25519_sk1_pw
cp rsa_1 rsa_n_pw
cp dsa_1 dsa_n_pw
cp ecdsa_1 ecdsa_n_pw

ssh-keygen -pf rsa_1_pw -m PEM -N "$PW"
ssh-keygen -pf dsa_1_pw -m PEM -N "$PW"
ssh-keygen -pf ecdsa_1_pw -m PEM -N "$PW"
ssh-keygen -pf ed25519_1_pw -N "$PW"
ssh-keygen -pf ecdsa_sk1_pw -m PEM -N "$PW"
ssh-keygen -pf ed25519_sk1_pw -N "$PW"
ssh-keygen -pf rsa_n_pw -N "$PW"
ssh-keygen -pf dsa_n_pw -N "$PW"
ssh-keygen -pf ecdsa_n_pw -N "$PW"

rsa_params rsa_1 rsa_1.param
rsa_params rsa_2 rsa_2.param
dsa_params dsa_1 dsa_1.param
dsa_params dsa_1 dsa_1.param
ecdsa_params ecdsa_1 ecdsa_1.param
ecdsa_params ecdsa_2 ecdsa_2.param
# XXX ed25519, *sk params

ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 1 rsa_1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 2 dsa_1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 3 ecdsa_1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 4 ed25519_1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 4 ecdsa_sk1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 4 ed25519_sk1.pub


# Make a few RSA variant signature too.
cp rsa_1 rsa_1_sha1
cp rsa_1 rsa_1_sha512
cp rsa_1.pub rsa_1_sha1.pub
cp rsa_1.pub rsa_1_sha512.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 -t ssh-rsa \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 1 rsa_1_sha1.pub
ssh-keygen -s rsa_2 -I hugo -n user1,user2 -t rsa-sha2-512 \
    -Oforce-command=/bin/ls -Ono-port-forwarding -Osource-address=10.0.0.0/8 \
    -V 19990101:20110101 -z 1 rsa_1_sha512.pub

ssh-keygen -s ed25519_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 5 rsa_1.pub
ssh-keygen -s ed25519_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 6 dsa_1.pub
ssh-keygen -s ecdsa_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 7 ecdsa_1.pub
ssh-keygen -s ed25519_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 8 ed25519_1.pub
ssh-keygen -s ecdsa_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 7 ecdsa_sk1.pub
ssh-keygen -s ed25519_1 -I julius -n host1,host2 -h \
    -V 19990101:20110101 -z 8 ed25519_sk1.pub

ssh-keygen -lf rsa_1 | awk '{print $2}' > rsa_1.fp
ssh-keygen -lf dsa_1 | awk '{print $2}' > dsa_1.fp
ssh-keygen -lf ecdsa_1 | awk '{print $2}' > ecdsa_1.fp
ssh-keygen -lf ed25519_1 | awk '{print $2}' > ed25519_1.fp
ssh-keygen -lf ecdsa_sk1 | awk '{print $2}' > ecdsa_sk1.fp
ssh-keygen -lf ed25519_sk1 | awk '{print $2}' > ed25519_sk1.fp
ssh-keygen -lf rsa_2 | awk '{print $2}' > rsa_2.fp
ssh-keygen -lf dsa_2 | awk '{print $2}' > dsa_2.fp
ssh-keygen -lf ecdsa_2 | awk '{print $2}' > ecdsa_2.fp
ssh-keygen -lf ed25519_2 | awk '{print $2}' > ed25519_2.fp
ssh-keygen -lf ecdsa_sk2 | awk '{print $2}' > ecdsa_sk2.fp
ssh-keygen -lf ed25519_sk2 | awk '{print $2}' > ed25519_sk2.fp

ssh-keygen -lf rsa_1-cert.pub  | awk '{print $2}' > rsa_1-cert.fp
ssh-keygen -lf dsa_1-cert.pub  | awk '{print $2}' > dsa_1-cert.fp
ssh-keygen -lf ecdsa_1-cert.pub  | awk '{print $2}' > ecdsa_1-cert.fp
ssh-keygen -lf ed25519_1-cert.pub  | awk '{print $2}' > ed25519_1-cert.fp
ssh-keygen -lf ecdsa_sk1-cert.pub  | awk '{print $2}' > ecdsa_sk1-cert.fp
ssh-keygen -lf ed25519_sk1-cert.pub  | awk '{print $2}' > ed25519_sk1-cert.fp

ssh-keygen -Bf rsa_1 | awk '{print $2}' > rsa_1.fp.bb
ssh-keygen -Bf dsa_1 | awk '{print $2}' > dsa_1.fp.bb
ssh-keygen -Bf ecdsa_1 | awk '{print $2}' > ecdsa_1.fp.bb
ssh-keygen -Bf ed25519_1 | awk '{print $2}' > ed25519_1.fp.bb
ssh-keygen -Bf ecdsa_sk1 | awk '{print $2}' > ecdsa_sk1.fp.bb
ssh-keygen -Bf ed25519_sk1 | awk '{print $2}' > ed25519_sk1.fp.bb
ssh-keygen -Bf rsa_2 | awk '{print $2}' > rsa_2.fp.bb
ssh-keygen -Bf dsa_2 | awk '{print $2}' > dsa_2.fp.bb
ssh-keygen -Bf ecdsa_2 | awk '{print $2}' > ecdsa_2.fp.bb
ssh-keygen -Bf ed25519_2 | awk '{print $2}' > ed25519_2.fp.bb
ssh-keygen -Bf ecdsa_sk2 | awk '{print $2}' > ecdsa_sk2.fp.bb
ssh-keygen -Bf ed25519_sk2 | awk '{print $2}' > ed25519_sk2.fp.bb

echo "$PW" > pw

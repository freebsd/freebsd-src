#!/bin/sh
# $OpenBSD: mktestdata.sh,v 1.1 2020/06/19 04:32:09 djm Exp $

NAMESPACE=unittest

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

rm -f signed-data namespace
rm -f rsa dsa ecdsa ed25519 ecdsa_sk ed25519_sk
rm -f rsa.sig dsa.sig ecdsa.sig ed25519.sig ecdsa_sk.sig ed25519_sk.sig

printf "This is a test, this is only a test" > signed-data
printf "$NAMESPACE" > namespace

ssh-keygen -t rsa -C "RSA test" -N "" -f rsa -m PEM
ssh-keygen -t dsa -C "DSA test" -N "" -f dsa -m PEM
ssh-keygen -t ecdsa -C "ECDSA test" -N "" -f ecdsa -m PEM
ssh-keygen -t ed25519 -C "ED25519 test key" -N "" -f ed25519
ssh-keygen -w "$SK_DUMMY" -t ecdsa-sk -C "ECDSA-SK test key" \
    -N "" -f ecdsa_sk
ssh-keygen -w "$SK_DUMMY" -t ed25519-sk -C "ED25519-SK test key" \
    -N "" -f ed25519_sk

ssh-keygen -Y sign -f rsa -n $NAMESPACE - < signed-data > rsa.sig
ssh-keygen -Y sign -f dsa -n $NAMESPACE - < signed-data > dsa.sig
ssh-keygen -Y sign -f ecdsa -n $NAMESPACE - < signed-data > ecdsa.sig
ssh-keygen -Y sign -f ed25519 -n $NAMESPACE - < signed-data > ed25519.sig
ssh-keygen -w "$SK_DUMMY" \
	-Y sign -f ecdsa_sk -n $NAMESPACE - < signed-data > ecdsa_sk.sig
ssh-keygen -w "$SK_DUMMY" \
	-Y sign -f ed25519_sk -n $NAMESPACE - < signed-data > ed25519_sk.sig

#!/bin/sh
#	$OpenBSD: ssh2putty.sh,v 1.9 2021/07/25 12:13:03 dtucker Exp $

if test "x$1" = "x" -o "x$2" = "x" -o "x$3" = "x" ; then
	echo "Usage: ssh2putty hostname port ssh-private-key"
	exit 1
fi

HOST=$1
PORT=$2
KEYFILE=$3

OPENSSL_BIN="${OPENSSL_BIN:-openssl}"

# XXX - support DSA keys too
if grep "BEGIN RSA PRIVATE KEY" $KEYFILE >/dev/null 2>&1 ; then
	:
else
	echo "Unsupported private key format"
	exit 1
fi

public_exponent=`
	$OPENSSL_BIN rsa -noout -text -in $KEYFILE | grep ^publicExponent |
	sed 's/.*(//;s/).*//'
`
test $? -ne 0 && exit 1

modulus=`
	$OPENSSL_BIN rsa -noout -modulus -in $KEYFILE | grep ^Modulus= |
	sed 's/^Modulus=/0x/' | tr A-Z a-z
`
test $? -ne 0 && exit 1

echo "rsa2@$PORT:$HOST $public_exponent,$modulus"


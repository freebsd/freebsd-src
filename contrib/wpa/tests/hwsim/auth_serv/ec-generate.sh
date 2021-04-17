#!/bin/sh

OPENSSL=openssl

CURVE=prime256v1
DIGEST="-sha256"
DIGEST_CA="-md sha256"

echo
echo "---[ Root CA ]----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = Suite B 128-bit Root CA/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec-ca.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -x509 -new -key ec-ca.key -out ec-ca.pem -outform PEM -days 3650 $DIGEST
mkdir -p ec-ca/certs ec-ca/crl ec-ca/newcerts ec-ca/private
touch ec-ca/index.txt
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Server ]-----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:server.w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec-server.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -nodes -key ec-server.key -out ec-server.req -outform PEM $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile ec-ca.key -cert ec-ca.pem -create_serial -in ec-server.req -out ec-server.pem -extensions ext_server $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ User ]-------------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user@w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec-user.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -nodes -key ec-user.key -out ec-user.req -outform PEM -extensions ext_client $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile ec-ca.key -cert ec-ca.pem -create_serial -in ec-user.req -out ec-user.pem -extensions ext_client $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Verify ]-----------------------------------------------------------"
echo

$OPENSSL verify -CAfile ec-ca.pem ec-server.pem
$OPENSSL verify -CAfile ec-ca.pem ec-user.pem

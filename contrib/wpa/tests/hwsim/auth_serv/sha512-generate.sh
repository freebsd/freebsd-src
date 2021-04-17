#!/bin/sh

OPENSSL=openssl

DIGEST="-sha512"
DIGEST_CA="-md sha512"

echo
echo "---[ Root CA ]----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = SHA384 and SHA512 Root CA/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -x509 -new -newkey rsa:4096 -nodes -keyout sha512-ca.key -out sha512-ca.pem -outform PEM -days 3650 $DIGEST
mkdir -p ec-ca/certs ec-ca/crl ec-ca/newcerts ec-ca/private
touch ec-ca/index.txt
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Server SHA-512 ]---------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = sha512.server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:sha512.server.w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -newkey rsa:3500 -nodes -keyout sha512-server.key -out sha512-server.req -outform PEM $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile sha512-ca.key -cert sha512-ca.pem -create_serial -in sha512-server.req -out sha512-server.pem -extensions ext_server $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Server SHA-384 ]---------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = sha384.server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:sha384.server.w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -newkey rsa:3072 -nodes -keyout sha384-server.key -out sha384-server.req -outform PEM $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile sha512-ca.key -cert sha512-ca.pem -create_serial -in sha384-server.req -out sha384-server.pem -extensions ext_server -md sha384
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ User SHA-512 ]-----------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user-sha512/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user-sha512@w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -newkey rsa:3400 -nodes -keyout sha512-user.key -out sha512-user.req -outform PEM -extensions ext_client $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile sha512-ca.key -cert sha512-ca.pem -create_serial -in sha512-user.req -out sha512-user.pem -extensions ext_client $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ User SHA-384 ]-----------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user-sha384/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user-sha384@w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -newkey rsa:2900 -nodes -keyout sha384-user.key -out sha384-user.req -outform PEM -extensions ext_client $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile sha512-ca.key -cert sha512-ca.pem -create_serial -in sha384-user.req -out sha384-user.pem -extensions ext_client -md sha384
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Verify ]-----------------------------------------------------------"
echo

$OPENSSL verify -CAfile sha512-ca.pem sha512-server.pem
$OPENSSL verify -CAfile sha512-ca.pem sha384-server.pem
$OPENSSL verify -CAfile sha512-ca.pem sha512-user.pem
$OPENSSL verify -CAfile sha512-ca.pem sha384-user.pem

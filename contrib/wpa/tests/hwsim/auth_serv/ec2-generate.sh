#!/bin/sh

OPENSSL=openssl

CURVE=secp384r1
DIGEST="-sha384"
DIGEST_CA="-md sha384"

echo
echo "---[ Root CA ]----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = Suite B 192-bit Root CA/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec2-ca.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -x509 -new -key ec2-ca.key -out ec2-ca.pem -outform PEM -days 3650 $DIGEST
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
$OPENSSL ecparam -out ec2-server.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -nodes -key ec2-server.key -out ec2-server.req -outform PEM $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile ec2-ca.key -cert ec2-ca.pem -create_serial -in ec2-server.req -out ec2-server.pem -extensions ext_server $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ User ]-------------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user@w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec2-user.key -name $CURVE -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -nodes -key ec2-user.key -out ec2-user.req -outform PEM -extensions ext_client $DIGEST
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile ec2-ca.key -cert ec2-ca.pem -create_serial -in ec2-user.req -out ec2-user.pem -extensions ext_client $DIGEST_CA
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ User p256 ]--------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user-p256/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user-p256@w1.fi/" \
	> ec-ca-openssl.cnf.tmp
$OPENSSL ecparam -out ec2-user-p256.key -name prime256v1 -genkey
$OPENSSL req -config ec-ca-openssl.cnf.tmp -batch -new -nodes -key ec2-user-p256.key -out ec2-user-p256.req -outform PEM -extensions ext_client -sha256
$OPENSSL ca -config ec-ca-openssl.cnf.tmp -batch -keyfile ec2-ca.key -cert ec2-ca.pem -create_serial -in ec2-user-p256.req -out ec2-user-p256.pem -extensions ext_client -md sha256
rm ec-ca-openssl.cnf.tmp

echo
echo "---[ Verify ]-----------------------------------------------------------"
echo

$OPENSSL verify -CAfile ec2-ca.pem ec2-server.pem
$OPENSSL verify -CAfile ec2-ca.pem ec2-user.pem
$OPENSSL verify -CAfile ec2-ca.pem ec2-user-p256.pem

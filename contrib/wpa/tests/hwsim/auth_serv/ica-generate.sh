#!/bin/sh

OPENSSL=openssl

echo
echo "---[ Intermediate CA - Server ]-----------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/ec-ca/rootCA/" |
	sed "s/#@CN@/commonName_default = Server Intermediate CA/" \
	> openssl.cnf.tmp
mkdir -p iCA-server/certs iCA-server/crl iCA-server/newcerts iCA-server/private
touch iCA-server/index.txt
$OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout iCA-server/private/cakey.pem -out iCA-server/careq.pem -outform PEM -days 3652 -sha256
$OPENSSL ca -config openssl.cnf.tmp -md sha256 -create_serial -out iCA-server/cacert.pem -days 3652 -batch -keyfile ca-key.pem -cert ca.pem -extensions v3_ca -outdir rootCA/newcerts -infiles iCA-server/careq.pem
cat iCA-server/cacert.pem ca.pem  > iCA-server/ca-and-root.pem
rm openssl.cnf.tmp

echo
echo "---[ Intermediate CA - User ]-------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/ec-ca/rootCA/" |
	sed "s/#@CN@/commonName_default = User Intermediate CA/" \
	> openssl.cnf.tmp
mkdir -p iCA-user/certs iCA-user/crl iCA-user/newcerts iCA-user/private
touch iCA-user/index.txt
$OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout iCA-user/private/cakey.pem -out iCA-user/careq.pem -outform PEM -days 3652 -sha256
$OPENSSL ca -config openssl.cnf.tmp -md sha256 -create_serial -out iCA-user/cacert.pem -days 3652 -batch -keyfile ca-key.pem -cert ca.pem -extensions v3_ca -outdir rootCA/newcerts -infiles iCA-user/careq.pem
cat iCA-user/cacert.pem ca.pem  > iCA-user/ca-and-root.pem
rm openssl.cnf.tmp

echo
echo "---[ Server ]-----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/ec-ca/iCA-server/" |
	sed "s/#@CN@/commonName_default = server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:server.w1.fi/" \
	> openssl.cnf.tmp
$OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout iCA-server/server.key -out iCA-server/server.req -outform PEM -sha256
$OPENSSL ca -config openssl.cnf.tmp -batch -keyfile iCA-server/private/cakey.pem -cert iCA-server/cacert.pem -create_serial -in iCA-server/server.req -out iCA-server/server.pem -extensions ext_server -md sha256
cat iCA-server/cacert.pem iCA-server/server.pem > iCA-server/server_and_ica.pem
rm openssl.cnf.tmp

echo
echo "---[ Server - revoked ]-------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/ec-ca/iCA-server/" |
	sed "s/#@CN@/commonName_default = server-revoked.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:server-revoked.w1.fi/" \
	> openssl.cnf.tmp
$OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout iCA-server/server-revoked.key -out iCA-server/server-revoked.req -outform PEM -sha256
$OPENSSL ca -config openssl.cnf.tmp -batch -keyfile iCA-server/private/cakey.pem -cert iCA-server/cacert.pem -create_serial -in iCA-server/server-revoked.req -out iCA-server/server-revoked.pem -extensions ext_server -md sha256
$OPENSSL ca -config openssl.cnf.tmp -revoke iCA-server/server-revoked.pem -keyfile iCA-server/private/cakey.pem -cert iCA-server/cacert.pem
cat iCA-server/cacert.pem iCA-server/server-revoked.pem > iCA-server/server-revoked_and_ica.pem
rm openssl.cnf.tmp

echo
echo "---[ User ]-----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/ec-ca/iCA-user/" |
	sed "s/#@CN@/commonName_default = user.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:user.w1.fi/" \
	> openssl.cnf.tmp
$OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout iCA-user/user.key -out iCA-user/user.req -outform PEM -sha256
$OPENSSL ca -config openssl.cnf.tmp -batch -keyfile iCA-user/private/cakey.pem -cert iCA-user/cacert.pem -create_serial -in iCA-user/user.req -out iCA-user/user.pem -extensions ext_client -md sha256
cat iCA-user/user.pem iCA-user/cacert.pem > iCA-user/user_and_ica.pem
rm openssl.cnf.tmp

echo
echo "---[ Verify ]-----------------------------------------------------------"
echo

$OPENSSL verify -CAfile ca.pem iCA-server/cacert.pem
$OPENSSL verify -CAfile ca.pem iCA-user/cacert.pem
$OPENSSL verify -CAfile ca.pem -untrusted iCA-server/cacert.pem iCA-server/server.pem
$OPENSSL verify -CAfile ca.pem -untrusted iCA-server/cacert.pem iCA-server/server-revoked.pem
$OPENSSL verify -CAfile ca.pem iCA-user/cacert.pem
$OPENSSL verify -CAfile ca.pem -untrusted iCA-user/cacert.pem iCA-user/user.pem

#!/bin/sh

OPENSSL=openssl

echo
echo "---[ DH parameters ]----------------------------------------------------"
echo

if [ -r dh_param_3072.pem ]; then
    echo "Use already generated dh_param_3072.pem"
else
    openssl dhparam -out dh_param_3072.pem 3072
fi

echo
echo "---[ Root CA ]----------------------------------------------------------"
echo

if [ -r rsa3072-ca.key ]; then
    echo "Use already generated Root CA"
else
    cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = Suite B RSA 3k Root CA/" |
	sed s%\./ec-ca$%./rsa3072-ca% \
	    > rsa3072-ca-openssl.cnf.tmp
    $OPENSSL req -config rsa3072-ca-openssl.cnf.tmp -batch -x509 -new -newkey rsa:3072 -nodes -keyout rsa3072-ca.key -out rsa3072-ca.pem -outform PEM -days 3650 -sha384
    mkdir -p rsa3072-ca/certs rsa3072-ca/crl rsa3072-ca/newcerts rsa3072-ca/private
    touch rsa3072-ca/index.txt
    rm rsa3072-ca-openssl.cnf.tmp
fi

echo
echo "---[ Server ]-----------------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = rsa3072.server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=critical,DNS:rsa3072.server.w1.fi/" |
	sed s%\./ec-ca$%./rsa3072-ca% \
	> rsa3072-ca-openssl.cnf.tmp
if [ ! -r rsa3072-server.req ]; then
    $OPENSSL req -config rsa3072-ca-openssl.cnf.tmp -batch -new -newkey rsa:3072 -nodes -keyout rsa3072-server.key -out rsa3072-server.req -outform PEM -sha384
fi
$OPENSSL ca -config rsa3072-ca-openssl.cnf.tmp -batch -keyfile rsa3072-ca.key -cert rsa3072-ca.pem -create_serial -in rsa3072-server.req -out rsa3072-server.pem -extensions ext_server -days 730 -md sha384
rm rsa3072-ca-openssl.cnf.tmp

echo
echo "---[ User SHA-384 ]-----------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user-rsa3072/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user-rsa3072@w1.fi/" |
	sed s%\./ec-ca$%./rsa3072-ca% \
	> rsa3072-ca-openssl.cnf.tmp
if [ ! -r rsa3072-user.req ]; then
    $OPENSSL req -config rsa3072-ca-openssl.cnf.tmp -batch -new -newkey rsa:3072 -nodes -keyout rsa3072-user.key -out rsa3072-user.req -outform PEM -extensions ext_client -sha384
fi
$OPENSSL ca -config rsa3072-ca-openssl.cnf.tmp -batch -keyfile rsa3072-ca.key -cert rsa3072-ca.pem -create_serial -in rsa3072-user.req -out rsa3072-user.pem -extensions ext_client -days 730 -md sha384
rm rsa3072-ca-openssl.cnf.tmp

echo
echo "---[ User RSA2048 ]-----------------------------------------------------"
echo

cat ec-ca-openssl.cnf |
	sed "s/#@CN@/commonName_default = user-rsa3072-rsa2048/" |
	sed "s/#@ALTNAME@/subjectAltName=email:user-rsa3072-rsa2048@w1.fi/" |
	sed s%\./ec-ca$%./rsa3072-ca% \
	> rsa3072-ca-openssl.cnf.tmp
if [ ! -r rsa3072-user-rsa2048.req ]; then
    $OPENSSL req -config rsa3072-ca-openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout rsa3072-user-rsa2048.key -out rsa3072-user-rsa2048.req -outform PEM -extensions ext_client -sha384
fi
$OPENSSL ca -config rsa3072-ca-openssl.cnf.tmp -batch -keyfile rsa3072-ca.key -cert rsa3072-ca.pem -create_serial -in rsa3072-user-rsa2048.req -out rsa3072-user-rsa2048.pem -extensions ext_client -days 730 -md sha384
rm rsa3072-ca-openssl.cnf.tmp

echo
echo "---[ Verify ]-----------------------------------------------------------"
echo

$OPENSSL verify -CAfile rsa3072-ca.pem rsa3072-server.pem
$OPENSSL verify -CAfile rsa3072-ca.pem rsa3072-user.pem
$OPENSSL verify -CAfile rsa3072-ca.pem rsa3072-user-rsa2048.pem

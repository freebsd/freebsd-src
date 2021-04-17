#!/bin/sh

OPENSSL=openssl

mkdir -p test-ca/newcerts

echo
echo "---[ DH parameters ]----------------------------------------------------"
echo

if [ -r dh.conf ]; then
    echo "Use already generated dh.conf"
else
    openssl dhparam -out dh.conf 2048
fi

echo
echo "---[ Root CA ]----------------------------------------------------------"
echo

if [ -r ca-key.pem ]; then
    echo "Use already generated Root CA"
else
    cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = TEST - Incorrect Root CA/" \
	    > ca-openssl.cnf.tmp
    $OPENSSL req -config ca-openssl.cnf.tmp -batch -x509 -new -newkey rsa:2048 -nodes -keyout ca-incorrect-key.pem -out ca-incorrect.der -outform DER -days 3650 -sha256
    $OPENSSL x509 -in ca-incorrect.der -inform DER -out ca-incorrect.pem -outform PEM -text

    cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = Root CA/" \
	    > ca-openssl.cnf.tmp
    $OPENSSL req -config ca-openssl.cnf.tmp -batch -x509 -new -newkey rsa:2048 -nodes -keyout ca-key.pem -out ca.der -outform DER -days 3650 -sha256
    $OPENSSL x509 -in ca.der -inform DER -out ca.pem -outform PEM -text
    mkdir -p test-ca/certs test-ca/crl test-ca/newcerts test-ca/private
    touch test-ca/index.txt
    echo 01 > test-ca/crlnumber
    cp ca.pem test-ca/cacert.pem
    cp ca-key.pem test-ca/private/cakey.pem
    $OPENSSL ca -config ca-openssl.cnf.tmp -gencrl -crldays 2922 -out crl.pem
    cat ca.pem crl.pem > ca-and-crl.pem
    faketime yesterday $OPENSSL ca -config ca-openssl.cnf.tmp -gencrl -crlhours 1 -out crl.pem
    cat ca.pem crl.pem > ca-and-crl-expired.pem
    rm crl.pem
    rm ca-openssl.cnf.tmp
fi

echo
echo "---[ Update server certificates ]---------------------------------------"
echo

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=DNS:server.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout server.key -out server.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server.csr -out server.pem -extensions ext_server

$OPENSSL pkcs12 -export -out server.pkcs12 -in server.pem -inkey server.key -passout pass:
$OPENSSL pkcs12 -export -out server-extra.pkcs12 -in server.pem -inkey server.key -descert -certfile user.pem -passout pass:whatever -name server

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server3.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server-no-dnsname.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout server-no-dnsname.key -out server-no-dnsname.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-no-dnsname.csr -out server-no-dnsname.pem -extensions ext_server

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server4.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server-expired.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout server-expired.key -out server-expired.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-expired.csr -out server-expired.pem -extensions ext_server -startdate 200101000000Z -enddate 200102000000Z

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server5.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server-eku-client.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout server-eku-client.key -out server-eku-client.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-eku-client.csr -out server-eku-client.pem -extensions ext_client

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server6.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server-eku-client-server.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout server-eku-client-server.key -out server-eku-client-server.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-eku-client-server.csr -out server-eku-client-server.pem -extensions ext_client_server

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server7.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r server-long-duration.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:4096 -nodes -keyout server-long-duration.key -out server-long-duration.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-long-duration.csr -out server-long-duration.pem -extensions ext_server -days 18250

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server-policies.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=DNS:server-policies.w1.fi/" |
	sed "s/#@CERTPOL@/certificatePolicies = 1.3.6.1.4.1.40808.1.3.1/" \
	> openssl.cnf.tmp
if [ ! -r server-certpol.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:3072 -nodes -keyout server-certpol.key -out server-certpol.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-certpol.csr -out server-certpol.pem -extensions ext_server

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = server-policies2.w1.fi/" |
	sed "s/#@ALTNAME@/subjectAltName=DNS:server-policies2.w1.fi/" |
	sed "s/#@CERTPOL@/certificatePolicies = 1.3.6.1.4.1.40808.1.3.2/" \
	> openssl.cnf.tmp
if [ ! -r server-certpol2.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:3072 -nodes -keyout server-certpol2.key -out server-certpol2.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in server-certpol2.csr -out server-certpol2.pem -extensions ext_server

echo
echo "---[ Update user certificates ]-----------------------------------------"
echo

cat openssl2.cnf | sed "s/#@CN@/commonName_default = Test User/" > openssl.cnf.tmp
if [ ! -r user.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout user.key -out user.csr -outform PEM -sha256
    $OPENSSL rsa -in user.key -out user.rsa-key
    $OPENSSL pkcs8 -topk8 -in user.key -out user.key.pkcs8 -inform PEM -v2 des-ede3-cbc -v2prf hmacWithSHA1 -passout pass:whatever
    $OPENSSL pkcs8 -topk8 -in user.key -out user.key.pkcs8.pkcs5v15 -inform PEM -v1 pbeWithMD5AndDES-CBC -passout pass:whatever
fi

$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in user.csr -out user.pem -extensions ext_client
rm openssl.cnf.tmp

$OPENSSL pkcs12 -export -out user.pkcs12 -in user.pem -inkey user.key -descert -passout pass:whatever
$OPENSSL pkcs12 -export -out user2.pkcs12 -in user.pem -inkey user.key -descert -name Test -certfile server.pem -passout pass:whatever
$OPENSSL pkcs12 -export -out user3.pkcs12 -in user.pem -inkey user.key -descert -name "my certificates" -certfile ca.pem -passout pass:whatever

echo
echo "---[ Update OCSP ]------------------------------------------------------"
echo

cat openssl2.cnf |
	sed "s/#@CN@/commonName_default = ocsp.w1.fi/" \
	> openssl.cnf.tmp
if [ ! -r ocsp-responder.csr ]; then
    $OPENSSL req -config openssl.cnf.tmp -batch -new -newkey rsa:2048 -nodes -keyout ocsp-responder.key -out ocsp-responder.csr -outform PEM -sha256
fi
$OPENSSL ca -config $PWD/openssl.cnf.tmp -batch -in ocsp-responder.csr -out ocsp-responder.pem -extensions v3_OCSP

$OPENSSL ocsp -CAfile test-ca/cacert.pem -issuer test-ca/cacert.pem -cert server.pem -reqout ocsp-req.der -no_nonce
$OPENSSL ocsp -index test-ca/index.txt -rsigner test-ca/cacert.pem -rkey test-ca/private/cakey.pem -CA test-ca/cacert.pem -resp_no_certs -reqin ocsp-req.der -respout ocsp-server-cache.der
SIZ=`ls -l ocsp-server-cache.der | cut -f5 -d' '`
(echo -n 000; echo "obase=16;$SIZ" | bc) | xxd -r -ps > ocsp-multi-server-cache.der
cat ocsp-server-cache.der >> ocsp-multi-server-cache.der

echo
echo "---[ Additional steps ]-------------------------------------------------"
echo

echo "test_ap_eap.py: ap_wpa2_eap_ttls_server_cert_hash srv_cert_hash"

$OPENSSL x509 -in server.pem -out server.der -outform DER
HASH=`sha256sum server.der | cut -f1 -d' '`
rm server.der
sed -i "s/srv_cert_hash =.*/srv_cert_hash = \"$HASH\"/" ../test_ap_eap.py

echo "index.txt: server time+serial"

grep -v CN=server.w1.fi index.txt > index.txt.new
grep CN=server.w1.fi test-ca/index.txt | tail -1 >> index.txt.new
mv index.txt.new index.txt

echo "start.sh: openssl ocsp -reqout serial"

SERIAL=`grep CN=server.w1.fi test-ca/index.txt | tail -1 | cut -f4`
sed -i "s/'-serial', '0x[^']*'/'-serial', '0x$SERIAL'/" ../test_ap_eap.py

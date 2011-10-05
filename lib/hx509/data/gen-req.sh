#!/bin/sh
# $Id$
#
# This script need openssl 0.9.8a or newer, so it can parse the
# otherName section for pkinit certificates.
#

openssl=openssl

gen_cert()
{
	keytype=${6:-rsa:1024}
	${openssl} req \
		-new \
		-subj "$1" \
		-config openssl.cnf \
		-newkey $keytype \
		-sha1 \
		-nodes \
		-keyout out.key \
		-out cert.req > /dev/null 2>/dev/null

        if [ "$3" = "ca" ] ; then
	    ${openssl} x509 \
		-req \
		-days 3650 \
		-in cert.req \
		-extfile openssl.cnf \
		-extensions $4 \
                -signkey out.key \
		-out cert.crt

		ln -s ca.crt `${openssl} x509 -hash -noout -in cert.crt`.0

		name=$3

        elif [ "$3" = "proxy" ] ; then

	    ${openssl} x509 \
		-req \
		-in cert.req \
		-days 3650 \
		-out cert.crt \
		-CA $2.crt \
		-CAkey $2.key \
		-CAcreateserial \
		-extfile openssl.cnf \
		-extensions $4

		name=$5
	else

	    ${openssl} ca \
		-name $4 \
		-days 3650 \
		-cert $2.crt \
		-keyfile $2.key \
		-in cert.req \
		-out cert.crt \
		-outdir . \
		-batch \
		-config openssl.cnf 

		name=$3
	fi

	mv cert.crt $name.crt
	mv out.key $name.key
}

echo "01" > serial
> index.txt
rm -f *.0

gen_cert "/CN=hx509 Test Root CA/C=SE" "root" "ca" "v3_ca"
gen_cert "/CN=OCSP responder/C=SE" "ca" "ocsp-responder" "ocsp"
gen_cert "/CN=Test cert/C=SE" "ca" "test" "usr"
gen_cert "/CN=Revoke cert/C=SE" "ca" "revoke" "usr"
gen_cert "/CN=Test cert KeyEncipherment/C=SE" "ca" "test-ke-only" "usr_ke"
gen_cert "/CN=Test cert DigitalSignature/C=SE" "ca" "test-ds-only" "usr_ds"
gen_cert "/CN=pkinit/C=SE" "ca" "pkinit" "pkinit_client"
$openssl ecparam -name secp256r1 -out eccurve.pem
gen_cert "/CN=pkinit-ec/C=SE" "ca" "pkinit-ec" "pkinit_client" "XXX" ec:eccurve.pem
gen_cert "/C=SE/CN=pkinit/CN=pkinit-proxy" "pkinit" "proxy" "proxy_cert" pkinit-proxy
gen_cert "/CN=kdc/C=SE" "ca" "kdc" "pkinit_kdc"
gen_cert "/CN=www.test.h5l.se/C=SE" "ca" "https" "https"
gen_cert "/CN=Sub CA/C=SE" "ca" "sub-ca" "subca"
gen_cert "/CN=Test sub cert/C=SE" "sub-ca" "sub-cert" "usr"
gen_cert "/C=SE/CN=Test cert/CN=proxy" "test" "proxy" "proxy_cert" proxy-test
gen_cert "/C=SE/CN=Test cert/CN=proxy/CN=child" "proxy-test" "proxy" "proxy_cert" proxy-level-test
gen_cert "/C=SE/CN=Test cert/CN=no-proxy" "test" "proxy" "usr_cert" no-proxy-test
gen_cert "/C=SE/CN=Test cert/CN=proxy10" "test" "proxy" "proxy10_cert" proxy10-test
gen_cert "/C=SE/CN=Test cert/CN=proxy10/CN=child" "proxy10-test" "proxy" "proxy10_cert" proxy10-child-test
gen_cert "/C=SE/CN=Test cert/CN=proxy10/CN=child/CN=child" "proxy10-child-test" "proxy" "proxy10_cert" proxy10-child-child-test


# combine
cat sub-ca.crt ca.crt > sub-ca-combined.crt
cat test.crt test.key > test.combined.crt
cat pkinit-proxy.crt pkinit.crt > pkinit-proxy-chain.crt

# password protected key
${openssl} rsa -in test.key -aes256 -passout pass:foobar -out test-pw.key
${openssl} rsa -in pkinit.key -aes256 -passout pass:foo -out pkinit-pw.key


${openssl} ca \
    -name usr \
    -cert ca.crt \
    -keyfile ca.key \
    -revoke revoke.crt \
    -config openssl.cnf 

${openssl} pkcs12 \
    -export \
    -in test.crt \
    -inkey test.key \
    -passout pass:foobar \
    -out test.p12 \
    -name "friendlyname-test" \
    -certfile ca.crt \
    -caname ca

${openssl} pkcs12 \
    -export \
    -in sub-cert.crt \
    -inkey sub-cert.key \
    -passout pass:foobar \
    -out sub-cert.p12 \
    -name "friendlyname-sub-cert" \
    -certfile sub-ca-combined.crt \
    -caname sub-ca \
    -caname ca

${openssl} pkcs12 \
    -keypbe NONE \
    -certpbe NONE \
    -export \
    -in test.crt \
    -inkey test.key \
    -passout pass:foobar \
    -out test-nopw.p12 \
    -name "friendlyname-cert" \
    -certfile ca.crt \
    -caname ca

${openssl} smime \
    -sign \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -outform DER \
    -out test-signed-data

${openssl} smime \
    -sign \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -noattr \
    -outform DER \
    -out test-signed-data-noattr

${openssl} smime \
    -sign \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -noattr \
    -nocerts \
    -outform DER \
    -out test-signed-data-noattr-nocerts

${openssl} smime \
    -sign \
    -md sha1 \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -outform DER \
    -out test-signed-sha-1

${openssl} smime \
    -sign \
    -md sha256 \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -outform DER \
    -out test-signed-sha-256

${openssl} smime \
    -sign \
    -md sha512 \
    -nodetach \
    -binary \
    -in static-file \
    -signer test.crt \
    -inkey test.key \
    -outform DER \
    -out test-signed-sha-512


${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-rc2-40 \
    -rc2-40 \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-rc2-64 \
    -rc2-64 \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-rc2-128 \
    -rc2-128 \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-des \
    -des \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-des-ede3 \
    -des3 \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-aes-128 \
    -aes128 \
    test.crt

${openssl} smime \
    -encrypt \
    -nodetach \
    -binary \
    -in static-file \
    -outform DER \
    -out test-enveloped-aes-256 \
    -aes256 \
    test.crt

echo ocsp requests

${openssl} ocsp \
    -issuer ca.crt \
    -cert test.crt \
    -reqout ocsp-req1.der

${openssl} ocsp \
    -index index.txt \
    -rsigner ocsp-responder.crt \
    -rkey ocsp-responder.key \
    -CA ca.crt \
    -reqin ocsp-req1.der \
    -noverify \
    -respout ocsp-resp1-ocsp.der

${openssl} ocsp \
    -index index.txt \
    -rsigner ca.crt \
    -rkey ca.key \
    -CA ca.crt \
    -reqin ocsp-req1.der \
    -noverify \
    -respout ocsp-resp1-ca.der

${openssl} ocsp \
    -index index.txt \
    -rsigner ocsp-responder.crt \
    -rkey ocsp-responder.key \
    -CA ca.crt \
    -resp_no_certs \
    -reqin ocsp-req1.der \
    -noverify \
    -respout ocsp-resp1-ocsp-no-cert.der

${openssl} ocsp \
    -index index.txt \
    -rsigner ocsp-responder.crt \
    -rkey ocsp-responder.key \
    -CA ca.crt \
    -reqin ocsp-req1.der \
    -resp_key_id \
    -noverify \
    -respout ocsp-resp1-keyhash.der

${openssl} ocsp \
    -issuer ca.crt \
    -cert revoke.crt \
    -reqout ocsp-req2.der

${openssl} ocsp \
    -index index.txt \
    -rsigner ocsp-responder.crt \
    -rkey ocsp-responder.key \
    -CA ca.crt \
    -reqin ocsp-req2.der \
    -noverify \
    -respout ocsp-resp2.der

${openssl} ca \
    -gencrl \
    -name usr \
    -crldays 3600 \
    -keyfile ca.key \
    -cert ca.crt \
    -crl_reason superseded \
    -out crl1.crl \
    -config openssl.cnf 

${openssl} crl -in crl1.crl -outform der -out crl1.der

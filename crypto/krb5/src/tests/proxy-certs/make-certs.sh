#!/bin/sh -e

PWD=`pwd`
NAMETYPE=1
KEYSIZE=2048
DAYS=4000
REALM=KRBTEST.COM
TLS_SERVER_EKU=1.3.6.1.5.5.7.3.1
PROXY_EKU_LIST=$TLS_SERVER_EKU

cat > openssl.cnf << EOF
[req]
prompt = no
distinguished_name = \$ENV::SUBJECT

[ca]
default_ca = test_ca

[test_ca]
new_certs_dir = $PWD
serial = $PWD/ca.srl
database = $PWD/ca.db
certificate = $PWD/ca.pem
private_key = $PWD/privkey.pem
default_days = $DAYS
x509_extensions = exts_proxy
policy = proxyname
default_md = sha256
unique_subject = no
email_in_dn = no

[signer]
CN = test CA certificate
C = US
ST = Massachusetts
L = Cambridge
O = MIT
OU = Insecure Kerberos test CA
CN = test suite CA; do not use otherwise

[proxy]
C = US
ST = Massachusetts
O = KRBTEST.COM
CN = PROXYinSubject

[localhost]
C = US
ST = Massachusetts
O = KRBTEST.COM
CN = localhost

[proxyname]
C = supplied
ST = supplied
O = supplied
CN = supplied

[exts_ca]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,dataEncipherment,keyAgreement,keyCertSign,cRLSign
basicConstraints = critical,CA:TRUE

[exts_proxy]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = DNS:proxyŠubjectÄltÑame,DNS:proxySubjectAltName,IP:127.0.0.1,IP:::1,DNS:localhost
extendedKeyUsage = $PROXY_EKU_LIST

[exts_proxy_no_san]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
extendedKeyUsage = $PROXY_EKU_LIST
EOF

# Generate a private key.
openssl genrsa $KEYSIZE > privkey.pem

# Generate a "CA" certificate.
SUBJECT=signer openssl req -config openssl.cnf -new -x509 -extensions exts_ca \
    -set_serial 1 -days $DAYS -key privkey.pem -out ca.pem

# Generate proxy certificate signing requests.
SUBJECT=proxy openssl req -config openssl.cnf -new -key privkey.pem \
	-out proxy.csr
SUBJECT=localhost openssl req -config openssl.cnf -new -key privkey.pem \
	-out localhost.csr

# Issue the certificate with the right name in a subjectAltName.
echo 02 > ca.srl
cat /dev/null > ca.db
SUBJECT=proxy openssl ca  -config openssl.cnf -extensions exts_proxy \
    -batch -days $DAYS -notext -out tmp.pem -in proxy.csr
cat privkey.pem tmp.pem > proxy-san.pem

# Issue a certificate that only has the name in the subject field
SUBJECT=proxy openssl ca  -config openssl.cnf -extensions exts_proxy_no_san \
    -batch -days $DAYS -notext -out tmp.pem -in localhost.csr
cat privkey.pem tmp.pem > proxy-subject.pem

# Issue a certificate that doesn't include any matching name values.
SUBJECT=proxy openssl ca  -config openssl.cnf -extensions exts_proxy_no_san \
    -batch -days $DAYS -notext -out tmp.pem -in proxy.csr
cat privkey.pem tmp.pem > proxy-no-match.pem

# Issue a certificate that contains all matching name values.
SUBJECT=proxy openssl ca  -config openssl.cnf -extensions exts_proxy \
    -batch -days $DAYS -notext -out tmp.pem -in localhost.csr
cat privkey.pem tmp.pem > proxy-ideal.pem

# Corrupt the signature on the certificate.
SUBJECT=proxy openssl x509 -outform der -in proxy-ideal.pem -out bad.der
length=`od -Ad bad.der | tail -n 1 | awk '{print $1}'`
dd if=/dev/zero bs=1 of=bad.der count=16 seek=`expr $length - 16`
SUBJECT=proxy openssl x509 -inform der -in bad.der -out tmp.pem
cat privkey.pem tmp.pem > proxy-badsig.pem

# Clean up.
rm -f openssl.cnf proxy.csr localhost.csr privkey.pem ca.db ca.db.old ca.srl ca.srl.old ca.db.attr ca.db.attr.old 02.pem 03.pem 04.pem 05.pem tmp.pem bad.der

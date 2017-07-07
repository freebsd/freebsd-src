#!/bin/sh -e

NAMETYPE=1
KEYSIZE=2048
DAYS=4000
REALM=KRBTEST.COM
KRB5_PRINCIPAL_SAN=1.3.6.1.5.2.2
PKINIT_KDC_EKU=1.3.6.1.5.2.3.5
PKINIT_CLIENT_EKU=1.3.6.1.5.2.3.4
TLS_SERVER_EKU=1.3.6.1.5.5.7.3.1
TLS_CLIENT_EKU=1.3.6.1.5.5.7.3.2
EMAIL_PROTECTION_EKU=1.3.6.1.5.5.7.3.4
# Add TLS EKUs to these if we're testing with NSS and we still have to
# piggy-back on the TLS trust settings.
KDC_EKU_LIST=$PKINIT_KDC_EKU
CLIENT_EKU_LIST=$PKINIT_CLIENT_EKU

cat > openssl.cnf << EOF
[req]
prompt = no
distinguished_name = \$ENV::SUBJECT

[ca]
CN = test CA certificate
C = US
ST = Massachusetts
L = Cambridge
O = MIT
OU = Insecure PKINIT Kerberos test CA
CN = pkinit test suite CA; do not use otherwise

[kdc]
C = US
ST = Massachusetts
O = KRBTEST.COM
CN = KDC

[user]
C = US
ST = Massachusetts
O = KRBTEST.COM
CN = user

[exts_ca]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,dataEncipherment,keyAgreement,keyCertSign,cRLSign
basicConstraints = critical,CA:TRUE

[components_kdc]
0.component=GeneralString:krbtgt
1.component=GeneralString:$REALM

[princ_kdc]
nametype=EXPLICIT:0,INTEGER:$NAMETYPE
components=EXPLICIT:1,SEQUENCE:components_kdc

[krb5princ_kdc]
realm=EXPLICIT:0,GeneralString:$REALM
princ=EXPLICIT:1,SEQUENCE:princ_kdc

[exts_kdc]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = otherName:$KRB5_PRINCIPAL_SAN;SEQUENCE:krb5princ_kdc
extendedKeyUsage = $KDC_EKU_LIST

[components_client]
component=GeneralString:user

[princ_client]
nametype=EXPLICIT:0,INTEGER:$NAMETYPE
components=EXPLICIT:1,SEQUENCE:components_client

[krb5princ_client]
realm=EXPLICIT:0,GeneralString:$REALM
princ=EXPLICIT:1,SEQUENCE:princ_client

[exts_client]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = otherName:$KRB5_PRINCIPAL_SAN;SEQUENCE:krb5princ_client
extendedKeyUsage = $CLIENT_EKU_LIST
EOF

# Generate a private key.
openssl genrsa $KEYSIZE -nodes > privkey.pem
openssl rsa -in privkey.pem -out privkey-enc.pem -des3 -passout pass:encrypted

# Generate a "CA" certificate.
SUBJECT=ca openssl req -config openssl.cnf -new -x509 -extensions exts_ca \
    -set_serial 1 -days $DAYS -key privkey.pem -out ca.pem

# Generate a KDC certificate.
SUBJECT=kdc openssl req -config openssl.cnf -new -subj /CN=kdc \
    -key privkey.pem -out kdc.csr
SUBJECT=kdc openssl x509 -extfile openssl.cnf -extensions exts_kdc \
    -set_serial 2 -days $DAYS -req -CA ca.pem -CAkey privkey.pem \
    -out kdc.pem -in kdc.csr

# Generate a client certificate and PKCS#12 bundles.
SUBJECT=user openssl req -config openssl.cnf -new -subj /CN=user \
    -key privkey.pem -out user.csr
SUBJECT=user openssl x509 -extfile openssl.cnf -extensions exts_client \
    -set_serial 3 -days $DAYS -req -CA ca.pem -CAkey privkey.pem \
    -out user.pem -in user.csr
openssl pkcs12 -export -in user.pem -inkey privkey.pem -out user.p12 \
    -passout pass:
openssl pkcs12 -export -in user.pem -inkey privkey.pem -out user-enc.p12 \
    -passout pass:encrypted

# Clean up.
rm -f openssl.cnf kdc.csr user.csr

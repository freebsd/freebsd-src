#!/bin/sh -e

NAMETYPE=1
KRBTGT_NAMETYPE=2
KEYSIZE=2048
DAYS=4000
REALM=KRBTEST.COM
LOWREALM=krbtest.com
KRB5_PRINCIPAL_SAN=1.3.6.1.5.2.2
KRB5_UPN_SAN=1.3.6.1.4.1.311.20.2.3
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
nametype=EXPLICIT:0,INTEGER:$KRBTGT_NAMETYPE
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

[exts_upn_client]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = otherName:$KRB5_UPN_SAN;UTF8:user@$LOWREALM
extendedKeyUsage = $CLIENT_EKU_LIST

[exts_upn2_client]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = otherName:$KRB5_UPN_SAN;UTF8:user
extendedKeyUsage = $CLIENT_EKU_LIST

[exts_upn3_client]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage = nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
basicConstraints = critical,CA:FALSE
subjectAltName = otherName:$KRB5_UPN_SAN;UTF8:user@$REALM
extendedKeyUsage = $CLIENT_EKU_LIST

[exts_none]
EOF

# Generate a private key.
openssl genrsa $KEYSIZE > privkey.pem
openssl rsa -in privkey.pem -out privkey-enc.pem -des3 -passout pass:encrypted

# Generate a "CA" certificate.
SUBJECT=ca openssl req -config openssl.cnf -new -x509 -extensions exts_ca \
    -set_serial 1 -days $DAYS -key privkey.pem -out ca.pem

serial=2
gen_cert() {
    SUBJECT=$1 openssl req -config openssl.cnf -new -key privkey.pem -out csr
    SUBJECT=$1 openssl x509 -extfile openssl.cnf -extensions $2 \
           -set_serial $serial -days $DAYS -req -CA ca.pem -CAkey privkey.pem \
           -in csr -out $3
    serial=$((serial + 1))
    rm -f csr
}

gen_pkcs12() {
    # Use -descert to make OpenSSL 1.1 generate files OpenSSL 3.0 can
    # read (the default uses RC2, which is only available in the
    # legacy provider in OpenSSL 3).  This option causes an algorithm
    # downgrade with OpenSSL 3.0 (AES to DES3), but that isn't
    # important for test certs.
    openssl pkcs12 -export -descert -in "$1" -inkey privkey.pem -out "$2" \
            -passout pass:"$3"
}

# Generate a KDC certificate.
gen_cert kdc exts_kdc kdc.pem

# Generate a client certificate and PKCS#12 bundles.
gen_cert user exts_client user.pem
gen_pkcs12 user.pem user.p12
gen_pkcs12 user.pem user-enc.p12 encrypted

# Generate a client certificate and PKCS#12 bundle with a UPN SAN.
gen_cert user exts_upn_client user-upn.pem
gen_pkcs12 user-upn.pem user-upn.p12

# Same, but with no realm in the UPN SAN.
gen_cert user exts_upn2_client user-upn2.pem
gen_pkcs12 user-upn2.pem user-upn2.p12

# Same, but with an uppercase realm in the UPN SAN.
gen_cert user exts_upn3_client user-upn3.pem
gen_pkcs12 user-upn3.pem user-upn3.p12

# Generate a client certificate and PKCS#12 bundle with no PKINIT extensions.
gen_cert user exts_none generic.pem
gen_pkcs12 generic.pem generic.p12

# Clean up.
rm -f openssl.cnf

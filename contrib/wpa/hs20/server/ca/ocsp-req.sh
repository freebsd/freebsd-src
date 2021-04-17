#!/bin/sh

for i in *.pem; do
    echo "===[ $i ]==================="
    openssl ocsp -text -CAfile ca.pem -verify_other demoCA/cacert.pem -trust_other -issuer demoCA/cacert.pem -cert $i -url http://localhost:8888/

#    openssl ocsp -text -CAfile rootCA/cacert.pem -issuer demoCA/cacert.pem -cert $i -url http://localhost:8888/

#    openssl ocsp -text -CAfile rootCA/cacert.pem -verify_other demoCA/cacert.pem -trust_other -issuer demoCA/cacert.pem -cert $i -url http://localhost:8888/
#    openssl ocsp -text -CAfile rootCA/cacert.pem -VAfile ca.pem -trust_other -issuer demoCA/cacert.pem -cert $i -url http://localhost:8888/
done

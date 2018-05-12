#!/bin/bash

CERT_EXPIRE_DAYS="$(( 365 * 15 ))"
DIR="$(dirname "$0")"


if [[ "$PWD" != *tdir ]]
then
    echo "You should run this script with a .tdir directory"
    exit 1
fi

for i in 1 2
do
    # Ephemeral key
    rm -f "${i}.key"
    dnscrypt-wrapper --gen-crypt-keypair \
        --crypt-secretkey-file="${i}.key"  \
        --provider-publickey-file="${DIR}/keys${i}/public.key" \
        --provider-secretkey-file="${DIR}/keys${i}/secret.key"
    # Cert file
    for cipher in salsa chacha
    do
        rm -f "${i}_${cipher}.cert"
        extraarg=""
        if [ "${cipher}" == "chacha" ]
        then
             extraarg="-x"
        fi

        dnscrypt-wrapper ${extraarg} --gen-cert-file \
            --provider-cert-file="${i}_${cipher}.cert" \
            --crypt-secretkey-file="${i}.key" \
            --provider-publickey-file="${DIR}/keys${i}/public.key" \
            --provider-secretkey-file="${DIR}/keys${i}/secret.key" \
            --cert-file-expire-days="${CERT_EXPIRE_DAYS}"
    done
done

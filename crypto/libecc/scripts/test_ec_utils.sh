#/*
# *  Copyright (C) 2021 - This file is part of libecc project
# *
# *  Authors:
# *      Ryad BENADJILA <ryadbenadjila@gmail.com>
# *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
# *
# *  This software is licensed under a dual BSD and GPL v2 license.
# *  See LICENSE file at the root folder of the project.
# */
#!/bin/bash

BASEDIR=$(dirname "$0")
EC_UTILS=$BASEDIR/../build/ec_utils

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
    echo "** Trapped CTRL-C, cleaning ..."
    rm -f test_key_public_key.bin test_key_private_key.bin test_key_private_key.h test_key_public_key.h signed_file.bin.signed
    exit
}

# Test ec_utils cases
curves=("FRP256V1" "SECP192R1" "SECP224R1" "SECP256R1" "SECP384R1" "SECP521R1" "BRAINPOOLP192R1" "BRAINPOOLP224R1" "BRAINPOOLP256R1" "BRAINPOOLP384R1" "BRAINPOOLP512R1" "GOST256" "GOST512" "SM2P256TEST" "SM2P256V1" "WEI25519" "WEI448" "GOST_R3410_2012_256_PARAMSETA" "SECP256K1")
signatures=("ECDSA" "ECKCDSA" "ECSDSA" "ECOSDSA" "ECFSDSA" "ECGDSA" "ECRDSA" "SM2" "EDDSA25519" "EDDSA25519CTX" "EDDSA25519PH" "EDDSA448" "EDDSA448PH" "DECDSA")
hashes=("SHA224" "SHA256" "SHA384" "SHA512" "SHA512_224" "SHA512_256" "SHA3_224" "SHA3_256" "SHA3_384" "SHA3_512" "SM3" "SHAKE256" "STREEBOG256" "STREEBOG512")

for c in "${!curves[@]}"
do
    for s in "${!signatures[@]}"
    do
        # Generate keys
        # NOTE: EDDSA family only accepts WEI curves
        if [[ "${signatures[s]}" == "EDDSA25519" || "${signatures[s]}" == "EDDSA25519CTX" || "${signatures[s]}" == "EDDSA25519PH" ]]
        then
            if [[ "${curves[c]}" != "WEI25519" ]]
            then
                continue
            fi
        fi
        if [[ "${signatures[s]}" == "EDDSA448" || "${signatures[s]}" == "EDDSA448PH" ]]
        then
            if [[ "${curves[c]}" != "WEI448" ]]
            then
                continue
            fi
        fi
        echo "===== ${curves[c]} ${signatures[s]}"
        $EC_UTILS gen_keys ${curves[c]} ${signatures[s]} test_key || exit 0
        for h in "${!hashes[@]}"
        do
            if [[ "${signatures[s]}" == "EDDSA25519" || "${signatures[s]}" == "EDDSA25519CTX" || "${signatures[s]}" == "EDDSA25519PH" ]]
            then
                if [[ "${hashes[h]}" != "SHA512" ]]
                then
                    continue
                fi
            fi
            if [[ "${signatures[s]}" == "EDDSA448" || "${signatures[s]}" == "EDDSA448PH" ]]
            then
                if [[ "${hashes[h]}" != "SHAKE256" ]]
                then
                    continue
                fi
            fi
            echo "========= TESTING ${curves[c]} ${signatures[s]} ${hashes[h]}"
            # Try to sign
            $EC_UTILS sign ${curves[c]} ${signatures[s]} ${hashes[h]} $EC_UTILS test_key_private_key.bin signed_file.bin.signed "ANCILLARY" || exit 0
            # Try to verify
            $EC_UTILS verify ${curves[c]} ${signatures[s]} ${hashes[h]} $EC_UTILS test_key_public_key.bin signed_file.bin.signed "ANCILLARY"  || exit 0
            rm -f signed_file.bin.signed
            # Try to "struct" sign
            $EC_UTILS struct_sign ${curves[c]} ${signatures[s]} ${hashes[h]} $EC_UTILS test_key_private_key.bin signed_file.bin.signed IMAGE_TYPE0 1337 "ANCILLARY"  || exit 0
            # Try to "struct" verify
            $EC_UTILS struct_verify ${curves[c]} ${signatures[s]} ${hashes[h]} signed_file.bin.signed test_key_public_key.bin "ANCILLARY"  || exit 0
            rm -f signed_file.bin.signed
        done
        rm -f test_key_public_key.bin test_key_private_key.bin test_key_private_key.h test_key_public_key.h
    done
done

#!/bin/sh

set -e
mkdir -p sntrup761_dec_fuzzing sntrup761_enc_fuzzing
(cd sntrup761_enc_fuzzing ;
 ../sntrup761_enc_fuzz -jobs=48 ../sntrup761_pubkey_corpus &)
(cd sntrup761_dec_fuzzing ;
 ../sntrup761_dec_fuzz -jobs=48 ../sntrup761_ciphertext_corpus &)

while true ; do
	clear
	uptime
	echo
	echo "Findings"
	ls -1 sntrup761_dec_fuzzing sntrup761_enc_fuzzing | grep -v '^fuzz-.*log$'
	printf "\n\n"
	printf "ciphertext_corpus: " ; ls -1 sntrup761_ciphertext_corpus | wc -l
	printf "    pubkey_corpus: "; ls -1 sntrup761_pubkey_corpus | wc -l
	sleep 10;
done

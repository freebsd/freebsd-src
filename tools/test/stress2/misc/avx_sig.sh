#!/bin/sh

# Test AVX context integrity under ctx switches and signals (amd64)

src=/usr/src/tools/test/avx_sig/avx_sig.c
[ -f $src ] || exit 0
[ `uname -p` != "amd64" ] && exit 0

cd /tmp
cc -Wall -Wextra -O -g -o avx_sig $src -lpthread || exit 1

timeout 1m /tmp/avx_sig; s=$?
[ $s -eq 124 ] && s=0

rm /tmp/avx_sig
exit $s

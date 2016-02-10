#!/bin/sh

UPSTREAM_TAG=freebsd-head-20160129

git diff --name-only ${UPSTREAM_TAG} | \
    grep -v -E '(cheri|bin/shmem_bench|contrib/(curl|jpeg|libpng|netsurf)|^ctsrd|diff-to-upstream.sh|lib/(lib(curl|helloworld|jpeg|png)|netsurf)|libexec/.*-helper|usr.bin/libpng_sb_test|share/netsurf|usr.bin/nsfb)' | \
    xargs git diff ${UPSTREAM_TAG} --

#!/bin/sh

UPSTREAM_TAG=freebsd-head-20150827

git diff --name-only ${UPSTREAM_TAG} | \
    grep -v -E '(cheri|bin/shmem_bench|contrib/(curl|jpeg|libpng|netsurf)|^ctsrd|lib/lib(curl|helloworld|jpeg|png)|libexec/.*-helper|usr.bin/libpng_sb_test|usr.bin/nsfb)' | \
    xargs git diff ${UPSTREAM_TAG} --

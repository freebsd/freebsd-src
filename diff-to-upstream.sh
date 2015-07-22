#!/bin/sh

git diff --name-only freebsd-head-20150514 | \
    grep -v -E '(cheri|bin/shmem_bench|contrib/(curl|jpeg|libpng|netsurf)|^ctsrd|lib/lib(curl|helloworld|jpeg|png)|libexec/.*-helper|usr.bin/libpng_sb_test|usr.bin/nsfb)' | \
    xargs git diff freebsd-head-20150514 --

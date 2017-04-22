#!/bin/sh

UPSTREAM_TAG=freebsd-head-20170418

git diff --name-only ${UPSTREAM_TAG} | \
    grep -v -E '(cheri|bin/(auxargs|helloworld|shmem_bench)|contrib/(curl|gdb|jpeg|libpng|netsurf)|^ctsrd|diff-to-upstream.sh|gnu/usr.bin/gdb|lib/(lib(curl|helloworld|jpeg|png|statcounters)|netsurf)|libexec/.*-helper|usr.bin/libpng_sb_test|share/netsurf|usr.bin/(capsize|nsfb|qtrace))' | \
    xargs git diff ${UPSTREAM_TAG} --

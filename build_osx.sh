#!/bin/sh

#export SHARED_TOOLCHAIN=yes
set -x
export MAKEFLAGS=" -m `pwd`/share/mk -d l"

#export TARGET=amd64
#export TARGET_ARCH=amd64
#export TARGET=amd64
#export TARGET_ARCH=amd64
export MACHINE_ARCH=i386

#export MACHINE_CPUARCH=i386
#export CPUTYPE=i386

export TARGET=i386
export TARGET_ARCH=i386
export NO_LIBC_A=yes
export WITH_SHARED_TOOLCHAIN=yes
export BUILDING_OS_X=yes

#WITH_SHARED_TOOLCHAIN=yes \

    CFLAGS='-D__FBSDID\(x\)=' \
    bsdmake buildworld

# -m `pwd`/share/mk -d l buildworld \


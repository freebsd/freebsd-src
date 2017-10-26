#!/bin/sh

# $FreeBSD$

#
# Full list of all arches, but we only build a subset. All different mips add any
# value, and there's a few others we just don't support.
#
#	mips/mipsel mips/mips mips/mips64el mips/mips64 mips/mipsn32 \
#	mips/mipselhf mips/mipshf mips/mips64elhf mips/mips64hf \
#	powerpc/powerpc powerpc/powerpc64 powerpc/powerpcspe \
#	riscv/riscv64 riscv/riscv64sf
#
# This script is expected to be run in sys/boot (though you could run it anywhere
# in the tree). It does a full clean build. For sys/boot you can do all the archs in
# about a minute or two on a fast machine. It's also possible that you need a full
# make universe for this to work completely.
#
# Output is put into _.boot.$TARGET_ARCH.log in sys.boot.
#

top=$(make -V SRCTOP)
cd $top/sys/boot

for i in \
	amd64/amd64 \
	arm/arm arm/armeb arm/armv7 \
	arm64/aarch64 \
	i386/i386 \
	mips/mips mips/mips64 \
	powerpc/powerpc powerpc/powerpc64 \
	sparc64/sparc64 \
	; do
    ta=${i##*/}
    echo -n "Building $ta..."
    ( ( make buildenv TARGET_ARCH=$ta BUILDENV_SHELL="make -j 20 clean cleandepend cleandir obj depend all" \
	 > _.boot.${ta}.log 2>&1 ) && echo Success ) || echo Fail
done




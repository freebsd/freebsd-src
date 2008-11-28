#
# Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
# Copyright (c) 2002-2008 Atheros Communications, Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile,v 1.3 2008/11/10 03:37:38 sam Exp $
#
DEPTH=	..

#
# Makefile for building the Atheros/MADWIFI HAL from source code with gmake
#
HAL=	${DEPTH}
PUBLIC=	${HAL}/public
DEBUG_OPTS=AH_DEBUG=1 AH_ASSERT=1

#
ifdef TARGET
ALL=	${TARGET}
else
ALL=	i386-elf \
	x86_64-elf \
	alpha-elf \
	arm-elf \
	mipsisa32-be-elf mips3-elf mips2-elf mips1-elf \
	powerpc-eabi powerpc-elf \
	sh4-elf sparc-elf soc
endif
#

all:	${ALL}

release:
	for i in ${ALL}; do \
		${MAKE} $$i-release; \
	done

debug:
	for i in ${ALL}; do \
		${MAKE} $$i-debug; \
	done

clean:
	for i in ${ALL}; do \
		${MAKE} $$i-clean; \
	done

#
# XXX someone with some gmake foo should be able to do this right...
#
i386-elf: ${PUBLIC}/i386-elf.inc
	${MAKE} -f Makefile.inc TARGET=i386-elf
i386-elf-release:
	${MAKE} -f Makefile.inc TARGET=i386-elf release
i386-elf-debug:
	${MAKE} -f Makefile.inc TARGET=i386-elf ${DEBUG_OPTS} debug
i386-elf-clean:
	${MAKE} -f Makefile.inc TARGET=i386-elf clean

x86_64-elf: ${PUBLIC}/x86_64-elf.inc
	${MAKE} -f Makefile.inc TARGET=x86_64-elf
x86_64-elf-release:
	${MAKE} -f Makefile.inc TARGET=x86_64-elf release
x86_64-elf-debug:
	${MAKE} -f Makefile.inc TARGET=x86_64-elf ${DEBUG_OPTS} debug
x86_64-elf-clean:
	${MAKE} -f Makefile.inc TARGET=x86_64-elf clean

alpha-elf: ${PUBLIC}/alpha-elf.inc
	${MAKE} -f Makefile.inc TARGET=alpha-elf
alpha-elf-release:
	${MAKE} -f Makefile.inc TARGET=alpha-elf release
alpha-elf-debug:
	${MAKE} -f Makefile.inc TARGET=alpha-elf ${DEBUG_OPTS} debug
alpha-elf-clean:
	${MAKE} -f Makefile.inc TARGET=alpha-elf clean

arm-elf: armv4-be-elf armv4-le-elf arm9-le-thumb-elf xscale-elf
arm-elf-release: armv4-be-elf-release armv4-le-elf-release \
	arm9-le-thumb-elf-release xscale-elf-release
arm-elf-debug: armv4-be-elf-debug armv4-le-elf-debug \
	arm9-le-thumb-elf-debug xscale-elf-debug
arm-elf-clean: armv4-be-elf-clean armv4-le-elf-clean \
	arm9-le-thumb-elf-clean xscale-elf-clean

armv4-be-elf: ${PUBLIC}/armv4-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=armv4-be-elf
armv4-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=armv4-be-elf release
armv4-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=armv4-be-elf ${DEBUG_OPTS} debug
armv4-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=armv4-be-elf clean
armv4-le-elf: ${PUBLIC}/armv4-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=armv4-le-elf
armv4-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=armv4-le-elf release
armv4-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=armv4-le-elf ${DEBUG_OPTS} debug
armv4-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=armv4-le-elf clean
arm9-le-thumb-elf: ${PUBLIC}/arm9-le-thumb-elf.inc
	${MAKE} -f Makefile.inc TARGET=arm9-le-thumb-elf
arm9-le-thumb-elf-release:
	${MAKE} -f Makefile.inc TARGET=arm9-le-thumb-elf release
arm9-le-thumb-elf-debug:
	${MAKE} -f Makefile.inc TARGET=arm9-le-thumb-elf ${DEBUG_OPTS} debug
arm9-le-thumb-elf-clean:
	${MAKE} -f Makefile.inc TARGET=arm9-le-thumb-elf clean

xscale-elf: xscale-be-elf xscale-le-elf
xscale-elf-release: xscale-be-elf-release xscale-le-elf-release
xscale-elf-debug: xscale-be-elf-debug xscale-le-elf-debug
xscale-elf-clean: xscale-be-elf-clean xscale-le-elf-clean

xscale-be-elf: ${PUBLIC}/xscale-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=xscale-be-elf
xscale-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=xscale-be-elf release
xscale-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=xscale-be-elf ${DEBUG_OPTS} debug
xscale-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=xscale-be-elf clean
xscale-le-elf: ${PUBLIC}/xscale-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=xscale-le-elf
xscale-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=xscale-le-elf release
xscale-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=xscale-le-elf ${DEBUG_OPTS} debug
xscale-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=xscale-le-elf clean

mipsisa32-be-elf: mips3-be-elf
mipsisa32-be-elf-release: mips3-be-elf-release
mipsisa32-be-elf-debug: mips3-be-elf-debug
mipsisa32-be-elf-clean: mips3-be-elf-clean

mips3-elf: mips3-be-elf mips3-le-elf
mips3-elf-release: mips3-be-elf-release mips3-le-elf-release
mips3-elf-debug: mips3-be-elf-debug mips3-le-elf-debug
mips3-elf-clean: mips3-be-elf-clean mips3-le-elf-clean

# NB: the .inc file naming is historical and needs changing
mips3-be-elf: ${PUBLIC}/mipsisa32-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=mipsisa32-be-elf
mips3-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-be-elf release
mips3-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-be-elf ${DEBUG_OPTS} debug
mips3-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-be-elf clean

mips3-le-elf: ${PUBLIC}/mipsisa32-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=mipsisa32-le-elf
mips3-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-le-elf release
mips3-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-le-elf ${DEBUG_OPTS} debug
mips3-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mipsisa32-le-elf clean

# NB: the .inc file naming is historical and needs changing
mips2-elf: mips2-be-elf mips2-le-elf
mips2-elf-release: mips2-be-elf-release mips2-le-elf-release
mips2-elf-debug: mips2-be-elf-debug mips2-le-elf-debug
mips2-elf-clean: mips2-be-elf-clean mips2-le-elf-clean

mips2-be-elf: ${PUBLIC}/mips-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=mips-be-elf
mips2-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=mips-be-elf release
mips2-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mips-be-elf ${DEBUG_OPTS} debug
mips2-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mips-be-elf clean

mips2-le-elf: ${PUBLIC}/mips-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=mips-le-elf
mips2-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=mips-le-elf release
mips2-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mips-le-elf ${DEBUG_OPTS} debug
mips2-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mips-le-elf clean

mips1-elf: mips1-be-elf mips1-le-elf
mips1-elf-release: mips1-be-elf-release mips1-le-elf-release
mips1-elf-debug: mips1-be-elf-debug mips1-le-elf-debug
mips1-elf-clean: mips1-be-elf-clean mips1-le-elf-clean

mips1-be-elf: ${PUBLIC}/mips1-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=mips1-be-elf
mips1-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=mips1-be-elf release
mips1-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mips1-be-elf ${DEBUG_OPTS} debug
mips1-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mips1-be-elf clean

mips1-le-elf: ${PUBLIC}/mips1-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=mips1-le-elf
mips1-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=mips1-le-elf release
mips1-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=mips1-le-elf ${DEBUG_OPTS} debug
mips1-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=mips1-le-elf clean

powerpc-eabi: powerpc-be-eabi powerpc-le-eabi
powerpc-eabi-release: powerpc-be-eabi-release powerpc-le-eabi-release
powerpc-eabi-debug: powerpc-be-eabi-debug powerpc-le-eabi-debug
powerpc-eabi-clean: powerpc-be-eabi-clean powerpc-le-eabi-clean

powerpc-be-eabi: ${PUBLIC}/powerpc-be-eabi.inc
	${MAKE} -f Makefile.inc TARGET=powerpc-be-eabi
powerpc-be-eabi-release:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-eabi release
powerpc-be-eabi-debug:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-eabi ${DEBUG_OPTS} debug
powerpc-be-eabi-clean:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-eabi clean
powerpc-le-eabi: ${PUBLIC}/powerpc-le-eabi.inc
	${MAKE} -f Makefile.inc TARGET=powerpc-le-eabi
powerpc-le-eabi-release:
	${MAKE} -f Makefile.inc TARGET=powerpc-le-eabi release
powerpc-le-eabi-debug:
	${MAKE} -f Makefile.inc TARGET=powerpc-le-eabi ${DEBUG_OPTS} debug
powerpc-le-eabi-clean:
	${MAKE} -f Makefile.inc TARGET=powerpc-le-eabi clean

powerpc-elf: powerpc-be-elf
powerpc-elf-release: powerpc-be-elf-release
powerpc-elf-debug: powerpc-be-elf-debug
powerpc-elf-clean: powerpc-be-elf-clean

powerpc-be-elf: ${PUBLIC}/powerpc-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=powerpc-be-elf
powerpc-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-elf release
powerpc-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-elf ${DEBUG_OPTS} debug
powerpc-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=powerpc-be-elf clean

sh4-elf: sh4-le-elf
sh4-elf-release: sh4-le-elf-release
sh4-elf-debug: sh4-le-elf-debug
sh4-elf-clean: sh4-le-elf-clean

sh4-le-elf: ${PUBLIC}/sh4-le-elf.inc
	${MAKE} -f Makefile.inc TARGET=sh4-le-elf
sh4-le-elf-release:
	${MAKE} -f Makefile.inc TARGET=sh4-le-elf release
sh4-le-elf-debug:
	${MAKE} -f Makefile.inc TARGET=sh4-le-elf ${DEBUG_OPTS} debug
sh4-le-elf-clean:
	${MAKE} -f Makefile.inc TARGET=sh4-le-elf clean

sparc-elf: sparc64-be-elf sparc-be-elf
sparc-elf-release: sparc64-be-elf-release sparc-be-elf-release
sparc-elf-debug: sparc64-be-elf-debug sparc-be-elf-debug
sparc-elf-clean: sparc64-be-elf-clean sparc-be-elf-clean

sparc64-be-elf: ${PUBLIC}/sparc64-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=sparc64-be-elf
sparc64-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=sparc64-be-elf release
sparc64-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=sparc64-be-elf ${DEBUG_OPTS} debug
sparc64-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=sparc64-be-elf clean

sparc-be-elf: ${PUBLIC}/sparc-be-elf.inc
	${MAKE} -f Makefile.inc TARGET=sparc-be-elf
sparc-be-elf-release:
	${MAKE} -f Makefile.inc TARGET=sparc-be-elf release
sparc-be-elf-debug:
	${MAKE} -f Makefile.inc TARGET=sparc-be-elf ${DEBUG_OPTS} debug
sparc-be-elf-clean:
	${MAKE} -f Makefile.inc TARGET=sparc-be-elf clean

#
# SoC builds
#
# Use the ap43 build for an ap48 board
#
soc:	ap30 ap43 ap51 ap61 wisoc
soc-release:	ap30-release ap43-release ap51-release ap61-release wisoc-release
soc-debug:	ap30-debug ap43-debug ap51-debug ap61-debug wisoc-debug
soc-clean:	ap30-clean ap43-clean ap51-clean ap61-clean wisoc-clean

ap30: ${PUBLIC}/ap30.inc
	${MAKE} -f Makefile.inc TARGET=ap30 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1
ap30-release: ${PUBLIC}/ap30.inc
	${MAKE} -f Makefile.inc TARGET=ap30 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1 release
ap30-debug: ${PUBLIC}/ap30.inc
	${MAKE} -f Makefile.inc TARGET=ap30 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1 ${DEBUG_OPTS} debug
ap30-clean:
	${MAKE} -f Makefile.inc TARGET=ap30 clean

ap43: ${PUBLIC}/ap43.inc
	${MAKE} -f Makefile.inc TARGET=ap43 AH_SUPPORT_AR5312=1
ap43-release: ${PUBLIC}/ap43.inc
	${MAKE} -f Makefile.inc TARGET=ap43 AH_SUPPORT_AR5312=1 release
ap43-debug: ${PUBLIC}/ap43.inc
	${MAKE} -f Makefile.inc TARGET=ap43 AH_SUPPORT_AR5312=1 ${DEBUG_OPTS} debug
ap43-clean:
	${MAKE} -f Makefile.inc TARGET=ap43 clean

ap51: ${PUBLIC}/ap51.inc
	${MAKE} -f Makefile.inc TARGET=ap51 \
		AH_SUPPORT_2316=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1
ap51-release: ${PUBLIC}/ap51.inc
	${MAKE} -f Makefile.inc TARGET=ap51 \
		AH_SUPPORT_2316=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1 release
ap51-debug: ${PUBLIC}/ap51.inc
	${MAKE} -f Makefile.inc TARGET=ap51 \
		AH_SUPPORT_2316=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1 AH_NEED_DESC_SWAP=1 ${DEBUG_OPTS} debug
ap51-clean:
	${MAKE} -f Makefile.inc TARGET=ap51 clean

ap61: ${PUBLIC}/ap61.inc
	${MAKE} -f Makefile.inc TARGET=ap61 \
		AH_SUPPORT_2317=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1
ap61-release: ${PUBLIC}/ap61.inc
	${MAKE} -f Makefile.inc TARGET=ap61 \
		AH_SUPPORT_2317=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1 release
ap61-debug: ${PUBLIC}/ap61.inc
	${MAKE} -f Makefile.inc TARGET=ap61 \
		AH_SUPPORT_2317=1 AH_SUPPORT_5111=0 AH_SUPPORT_5112=0 \
		AH_SUPPORT_AR5312=1 ${DEBUG_OPTS} debug
ap61-clean:
	${MAKE} -f Makefile.inc TARGET=ap61 clean

wisoc: ${PUBLIC}/wisoc.inc
	${MAKE} -f Makefile.inc TARGET=wisoc \
		AH_SUPPORT_2316=1 \
		AH_SUPPORT_2317=1 \
		AH_NEED_DESC_SWAP=1 \
		AH_SUPPORT_AR5312=1
wisoc-release: ${PUBLIC}/wisoc.inc
	${MAKE} -f Makefile.inc TARGET=wisoc \
		AH_SUPPORT_2316=1 \
		AH_SUPPORT_2317=1 \
		AH_NEED_DESC_SWAP=1 \
		AH_SUPPORT_AR5312=1 release
wisoc-debug: ${PUBLIC}/wisoc.inc
	${MAKE} -f Makefile.inc TARGET=wisoc \
		AH_SUPPORT_2316=1 \
		AH_SUPPORT_2317=1 \
		AH_NEED_DESC_SWAP=1 \
		AH_SUPPORT_AR5312=1 ${DEBUG_OPTS} debug
wisoc-clean:
	${MAKE} -f Makefile.inc TARGET=wisoc clean

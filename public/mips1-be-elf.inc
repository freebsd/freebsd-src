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
# $Id: mips1-be-elf.inc,v 1.2 2008/11/10 03:44:09 sam Exp $
#

#
# Configuration for building big-endian MIPS1 using the 5.01
# linux-mips.org toolchain (manually moved to /pub/gnu from
# /pub/gnu/local where rpm --prefix=/pub/gnu bogusly installs them).

#
# http://www.linux-mips.org/toolchain.html
#
# /pub/gnu/bin/mips-linux-gcc -v
# Reading specs from /pub/gnu/bin/../lib/gcc-lib/mips-linux/2.96-mips3264-000710/specs
# gcc version 2.96-mips3264-000710
# /pub/gnu/bin/mips-linux-as -v
# GNU assembler version 2.12.90.0.7 (mips-linux) using BFD version 2.12.90.0.7 20020423

#
ifndef TOOLPREFIX
TOOLPREFIX=	/pub/gnu/bin/mips-linux-
endif
#
CC=	${TOOLPREFIX}gcc
LD=	${TOOLPREFIX}ld
STRIP=	${TOOLPREFIX}strip
OBJCOPY=${TOOLPREFIX}objcopy
NM=	${TOOLPREFIX}nm

#
# Force register read/write operations to go through a function.
#
AH_REGOPS_FUNC=1

LDOPTS=	-EB
COPTS+=	-DAH_BYTE_ORDER=AH_BIG_ENDIAN
COPTS+=	-G 0 -EB -mno-abicalls -fno-pic -mips1 -Wa,--trap \
	-fno-strict-aliasing -fno-common -fomit-frame-pointer -mlong-calls

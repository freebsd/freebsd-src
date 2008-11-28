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
# $Id: alpha-elf.inc,v 1.2 2008/11/10 03:43:26 sam Exp $
#

#
# Compilation configuration for building big-endian Alpha w/ elf.
#
# Built with GNU cross-devel tools:
#
# PREFIX=/pub/gnu
# BINUTILS=binutils-2.14
# GCC=gcc-3.3.2
# target=alpha-elf-linux
#
# ${BINUTILS}/configure --target=$target --prefix=${PREFIX}
# ${GCC}/configure --target=$target   --prefix=${PREFIX} \
#	--enable-languages=c   --with-gnu-as --with-gnu-ld \
#	--with-newlib   --with-gxx-include-dir=${PREFIX}/$target/include
#

#
ifndef TOOLPREFIX
TOOLPREFIX=	/pub/gnu/bin/alpha-elf-linux-
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

COPTS+=	-DAH_BYTE_ORDER=AH_LITTLE_ENDIAN
#
# NB: this should come from inttypes.h but can't until we cleanp
#     the definition of va_list on linux
#
COPTS+=	-DAH_WORDSIZE=64

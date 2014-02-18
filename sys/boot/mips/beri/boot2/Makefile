#-
# Copyright (c) 2013-2014 Robert N. M. Watson
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# @BERI_LICENSE_HEADER_START@
#
# Licensed to BERI Open Systems C.I.C (BERI) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  BERI licenses this
# file to you under the BERI Hardware-Software License, Version 1.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at:
#
#   http://www.beri-open-systems.org/legal/license-1-0.txt
#
# Unless required by applicable law or agreed to in writing, Work distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations under the License.
#
# @BERI_LICENSE_HEADER_END@
#
#  $FreeBSD$

BINDIR?=	/boot
INSTALLFLAGS=	-b

LOADERS=	flashboot jtagboot
FILES=		${LOADERS} ${LOADERS:S/$/.md5/}

SRCS=		relocate.S		\
		start.S			\
		boot2.c			\
		altera_jtag_uart.c	\
		cfi.c			\
		sdcard.c

NO_MAN=

AFLAGS=		-G0

CFLAGS=		-ffreestanding			\
		-I${.CURDIR}			\
		-I${.CURDIR}/../../../common	\
		-I${.CURDIR}/../../../..	\
		-D_KERNEL			\
		-Wall				\
		-G0 -Xassembler -G0		\
		-fno-pic -mno-abicalls		\
		-msoft-float			\
		-g

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../../lib/libstand/
LIBSTAND=	${.OBJDIR}/../../../../../lib/libstand/libstand.a

LDFLAGS=	-nostdlib			\
		-static				\
		-N				\
		-G0				\
		-L${.CURDIR}

.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

flashboot.elf: relocate.o start.o boot2.o altera_jtag_uart.o cfi.o sdcard.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/flashboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}
flashboot: flashboot.elf
	objcopy -S -O binary ${.TARGET}.elf ${.TARGET}
flashboot.md5: flashboot
	md5 flashboot > flashboot.md5

jtagboot: start.o boot2.o altera_jtag_uart.o cfi.o sdcard.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/jtagboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}
jtagboot.md5: jtagboot
	md5 jtagboot > jtagboot.md5

CLEANFILES+=	flashboot.elf

.include <bsd.prog.mk>

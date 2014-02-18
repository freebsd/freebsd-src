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

# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

PROG?=		loader
NEWVERSWHAT=	"BERI loader" ${MACHINE_CPUARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		start.S						\
		main.c						\
		devicename.c					\
		exec.c						\
		metadata.c					\
		vers.c						\
		arch.c

# libstand front-ends for shared driver code
SRCS+=		beri_console.c					\
		beri_disk_cfi.c					\
		beri_disk_sdcard.c

# Common code with boot2
SRCS+=		altera_jtag_uart.c				\
		cfi.c						\
		sdcard.c

# Since we don't have a backward compatibility issue, default to this on BERI.
CFLAGS+=	-DBOOT_PROMPT_123

CFLAGS+=	-DLOADER_DISK_SUPPORT
CFLAGS+=	-DLOADER_UFS_SUPPORT
CFLAGS+=	-DLOADER_GZIP_SUPPORT
CFLAGS+=	-DLOADER_BZIP2_SUPPORT

#CFLAGS+=	-DLOADER_NET_SUPPORT
#CFLAGS+=	-DLOADER_NFS_SUPPORT
#CFLAGS+=	-DLOADER_TFTP_SUPPORT

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../../ficl
CFLAGS+=	-I${.CURDIR}/../../../ficl/mips
LIBFICL=	${.OBJDIR}/../../../ficl/libficl.a
.endif

# Common code across BERI boot loader parts
.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

# Always add MI sources
.PATH:		${.CURDIR}/../../../common
.include	"${.CURDIR}/../../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../../common

# BERI files common to boot2 and loader
.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

# Loader-specific MD headers
CFLAGS+=	-I${.CURDIR}

CLEANFILES+=	vers.c loader.help

# Generate code appropriate for the loader environment
CFLAGS+=	-G0					\
		-fno-pic				\
		-mno-abicalls				\
		-msoft-float				\
		-g

LDFLAGS=	-nostdlib				\
		-static					\
		-T ${.CURDIR}/loader.ldscript		\
		-L${.CURDIR}				\
		-e __start

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../../lib/libstand/
LIBSTAND=	${.OBJDIR}/../../../../../lib/libstand/libstand.a

DPADD=		${LIBFICL} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBSTAND}

vers.c: ${.CURDIR}/../../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../../common/newvers.sh ${.CURDIR}/version	\
	    ${NEWVERSWHAT}

loader.help: help.common help.mips
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../../forth
FILES=	loader.help loader.4th support.4th loader.conf
FILES+=	screen.4th frames.4th
FILES+=	beastie.4th brand.4th check-password.4th color.4th delay.4th
FILES+=	menu.4th menu-commands.4th menusets.4th shortcuts.4th version.4th
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+= loader.rc
.endif

.if !exists(${DESTDIR}/boot/menu.rc)
FILES+= menu.rc
.endif

all: loader

.include <bsd.prog.mk>

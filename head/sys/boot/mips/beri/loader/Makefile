#-
# Copyright (c) 2013-2014 Robert N. M. Watson
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

.include <src.opts.mk>
MK_SSP=		no
MAN=

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
CFLAGS+=	-I${.CURDIR}/../../../ficl/mips64
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
.include	"${.CURDIR}/../../../forth/Makefile.inc"

FILES+= loader.rc menu.rc

.if defined(LOADER_USB_SUPPORT)
# Do garbage collection
CFLAGS+= -ffunction-sections -fdata-sections
CFLAGS+= -Wl,--gc-sections
# Link USB BOOT library
LDADD+= ${.OBJDIR}/../../../usb/libusbboot.a
CFLAGS+= -I${.CURDIR}/../../../usb
# Define USB SUPPORT
CFLAGS+= -DLOADER_USB_SUPPORT
.endif

all: loader

.include <bsd.prog.mk>

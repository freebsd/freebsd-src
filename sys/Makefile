# $FreeBSD: src/sys/Makefile,v 1.20 1999/11/14 13:54:38 marcel Exp $

# This is the old aout only boot loader.
.if	exists(${.CURDIR}/${MACHINE_ARCH}/boot) && ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE_ARCH}/boot
.elif	exists(${.CURDIR}/boot) && ${MACHINE_ARCH} == "i386" && ${OBJFORMAT} == "elf"
SUBDIR=	boot
.endif

.if	exists(${.CURDIR}/boot) && ${MACHINE_ARCH} == "alpha"
SUBDIR= boot
.endif

# KLD modules build for both a.out and ELF
SUBDIR+=modules

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

# $FreeBSD: src/sys/Makefile,v 1.20.2.1 2000/07/10 08:22:34 obrien Exp $

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
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.17 1999/04/21 11:09:40 jkoshy Exp $

# This is the old aout only boot loader.
.if	exists(${.CURDIR}/${MACHINE}/boot) && ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE}/boot
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

#	$Id: Makefile,v 1.15 1998/12/30 11:17:09 bde Exp $

# This is the old aout only boot loader.
.if	exists(${MACHINE}/boot) && ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE}/boot
.elif	exists(boot) && ${MACHINE} == "i386" && ${OBJFORMAT} == "elf"
SUBDIR=	boot
.endif

.if	exists(boot) && ${MACHINE_ARCH} == "alpha"
SUBDIR= boot
.endif

# KLD modules build for both a.out and ELF
SUBDIR+=modules

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.12 1998/10/04 00:42:08 gpalmer Exp $

# This is the old aout only boot loader.
.if	exists(${MACHINE}/boot) && ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE}/boot
.elif	exists(boot) && ${MACHINE} == "i386" && ${OBJFORMAT} == "elf"
SUBDIR=	boot
.endif

.if	exists(boot) && ${MACHINE_ARCH} == "alpha"
SUBDIR= boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

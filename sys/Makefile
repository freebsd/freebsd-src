#	$Id: Makefile,v 1.9 1998/02/20 07:37:08 jb Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.elif	exists(${MACHINE}/boot) && ${MACHINE} == "i386"
.if ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE}/boot
.endif
.elif	exists(${MACHINE}/boot)
SUBDIR=	${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.14 1998/11/03 06:50:58 peter Exp $

SUBDIR=	modules

.if ${MACHINE_ARCH} == "alpha" || ${OBJFORMAT} == "elf"
SUBDIR+=boot
.endif

.if exists(${.CURDIR}/${MACHINE}/boot)
SUBDIR+=${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

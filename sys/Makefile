#	$Id: Makefile,v 1.8 1997/07/13 07:36:20 jkh Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.elif	exists(${MACHINE}/boot)
SUBDIR=	${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

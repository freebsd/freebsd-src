#	$Id: Makefile,v 1.5.2.1 1997/04/20 20:16:22 jkh Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.else
SUBDIR=	${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

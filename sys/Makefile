#	$Id: Makefile,v 1.7 1997/02/22 09:27:56 peter Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.else
SUBDIR=	${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

# $FreeBSD$

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.else
SUBDIR=	${MACHINE}/boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

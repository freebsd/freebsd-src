#	$Id: Makefile,v 1.11 1998/09/05 08:27:56 jb Exp $

# This is the old aout only boot loader.
.if	exists(${MACHINE}/boot) && ${OBJFORMAT} == "aout"
SUBDIR=	${MACHINE}/boot
.endif

.if	exists(boot) && ${MACHINE_ARCH} == "alpha"
SUBDIR= boot
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>

#	@(#)Makefile	5.1 (Berkeley) 5/28/90

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c
OBJS+=	version.o
CLEANFILES+=version.c version.o
CFLAGS+=-I${.CURDIR}/..
.PATH:	${.CURDIR}/..
BINDIR=	/usr/libexec
MAN8= named-xfer.8


VER	= 4.9.3-BETA26-LOCAL

version.c: ${.CURDIR}/../Version.c ${.CURDIR}/Makefile ${SRCS}
	(LANG=; LC_TIME=; u=$${USER-root} d=`pwd |sed -e 's|/obj/|/src/|'` \
	 h=`hostname` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${.CURDIR}/../Version.c > version.c)

.include "../../Makefile.inc"
.include <bsd.prog.mk>

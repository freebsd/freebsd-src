# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c dmalloc.c version.c
MAN8=	named-xfer.8

BINDIR=	${DESTLIBEXEC}
CLEANFILES+= version.c

version.c: Version.c ${BIND_DIR}/Makefile
	(u=$${USER-root} d=`pwd` h=`hostname` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${BIND_DIR}/named/Version.c > version.c)

.include <bsd.prog.mk>

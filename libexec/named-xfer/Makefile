# $Id: Makefile,v 1.2 1996/09/01 00:53:45 ache Exp $

.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c dmalloc.c version.c
MAN8=	named-xfer.8

CLEANFILES+= version.c

version.c: Version.c ${BIND_DIR}/Makefile
	(u=$${USER-root} d=`pwd` h=`hostname` t=`LC_TIME=C date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${BIND_DIR}/named/Version.c > version.c)

.include <bsd.prog.mk>

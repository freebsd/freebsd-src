# $Id: Makefile,v 1.2.2.1 1996/11/28 21:51:40 phk Exp $

.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c version.c
MAN8=	named-xfer.8

CLEANFILES+= version.c

version.c: Version.c ${BIND_DIR}/Makefile
	(u=$${USER-root} d=`pwd` h=`hostname` t=`LC_TIME=C date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${BIND_DIR}/named/Version.c > version.c)

.include <bsd.prog.mk>

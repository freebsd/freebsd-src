MAINTAINER=ken@FreeBSD.ORG

LIB=	devstat
SRCS=	devstat.c

MAN3=	devstat.3

MLINKS+=devstat.3 getnumdevs.3 
MLINKS+=devstat.3 getgeneration.3 
MLINKS+=devstat.3 getversion.3
MLINKS+=devstat.3 checkversion.3 
MLINKS+=devstat.3 getdevs.3 
MLINKS+=devstat.3 selectdevs.3
MLINKS+=devstat.3 buildmatch.3 
MLINKS+=devstat.3 compute_stats.3
MLINKS+=devstat.3 compute_etime.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/devstat.h \
	    ${DESTDIR}/usr/include

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../sys

.include <bsd.lib.mk>

# $FreeBSD$

LIB=	geom
SHLIBDIR?= /lib
SRCS+=	geom_getxml.c
SRCS+=	geom_stats.c
SRCS+=	geom_xml2tree.c
SRCS+=	geom_ctl.c
INCS=	libgeom.h

CFLAGS += -I${.CURDIR}

WARNS?=	3

DPADD=	${LIBBSDXML} ${LIBSBUF}
LDADD=	-lbsdxml -lsbuf

MAN=	libgeom.3

MLINKS+= \
	libgeom.3 geom_stats_open.3 \
	libgeom.3 geom_stats_close.3 \
	libgeom.3 geom_stats_resync.3 \
	libgeom.3 geom_stats_snapshot_get.3 \
	libgeom.3 geom_stats_snapshot_free.3 \
	libgeom.3 geom_stats_snapshot_timestamp.3 \
	libgeom.3 geom_stats_snapshot_reset.3 \
	libgeom.3 geom_stats_snapshot_next.3

.include <bsd.lib.mk>

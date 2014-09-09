#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	nsgif

.PATH:		${NETSURF_DISTDIR}/libnsgif/src
SRCS=		libnsgif.c

CFLAGS+=	-I${NETSURF_DISTDIR}/libnsgif/include \
		-I${NETSURF_DISTDIR}/libnsgif/src \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

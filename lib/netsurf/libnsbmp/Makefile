#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	nsbmp

.PATH:		${NETSURF_DISTDIR}/libnsbmp/src
SRCS=		libnsbmp.c
CFLAGS+=	-I${NETSURF_DISTDIR}/libnsbmp/include \
		-I${NETSURF_DISTDIR}/libnsbmp/src \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

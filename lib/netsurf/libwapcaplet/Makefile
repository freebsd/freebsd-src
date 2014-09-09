#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	wapcaplet

.PATH:		${NETSURF_DISTDIR}/libwapcaplet/src
SRCS=		libwapcaplet.c


WARNFLAGS=	-Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations\
		-Wnested-externs -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/libwapcaplet/include \
		-I${NETSURF_DISTDIR}/libwapcaplet/src \
		'-D_ALIGNED=__attribute__((aligned))' \
		-D_LONGLONG -D_BSD_SOURCE -DNDEBUG $(WARNFLAGS)

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

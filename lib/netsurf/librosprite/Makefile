#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	rosprite

.PATH:		${NETSURF_DISTDIR}/librosprite
SRCS=		src/librosprite.c

WARNFLAGS=	-Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations\
		-Wnested-externs -pedantic -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/librosprite/include \
		-I${NETSURF_DISTDIR}/librosprite/src \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG $(WARNFLAGS)

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	svgtiny

CSTD=	c11

.PATH:		${.CURDIR}
SRCS=		src/src_colors.c

.PATH:		${NETSURF_DISTDIR}/libsvgtiny
SRCS+= 		src/svgtiny.c \
		src/svgtiny_gradient.c \
		src/svgtiny_list.c

WARNFLAGS=      -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings   \
                -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
                -Wnested-externs -pedantic -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/libsvgtiny/include \
		-I${NETSURF_DISTDIR}/libsvgtiny/src \
		-I${NETSURF_DISTDIR}/libdom/include \
		-I${NETSURF_DISTDIR}/libwapcaplet/include \
		-I${.CURDIR}/../libdom \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG $(WARNFLAGS)

USE_CHERI=yes

.include <src.opts.mk>
.include <bsd.lib.mk>
.include <bsd.subdir.mk>

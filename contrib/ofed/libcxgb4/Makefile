# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		cxgb4
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= \
cq.c \
dev.c \
qp.c \
verbs.c

LIBADD=		ibverbs pthread
CFLAGS+=	-I${.CURDIR}

.include <bsd.lib.mk>

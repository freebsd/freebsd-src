# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		mlx5
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= \
buf.c \
cq.c \
dbrec.c \
mlx5.c \
qp.c \
srq.c \
verbs.c

LDADD=		ibverbs pthread
CFLAGS+=	-I${.CURDIR}
VERSION_MAP=	${.CURDIR}/libmlx5.map

.include <bsd.lib.mk>


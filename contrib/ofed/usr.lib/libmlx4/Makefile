# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

MLX4DIR=	${.CURDIR}/../../libmlx4
IBVERBSDIR=	${.CURDIR}/../../libibverbs
MLXSRCDIR=	${MLX4DIR}/src

.PATH: ${MLXSRCDIR}

LIB=		mlx4
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	buf.c cq.c dbrec.c mlx4.c qp.c srq.c verbs.c

LIBADD=	ibverbs pthread
CFLAGS+= -DHAVE_CONFIG_H 
CFLAGS+= -I${.CURDIR} -I${MLXSRCDIR} -I${IBVERBSDIR}/include

VERSION_MAP= ${MLXSRCDIR}/mlx4.map

.include <bsd.lib.mk>

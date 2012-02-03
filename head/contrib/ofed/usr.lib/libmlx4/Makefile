# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

MLX4DIR=	${.CURDIR}/../../libmlx4
IBVERBSDIR=	${.CURDIR}/../../libibverbs
MLXSRCDIR=	${MLX4DIR}/src

.PATH: ${MLXSRCDIR}

LIB=		mlx4
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	buf.c cq.c dbrec.c mlx4.c qp.c srq.c verbs.c

CFLAGS+= -DHAVE_CONFIG_H 
CFLAGS+= -I${.CURDIR} -I${MLXSRCDIR} -I${IBVERBSDIR}/include

VERSION_MAP= ${MLXSRCDIR}/mlx4.map

.include <bsd.lib.mk>

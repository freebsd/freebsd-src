# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		ibumad
SHLIB_MAJOR=	1
MK_PROFILE=	no
SRCS= \
umad.c \
umad_str.c \
sysfs.c
CFLAGS+=	-I${.CURDIR}
VERSION_MAP=	${.CURDIR}/libibumad.map

.include <bsd.lib.mk>

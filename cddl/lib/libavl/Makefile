# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/module/avl

PACKAGE=	zfs
LIB=	avl
LIBADD=	spl
SRCS=	avl.c
WARNS?=	3
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
.include <bsd.lib.mk>

# $FreeBSD$

KMOD=	star_saver
SRCS= 	star_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/syscons/snake

KMOD=	snake_saver
SRCS=	snake_saver.c
NOMAN=

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

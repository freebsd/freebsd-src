# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/syscons/star

KMOD=	star_saver
SRCS=	star_saver.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/syscons/apm

KMOD=	apm_saver
SRCS= 	apm_saver.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

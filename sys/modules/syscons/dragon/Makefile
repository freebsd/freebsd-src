# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/syscons/dragon

KMOD=	dragon_saver
SRCS= 	dragon_saver.c

#CWARNFLAGS= -Wall -Wno-long-long -pedantic

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

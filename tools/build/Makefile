# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

BOOTSTRAPPING?=	0

# usr.bin/mklocale needs <runefile.h>.
.if !exists(/usr/include/runefile.h)
INCS+=		runefile.h
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.include <bsd.lib.mk>

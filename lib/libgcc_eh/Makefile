# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	clibs
LIB=		gcc_eh
NO_PIC=
MK_SSP=		no
WARNS?=	2

.ifndef LIBCHERI
.include "Makefile.inc"
.else
LDFLAGS+=	-nodefaultlibs
.PATH:	${.CURDIR}/../libgcc_s
SRCS+= empty.c
.endif

.include <bsd.lib.mk>

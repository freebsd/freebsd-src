# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../mount

PROG=	mksnap_ffs
SRCS=	mksnap_ffs.c getmntopts.c
MAN=	mksnap_ffs.8

CFLAGS+=-I${.CURDIR}/../mount

.if defined(NOSUID)
BINMODE=550
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	operator

.include <bsd.prog.mk>

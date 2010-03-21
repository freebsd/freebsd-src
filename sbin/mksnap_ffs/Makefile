# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.1.34.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	mksnap_ffs
MAN=	mksnap_ffs.8

.if defined(NOSUID)
BINMODE=550
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	operator

.include <bsd.prog.mk>

# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $

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

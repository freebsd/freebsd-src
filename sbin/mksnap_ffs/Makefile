# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.1.32.1 2009/04/15 03:14:26 kensmith Exp $

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

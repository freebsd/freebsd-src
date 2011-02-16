# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.1.36.1 2010/12/21 17:10:29 kensmith Exp $

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

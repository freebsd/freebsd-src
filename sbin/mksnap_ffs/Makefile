# $FreeBSD: src/sbin/mksnap_ffs/Makefile,v 1.1 2003/03/02 01:50:33 mckusick Exp $

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

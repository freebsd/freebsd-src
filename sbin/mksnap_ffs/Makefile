# $FreeBSD$

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

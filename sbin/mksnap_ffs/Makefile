.PATH:	${.CURDIR:H}/mount

PACKAGE=ufs
PROG=	mksnap_ffs
LIBADD=	util
MAN=	mksnap_ffs.8

WARNS?=	2

.if defined(NOSUID)
BINMODE=554
.else
BINMODE=4554
BINOWN=	root
.endif
BINGRP=	operator

.include <bsd.prog.mk>

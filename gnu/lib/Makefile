# $FreeBSD: src/gnu/lib/Makefile,v 1.25 1999/11/27 21:52:54 obrien Exp $

SUBDIR= libdialog libgcc libgcc_r libgmp libmp libregex libreadline libstdc++

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>

# $FreeBSD: src/gnu/lib/Makefile,v 1.25.2.2 2000/10/31 09:44:58 obrien Exp $

SUBDIR= csu libdialog libgcc libgcc_r libgmp libmp libregex libreadline

.if !defined(NO_CXX)
SUBDIR+= libstdc++
.endif

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>

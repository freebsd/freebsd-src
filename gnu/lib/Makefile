# $FreeBSD: src/gnu/lib/Makefile,v 1.39 2002/09/17 01:48:49 peter Exp $

SUBDIR= csu libgcc libdialog libregex libreadline

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if !defined(NO_CXX)
SUBDIR+= libstdc++ libsupc++
.endif

.if !defined(NO_OBJC)
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>

# $FreeBSD: src/gnu/lib/Makefile,v 1.40.20.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR= csu libgcc libgcov libdialog libregex libreadline

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

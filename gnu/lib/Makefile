# $FreeBSD: src/gnu/lib/Makefile,v 1.40 2004/08/14 03:23:24 kan Exp $

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

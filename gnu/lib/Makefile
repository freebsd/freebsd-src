# $FreeBSD: src/gnu/lib/Makefile,v 1.43.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libregex libreadline libssp

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.include <bsd.subdir.mk>

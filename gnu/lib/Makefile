# $FreeBSD: src/gnu/lib/Makefile,v 1.43.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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

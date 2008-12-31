# $FreeBSD: src/gnu/lib/Makefile,v 1.42.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libregex libreadline

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.if ${MK_SSP} != "no"
SUBDIR+= libssp
.endif

.include <bsd.subdir.mk>

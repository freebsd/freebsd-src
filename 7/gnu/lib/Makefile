# $FreeBSD$

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

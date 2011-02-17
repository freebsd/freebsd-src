# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libodialog libregex libreadline \
	libssp

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.include <bsd.subdir.mk>

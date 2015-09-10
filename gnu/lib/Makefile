# $FreeBSD$

.include <src.opts.mk>

SUBDIR= csu libgcc libdialog libregex

.if ${MK_GCC} != "no"
SUBDIR+= libgcov libgomp
.endif

.if ${MK_SSP} != "no"
SUBDIR+= libssp
.endif

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.if ${MK_BINUTILS} != "no" && ${MK_GDB} != "no"
SUBDIR+=	libreadline
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_GNUCXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if defined(LIBCHERI)
.if ${MK_LIBCHERI_ALL} == "no"
SUBDIR:=
.else
# No C++ support
SUBDIR:=	${SUBDIR:Nlibstdc++:Nlibsupc++}

.if ${MK_LIBCHERI_BROKEN} == "no"
# CHERI-Clang #38
SUBDIR:=	${SUBDIR:Nlibgcc}
# Broken and heading for the dustbin of history
SUBDIR:=	${SUBDIR:Nlibgomp}
# Include issue
SUBDIR:=	${SUBDIR:Nlibreadline}
.endif
.endif
.endif

.include <bsd.subdir.mk>

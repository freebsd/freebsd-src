# $FreeBSD$

.include <src.opts.mk>

SUBDIR=			csu
SUBDIR+=		libgcc

SUBDIR.${MK_DIALOG}+=	libdialog
SUBDIR.${MK_GCC}+=	libgcov libgomp
SUBDIR.${MK_SSP}+=	libssp
SUBDIR.${MK_TESTS}+=	tests

.if ${MK_BINUTILS} != "no" && ${MK_GDB} != "no"
SUBDIR+=	libreadline
.endif

.if ${MK_GNU_DIFF} != "no" || ${MK_GNU_GREP} != "no" || \
    ${MK_GNU_GREP_COMPAT} != "no" || ${MK_GDB} != "no"
SUBDIR+=		libregex
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_GNUCXX} != "no"
SUBDIR+= libstdc++ libsupc++
SUBDIR_DEPENDS_libsupc++:= libstdc++
.endif

.if defined(LIBCHERI)
.if ${MK_LIBCHERI_ALL} == "no"
SUBDIR:=
.else
# No C++ support
SUBDIR:=	${SUBDIR:Nlibstdc++:Nlibsupc++}
# Don't use csu for CheriABI
SUBDIR:=	${SUBDIR:Ncsu}

.if ${MK_LIBCHERI_BROKEN} == "no"
# CHERI-Clang #38
SUBDIR:=	${SUBDIR:Nlibgcc}
# Broken and heading for the dustbin of history
SUBDIR.${MK_GCC}:=	${SUBDIR.${MK_GCC}:Nlibgomp}
# Include issue
SUBDIR:=	${SUBDIR:Nlibreadline}
.endif
.endif
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

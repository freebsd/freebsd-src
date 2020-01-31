# $FreeBSD$

.include <src.opts.mk>

SUBDIR=
SUBDIR.${MK_DIALOG}+=	libdialog
SUBDIR.${MK_GCC}+=	libgcov
.if ${MK_GCC} != "no" && ${MK_OPENMP} == "no"
SUBDIR+=		libgomp
.endif
SUBDIR.${MK_TESTS}+=	tests

.if ${MK_GNU_GREP} != "no" || ${MK_GNU_GREP_COMPAT} != "no" || \
    ${MK_GDB} != "no"
SUBDIR+=		libregex
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_GNUCXX} != "no"
SUBDIR+= libstdc++ libsupc++
SUBDIR_DEPENDS_libsupc++:= libstdc++
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

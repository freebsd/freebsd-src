# $FreeBSD$

.include <src.opts.mk>

SUBDIR= diff \
	diff3 \
	grep

SUBDIR_DEPEND_gdb= ${_binutils}

.if ${MK_CXX} != "no"
SUBDIR.${MK_GCC}+=	gperf
SUBDIR.${MK_GROFF}+=	groff
.endif

SUBDIR.${MK_BINUTILS}+=	binutils
SUBDIR.${MK_DIALOG}+=	dialog

.if ${MK_BINUTILS} != "no"
SUBDIR.${MK_GDB}+=	gdb
.endif

SUBDIR.${MK_GCC}+=	cc
SUBDIR.${MK_GPL_DTC}+=	dtc
SUBDIR.${MK_TESTS}+=	tests

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

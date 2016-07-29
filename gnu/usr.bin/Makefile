# $FreeBSD$

.include <src.opts.mk>

SUBDIR= ${_binutils} \
	${_cc} \
	dialog \
	diff \
	diff3 \
	${_dtc} \
	${_gdb} \
	${_gperf} \
	grep \
	${_groff} \
	${_rcs} \
	${_tests}

SUBDIR_DEPEND_gdb= ${_binutils}

.if ${MK_CXX} != "no"
.if ${MK_GCC} != "no"
_gperf=		gperf
.endif
.if ${MK_GROFF} != "no"
_groff=		groff
.endif
.endif

.if ${MK_GPL_DTC} != "no"
_dtc=		dtc
.endif

.if ${MK_RCS} != "no"
_rcs=		rcs
.endif

.if ${MK_TESTS} != "no"
_tests=		tests
.endif

.if ${MK_BINUTILS} != "no"
_binutils=	binutils
.if ${MK_GDB} != "no"
_gdb=		gdb
.endif
.endif

.if ${MK_GCC} != "no"
_cc=		cc
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

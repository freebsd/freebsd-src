# $FreeBSD$

.include <bsd.own.mk>

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
	patch \
	${_rcs} \
	sdiff \
	send-pr \
	${_texinfo}

.if ${MK_CXX} != "no"
_gperf=		gperf
.if ${MK_GROFF} != "no"
_groff=		groff
.endif
.endif

.if ${MK_GPL_DTC} != "no"
_dtc=		dtc
.endif

.if ${MK_INFO} != "no"
_texinfo=	texinfo
.endif

.if ${MK_RCS} != "no"
_rcs=		rcs
.endif

.if ${MK_BINUTILS} != "no"
_binutils=	binutils
.endif
.if ${MK_GCC} != "no"
_cc=		cc
.endif
.if ${MK_GDB} != "no"
_gdb=		gdb
.endif

.include <bsd.subdir.mk>

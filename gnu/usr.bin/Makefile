# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= ${_binutils} \
	${_cc} \
	${_cvs} \
	dialog \
	diff \
	diff3 \
	${_dtc} \
	${_gdb} \
	${_gperf} \
	${_grep} \
	${_groff} \
	patch \
	${_rcs} \
	sdiff \
	send-pr \
	sort \
	${_texinfo}

.if ${MK_CXX} != "no"
_gperf=		gperf
.if ${MK_GROFF} != "no"
_groff=		groff
.endif
.endif

.if ${MK_BSD_GREP} != "yes"
_grep=          grep
.endif

.if ${MK_CVS} != "no"
_cvs=		cvs
.endif

.if ${MK_FDT}	!= "no"
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

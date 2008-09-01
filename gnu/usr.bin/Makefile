# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= bc \
	${_binutils} \
	${_cc} \
	${_cpio} \
	${_cvs} \
	dc \
	dialog \
	diff \
	diff3 \
	${_gdb} \
	${_gperf} \
	${_grep} \
	${_groff} \
	${_man} \
	patch \
	${_rcs} \
	sdiff \
	send-pr \
	sort \
	${_texinfo}

.if ${MACHINE_ARCH} == "mips"
MK_GDB=no	# not yet
.endif

.if ${MK_CXX} != "no"
_gperf=		gperf
.if ${MK_GROFF} != "no"
_groff=		groff
.endif
.endif

.if ${MK_GNU_CPIO} == "yes"
_cpio=		cpio
.endif

.if ${MK_CVS} != "no"
_cvs=		cvs
.endif

.if ${MK_GNU_GREP} != "no"
_grep=		grep
.endif

.if ${MK_MAN} != "no"
_man=		man
.endif

.if ${MK_INFO} != "no"
_texinfo=	texinfo
.endif

.if ${MK_TOOLCHAIN} != "no"
_binutils=	binutils
_cc=		cc
.if ${MK_GDB} != "no"
_gdb=		gdb
.endif
.endif
.if ${MK_RCS} != "no"
_rcs=		rcs
.endif

.include <bsd.subdir.mk>

# $FreeBSD$

SUBDIR= bc \
	${_binutils} \
	${_cc} \
	cpio \
	${_cvs} \
	dc \
	dialog \
	diff \
	diff3 \
	${_gdb} \
	${_gperf} \
	grep \
	${_groff} \
	gzip \
	${_man} \
	patch \
	${_rcs} \
	sdiff \
	send-pr \
	sort \
	${_texinfo}
 
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
NO_GDB=	# not yet
.endif

.if !defined(NO_CXX)
.if ${MACHINE_ARCH} != "powerpc"
_gperf=		gperf
.endif
.if !defined(NO_GROFF)
_groff=		groff
.endif
.endif

.if !defined(NO_CVS)
_cvs=		cvs
.endif

.if !defined(NO_MAN)
_man=		man
.endif

.if !defined(NO_INFO)
_texinfo=	texinfo
.endif

.if !defined(NO_TOOLCHAIN)
_binutils=	binutils
_cc=		cc
.if !defined(NO_GDB)
_gdb=		gdb
.endif
.endif
.if !defined(NO_RCS)
_rcs=		rcs
.endif

.include <bsd.subdir.mk>

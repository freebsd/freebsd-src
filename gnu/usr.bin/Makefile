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
	man \
	patch \
	rcs \
	sdiff \
	send-pr \
	sort \
	texinfo
 
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
NO_GDB=	# not yet
.endif

.if !defined(NO_CXX)
.if ${MACHINE_ARCH} != "powerpc"
_gperf=		gperf
.endif
_groff=		groff
.endif

.if !defined(NO_CVS)
_cvs=		cvs
.endif

.if !defined(NO_TOOLCHAIN)
_binutils=	binutils
_cc=		cc
.if !defined(NO_GDB)
_gdb=		gdb
.endif
.endif

.include <bsd.subdir.mk>

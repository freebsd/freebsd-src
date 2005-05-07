# $FreeBSD: src/gnu/usr.bin/Makefile,v 1.82.2.1 2005/02/13 07:23:06 obrien Exp $

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
	tar \
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

# $FreeBSD$

SUBDIR= bc ${_binutils} ${_cc} cpio dc dialog diff diff3 \
	${_gperf} grep gzip man patch rcs sdiff send-pr sort tar texinfo

.if !defined(NO_TOOLCHAIN)
_cc=cc
_binutils=binutils
.endif

.if !defined(NO_CXX)
.if ${MACHINE_ARCH} != "powerpc"
_gperf=gperf
.endif
SUBDIR+=groff
.endif

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.include <bsd.subdir.mk>

# $FreeBSD$

SUBDIR= bc ${_binutils} ${_cc} cpio dc dialog diff diff3 ${_gdb} \
	${_gperf} grep gzip man patch rcs sdiff send-pr sort tar texinfo

.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
NO_GDB=	not yet
.endif

.if !defined(NO_TOOLCHAIN)
_cc=cc
_binutils=binutils
.if !defined(NO_GDB)
_gdb=gdb
.endif
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

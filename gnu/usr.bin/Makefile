# $FreeBSD$

SUBDIR= awk bc binutils cpio dc dialog diff diff3 \
	grep gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if defined(WANT_AOUT)
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif
.endif

.if ${MACHINE_ARCH} != "ia64" && ${MACHINE_ARCH} != "sparc64"
SUBDIR+=cc gperf
.endif

.if !defined(NO_CXX)
SUBDIR+=groff
.endif

.if !defined(NOPERL) && !defined(NO_PERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

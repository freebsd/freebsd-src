# $FreeBSD$

SUBDIR= awk bc binutils cpio dc dialog diff diff3 gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if defined(WANT_AOUT)
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif
.endif

.if ${MACHINE_ARCH} != "ia64" && ${MACHINE_ARCH} != "sparc64"
SUBDIR+=cc
.endif

.if !defined(NOPERL) && !defined(NO_PERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

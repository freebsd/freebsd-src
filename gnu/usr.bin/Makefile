# $FreeBSD$

SUBDIR= bc binutils cc cpio dc dialog diff diff3 gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if defined(WANT_AOUT)
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

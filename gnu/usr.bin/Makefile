# $FreeBSD$

SUBDIR= bc binutils cc cpio dc dialog diff diff3 \
	gperf grep groff gzip man patch rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if defined(WANT_AOUT)
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif
.endif

.include <bsd.subdir.mk>

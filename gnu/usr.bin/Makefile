# $FreeBSD$

.if ${MACHINE_ARCH} != "powerpc"
_gperf=gperf
.endif

SUBDIR= bc binutils cc cpio dc dialog diff diff3 \
	${_gperf} grep groff gzip man patch rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.include <bsd.subdir.mk>

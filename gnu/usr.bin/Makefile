# $FreeBSD$

SUBDIR= awk bc binutils cc cpio dc dialog diff diff3 \
	grep gzip man patch rcs sdiff send-pr sort tar texinfo

.if ${CXX:T} != "c++"
SUBDIR+=groff
.endif

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if defined(WANT_AOUT)
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif
.endif

.include <bsd.subdir.mk>

# $FreeBSD$

SUBDIR= awk bc binutils bison cc cpio dc dialog diff diff3 genclass gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.45 1999/04/05 08:48:08 obrien Exp $

SUBDIR= awk bc binutils bison cc cpio cvs dc dialog diff diff3 gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.36 1997/04/12 07:19:09 peter Exp $

SUBDIR= bc bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep gzip man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR=as awk gdb groff ld

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif
.endif

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.37 1998/02/19 09:34:59 jb Exp $

SUBDIR= bc bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep gzip man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as awk gdb groff ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

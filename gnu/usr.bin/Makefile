#	$Id: Makefile,v 1.38 1998/02/20 16:14:37 jkh Exp $

SUBDIR= bc bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep gzip man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if ${BINFORMAT} == "elf"
SUBDIR+= binutils
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as awk gdb groff ld

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif
.endif

.include <bsd.subdir.mk>

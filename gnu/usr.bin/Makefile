#	$Id: Makefile,v 1.39 1998/03/13 19:34:40 jb Exp $

SUBDIR= awk bc bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if ${BINFORMAT} == "elf"
SUBDIR+= binutils
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as gdb ld

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif
.endif

.include <bsd.subdir.mk>

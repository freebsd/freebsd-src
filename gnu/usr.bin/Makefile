#	$Id: Makefile,v 1.40 1998/05/12 21:07:45 jb Exp $

SUBDIR= awk bc bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if ${BINFORMAT} == "elf"
SUBDIR+= binutils
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as gdb ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

#	$Id: Makefile,v 1.41 1998/05/12 23:09:36 jb Exp $

SUBDIR= awk bc binutils bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as gdb ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

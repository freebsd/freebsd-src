#	$Id: Makefile,v 1.44 1998/09/09 19:30:45 markm Exp $

SUBDIR= awk bc binutils bison cc cpio cvs dc dialog diff diff3 gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as gdb ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

#	$Id$

SUBDIR= awk bc binutils bison cc cpio cvs dc dialog diff diff3 genclass gperf \
	grep groff gzip man patch ptx rcs sdiff send-pr sort tar texinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=as ld
.endif

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

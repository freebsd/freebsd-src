#	$Id: Makefile,v 1.33 1996/10/04 08:45:34 peter Exp $

SUBDIR= as awk bc bison cc cpio cvs dc dialog diff diff3 gdb genclass gperf \
	grep groff gzip ld man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

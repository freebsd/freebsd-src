#	$Id: Makefile,v 1.33.2.1 1998/02/15 15:18:36 jkh Exp $

SUBDIR= as awk bc bison cc cpio cvs dc dialog diff diff3 gdb genclass gperf \
	grep groff gzip ld man patch ptx rcs sdiff send-pr \
	sort tar texinfo

.if !defined(NOPERL) && exists(${.CURDIR}/perl)
SUBDIR+=perl
.endif

.include <bsd.subdir.mk>

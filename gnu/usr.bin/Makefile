# $FreeBSD: src/gnu/usr.bin/Makefile,v 1.72 2002/09/17 01:43:21 obrien Exp $

SUBDIR= bc binutils cc cpio dc dialog diff diff3 \
	gperf grep groff gzip man patch rcs sdiff send-pr sort tar texinfo

.if !defined(NO_CVS)
SUBDIR+=cvs
.endif

.include <bsd.subdir.mk>

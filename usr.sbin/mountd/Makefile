#	From: @(#)Makefile	8.3 (Berkeley) 1/25/94
#	$Id: Makefile,v 1.6 1997/02/22 14:33:01 peter Exp $

PROG=	mountd
CFLAGS+= -D_NEW_VFSCONF
CFLAGS+=-DNFS -DMFS -DCD9660 -DMSDOSFS
MAN5=	exports.5 netgroup.5
MAN8=	mountd.8

.include <bsd.prog.mk>

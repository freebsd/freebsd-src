#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.5 1996/09/22 21:54:03 wosch Exp $

PROG=	mail.local
MAN8=	mail.local.8
.if defined(DONT_FSYNC)
CFLAGS+= -DDONT_FSYNC
.endif
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

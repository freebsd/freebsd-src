#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.9 1998/09/19 22:42:02 obrien Exp $

.PATH:	${.CURDIR}/../../contrib/sendmail/mail.local \
	${.CURDIR}/../../contrib/sendmail/src

PROG=	mail.local
SRCS=	mail.local.c snprintf.c
MAN8=	mail.local.8
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

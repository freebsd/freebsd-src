#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.8 1998/08/04 15:30:17 peter Exp $

.PATH:	${.CURDIR}/../../contrib/sendmail/mail.local \
	${.CURDIR}/../../contrib/sendmail/src

PROG=	mail.local
SRCS=	mail.local.c snprintf.c
MAN8=	mail.local.8
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

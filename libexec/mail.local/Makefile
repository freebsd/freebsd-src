#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/libexec/mail.local/Makefile,v 1.9.2.1 1999/08/29 15:03:25 peter Exp $

.PATH:	${.CURDIR}/../../contrib/sendmail/mail.local \
	${.CURDIR}/../../contrib/sendmail/src

PROG=	mail.local
SRCS=	mail.local.c snprintf.c
MAN8=	mail.local.8
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

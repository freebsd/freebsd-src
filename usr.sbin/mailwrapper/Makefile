#	$NetBSD: Makefile,v 1.4 1999/03/25 16:40:18 is Exp $
# $FreeBSD$

PROG=	mailwrapper
MAN8=	mailwrapper.8

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

.include <bsd.prog.mk>

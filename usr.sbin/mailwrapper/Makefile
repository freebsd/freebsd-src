#	$NetBSD: Makefile,v 1.4 1999/03/25 16:40:18 is Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8 mailer.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	/usr/sbin/mailwrapper /usr/sbin/sendmail  \
		/usr/sbin/mailwrapper /usr/bin/newaliases \
		/usr/sbin/mailwrapper /usr/bin/mailq

.include <bsd.prog.mk>

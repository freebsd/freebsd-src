# $FreeBSD: src/usr.sbin/mailwrapper/Makefile,v 1.4 1999/12/29 18:51:36 peter Exp $

PROG=	mailwrapper
MAN8=	mailwrapper.8

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

afterinstall:
	@if [ ! -f ${DESTDIR}/etc/mail/mailer.conf ]; then \
		${INSTALL} -c -o root -g wheel -m 644 \
		    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail; \
	fi

.include <bsd.prog.mk>

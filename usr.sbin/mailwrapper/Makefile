# $FreeBSD$

PROG=	mailwrapper
MAN8=	mailwrapper.8

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

afterinstall:
	@if [ ! -f /etc/mail/mailer.conf ]; then \
		${INSTALL} -c -o root -g wheel -m 644 \
		    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail; \
	fi

.include <bsd.prog.mk>

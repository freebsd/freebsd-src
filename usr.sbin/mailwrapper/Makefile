# $FreeBSD: src/usr.sbin/mailwrapper/Makefile,v 1.4.2.1 2000/07/01 11:05:40 ps Exp $

PROG=	mailwrapper
MAN8=	mailwrapper.8

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

afterinstall:
.if !exists(${DESTDIR}/etc/mail/mailer.conf)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail
.endif

.include <bsd.prog.mk>

# $FreeBSD: src/usr.sbin/mailwrapper/Makefile,v 1.4.2.3 2002/04/09 02:00:58 gshapiro Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/sbin/hoststat  \
		${BINDIR}/mailwrapper /usr/sbin/purgestat \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

afterinstall:
.if !exists(${DESTDIR}/etc/mail/mailer.conf)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail
.endif

.include <bsd.prog.mk>

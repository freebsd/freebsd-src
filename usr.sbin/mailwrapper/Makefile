# $FreeBSD$

.if !defined(NO_MAILWRAPPER)
PROG=	mailwrapper
MAN=	mailwrapper.8

DPADD=	${LIBUTIL}
LDADD=	-lutil
.endif

.if !defined(NO_MAILWRAPPER) || !defined(NO_SENDMAIL)
SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/sbin/hoststat  \
		${BINDIR}/mailwrapper /usr/sbin/purgestat \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

.if defined(NO_MAILWRAPPER) && !defined(NO_SENDMAIL)
SYMLINKS+=	/usr/libexec/sendmail/sendmail ${BINDIR}/mailwrapper
.endif
.endif

afterinstall:
.if !defined(NO_MAILWRAPPER)
.if !exists(${DESTDIR}/etc/mail/mailer.conf)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail
.endif
.endif

.include <bsd.prog.mk>

# $FreeBSD$

.if !defined(NO_MAILWRAPPER)
PROG=	mailwrapper
MAN=	mailwrapper.8

DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	6
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

.if !defined(NO_MAILWRAPPER)
.if !exists(${DESTDIR}/etc/mail/mailer.conf)
FILES=		${.CURDIR}/../../etc/mail/mailer.conf
FILESDIR=	/etc/mail
FILESMODE=	644
.endif
.endif

.include <bsd.prog.mk>

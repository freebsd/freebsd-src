
.include <src.opts.mk>

.if ${MK_MAILWRAPPER} != "no"
PROG=	mailwrapper
MAN=	mailwrapper.8

LIBADD=	util
.endif

.if ${MK_MAILWRAPPER} != "no" || ${MK_SENDMAIL} != "no" || ${MK_DMAGENT} != "no"
SYMLINKS=	../sbin/mailwrapper /usr/bin/mailq \
		../sbin/mailwrapper /usr/bin/newaliases \
		mailwrapper /usr/sbin/hoststat \
		mailwrapper /usr/sbin/purgestat \
		mailwrapper /usr/sbin/sendmail

.if ${MK_MAILWRAPPER} == "no" && ${MK_DMAGENT} != "no"
SYMLINKS+=	../libexec/dma ${BINDIR}/mailwrapper
.elif ${MK_MAILWRAPPER} == "no" && ${MK_DMAGENT} == "no" && ${MK_SENDMAIL} != "no"
SYMLINKS+=	../libexec/sendmail/sendmail ${BINDIR}/mailwrapper
.endif
.endif

.if ${MK_MAILWRAPPER} != "no" && ${MK_SENDMAIL} == "no"
SYMLINKS+=	..${BINDIR}/mailwrapper /bin/rmail
.endif

.if ${MK_SENDMAIL} != "no"
FILES=		${SRCTOP}/etc/mail/mailer.conf
FILESDIR=	${SHAREDIR}/examples/sendmail
FILESPACKAGE=	sendmail
.endif

.if ${MK_MAILWRAPPER} != "no"
# We install here if either dma(8) is enabled, or sendmail(8) isn't.  In the
# latter scenario, we take care of the possibility that neither sendmail(8) nor
# dma(8) are installed and simply provide a default that can be changed for an
# alternative in ports.
.if ${MK_DMAGENT} != "no" || ${MK_SENDMAIL} == "no"
CONFS=		${SRCTOP}/libexec/dma/dmagent/mailer.conf
.else
CONFS=		${SRCTOP}/etc/mail/mailer.conf
.endif
CONFSDIR=	/etc/mail
CONFSMODE=	644
.endif

.include <bsd.prog.mk>

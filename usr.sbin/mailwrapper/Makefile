# $FreeBSD$

.include <src.opts.mk>

.if ${MK_MAILWRAPPER} != "no"
PROG=	mailwrapper
MAN=	mailwrapper.8

LIBADD=	util
.endif

.if ${MK_MAILWRAPPER} != "no" || ${MK_SENDMAIL} != "no"
SYMLINKS=	../sbin/mailwrapper /usr/bin/mailq \
		../sbin/mailwrapper /usr/bin/newaliases \
		mailwrapper /usr/sbin/hoststat \
		mailwrapper /usr/sbin/purgestat \
		mailwrapper /usr/sbin/sendmail

.if ${MK_MAILWRAPPER} == "no" && ${MK_SENDMAIL} != "no"
SYMLINKS+=	../libexec/sendmail/sendmail ${BINDIR}/mailwrapper
.endif
.endif

.if ${MK_MAILWRAPPER} != "no" && ${MK_SENDMAIL} == "no"
SYMLINKS+=	..${BINDIR}/mailwrapper /bin/rmail
.endif

.if ${MK_MAILWRAPPER} != "no"
# We install here if either sendmail(8) is enabled, or dma(8) isn't.  In the
# latter scenario, we take care of the possibility that neither sendmail(8) nor
# dma(8) are installed and simply provide a default that can be changed for an
# alternative in ports.
.if ${MK_SENDMAIL} != "no" || ${MK_DMAGENT} == "no"
CONFS=		${SRCTOP}/etc/mail/mailer.conf
CONFSDIR=	/etc/mail
CONFSMODE=	644
.endif
.endif

.include <bsd.prog.mk>


.include <src.opts.mk>

PACKAGE=	newsyslog
CONFSDIR=	/etc/newsyslog.conf.d

CONFGROUPS=	CONFS
CONFS=

.if ${MK_FTP} != "no"
CONFGROUPS+=	FTP
FTP+=		ftp.conf
FTPPACKAGE=	ftpd
FTPDIR=		/etc/newsyslog.conf.d
.endif

.if ${MK_LPR} != "no"
CONFGROUPS+=	LP
LP+=		lpr.conf
LPPACKAGE=	lp
LPDIR=		/etc/newsyslog.conf.d
.endif

.if ${MK_OFED} != "no"
CONFS+=		opensm.conf
.endif

.if ${MK_PF} != "no"
CONFGROUPS+=	PF
PF+=		pf.conf
PFPACKAGE=	pf
PFDIR=		/etc/newsyslog.conf.d
.endif

.if ${MK_PPP} != "no"
CONFGROUPS+=	PPP
PPP+=		ppp.conf
PPPPACKAGE=	ppp
PPPDIR=		/etc/newsyslog.conf.d
.endif

.if ${MK_SENDMAIL} != "no"
CONFGROUPS+=		SENDMAIL
SENDMAIL+=		sendmail.conf
SENDMAILPACKAGE=	sendmail
SENDMAILDIR=		/etc/newsyslog.conf.d
.endif

.include <bsd.prog.mk>

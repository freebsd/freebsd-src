.include <src.opts.mk>

PACKAGE=	runtime

NO_OBJ=

CONFGROUPS=	CONFS
CONFS=	README \
	imap \
	login \
	other \
	passwd pop3 \
	sshd su system \
	xdm

CONFDIR=	/etc/pam.d
CONFSMODE_README=	444

CONFGROUPS+=	CRON
CRON+=		cron
CRONPACKAGE=	cron

.if ${MK_AT} != "no"
CONFGROUPS+=	AT
AT+=		atrun
ATPACKAGE+=	at
.endif

.if ${MK_FTP} != "no"
CONFGROUPS+=	FTP
FTP+=		ftp ftpd
# Do not put these in the ftp package, since ports also use them.
FTPPACKAGE=	runtime
.endif

.include <bsd.prog.mk>

# $FreeBSD$

.include <src.opts.mk>

NO_OBJ=

FILESGROUPS=	FILES

FILES=	README \
	cron \
	imap \
	login \
	other \
	passwd pop3 \
	sshd su system \
	xdm

FILESDIR=	/etc/pam.d
FILESMODE=	644

.if ${MK_AT} != "no"
FILESGROUPS+=	AT
AT+=	atrun
ATPACKAGE+=	at
ATDIR=		${FILESDIR}
ATMODE=		${FILESMODE}
.endif

.if ${MK_FTP} != "no"
FILESGROUPS+=	FTP
FTP+=	ftpd
FTPPACKAGE+=	ftp
FTPDIR=		${FILESDIR}
FTPMODE=	${FILESMODE}
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp
.endif

.if ${MK_TELNET} != "no"
FILESGROUPS+=	TELNET
TELNET+=	telnetd
TELNETPACKAGE+=	telnet
TELNETDIR=	${FILESDIR}
TELNETMODE=	${FILESMODE}
.endif

FILESMODE_README=	444

.include <bsd.prog.mk>

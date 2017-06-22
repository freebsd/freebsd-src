# $FreeBSD$

.include <bsd.own.mk>

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
ATDIR=		${FILESDIR}
ATMODE=		${FILESMODE}
.endif

.if ${MK_FTP} != "no"
FILESGROUPS+=	FTP
FTP+=	ftpd
FTPDIR=		${FILESDIR}
FTPMODE=	${FILESMODE}
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp
.endif

.if ${MK_RCMDS} != "no"
FILESGROUPS+=	RCMDS
RCMDS+=		rsh
RCMDSDIR=	${FILESDIR}
RCMDSMODE=	${FILESMODE}
.endif

.if ${MK_TELNET} != "no"
FILESGROUPS+=	TELNET
TELNET+=	telnetd
TELNETDIR=	${FILESDIR}
TELNETMODE=	${FILESMODE}
.endif

FILESMODE_README=	444

.include <bsd.prog.mk>

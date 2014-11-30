# $FreeBSD$

NO_OBJ=

.include <src.opts.mk>

FILES=	README \
	cron \
	login \
	other \
	passwd \
	su \
	system \
	xdm

LINKS=

.if ${MK_AT} != "no"
FILES+=	atrun
.endif

.if ${MK_FTP} != "no"
FILES+=	ftpd
LINKS+=	${FILESDIR}/ftpd ${FILESDIR}/ftp
.endif

.if ${MK_MAIL} != "no"
FILES+=	imap
FILES+=	pop3
.endif

.if ${MK_OPENSSH} != "no"
FILES+=	sshd
.endif

.if ${MK_RCMDS} != "no"
FILES+=	rsh
.endif

.if ${MK_TELNET} != "no"
FILES+=	telnetd
.endif

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444

.include <bsd.prog.mk>

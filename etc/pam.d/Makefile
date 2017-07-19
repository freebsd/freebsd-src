# $FreeBSD$

.include <bsd.own.mk>

NO_OBJ=

FILES=	README \
	cron \
	imap \
	login \
	other \
	passwd pop3 \
	rsh \
	sshd su system \
	xdm

.if ${MK_AT} != "no"
FILES+=	atrun
.endif

.if ${MK_FTP} != "no"
FILES+=	ftpd
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp
.endif

.if ${MK_TELNET} != "no"
FILES+=	telnetd
.endif

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444

.include <bsd.prog.mk>

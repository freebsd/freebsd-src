# $FreeBSD$

NO_OBJ=

.include <src.opts.mk>

FILES=	README \
	atrun \
	cron \
	ftpd \
	imap \
	login \
	other \
	passwd pop3 \
	sshd su system \
	xdm

.if ${MK_RCMDS} != "no"
FILES+=	rsh
.endif

.if ${MK_TELNET} != "no"
FILES+=	telnetd
.endif

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

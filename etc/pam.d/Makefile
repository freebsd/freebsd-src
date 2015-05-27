# $FreeBSD$

.include <src.opts.mk>

NO_OBJ=

FILES=	README \
	cron \
	ftpd \
	imap \
	login \
	other \
	passwd pop3 \
	rsh \
	sshd su system \
	telnetd \
	xdm

.if ${MK_AT} != "no"
FILES+=	atrun
.endif

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

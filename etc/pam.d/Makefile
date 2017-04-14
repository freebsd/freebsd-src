# $FreeBSD$

.include <src.opts.mk>

NO_OBJ=

FILESGROUPS=	FILES

FILES=	README \
	cron \
	ftpd \
	imap \
	login \
	other \
	passwd pop3 \
	sshd su system \
	telnetd \
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

.if ${MK_RCMDS} != "no"
FILESGROUPS+=	RCMDS
RCMDS+=		rsh
RCMDSPACKAGE+=	rcmds
RCMDSDIR=	${FILESDIR}
RCMDSMODE=	${FILESMODE}
.endif

FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

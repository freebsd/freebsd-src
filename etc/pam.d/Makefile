# $FreeBSD: src/etc/pam.d/Makefile,v 1.17.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

NO_OBJ=

FILES=	README \
	atrun \
	cron \
	ftpd \
	imap \
	kde \
	login \
	other \
	passwd pop3 \
	rsh \
	sshd su system \
	telnetd \
	xdm

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

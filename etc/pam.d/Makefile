# $FreeBSD: src/etc/pam.d/Makefile,v 1.16.6.1 2008/11/25 02:59:29 kensmith Exp $

NO_OBJ=

FILES=	README \
	atrun \
	cron \
	ftpd \
	gdm \
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

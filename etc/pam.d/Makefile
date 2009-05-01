# $FreeBSD: src/etc/pam.d/Makefile,v 1.16.8.1 2009/04/15 03:14:26 kensmith Exp $

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

# $FreeBSD: src/etc/pam.d/Makefile,v 1.16.12.1 2010/12/21 17:10:29 kensmith Exp $

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

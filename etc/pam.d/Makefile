# $FreeBSD: src/etc/pam.d/Makefile,v 1.13.12.1 2008/10/02 02:57:24 kensmith Exp $

NO_OBJ=
FILES=	README ftpd gdm imap kde login other passwd pop3 \
	rsh sshd su system telnetd xdm
FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

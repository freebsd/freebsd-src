# $FreeBSD: src/etc/pam.d/Makefile,v 1.10 2003/06/14 12:35:05 des Exp $

NOOBJ=	noobj
FILES=	README ftpd gdm imap kde login other passwd pop3 \
	rexecd rsh sshd su system telnetd xdm
FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

# $FreeBSD: src/etc/pam.d/Makefile,v 1.9 2003/03/14 09:01:22 ru Exp $

NOOBJ=	noobj
FILES=	README ftpd gdm imap kde login other passwd pop3 \
	rexecd rsh sshd su telnetd xdm
FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>

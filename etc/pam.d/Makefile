# $FreeBSD$

NOOBJ=	noobj
FILES=	README ftpd gdm imap kde login other passwd pop3 \
	rexecd rsh sshd su telnetd xdm
FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444

afterinstall:
	ln -fs ftpd ${DESTDIR}${FILESDIR}/ftp

.include <bsd.prog.mk>

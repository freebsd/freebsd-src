#	$OpenBSD: Makefile,v 1.5 1999/10/25 20:27:26 markus Exp $

.include <bsd.own.mk>

SUBDIR=	lib ssh sshd ssh-add ssh-keygen ssh-agent scp

distribution:
	install -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh_config
	install -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/sshd_config

.include <bsd.subdir.mk>

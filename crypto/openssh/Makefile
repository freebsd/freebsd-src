#	$OpenBSD: Makefile,v 1.8 2001/02/04 11:11:53 djm Exp $

.include <bsd.own.mk>

SUBDIR=	lib ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keyscan sftp

distribution:
	install -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh_config
	install -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/sshd_config

.include <bsd.subdir.mk>

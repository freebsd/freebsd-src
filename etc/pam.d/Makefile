# $FreeBSD$

PAMD  =
PAMD +=	README
PAMD +=	csshd
PAMD +=	ftp
PAMD +=	ftpd
PAMD +=	gdm
PAMD +=	imap
PAMD +=	kde
PAMD +=	login
PAMD +=	other
PAMD +=	passwd
PAMD +=	pop3
PAMD +=	rsh
PAMD +=	sshd
PAMD +=	su
PAMD +=	telnetd
PAMD +=	xdm
PAMD +=	xserver

install:
	(cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${PAMD} \
		${DESTDIR}/etc/pam.d)

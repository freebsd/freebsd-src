#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SUBDIR=	atrun \
	bootpd \
	comsat \
	fingerd \
	ftpd \
	getNAME \
	getty \
	makekey \
	mknetid \
	pppoed \
	pt_chown \
	rbootd \
	revnetgroup \
	rexecd \
	rlogind \
	rpc.rquotad \
	rpc.rstatd \
	rpc.rusersd \
	rpc.rwalld \
	rpc.sprayd \
	rshd \
	save-entropy \
	talkd \
	tcpd \
	telnetd \
	tftpd \
	ypxfr

.if !defined(NOPIC)
SUBDIR+=rtld-elf
.endif

.if !defined(NO_BIND)
SUBDIR+=named-xfer
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if defined(WANT_LUKEMFTPD)
SUBDIR+=lukemftpd
.endif

.include <bsd.subdir.mk>

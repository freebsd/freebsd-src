#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SUBDIR=	atrun \
	bootpd \
	comsat \
	fingerd \
	ftpd \
	${_ftp-proxy} \
	getty \
	${_lukemftpd} \
	${_mail.local} \
	makekey \
	${_mknetid} \
	pppoed \
	pt_chown \
	rbootd \
	revnetgroup \
	${_rexecd} \
	${_rlogind} \
	rpc.rquotad \
	rpc.rstatd \
	rpc.rusersd \
	rpc.rwalld \
	rpc.sprayd \
	${_rshd} \
	${_rtld-elf} \
	save-entropy \
	${_smrsh} \
	talkd \
	tcpd \
	telnetd \
	tftpd \
	${_ypxfr}

.if !defined(NO_NIS)
_mknetid=	mknetid
_ypxfr=		ypxfr
.endif

.if !defined(NO_PF)
_ftp-proxy=	ftp-proxy
.endif

.if !defined(NO_PIC)
_rtld-elf=	rtld-elf
.endif

.if !defined(NO_RCMDS)
_rexecd=	rexecd
_rlogind=	rlogind
_rshd=		rshd
.endif

.if !defined(NO_SENDMAIL)
_mail.local=	mail.local
_smrsh=	smrsh
.endif

.if defined(WANT_LUKEMFTPD)
_lukemftpd=	lukemftpd
.endif

.include <bsd.subdir.mk>

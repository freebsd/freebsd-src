#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	atrun \
	bootpd \
	comsat \
	fingerd \
	ftpd \
	getty \
	lukemftpd \
	${_mail.local} \
	makekey \
	${_mknetid} \
	pppoed \
	pt_chown \
	rbootd \
	revnetgroup \
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
	${_tftp-proxy} \
	${_ypxfr}

.if ${MK_NIS} != "no"
_mknetid=	mknetid
_ypxfr=		ypxfr
.endif

.if ${MK_PF} != "no"
_tftp-proxy=	tftp-proxy
.endif

.if !defined(NO_PIC)
_rtld-elf=	rtld-elf
.endif

.if ${MK_RCMDS} != "no"
_rlogind=	rlogind
_rshd=		rshd
.endif

.if ${MK_SENDMAIL} != "no"
_mail.local=	mail.local
_smrsh=	smrsh
.endif

.include <bsd.subdir.mk>

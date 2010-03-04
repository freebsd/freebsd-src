#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_atrun} \
	bootpd \
	${_comsat} \
	fingerd \
	ftpd \
	getty \
	${_mail.local} \
	${_mknetid} \
	${_pppoed} \
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
	${_telnetd} \
	tftpd \
	${_tftp-proxy} \
	ulog-helper \
	${_ypxfr}

.if ${MK_AT} != "no"
_atrun=		atrun
.endif

.if ${MK_MAIL} != "no"
_comsat=	comsat
.endif

.if ${MK_NIS} != "no"
_mknetid=	mknetid
_ypxfr=		ypxfr
.endif

.if ${MK_NETGRAPH} != "no"
_pppoed=	pppoed
.endif

.if ${MK_PF} != "no"
_tftp-proxy=	tftp-proxy
.endif

.if !defined(NO_PIC) && !defined(NO_RTLD)
_rtld-elf=	rtld-elf
.endif

.if ${MK_RCMDS} != "no"
_rlogind=	rlogind
_rshd=		rshd
.endif

.if ${MK_SENDMAIL} != "no"
_mail.local=	mail.local
_smrsh=		smrsh
.endif

.if ${MK_TELNET} != "no"
_telnetd=	telnetd
.endif

.include <bsd.subdir.mk>

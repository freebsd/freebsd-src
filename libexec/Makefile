#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/Makefile,v 1.42.2.7 2004/12/13 14:10:30 marius Exp $

# Present but disabled: kpasswdd
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
	talkd \
	tcpd \
	tftpd \
	xtend \
	ypxfr

.if !defined(NO_BIND)
SUBDIR+=named-xfer
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if !defined(NOUUCP)
SUBDIR+=uucpd
.endif

.if ${OBJFORMAT} == aout
SUBDIR+=rtld-aout
.endif

.if ${OBJFORMAT} == elf
SUBDIR+=rtld-elf
.endif

.if defined(RELEASEDIR) || \
    !exists(${.CURDIR}/../crypto) || defined(NOCRYPT) || defined(NO_OPENSSL)
# make release needs both
SUBDIR+=telnetd
.endif

.if defined(WANT_LUKEMFTPD)
SUBDIR+=lukemftpd
.endif

.include <bsd.subdir.mk>

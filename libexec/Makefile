#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# Present but disabled: kpasswdd
SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty \
	makekey mknetid named-xfer rbootd revnetgroup rexecd \
	rlogind rpc.rquotad rpc.rstatd rpc.rusersd rpc.rwalld \
	rpc.sprayd rshd talkd tftpd uucpd xtend ypxfr

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if ${OBJFORMAT} == aout
SUBDIR+=rtld-aout
.endif

.if ${OBJFORMAT} == elf
SUBDIR+=rtld-elf
.endif

.if defined(RELEASEDIR) || (!exists(${.CURDIR}/../secure) && !exists(${.CURDIR}/../kerberosIV)) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# make release needs both
SUBDIR+=telnetd
.endif

.include <bsd.subdir.mk>

#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.35 1998/09/04 22:55:17 jb Exp $

SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty \
	makekey mknetid revnetgroup rexecd rlogind \
	rpc.rquotad rpc.rstatd \
	rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd \
	xtend ypxfr

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if ${MACHINE_ARCH} == i386
SUBDIR+=named-xfer
.if ${OBJFORMAT} == aout
SUBDIR+=rtld-aout
.endif
.endif

.if ${OBJFORMAT} == elf
SUBDIR+=rtld-elf
.endif

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# make release needs both
SUBDIR+=telnetd
.endif

# Present but disabled: kpasswdd

.if	${MACHINE_ARCH} == "i386"
SUBDIR+=rbootd
.endif

.include <bsd.subdir.mk>

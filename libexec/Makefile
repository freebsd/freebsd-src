#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.33 1998/08/04 15:32:43 peter Exp $

SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty \
	makekey mknetid revnetgroup rexecd rlogind \
	rpc.rquotad rpc.rstatd \
	rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd \
	xtend ypxfr

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if ${MACHINE} == i386
SUBDIR+=named-xfer
.if ${OBJFORMAT} == aout
SUBDIR+=rtld-aout
.endif
.if ${OBJFORMAT} == elf
SUBDIR+=rtld-elf
.endif
.endif

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# make release needs both
SUBDIR+=telnetd
.endif

# Present but disabled: kpasswdd

.if	${MACHINE} == "hp300"
SUBDIR+=rbootd
.elif	${MACHINE} == "i386"
SUBDIR+=rbootd
.elif	${MACHINE} == "luna68k"
SUBDIR+=rbootd
.endif

.include <bsd.subdir.mk>

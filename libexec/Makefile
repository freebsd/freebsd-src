#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.32 1998/06/12 19:45:14 peter Exp $

SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty \
	makekey mknetid revnetgroup rexecd rlogind \
	rpc.rquotad \
	rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd \
	xtend ypxfr

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if ${MACHINE} == i386
SUBDIR+=named-xfer rpc.rstatd
.if ${BINFORMAT} == aout
SUBDIR+=rtld-aout
.endif
.if ${BINFORMAT} == elf
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

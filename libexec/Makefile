#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.20 1996/09/22 21:51:36 wosch Exp $

SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty lfs_cleanerd \
	mail.local makekey mknetid named-xfer revnetgroup rexecd rlogind \
	rpc.rquotad \
	rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd \
	xtend ypxfr

.if !exists(../eBones) || defined(NOSECURE)
SUBDIR+=telnetd
.else
.if defined(RELEASEDIR)
# make release needs both
SUBDIR+=telnetd
.endif
SUBDIR+= ../eBones/libexec/telnetd
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

#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id$

SUBDIR=	atrun bootpd comsat fingerd ftpd getNAME getty lfs_cleanerd \
	mail.local makekey mknetid named-xfer revnetgroup rexecd rlogind \
	rpc.rquotad \
	rpc.rstatd rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd \
	xtend ypxfr

.if !(exists(${.CURDIR}/../kerberosIV) && exists(${.CURDIR}/../crypto)) || defined(NOSECURE) || !defined(MAKE_EBONES)
SUBDIR+=telnetd
.else
.if defined(RELEASEDIR)
# make release needs both
SUBDIR+=telnetd
.endif
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

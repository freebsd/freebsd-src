#	@(#)Makefile	8.1 (Berkeley) 6/4/93

SUBDIR=	atrun bootpd bootpgw comsat fingerd ftpd getNAME getty lfs_cleanerd \
	mail.local makekey revnetgroup rexecd rlogind rpc.rquotad rpc.rstatd \
	rpc.rusersd rpc.rwalld rpc.sprayd rshd talkd tftpd uucpd xtend

.if !exists(../secure) || defined(NOSECURE)
SUBDIR+=telnetd
.else
SUBDIR+= ../secure/libexec/telnetd
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

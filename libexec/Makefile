#	@(#)Makefile	8.1 (Berkeley) 6/4/93

SUBDIR=	atrun comsat fingerd ftpd getNAME getty lfs_cleanerd \
	mail.local makekey rexecd rlogind rpc.rstatd rpc.rusersd \
	rpc.rwalld rshd talkd telnetd tftpd uucpd

# Present but disabled: kpasswdd

.if	${MACHINE} == "hp300"
SUBDIR+=rbootd
.elif	${MACHINE} == "i386"
SUBDIR+=rbootd
.elif	${MACHINE} == "luna68k"
SUBDIR+=rbootd
.endif

.include <bsd.subdir.mk>

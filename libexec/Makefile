#	@(#)Makefile	8.1 (Berkeley) 6/4/93

SUBDIR=	bugfiler comsat fingerd ftpd getNAME getty kpasswdd lfs_cleanerd \
	mail.local makekey rexecd rlogind rshd talkd telnetd tftpd uucpd

.if	${MACHINE} == "hp300"
SUBDIR+=rbootd
.elif	${MACHINE} == "luna68k"
SUBDIR+=rbootd
.endif

.include <bsd.subdir.mk>

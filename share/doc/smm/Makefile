#	From: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$Id$

# The following modules do not describe FreeBSD:
# 14.uucpimpl, 15.uucpnet

# The following modules do not build/install:
# 06.nfs 08.sendmailop 09.sendmail (requre -me macros, which are broken)
# 10.named (no documentation at all)
# 13.amd (documentation is TeXinfo)
# 16.security 17.password (encumbered)

BINDIR=	/usr/share/doc/smm
SUBDIR=	title contents 01.setup 02.config 03.fsck 04.quotas 05.fastfs \
	07.lpd 11.timedop 12.timed 18.net

.if	${MACHINE} == "tahoe"
SUBDIR+=01.setup
.elif	${MACHINE} == "vax"
SUBDIR+=01.setup
.endif

.include <bsd.subdir.mk>

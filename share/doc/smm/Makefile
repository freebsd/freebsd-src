#	From: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$Id: Makefile,v 1.7 1997/02/22 13:05:19 peter Exp $

# The following modules do not describe FreeBSD:
# 14.uucpimpl, 15.uucpnet

# The following modules do not build/install:
# 13.amd (documentation is TeXinfo)
# 16.security 17.password (encumbered)

# The following modules are built with their programs:
# 07.lpd

BINDIR=	/usr/share/doc/smm
SUBDIR=	title contents 01.setup 02.config 03.fsck 04.quotas 05.fastfs \
	06.nfs 08.sendmailop 09.sendmail 10.named 11.timedop \
	12.timed 18.net

.include <bsd.subdir.mk>

# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id: Makefile,v 1.5 1994/11/20 23:21:06 wollman Exp $

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c
LDADD=	-lcrypt
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin
CFLAGS+=-DCRYPT -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

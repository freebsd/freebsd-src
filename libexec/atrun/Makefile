#	$Id: Makefile,v 1.1 1994/01/05 01:02:56 nate Exp $

PROG=	atrun
MAN8=	atrun.8

BINDIR=	/usr/libexec
BINOWN=	root

CFLAGS+= -I${.CURDIR}/../../usr.bin/at
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>

#	$Id: Makefile,v 1.2 1993/12/05 12:26:10 cgd Exp $

PROG=	atrun
MAN8=	atrun.8

BINDIR=	/usr/libexec
BINOWN=	root

CFLAGS+= -I${.CURDIR}/../../usr.bin/at
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>

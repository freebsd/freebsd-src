#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

BINDIR=	/sbin
NOSHARED?=	yes

PROG=	chown
CFLAGS+=-DSUPPORT_DOT
MAN1=	chgrp.1
MAN8=	chown.8
LINKS=	${BINDIR}/chown /bin/chgrp
SYMLINKS+=	${BINDIR}/chown /usr/sbin/chown
SYMLINKS+=	/bin/chgrp /usr/bin/chgrp

.include <bsd.prog.mk>

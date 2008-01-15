# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2 2005/02/27 11:22:58 ru Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

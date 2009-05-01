# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

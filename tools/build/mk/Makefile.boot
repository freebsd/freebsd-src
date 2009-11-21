# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

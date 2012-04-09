# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

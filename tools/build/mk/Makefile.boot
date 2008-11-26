# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

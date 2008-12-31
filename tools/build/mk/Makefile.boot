# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

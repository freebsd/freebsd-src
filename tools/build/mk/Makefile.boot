# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

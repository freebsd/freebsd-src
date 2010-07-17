# $FreeBSD: src/tools/build/mk/Makefile.boot,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

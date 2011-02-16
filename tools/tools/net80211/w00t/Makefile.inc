# $FreeBSD: src/tools/tools/net80211/w00t/Makefile.inc,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

W00T=	../libw00t
# NB: we get crc32 from -lz
DPADD=	${W00T}/libw00t.a ${LIBCRYPTO} ${LIBZ}
LDADD=	-L${W00T} -lw00t  -lcrypto -lz

BINDIR=	/usr/local/bin
CFLAGS=	-g -I${W00T}

NO_MAN=

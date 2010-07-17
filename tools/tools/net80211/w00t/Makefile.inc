# $FreeBSD: src/tools/tools/net80211/w00t/Makefile.inc,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

W00T=	../libw00t
# NB: we get crc32 from -lz
DPADD=	${W00T}/libw00t.a ${LIBCRYPTO} ${LIBZ}
LDADD=	-L${W00T} -lw00t  -lcrypto -lz

BINDIR=	/usr/local/bin
CFLAGS=	-g -I${W00T}

NO_MAN=

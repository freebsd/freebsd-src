# $FreeBSD$

W00T=	../libw00t
# NB: we get crc32 from -lz
DPADD=	${W00T}/libw00t.a ${LIBCRYPTO} ${LIBZ}
LDFLAGS= -L${W00T}
LDADD=	-lw00t  -lcrypto -lz

BINDIR=	/usr/local/bin
CFLAGS=	-g -I${W00T}

NO_MAN=

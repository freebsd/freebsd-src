PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

PACKAGE=	pkgconf
PRIVATELIB=	yes
LIB=		pkgconf
MAN=		pkg.m4.7
MANNODEV=	pc.5
MANNODEV+=	pkgconf-personality.5

SRCS=		audit.c buffer.c cache.c client.c pkg.c bsdstubs.c fragment.c
SRCS+=		argvsplit.c fileio.c tuple.c dependency.c queue.c
SRCS+=		path.c personality.c parser.c
INCS=		bsdstubs.h iter.h libpkgconf-api.h libpkgconf.h stdinc.h

WARNS?=		3

CFLAGS+=	-DPERSONALITY_PATH=\"/usr/share/pkgconfig/personality.d:/etc/pkgconfig/personality.d:${LOCALBASE:U}/etc/pkgconfig/personality.d\"
CFLAGS+=	-DPKG_DEFAULT_PATH=\"${LOCALBASE:U}/libdata/pkgconfig:/usr/libdata/pkgconfig:${LOCALBASE:U}/share/pkgconfig\"
CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\"
CFLAGS+=	-DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/libpkgconf
.PATH:		${PKGCONFDIR}/man

.include <bsd.lib.mk>

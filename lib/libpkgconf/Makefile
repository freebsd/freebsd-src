PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

PACKAGE=	pkgconf
PRIVATELIB=	yes
LIB=		pkgconf
MAN=		pkg.m4.7
MANNODEV=	pc.5
MANNODEV+=	pkgconf-personality.5

SRCS=		argvsplit.c audit.c buffer.c bufferset.c bytecode.c bsdstubs.c
SRCS+=		cache.c client.c dependency.c fileio.c fragment.c license.c
SRCS+=		output.c parser.c path.c personality.c pkg.c queue.c tuple.c
SRCS+=		variable.c version.c
INCS=		bsdstubs.h iter.h libpkgconf-api.h libpkgconf.h stdinc.h

WARNS?=		3

CFLAGS+=	-DPERSONALITY_PATH=\"/usr/share/pkgconfig/personality.d:/etc/pkgconfig/personality.d:${LOCALBASE:U/usr/local}/etc/pkgconfig/personality.d\"
CFLAGS+=	-DPKG_DEFAULT_PATH=\"${LOCALBASE:U/usr/local}/libdata/pkgconfig:/usr/libdata/pkgconfig:${LOCALBASE:U/usr/local}/share/pkgconfig\"
CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\"
CFLAGS+=	-DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/libpkgconf
.PATH:		${PKGCONFDIR}/man

.include <bsd.lib.mk>

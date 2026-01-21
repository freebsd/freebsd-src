PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

PACKAGE=	pkgconf
LIB=		pkgconf
SHLIB_MAJOR=	6

SRCS=		audit.c buffer.c cache.c client.c pkg.c bsdstubs.c fragment.c
SRCS+=		argvsplit.c fileio.c tuple.c dependency.c queue.c path.c
SRCS+=		personality.c parser.c
INCS=		bsdstubs.h iter.h libpkgconf-api.h libpkgconf.h stdinc.h

CFLAGS+=	-Wno-error=cast-align
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-DPERSONALITY_PATH=\"/usr/share/pkgconfig/personality.d:/etc/pkgconfig/personality.d\"
CFLAGS+=	-DPKG_DEFAULT_PATH=\"/usr/local/libdata/pkgconfig:/usr/libdata/pkgconfig:/usr/local/share/pkgconfig\"
CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\"
CFLAGS+=	-DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/libpkgconf

.include <bsd.lib.mk>

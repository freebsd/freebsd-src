PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

PACKAGE=	pkgconf
PRIVATELIB=	yes
LIB=		pkgconf
MAN=		pkg.m4.7
MANNODEV=	pc.5
MANNODEV+=	pkgconf-personality.5

# from contrib/pkgconf/meson.build:171
SRCS=		argvsplit.c
SRCS+=		audit.c
SRCS+=		buffer.c
SRCS+=		bufferset.c
SRCS+=		bytecode.c
SRCS+=		bsdstubs.c
SRCS+=		cache.c
SRCS+=		client.c
SRCS+=		dependency.c
SRCS+=		fileio.c
SRCS+=		fragment.c
SRCS+=		license.c
SRCS+=		output.c
SRCS+=		parser.c
SRCS+=		path.c
SRCS+=		personality.c
SRCS+=		pkg.c
SRCS+=		queue.c
SRCS+=		tuple.c
SRCS+=		variable.c
SRCS+=		version.c

# from contrib/pkgconf/libpkgconf/meson.build:6
INCS=		libpkgconf.h
INCS+=		stdinc.h
INCS+=		iter.h
INCS+=		bsdstubs.h
INCS+=		libpkgconf-api.h

WARNS?=		3

CFLAGS+=	-DPERSONALITY_PATH=\"/usr/share/pkgconfig/personality.d:/etc/pkgconfig/personality.d:${LOCALBASE:U/usr/local}/etc/pkgconfig/personality.d\"
CFLAGS+=	-DPKG_DEFAULT_PATH=\"${LOCALBASE:U/usr/local}/libdata/pkgconfig:/usr/libdata/pkgconfig:${LOCALBASE:U/usr/local}/share/pkgconfig\"
CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\"
CFLAGS+=	-DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/libpkgconf
.PATH:		${PKGCONFDIR}/man

.include <bsd.lib.mk>

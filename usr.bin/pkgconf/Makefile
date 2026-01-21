.include <src.opts.mk>

PACKAGE=	pkgconf

PROG=		pkgconf
SRCS=		main.c
SRCS+=		getopt_long.c
SRCS+=		renderer-msvc.c

LIBADD=		pkgconf

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

CFLAGS+=	-Wno-error=missing-variable-declarations
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\" -DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/cli
.PATH:		${PKGCONFDIR}/man

.include <bsd.prog.mk>

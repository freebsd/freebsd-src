PACKAGE=	pkgconf

PROG=		pkgconf
SRCS=		main.c core.c getopt_long.c renderer-msvc.c

LIBADD=		pkgconf

SYMLINKS=	pkgconf ${BINDIR}/pkg-config

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

WARNS?=		3

CFLAGS+=	-DSYSTEM_INCLUDEDIR=\"/usr/include\" -DSYSTEM_LIBDIR=\"/usr/lib\"
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR}

.PATH:		${PKGCONFDIR}/cli
.PATH:		${PKGCONFDIR}/man

.include <bsd.prog.mk>

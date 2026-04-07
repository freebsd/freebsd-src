.include <src.opts.mk>

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

.PATH:		${PKGCONFDIR}/cli/bomtool
.PATH:		${PKGCONFDIR}/cli
.PATH:		${PKGCONFDIR}/man

PACKAGE=	pkgconf

PROG=		bomtool

SRCS=

LIBADD=		pkgconf

SRCS+=		main.c
SRCS+=		getopt_long.c

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

CFLAGS+=	-Wno-error=missing-variable-declarations
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

all:		bomtool

build-tools:	bomtool

.include <bsd.prog.mk>

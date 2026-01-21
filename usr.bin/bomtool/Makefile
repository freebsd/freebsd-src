.include <src.opts.mk>

PACKAGE=	pkgconf

PROG=		bomtool
SRCS=		main.c
SRCS+=		getopt_long.c

LIBADD=		pkgconf

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

CFLAGS+=	-Wno-error=missing-variable-declarations
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

.PATH:		${PKGCONFDIR}/cli/bomtool ${PKGCONFDIR}/cli

.include <bsd.prog.mk>

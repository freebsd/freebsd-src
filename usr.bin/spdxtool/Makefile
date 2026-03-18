.include <src.opts.mk>

PACKAGE=	pkgconf

PROG=		spdxtool
SRCS=		main.c core.c software.c serialize.c simplelicensing.c util.c
SRCS+=		getopt_long.c

LIBADD=		pkgconf

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

CFLAGS+=	-Wno-error=missing-variable-declarations
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-I${PKGCONFDIR}/cli/spdxtool -I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

.PATH:		${PKGCONFDIR}/cli/spdxtool
.PATH:		${PKGCONFDIR}/cli

.include <bsd.prog.mk>

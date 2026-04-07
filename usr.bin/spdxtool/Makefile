.include <src.opts.mk>

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

.PATH:		${PKGCONFDIR}/cli/spdxtool
.PATH:		${PKGCONFDIR}/cli

PACKAGE=	pkgconf

PROG=		spdxtool

SRCS=

.if !make(build-tools)
LIBADD=		pkgconf
.else
.include "../../lib/libpkgconf/Makefile"
.endif

SRCS+=		main.c core.c software.c serialize.c simplelicensing.c util.c
SRCS+=		getopt_long.c

CFLAGS+=	-Wno-error=missing-variable-declarations
CFLAGS+=	-Wno-error=incompatible-pointer-types-discards-qualifiers
CFLAGS+=	-Wno-error=cast-qual
CFLAGS+=	-I${PKGCONFDIR}/cli/spdxtool -I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

all:		spdxtool

build-tools:	spdxtool

.include <bsd.prog.mk>

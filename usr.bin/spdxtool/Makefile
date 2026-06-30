PACKAGE=	pkgconf

PROG=		spdxtool
SRCS=		main.c core.c software.c serialize.c simplelicensing.c util.c
SRCS+=		getopt_long.c

LIBADD=		pkgconf

.if defined(TOOLS_PREFIX)
NO_SHARED=	yes
.endif

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

WARNS?=		3

CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

.PATH:		${PKGCONFDIR}/cli/spdxtool
.PATH:		${PKGCONFDIR}/cli
.PATH:		${PKGCONFDIR}/man

.include <bsd.prog.mk>

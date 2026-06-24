PACKAGE=	pkgconf

# from contrib/pkgconf/meson.build:266
PROG=		spdxtool
SRCS=		main.c
SRCS+=		core.c
SRCS+=		software.c
SRCS+=		serialize.c
SRCS+=		simplelicensing.c
SRCS+=		util.c
SRCS+=		generate.c
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

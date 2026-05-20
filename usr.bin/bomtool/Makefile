PACKAGE=	pkgconf

PROG=		bomtool
SRCS=		main.c
SRCS+=		getopt_long.c

LIBADD=		pkgconf

PKGCONFDIR=	${SRCTOP}/contrib/pkgconf

WARNS?=		3

CFLAGS+=	-I${SRCTOP}/lib/libpkgconf -I${PKGCONFDIR} -I${PKGCONFDIR}/cli

.PATH:		${PKGCONFDIR}/cli/bomtool
.PATH:		${PKGCONFDIR}/cli
.PATH:		${PKGCONFDIR}/man

.include <bsd.prog.mk>

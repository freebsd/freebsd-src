# $FreeBSD$

.include <bsd.own.mk>

CFLAGS+=	-DPKG_INSTALL_VERSION=20101012
CFLAGS+=	-DYES_I_KNOW_THE_API_IS_RUBBISH_AND_IS_DOOMED_TO_CHANGE

DPADD+=		${LIBPKG}
LDADD+=		-lpkg

# Inherit BINDIR from one level up.
.include "../Makefile.inc"

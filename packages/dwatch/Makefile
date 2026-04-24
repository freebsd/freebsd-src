WORLDPACKAGE=	dwatch
SUBPACKAGES=	man

# DTrace doesn't work in a jail.
PKG_SETS=	optional

# dwatch is implemented using dtrace.
PKG_DEPS.dwatch+=	dtrace

.include <bsd.pkg.mk>

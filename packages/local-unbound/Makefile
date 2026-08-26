WORLDPACKAGE=	local-unbound
SUBPACKAGES=	dbg dev man
COMPAT_PKGS=	dbg dev lib

PKG_LICENSES=	BSD4CLAUSE

UCLSRC=			common.ucl
UCLSRC.local-unbound=	local-unbound.ucl

# local-unbound uses resolvconf to update resolv.conf and to generate
# its forward.conf.
PKG_DEPS.local-unbound+=resolvconf

.include <bsd.pkg.mk>

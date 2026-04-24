.include <src.opts.mk>

WORLDPACKAGE=	libcompat
SUBPACKAGES=	dev
COMPAT_PKGS=	dev

# libcompat only contains static libraries and manpages, and therefore
# doesn't have a base package.
PKG_NO_BASE=

.if ${MK_MANSPLITPKG} != "no"
SUBPACKAGES=	man
.endif

.include <bsd.pkg.mk>

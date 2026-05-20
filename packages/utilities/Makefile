WORLDPACKAGE=	utilities
SUBPACKAGES=	dbg dev man
COMPAT_PKG=	yes
COMPAT_PKGS=	dbg dev

.include <bsd.endian.mk>
PKG_UCLVARS+=	CAP_MKDB_ENDIAN "${CAP_MKDB_ENDIAN}"

PKG_SETS=	minimal minimal-jail

UCLSRC=			common.ucl
UCLSRC.utilities=	utilities.ucl

.include <bsd.pkg.mk>

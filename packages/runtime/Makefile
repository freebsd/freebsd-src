WORLDPACKAGE=	runtime
SUBPACKAGES=	dbg dev man
COMPAT_PKGS=	dbg dev lib

.include <bsd.endian.mk>
PKG_UCLVARS+=	CAP_MKDB_ENDIAN "${CAP_MKDB_ENDIAN}"

PKG_SETS=	minimal minimal-jail
PKG_VITAL.runtime=

UCLSRC=		common.ucl
UCLSRC.runtime=	runtime.ucl

.include <bsd.pkg.mk>

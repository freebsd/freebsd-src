.include <src.opts.mk>

WORLDPACKAGE=	toolchain
SUBPACKAGES=	dbg dev man
COMPAT_PKGS=	dev

PKG_SETS=	devel

.if ${MK_LLVM_BINUTILS} != "no"
PKG_DEPS.toolchain+=	llvm
.endif

.include <bsd.pkg.mk>

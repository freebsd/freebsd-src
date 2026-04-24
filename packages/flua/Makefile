.include <src.opts.mk>

WORLDPACKAGE=	flua
SUBPACKAGES=	dbg

# The flua "dev" package only contains manpages.
.if ${MK_MAN} != "no"
. if ${MK_MANSPLITPKG} != "no"
SUBPACKAGES+=	man
. else
SUBPACKAGES+=	dev
. endif
.endif

.include <bsd.pkg.mk>

.include <src.opts.mk>

WORLDPACKAGE=	bootloader
SUBPACKAGES=

# bootloader-dbg is only built if we're building userboot, which is specific
# to amd64.
.if ${MACHINE_CPUARCH} == "amd64"
SUBPACKAGES+=	dbg
.endif

# The bootloader "dev" package only contains manpages.
.if ${MK_MAN} != "no"
. if ${MK_MANSPLITPKG} != "no"
SUBPACKAGES+=	man
. else
SUBPACKAGES+=	dev
. endif
.endif

# The bootloader isn't normally used in a jail.
PKG_SETS=	minimal

.include <bsd.pkg.mk>

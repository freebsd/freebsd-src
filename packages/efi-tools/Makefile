WORLDPACKAGE=	efi-tools
SUBPACKAGES=	dbg dev man

# The 32-bit libraries are only built on certain architectures.
.if ${MACHINE_ARCH} == "aarch64"
COMPAT_PKG=	yes
COMPAT_PKGS=	dbg dev
.endif

# efi-tools is not applicable to jails.
PKG_SETS=	minimal

.include <bsd.pkg.mk>

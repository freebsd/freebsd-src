WORLDPACKAGE=	bluetooth
SUBPACKAGES=	dbg dev lib man
COMPAT_PKGS=	dbg dev lib

# Bluetooth isn't applicable to jails.
PKG_SETS=	optional

# rfcomm_pppd(8) uses ppp(8)
PKG_DEPS.bluetooth+=	ppp

.include <bsd.pkg.mk>

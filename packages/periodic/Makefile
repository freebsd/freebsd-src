WORLDPACKAGE=	periodic
SUBPACKAGES=	man

PKG_SETS=	minimal minimal-jail

# periodic scripts are invoked from cron.
PKG_DEPS.periodic+=	cron
# periodic scripts are written in shell script.
PKG_DEPS.periodic+=	runtime

.include <bsd.pkg.mk>

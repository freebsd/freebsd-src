WORLDPACKAGE=	at

PKG_SETS=	minimal minimal-jail

# atrun relies on cron to work.
PKG_DEPS.at+=	cron
# at(1) passes the command to /bin/sh
PKG_DEPS.at+=	runtime

.include <bsd.pkg.mk>

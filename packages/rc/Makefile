WORLDPACKAGE=	rc

PKG_SETS=	minimal minimal-jail

# rc scripts require /bin/sh.
PKG_DEPS.rc+=	runtime

# /etc/rc.d/var_run depends on mtree.
PKG_DEPS.rc+=	mtree

.include <bsd.pkg.mk>

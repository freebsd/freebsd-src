WORLDPACKAGE=	apm

# On non-amd64 platforms, this package only contain an rc script.
# (This should be fixed.)
.if ${MACHINE_CPUARCH} != "amd64"
SUBPACKAGES=
.endif

# APM isn't applicable to jails.
PKG_SETS=	optional

.include <bsd.pkg.mk>

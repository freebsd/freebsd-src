WORLDPACKAGE=	bhyve

# bhyve needs iasl(8) on amd64
.if ${MACHINE_ARCH} == "amd64"
PKG_DEPS.bhyve+=	acpi
.endif

.include <bsd.pkg.mk>

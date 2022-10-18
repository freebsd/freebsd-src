.PATH:	${SRCTOP}/sys/dev/dpaa2

KMOD=	dpaa2

SRCS=	dpaa2_mc.c
SRCS+=	dpaa2_rc.c
SRCS+=	dpaa2_io.c
SRCS+=	dpaa2_bp.c
SRCS+=	dpaa2_ni.c
SRCS+=	dpaa2_mcp.c
SRCS+=	dpaa2_swp.c
SRCS+=	dpaa2_mac.c
SRCS+=	dpaa2_con.c

SRCS+=	dpaa2_cmd_if.c dpaa2_cmd_if.h
SRCS+=	dpaa2_swp_if.c dpaa2_swp_if.h
SRCS+=	dpaa2_mc_if.c dpaa2_mc_if.h
SRCS+=	memac_mdio_common.c memac_mdio_if.c memac_mdio_if.h

SRCS+=	bus_if.h device_if.h miibus_if.h
SRCS+=	pcib_if.h pci_if.h
SRCS+=	mdio_if.h
SRCS+=	opt_acpi.h opt_platform.h

.if !empty(OPT_ACPI)
SRCS+=	dpaa2_mc_acpi.c \
	memac_mdio_acpi.c \
	opt_acpi.h \
	acpi_if.h \
	acpi_bus_if.h
.endif

.if !empty(OPT_FDT)
SRCS+=	dpaa2_mc_fdt.c \
	memac_mdio_fdt.c \
	opt_platform.h \
	ofw_bus_if.h
.endif

MFILES=	dev/dpaa2/dpaa2_cmd_if.m \
	dev/dpaa2/dpaa2_swp_if.m \
	dev/dpaa2/dpaa2_mc_if.m \
	dev/dpaa2/memac_mdio_if.m

.include <bsd.kmod.mk>

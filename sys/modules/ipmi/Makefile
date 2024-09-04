.PATH: ${SRCTOP}/sys/dev/ipmi

# XXX - ipmi_smbus and ipmi_ssif depend on smbus
# XXX - ipmi_acpi depends on acpi
KMOD=	ipmi
SRCS=	ipmi.c
SRCS+=	bus_if.h device_if.h

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	ipmi_kcs.c ipmi_smic.c ipmi_bt.c
SRCS+=	ipmi_acpi.c
SRCS+=	opt_acpi.h
SRCS+=	acpi_if.h
SRCS+=	ipmi_smbios.c
SRCS+=	ipmi_ssif.c
SRCS+=	ipmi_pci.c
SRCS+=	ipmi_smbus.c
SRCS+=	pci_if.h
SRCS+=	smbus_if.h
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SRCS+=	ipmi_isa.c
SRCS+=	isa_if.h

SUBDIR+= ipmi_linux

.elif ${MACHINE_ARCH:Mpowerpc64*} != ""
SRCS+=	ipmi_opal.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ipmi

# XXX - ipmi_smbus and ipmi_ssif depend on smbus
# XXX - ipmi_acpi depends on acpi
KMOD=	ipmi
SRCS=	ipmi.c ipmi_kcs.c ipmi_smic.c ipmi_smbios.c ipmi_ssif.c
SRCS+=	ipmi_acpi.c ipmi_isa.c ipmi_pci.c ipmi_smbus.c
SRCS+=	opt_acpi.h
SRCS+=	acpi_if.h bus_if.h device_if.h isa_if.h pci_if.h smbus_if.h

.include <bsd.kmod.mk>

.PATH:	${SRCTOP}/sys/dev/tpm

KMOD=	tpm

SRCS=	bus_if.h device_if.h tpm_if.c tpm_if.h
SRCS+=	opt_acpi.h opt_tpm.h

#Bus specific stuff.
.if ${MACHINE_ARCH:Namd64:Ni386} == ""
SRCS+= tpm.c
SRCS+=	tpm_acpi.c tpm_isa.c
SRCS+=	isa_if.h
.endif

#TPM 2.0
SRCS+=	tpm20.c tpm_tis_core.c
.if defined(${OPT_FDT})
SRCS+=  tpm_spibus.c tpm_tis_spibus.c spibus_if.h
.endif
.if ${MACHINE_ARCH:Namd64:Ni386:Narm64} == ""
SRCS+= acpi_if.h
SRCS+= tpm_tis_acpi.c
SRCS+= tpm_crb.c
SRCS+= tpm_bus.c
CFLAGS+=-DDEV_ACPI
.endif

.include <bsd.kmod.mk>

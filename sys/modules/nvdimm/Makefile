# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/nvdimm

KMOD=	nvdimm
SRCS=	nvdimm.c \
	nvdimm_acpi.c \
	nvdimm_e820.c \
	nvdimm_nfit.c \
	nvdimm_ns.c \
	nvdimm_spa.c

SRCS+=	acpi_if.h bus_if.h device_if.h
SRCS+=	opt_acpi.h opt_ddb.h

.include <bsd.kmod.mk>

# Mute spurious Clang warning.
.if ${COMPILER_TYPE} == "clang"
CWARNFLAGS.nvdimm_e820.c+=	-Wno-missing-braces
.endif

# $FreeBSD$

.PATH:  ${SRCTOP}/sys/arm64/spe

KMOD   = spe
SRCS   = \
	arm_spe_dev.c \
	arm_spe_backend.c \
	arm_spe.h \
	arm_spe_dev.h

SRCS+= opt_platform.h

SRCS.DEV_ACPI= \
	arm_spe_acpi.c

.if !empty(OPT_FDT)
SRCS+= \
	arm_spe_fdt.c
.endif

EXPORT_SYMS=   YES

.include <bsd.kmod.mk>

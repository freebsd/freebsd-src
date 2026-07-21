.PATH: ${SRCTOP}/sys/dev/hwpmc
.PATH: ${SRCTOP}/sys/arm64/arm64

KMOD=	hwpmc

SRCS=	hwpmc_mod.c \
	hwpmc_logging.c \
	hwpmc_soft.c
SRCS+=	bus_if.h \
	device_if.h \
	vnode_if.h

.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	hwpmc_arm64.c \
	hwpmc_arm64_md.c
SRCS.DEV_ACPI= cmn600.c \
	hwpmc_cmn600.c \
	hwpmc_dmc620.c \
	pmu_dmc620.c
SRCS+=	opt_acpi.h

.elif ${MACHINE_CPUARCH} == "amd64"
SRCS+=	hwpmc_amd.c \
	hwpmc_core.c \
	hwpmc_ibs.c \
	hwpmc_intel.c \
	hwpmc_tsc.c \
	hwpmc_uncore.c \
	hwpmc_x86.c

.elif ${MACHINE_CPUARCH} == "arm"
SRCS+=	hwpmc_arm.c \
	hwpmc_armv7.c \

.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	hwpmc_amd.c \
	hwpmc_core.c \
	hwpmc_ibs.c \
	hwpmc_intel.c \
	hwpmc_tsc.c \
	hwpmc_uncore.c \
	hwpmc_x86.c

.elif ${MACHINE_CPUARCH} == "powerpc"
SRCS+=	hwpmc_e500.c \
	hwpmc_mpc7xxx.c \
	hwpmc_ppc970.c \
	hwpmc_power8.c \
	hwpmc_powerpc.c
.endif

.include <bsd.kmod.mk>

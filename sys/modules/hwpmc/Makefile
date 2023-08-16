#
#

.PATH: ${SRCTOP}/sys/dev/hwpmc
.PATH: ${SRCTOP}/sys/arm64/arm64

KMOD=	hwpmc

SRCS=	bus_if.h device_if.h hwpmc_mod.c hwpmc_logging.c hwpmc_soft.c
SRCS+=	vnode_if.h

.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	hwpmc_arm64.c hwpmc_arm64_md.c
SRCS.DEV_ACPI=	cmn600.c hwpmc_cmn600.c
SRCS.DEV_ACPI+=	hwpmc_dmc620.c pmu_dmc620.c
.endif

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c hwpmc_tsc.c
SRCS+=	hwpmc_x86.c hwpmc_uncore.c
.endif

.if ${MACHINE_CPUARCH} == "arm"
SRCS+=	hwpmc_arm.c
.endif

.if ${MACHINE_ARCH} == "armv7"
SRCS+= hwpmc_armv7.c
.endif

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c
SRCS+=	hwpmc_tsc.c hwpmc_x86.c hwpmc_uncore.c
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
SRCS+=	hwpmc_powerpc.c hwpmc_e500.c hwpmc_mpc7xxx.c hwpmc_ppc970.c \
	hwpmc_power8.c
.endif

.include <bsd.kmod.mk>

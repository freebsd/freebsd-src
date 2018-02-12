# $FreeBSD$

.include <bsd.init.mk>

# In-tree GCC does not support __attribute__((ms_abi)), but gcc newer
# than 4.5 supports it.
.if ${COMPILER_TYPE} != "gcc" || ${COMPILER_VERSION} >= 40500

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm"
.if ${MK_FDT} != "no"
SUBDIR+=	fdt
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "arm"
SUBDIR+=	libefi loader boot1
.endif

.endif # ${COMPILER_TYPE} != "gcc" || ${COMPILER_VERSION} >= 40500

.include <bsd.subdir.mk>

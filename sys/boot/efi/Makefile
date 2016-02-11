# $FreeBSD$

.include <bsd.own.mk>

# In-tree GCC does not support __attribute__((ms_abi)).
.if ${COMPILER_TYPE} != "gcc"

.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	libefi loader boot1
.endif

.endif # ${COMPILER_TYPE} != "gcc"

.if ${MACHINE_CPUARCH} == "ia64"
SUBDIR+=	libefi
.endif

.include <bsd.subdir.mk>


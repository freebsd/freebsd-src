# $FreeBSD$

NO_OBJ=t

.include <bsd.init.mk>

# In-tree GCC does not support __attribute__((ms_abi)), but gcc newer
# than 4.5 supports it.
.if ${COMPILER_TYPE} != "gcc" || ${COMPILER_VERSION} >= 40500

SUBDIR.${MK_FDT}+=	fdt
SUBDIR.yes+=	libefi loader boot1

.endif # ${COMPILER_TYPE} != "gcc" || ${COMPILER_VERSION} >= 40500

.include <bsd.subdir.mk>

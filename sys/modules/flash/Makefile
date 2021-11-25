# $FreeBSD$
# Build dev/flash modules.

SUBDIR = 

.if (${MACHINE_CPUARCH} == "aarch64") && !empty(OPT_FDT)
SUBDIR+=	flexspi
.endif
	
.include <bsd.subdir.mk>

# $FreeBSD$

SUBDIR=		capi devcrypto loader_attic
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	padlock
.endif
SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

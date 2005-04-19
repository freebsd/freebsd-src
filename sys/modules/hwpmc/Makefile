# 
# $FreeBSD$
#

.PATH: ${.CURDIR}/../../hwpmc

KMOD=	hwpmc

SRCS=	hwpmc_mod.c 

WARNS?=	2

.if ${MACHINE_ARCH} == "i386"
SRCS+=  hwpmc_amd.c hwpmc_intel.c hwpmc_piv.c hwpmc_ppro.c hwpmc_pentium.c
.endif

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	hwpmc_amd.c
.endif

.include <bsd.kmod.mk>

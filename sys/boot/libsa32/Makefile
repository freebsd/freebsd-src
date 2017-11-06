# $FreeBSD$

DO32=1

.include <bsd.init.mk>

LIB=sa32
.if ${MACHINE_CPUARCH} == "amd64"
LIBSA_CPUARCH=i386
.else
LIBSA_CPUARCH=${MACHINE_CPUARCH}
.endif

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-m32 -I.
.endif

.PATH:	${SASRC}
.include "${SASRC}/Makefile"

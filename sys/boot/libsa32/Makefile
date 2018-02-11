# $FreeBSD$

.include <src.opts.mk>

.include "../Makefile.inc"

LIB=sa32
.if ${MACHINE_CPUARCH} == "amd64"
LIBSTAND_CPUARCH=i386
.else
LIBSTAND_CPUARCH=${MACHINE_CPUARCH}
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -I.
.endif

.PATH:	${SASRC}
.include "${SASRC}/Makefile"

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+= machine
beforedepend ${OBJS}: machine
machine: .NOMETA
	ln -fs ${.CURDIR}/../../i386/include machine
.endif

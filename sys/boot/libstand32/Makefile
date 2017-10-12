# $FreeBSD$

.include <src.opts.mk>

.include "../Makefile.inc"

.if ${MACHINE_CPUARCH} == "amd64"
LIBSTAND_CPUARCH=i386
.else
LIBSTAND_CPUARCH=${MACHINE_CPUARCH}
.endif
INTERNALLIB=
INCS=
MAN=
.PATH:	${SASRC}

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -I.
.endif

.include "${SASRC}/Makefile"

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+= machine
beforedepend ${OBJS}: machine
machine: .NOMETA
	ln -fs ${.CURDIR}/../../i386/include machine
.endif

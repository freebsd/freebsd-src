# $FreeBSD$

.PATH: ${.CURDIR}/../../i386/bios
.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/apm
.endif

KMOD=	apm
SRCS=	apm.c apm.h
.if ${MACHINE} == "pc98"
SRCS+=	apm_bioscall.s
.endif
SRCS+=	device_if.h bus_if.h

EXPORT_SYMS=	apm_display	\
		apm_softc

.if ${MACHINE} == "pc98"
apm_bioscall.o: apm_bioscall.s
	${CC} -x assembler-with-cpp -DLOCORE ${CFLAGS} -c ${.IMPSRC}
.endif

.include <bsd.kmod.mk>

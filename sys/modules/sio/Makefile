# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/sio

KMOD=	sio
SRCS=	sio.c sio_pccard.c sio_pci.c sio_puc.c

.if ${MACHINE} == "sparc64"
### untested
CFLAGS += sio_ebus.c
.endif
.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.else
SRCS+=	sio_isa.c
.endif

opt_compat.h:
	@echo '#define COMPAT_43 1' > opt_compat.h
	@echo '#define COMPAT_FREEBSD4 1' >> opt_compat.h

SRCS+=	opt_sio.h opt_ddb.h opt_compat.h opt_comconsole.h \
	bus_if.h card_if.h device_if.h isa_if.h pci_if.h

.include <bsd.kmod.mk>

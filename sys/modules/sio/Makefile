# $FreeBSD$

.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/pc98
.endif
.PATH: ${.CURDIR}/../../dev/sio

KMOD=	sio
SRCS=	sio.c sio_pccard.c sio_pci.c sio_puc.c
SRCS+=	opt_sio.h opt_ddb.h opt_compat.h opt_comconsole.h \
	bus_if.h card_if.h device_if.h isa_if.h pci_if.h

.if ${MACHINE} == "pc98"
SRCS+=	sio_cbus.c
.else
SRCS+=	sio_isa.c
.endif
.if ${MACHINE} == "sparc64"
### untested
SRCS+=	sio_ebus.c
.endif

opt_compat.h:
	@echo '#define COMPAT_43 1' > opt_compat.h
	@echo '#define COMPAT_FREEBSD4 1' >> opt_compat.h

.if ${MACHINE} == "pc98"
opt_sio.h:
	@echo '#define COM_MULTIPORT 1' > opt_sio.h
	@echo '#define COM_ESP 1' >> opt_sio.h
.endif

.include <bsd.kmod.mk>

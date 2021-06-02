# $FreeBSD$
HPT27XX=	${SRCTOP}/sys/dev/hpt27xx
.PATH:	${HPT27XX}

KMOD=	hpt27xx
SRCS=	bus_if.h device_if.h pci_if.h
SRCS+=	opt_cam.h opt_scsi.h
SRCS+=	os_bsd.h hpt27xx_os_bsd.c hpt27xx_osm_bsd.c hpt27xx_config.c
BLOB_OBJS+=	${HPT27XX}/$(MACHINE_ARCH)-elf.hpt27xx_lib.o

.include <bsd.kmod.mk>

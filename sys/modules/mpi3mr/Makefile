.PATH: ${SRCTOP}/sys/dev/mpi3mr

KMOD=	mpi3mr
SRCS=	mpi3mr_pci.c mpi3mr.c mpi3mr_cam.c mpi3mr_app.c
SRCS+=	opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CWARNFLAGS.mpi3mr_sas.c=		${NO_WUNNEEDED_INTERNAL_DECL}
CWARNFLAGS.mpi3mr_mapping.c=	${NO_WSOMETIMES_UNINITIALIZED}

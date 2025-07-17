.PATH: ${SRCTOP}/sys/dev/ufshci

KMOD = ufshci

SRCS =	ufshci.c	\
	ufshci_pci.c	\
	ufshci_ctrlr.c	\
	ufshci_dev.c	\
	ufshci_ctrlr_cmd.c	\
	ufshci_uic_cmd.c	\
	ufshci_req_queue.c	\
	ufshci_req_sdb.c	\
	ufshci_sim.c		\
	ufshci_sysctl.c		\
	bus_if.h		\
	device_if.h		\
	opt_cam.h		\
	pci_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>

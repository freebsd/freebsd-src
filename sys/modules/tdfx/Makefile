.PATH: $(.CURDIR)/../../dev/tdfx
KMOD		= tdfx
NOMAN		= YES
SRCS		= opt_tdfx.h  pci.h bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
		  tdfx_vars.h tdfx_pci.c
INCSRC		= ../../sys

# Uncomment this for debugging messages
TDFX_VERBOSE = "\#define TDFX_VERBOSE"

# kludge to allow opt_tdfx.h to exist
pci.h: opt_tdfx.h
	echo "#define NPCI 1" > pci.h

opt_tdfx.h:
	echo $(TDFX_VERBOSE) > opt_tdfx.h

.include <bsd.kmod.mk>

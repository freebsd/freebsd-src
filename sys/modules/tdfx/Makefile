# $FreeBSD$
.PATH: $(.CURDIR)/../../dev/tdfx
KMOD		= 3dfx
NOMAN		= YES
SRCS		= opt_tdfx.h  pci.h bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
		  tdfx_vars.h tdfx_pci.c
INCSRC		= ../../sys

# This line enables linux ioctl handling by default
# comment out if you don't want it
TDFX_OPTS = "\#define TDFX_LINUX"
# Uncomment this for debugging messages
#CFLAGS += -DDEBUG

pci.h:
	echo "#define NPCI 1" > pci.h

opt_tdfx.h:
	touch opt_tdfx.h
	echo $(TDFX_OPTS) >> opt_tdfx.h

.include <bsd.kmod.mk>

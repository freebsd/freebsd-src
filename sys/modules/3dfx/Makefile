# $FreeBSD$

MAINTAINER=	cokane@FreeBSD.org

.PATH: $(.CURDIR)/../../dev/tdfx

KMOD=	3dfx
SRCS=	opt_tdfx.h  bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c
INCSRC=	../../sys

.if ${MACHINE_ARCH} == "i386"
# This line enables linux ioctl handling by default
# comment out if you don't want it
TDFX_OPTS=	"\#define TDFX_LINUX"
.endif

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.if !defined(KERNBUILDDIR)
opt_tdfx.h:
	echo $(TDFX_OPTS) > opt_tdfx.h
.endif

.include <bsd.kmod.mk>

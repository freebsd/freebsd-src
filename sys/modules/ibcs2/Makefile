#	$Id: Makefile,v 1.1 1994/09/21 23:27:03 wollman Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_mod
SRCS= 	ibcs2.c ibcs2_dummy.c ibcs2_file.c ibcs2_ioctl.c ibcs2_ipc.c \
	ibcs2_isc.c ibcs2_misc.c ibcs2_signal.c ibcs2_stats.c \
	ibcs2_sysent.c ibcs2_sysi86.c ibcs2_xenix.c \
	imgact_coff.c #imgact_elf.c imgact_linux.c imgact_xout.c

NOMAN=
VFS_LKM=
CFLAGS+= -DLKM -I. -DIBCS2

.include <bsd.kmod.mk>

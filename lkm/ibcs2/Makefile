#	$Id$

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_mod
SRCS= 	ibcs2.c ibcs2_dummy.c ibcs2_file.c ibcs2_ioctl.c ibcs2_ipc.c \
	ibcs2_isc.c ibcs2_misc.c ibcs2_signal.c ibcs2_stats.c \
	ibcs2_sysent.c ibcs2_sysi86.c ibcs2_xenix.c 
NOMAN=
VFS_LKM=
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2

.afterinstall:
	${INSTALL} -c -o bin -g bin -m 755 ibcs2 /usr/bin

.include <bsd.kmod.mk>

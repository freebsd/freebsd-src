#	Makefile,v 1.5 1994/12/11 21:27:14 wollman Exp

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_mod
SRCS=	opt_sysvipc.h \
	ibcs2.c ibcs2_errno.c ibcs2_ipc.c ibcs2_stat.c ibcs2_misc.c \
	ibcs2_fcntl.c ibcs2_signal.c ibcs2_sysent.c ibcs2_ioctl.c \
	ibcs2_socksys.c ibcs2_util.c ibcs2_xenix.c ibcs2_xenix_sysent.c \
	ibcs2_isc.c ibcs2_isc_sysent.c \
	ibcs2_msg.c ibcs2_other.c ibcs2_sysi86.c ibcs2_sysvec.c vnode_if.h
MAN8=	ibcs2.8

CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2
CLEANFILES+= vnode_if.h vnode_if.c opt_sysvipc.h
EXPORT_SYMS= _ibcs2_mod _ibcs2_emul_path _ibcs2_svr3_sysvec

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/ibcs2 ${DESTDIR}/usr/bin

opt_sysvipc.h:
	touch opt_sysvipc.h

.include <bsd.kmod.mk>

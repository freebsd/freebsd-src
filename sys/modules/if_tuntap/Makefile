# $FreeBSD$

.PATH: ${SRCTOP}/sys/net

KMOD=	if_tuntap
SRCS=	if_tuntap.c opt_inet.h opt_inet6.h

# Symlink for backwards compatibility with systems installed at 12.0 or older
.if ${MACHINE_CPUARCH} != "powerpc"
SYMLINKS=	${KMOD}.ko ${KMODDIR}/if_tun.ko \
	${KMOD}.ko ${KMODDIR}/if_tap.ko
.else
# Some PPC systems use msdosfs for /boot, which can't handle links or symlinks
afterinstall: alias alias_debug
alias: .PHONY
	${INSTALL} -T release -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}/if_tun.ko
	${INSTALL} -T release -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}/if_tap.ko
.if defined(DEBUG_FLAGS) && !defined(INSTALL_NODEBUG) && \
    "${MK_KERNEL_SYMBOLS}" != "no"
alias_debug: .PHONY
	${INSTALL} -T debug -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG}.debug \
	    ${DESTDIR}${KERN_DEBUGDIR}${KMODDIR}/if_tun.ko
	${INSTALL} -T debug -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG}.debug \
	    ${DESTDIR}${KERN_DEBUGDIR}${KMODDIR}/if_tap.ko
.else
alias_debug: .PHONY
.endif
.endif


.include <bsd.kmod.mk>

#$FreeBSD$

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../dev/ixl

KMOD    = if_ixl
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += opt_inet.h opt_inet6.h
SRCS    += if_ixl.c ixl_txrx.c i40e_osdep.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c i40e_lan_hmc.c i40e_hmc.c

CFLAGS	+= -DSMP -DIXL_DEBUG_SYSCTL

# Add Flow Director support
# CFLAGS += -DIXL_FDIR
# Debug messages / sysctls
# CFLAGS += -DIXL_DEBUG

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

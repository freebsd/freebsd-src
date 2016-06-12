# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/urtwn

SYSDIR?=${.CURDIR}/../..
.include "${SYSDIR}/conf/kern.opts.mk"

KMOD    = if_urtwn
SRCS    = if_urtwn.c if_urtwnreg.h if_urtwnvar.h \
	  bus_if.h device_if.h \
	  opt_bus.h opt_urtwn.h opt_usb.h opt_wlan.h usb_if.h usbdevs.h

.if ${MK_SOURCELESS_UCODE} == "no"
opt_urtwn.h:
	@echo "#define URTWN_WITHOUT_UCODE 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>

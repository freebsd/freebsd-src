# $FreeBSD$

#
# Uncomment the following line to bundle firmware with the driver,
# which may be flashed onto the controller, if the firmware on the
# controller is older than the one bundled, and needs to be upgraded.
# The size of the driver will increase significantly (to over 500KB)
# if this option is selected.
#
FLASH_FIRMWARE=1

KMOD = twa
.PATH:	${.CURDIR}/../../dev/${KMOD}
SRCS = twa_freebsd.c twa_cam.c twa.c twa_globals.c \
      bus_if.h device_if.h pci_if.h opt_scsi.h opt_cam.h opt_twa.h

.if defined(FLASH_FIRMWARE)
CFLAGS+=-DTWA_FLASH_FIRMWARE
SRCS += twa_fwimg.c
.endif

#CFLAGS+=-DTWA_DEBUG=0

.include <bsd.kmod.mk>

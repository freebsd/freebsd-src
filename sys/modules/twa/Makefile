# $FreeBSD$

#
# In the line following this comment, a value of 0 on the right hand
# side will not cause firmware to be bundled.  Change this value to 1
# to bundle the firmware with the driver, which may be flashed onto
# the controller, if the firmware on the controller is older than the
# one bundled, and needs to be upgraded.  The size of the driver will
# increase significantly (to over 500kB) if this option is selected.
#
TWA_FLASH_FIRMWARE?=0

KMOD = twa
.PATH:	${.CURDIR}/../../dev/${KMOD}
SRCS = twa_freebsd.c twa_cam.c twa.c twa_globals.c \
      bus_if.h device_if.h pci_if.h opt_scsi.h opt_cam.h opt_twa.h

.if $(TWA_FLASH_FIRMWARE) != 0
CFLAGS+=-DTWA_FLASH_FIRMWARE
SRCS += twa_fwimg.c
.endif

#CFLAGS+=-DTWA_DEBUG=0

.include <bsd.kmod.mk>

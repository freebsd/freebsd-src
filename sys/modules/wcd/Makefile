#	$Id: Makefile,v 1.9 1998/01/24 02:21:53 eivind Exp $

.PATH:          ${.CURDIR}/../../sys/i386/isa
KMOD            = wcd_mod
SRCS            = wcd.c wdc.h wcd.h opt_atapi.h opt_devfs.h
NOMAN           =
CFLAGS         += -DWCD_MODULE
CLEANFILES     += $(KMOD) machine wdc.h wcd.h opt_atapi.h opt_devfs.h *.b

load:           ${PROG}
		${MODLOAD} -u -e wcd $(PROG)

unload:         ${PROG}
		${MODUNLOAD} -n wcd

wdc.h:
		echo "#define NWDC 2" > wdc.h
		echo "#define NWD 4" >> wdc.h

wcd.h:
		echo "#define NWCD 1" > wcd.h

opt_atapi.h:
		echo "#define ATAPI 1"> opt_atapi.h

opt_devfs.h:
		touch opt_devfs.h

.include <bsd.kmod.mk>

#	$Id: Makefile,v 1.12 1998/10/16 04:30:52 peter Exp $

.PATH:          ${.CURDIR}/../../i386/isa
KMOD            = wcd
SRCS            = wcd.c wdc.h wcd.h opt_atapi.h opt_devfs.h
NOMAN           =
CFLAGS         += -DWCD_MODULE
CLEANFILES     += wdc.h wcd.h opt_atapi.h opt_devfs.h

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

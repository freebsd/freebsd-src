#	$Id: Makefile,v 1.5 1996/10/06 22:17:16 wosch Exp $

.PATH:          ${.CURDIR}/../../sys/i386/isa
KMOD            = wcd_mod
SRCS            = wcd.c wdc.h wcd.h opt_atapi.h
NOMAN           =
CFLAGS         += -nostdinc -I. -DWCD_MODULE
CLEANFILES     += $(KMOD) machine wdc.h wcd.h opt_atapi.h *.b
LN              = ln -f -s

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

.c.o:
		-@$(LN) ${.CURDIR}/../../sys/i386/include machine
		$(CC) $(CFLAGS) -c $<
		-@rm -f machine

.include <bsd.kmod.mk>

#	$Id: Makefile,v 1.8 1997/02/22 12:47:39 peter Exp $

.PATH:          ${.CURDIR}/../../sys/i386/isa
KMOD            = atapi_mod
SRCS            = atapi.c wdc.h opt_atapi.h #wcd.h 
NOMAN           =
CFLAGS         += -nostdinc -I. -DATAPI_MODULE
CLEANFILES     += $(KMOD) machine wdc.h wcd.h opt_atapi.h *.b
LN              = ln -f -s
EXPORT_SYMS	= _atapi_mod _atapi_debug_ptr _atapi_intr_ptr \
		  _atapi_request_callback_ptr _atapi_request_immediate_ptr \
		  _atapi_request_wait_ptr _atapi_start_ptr

load:           ${PROG}
		${MODLOAD} -u -e atapi_mod $(PROG)

unload:         ${PROG}
		${MODUNLOAD} -n atapi

wdc.h:
		echo "#define NWDC 2" > wdc.h

#		echo "#define NWD 4" >> wdc.h

wcd.h:
		echo "#define NWCD 1" > wcd.h

opt_atapi.h:
		echo "#define ATAPI 1"> opt_atapi.h

.c.o:
		-@$(LN) ${.CURDIR}/../../sys/i386/include machine
		$(CC) $(CFLAGS) -c $<
		-@rm -f machine

.include <bsd.kmod.mk>

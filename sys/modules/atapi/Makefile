# $FreeBSD$

.PATH:          ${.CURDIR}/../../i386/isa
KMOD            = atapi
SRCS            = atapi.c wdc.h opt_atapi.h #wcd.h 
NOMAN           =
CFLAGS         += -DATAPI_MODULE
CLEANFILES     += wdc.h wcd.h opt_atapi.h
EXPORT_SYMS	= _atapi_mod _atapi_debug_ptr _atapi_intr_ptr \
		  _atapi_request_callback_ptr _atapi_request_immediate_ptr \
		  _atapi_request_wait_ptr _atapi_start_ptr

MODLOAD		= /sbin/modload
MODUNLOAD	= /sbin/modunload

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

.include <bsd.kmod.mk>

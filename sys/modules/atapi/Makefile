#	$Id: $

.PATH:          ${.CURDIR}/../../sys/i386/isa
KMOD            = atapi_mod
SRCS            = atapi.c wdc.h #wcd.h
NOMAN           =
BINDIR          = /lkm
CFLAGS         += -nostdinc -I. -I/sys -DATAPI -DATAPI_MODULE
CLEANFILES     += $(KMOD) machine wdc.h wcd.h *.b
LN              = ln -f -s
EXPORT_SYMS	= _atapi_mod _atapi_debug_ptr _atapi_intr_ptr \
		  _atapi_request_callback_ptr _atapi_request_immediate_ptr \
		  _atapi_request_wait_ptr _atapi_start_ptr

load:           ${PROG}
		/sbin/modload -u -e atapi_mod $(PROG)

unload:         ${PROG}
		/sbin/modunload -n atapi

wdc.h:
		echo "#define NWDC 2" > wdc.h

#		echo "#define NWD 4" >> wdc.h

wcd.h:
		echo "#define NWCD 1" > wcd.h

.c.o:
		-@$(LN) /sys/i386/include machine
		$(CC) $(CFLAGS) -c $<
		-@rm -f machine

.include <bsd.kmod.mk>

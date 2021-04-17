ALL=radius_example

include ../src/build.rules

CFLAGS += -I.
CFLAGS += -I../src
CFLAGS += -I../src/utils

LIBS = ../src/radius/libradius.a
LIBS += ../src/crypto/libcrypto.a
LIBS += ../src/utils/libutils.a
LLIBS = -lrt

#CLAGS += -DCONFIG_IPV6

OBJS_ex = radius_example.o

_OBJS_VAR := OBJS_ex
include ../src/objs.mk

_OBJS_VAR := LIBS
include ../src/objs.mk

radius_example: $(OBJS_ex) $(LIBS)
	$(LDO) $(LDFLAGS) -o radius_example $(OBJS_ex) $(LIBS) $(LLIBS)

clean: common-clean
	rm -f core *~ *.o *.d

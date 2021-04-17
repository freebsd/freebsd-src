ALL=json
include ../rules.include

OBJS += $(SRC)/utils/base64.o
OBJS += $(SRC)/utils/common.o
OBJS += $(SRC)/utils/json.o
OBJS += $(SRC)/utils/os_unix.o
OBJS += $(SRC)/utils/wpa_debug.o
OBJS += $(SRC)/utils/wpabuf.o

OBJS += json.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

json: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)

clean: common-clean
	rm -f json *~ *.o *.d ../*~ ../*.o ../*.d

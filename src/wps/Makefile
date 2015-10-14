all: libwps.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libwps.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DCONFIG_P2P
CFLAGS += -DCONFIG_WPS_OOB
CFLAGS += -DCONFIG_WPS_NFC

LIB_OBJS= \
	http_client.o \
	httpread.o \
	http_server.o \
	ndef.o \
	upnp_xml.o \
	wps_attr_build.o \
	wps_attr_parse.o \
	wps_attr_process.o \
	wps.o \
	wps_common.o \
	wps_dev_attr.o \
	wps_enrollee.o \
	wps_er.o \
	wps_er_ssdp.o \
	wps_module_tests.o \
	wps_registrar.o \
	wps_upnp_ap.o \
	wps_upnp.o \
	wps_upnp_event.o \
	wps_upnp_ssdp.o \
	wps_upnp_web.o

libwps.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

all: libwpadbus.a

clean:
	rm -f *~ *.o *.d
	rm -f libwpadbus.a

install:
	@echo Nothing to be made.

ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

CFLAGS += -I../../src -I../../src/utils


Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<


ifdef CONFIG_WPS
CFLAGS += -DCONFIG_WPS
endif

CFLAGS += -DCONFIG_CTRL_IFACE_DBUS_NEW
CFLAGS += -DCONFIG_CTRL_IFACE_DBUS

ifndef DBUS_LIBS
DBUS_LIBS := $(shell pkg-config --libs dbus-1)
endif
ifndef DBUS_INCLUDE
DBUS_INCLUDE := $(shell pkg-config --cflags dbus-1)
endif
ifdef CONFIG_CTRL_IFACE_DBUS_INTRO
CFLAGS += -DCONFIG_CTRL_IFACE_DBUS_INTRO
DBUS_INCLUDE += $(shell xml2-config --cflags)
DBUS_LIBS += $(shell xml2-config --libs)
endif

dbus_version=$(subst ., ,$(shell pkg-config --modversion dbus-1))
DBUS_VERSION_MAJOR=$(word 1,$(dbus_version))
DBUS_VERSION_MINOR=$(word 2,$(dbus_version))
ifeq ($(DBUS_VERSION_MAJOR),)
DBUS_VERSION_MAJOR=0
endif
ifeq ($(DBUS_VERSION_MINOR),)
DBUS_VERSION_MINOR=0
endif
DBUS_INCLUDE += -DDBUS_VERSION_MAJOR=$(DBUS_VERSION_MAJOR)
DBUS_INCLUDE += -DDBUS_VERSION_MINOR=$(DBUS_VERSION_MINOR)

CFLAGS += $(DBUS_INCLUDE)

LIB_OBJS= \
	dbus_common.o \
	dbus_old.o \
	dbus_old_handlers.o \
	dbus_new.o \
	dbus_new_handlers.o \
	dbus_new_helpers.o \
	dbus_new_introspect.o \
	dbus_dict_helpers.o

ifdef CONFIG_WPS
LIB_OBJS += dbus_old_handlers_wps.o
LIB_OBJS += dbus_new_handlers_wps.o
endif

libwpadbus.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

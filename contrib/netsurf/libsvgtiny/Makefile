# Component settings
COMPONENT := svgtiny
COMPONENT_VERSION := 0.1.1
# Default to a static library
COMPONENT_TYPE ?= lib-static

# Setup the tooling
PREFIX ?= /opt/netsurf
NSSHARED ?= $(PREFIX)/share/netsurf-buildsystem
include $(NSSHARED)/makefiles/Makefile.tools

TESTRUNNER := $(ECHO)

# Toolchain flags
WARNFLAGS := -Wall -W -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs -pedantic
# BeOS/Haiku/AmigaOS standard library headers create warnings
ifneq ($(TARGET),beos)
  ifneq ($(TARGET),amiga)
    WARNFLAGS := $(WARNFLAGS) -Werror
  endif
endif

CFLAGS := -D_BSD_SOURCE -I$(CURDIR)/include/ \
	-I$(CURDIR)/src $(WARNFLAGS) $(CFLAGS)
ifneq ($(GCCVER),2)
  CFLAGS := $(CFLAGS) -std=c99
else
  # __inline__ is a GCCism
  CFLAGS := $(CFLAGS) -Dinline="__inline__"
endif

# libdom
ifneq ($(PKGCONFIG),)
  CFLAGS := $(CFLAGS) \
		$(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --cflags libdom libwapcaplet)
  LDFLAGS := $(LDFLAGS) -lm \
		$(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --libs libdom libwapcaplet)
else
  CFLAGS := $(CFLAGS) -I$(PREFIX)/include
  LDFLAGS := $(CFLAGS) -ldom -lwapcaplet -lexpat -lm
endif

include $(NSBUILD)/Makefile.top

# Extra installation rules
I := /include
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/svgtiny.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR)/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR):$(OUTPUT)

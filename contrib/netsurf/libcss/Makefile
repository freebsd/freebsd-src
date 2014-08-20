# Component settings
COMPONENT := css
COMPONENT_VERSION := 0.3.0
# Default to a static library
COMPONENT_TYPE ?= lib-static

# Setup the tooling
PREFIX ?= /opt/netsurf
NSSHARED ?= $(PREFIX)/share/netsurf-buildsystem
include $(NSSHARED)/makefiles/Makefile.tools

TESTRUNNER := $(PERL) $(NSTESTTOOLS)/testrunner.pl

# Toolchain flags
WARNFLAGS := -Wall -W -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
# BeOS/Haiku/AmigaOS4 standard library headers create warnings
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

# Parserutils & wapcaplet
ifneq ($(findstring clean,$(MAKECMDGOALS)),clean)
  ifneq ($(PKGCONFIG),)
    CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libparserutils libwapcaplet --cflags)
    LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libparserutils libwapcaplet --libs)
  else
    CFLAGS := $(CFLAGS) -I$(PREFIX)/include
    LDFLAGS := $(LDFLAGS) -lparserutils -lwapcaplet
  endif
endif

include $(NSBUILD)/Makefile.top

# Extra installation rules
I := /include/libcss

INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/computed.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/errors.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/font_face.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/fpmath.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/functypes.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/hint.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/libcss.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/properties.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/select.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/stylesheet.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/libcss/types.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR)/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR):$(OUTPUT)

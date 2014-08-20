# Component settings
COMPONENT := rosprite
COMPONENT_VERSION := 0.1.1
# Default to a static library
COMPONENT_TYPE ?= lib-static

# FIXME
# palette2c tool needs building and installing as binary target
# examples/example.c needs a build target

# Setup the tooling
PREFIX ?= /opt/netsurf
NSSHARED ?= $(PREFIX)/share/netsurf-buildsystem
include $(NSSHARED)/makefiles/Makefile.tools

# Toolchain flags
WARNFLAGS := -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes \
	-Wnested-externs -pedantic -std=c99 \
	-Wno-format-zero-length -Wformat-security -Wstrict-aliasing=2 \
	-Wmissing-format-attribute -Wunused \
	-Wformat=2 -Werror-implicit-function-declaration \
	-Wmissing-declarations -Wmissing-prototypes

# BeOS/Haiku standard library headers create warnings
ifneq ($(TARGET),beos)
  WARNFLAGS := $(WARNFLAGS) -Werror
endif

CFLAGS := -I$(CURDIR)/include/ $(WARNFLAGS) $(CFLAGS)

ifneq ($(GCCVER),2)
  CFLAGS := $(CFLAGS) -std=c99
else
  # __inline__ is a GCCism
  CFLAGS := $(CFLAGS) -Dinline="__inline__"
endif

include $(NSBUILD)/Makefile.top

# Extra installation rules
I := /include
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/librosprite.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR)/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR):$(OUTPUT)

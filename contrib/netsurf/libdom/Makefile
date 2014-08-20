# Component settings
COMPONENT := dom
COMPONENT_VERSION := 0.1.0
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
# BeOS/Haiku standard library headers generate warnings
ifneq ($(TARGET),beos)
  WARNFLAGS := $(WARNFLAGS) -Werror
endif
# AmigaOS needs this to avoid warnings
ifeq ($(TARGET),amiga)
  CFLAGS := -U__STRICT_ANSI__ $(CFLAGS)
endif
CFLAGS := -D_BSD_SOURCE -I$(CURDIR)/include/ \
	-I$(CURDIR)/src -I$(CURDIR)/binding $(WARNFLAGS) $(CFLAGS)
# Some gcc2 versions choke on -std=c99, and it doesn't know about it anyway
ifneq ($(GCCVER),2)
  CFLAGS := -std=c99 $(CFLAGS)
endif

# Parserutils & wapcaplet
ifneq ($(findstring clean,$(MAKECMDGOALS)),clean)
  ifneq ($(PKGCONFIG),)
    CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libparserutils --cflags)
    CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libwapcaplet --cflags)
    LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libparserutils --libs)
    LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libwapcaplet --libs)
  else
    CFLAGS := $(CFLAGS) -I$(PREFIX)/include
    LDFLAGS := $(LDFLAGS) -lparserutils -lwapcaplet
  endif
endif

include $(NSBUILD)/Makefile.top

# Extra installation rules
Is := include/dom
I := /include/dom
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/dom.h;$(Is)/functypes.h

Is := include/dom/core
I := /include/dom/core
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/attr.h;$(Is)/characterdata.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/cdatasection.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/comment.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/doc_fragment.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/document.h;$(Is)/document_type.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/entity_ref.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/element.h;$(Is)/exceptions.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/implementation.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/namednodemap.h;$(Is)/node.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/nodelist.h;$(Is)/string.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/pi.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/text.h;$(Is)/typeinfo.h

Is := include/dom/events
I := /include/dom/events
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/event.h;$(Is)/ui_event.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/custom_event.h;$(Is)/mouse_event.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/keyboard_event.h;$(Is)/text_event.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/mouse_wheel_event.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/mouse_multi_wheel_event.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/mutation_event.h;$(Is)/event_target.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/mutation_name_event.h;$(Is)/events.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/event_listener.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/document_event.h

Is := include/dom/html
I := /include/dom/html
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_document.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_collection.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_html_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_head_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_link_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_title_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_body_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_meta_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_form_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_button_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_input_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_select_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_text_area_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_option_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_opt_group_element.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_options_collection.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):$(Is)/html_hr_element.h

INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR)/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /$(LIBDIR):$(OUTPUT)

ifeq ($(WITH_LIBXML_BINDING),yes)
  REQUIRED_PKGS := $(REQUIRED_PKGS) libxml-2.0
endif

ifeq ($(WITH_HUBBUB_BINDING),yes)
  REQUIRED_PKGS := $(REQUIRED_PKGS) libhubbub
endif


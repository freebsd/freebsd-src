ifeq ($(WITH_HUBBUB_BINDING),yes)
  DIR_SOURCES := parser.c

  DIR_INSTALL_ITEMS := /include/dom/bindings/hubbub:errors.h;parser.h

  # Hubbub
  ifneq ($(PKGCONFIG),)
    CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libhubbub --cflags)
    LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libhubbub --libs)
  else
    CFLAGS := $(CFLAGS) -I$(PREFIX)/include
    LDFLAGS := $(LDFLAGS) -lhubbub -lparserutils
  endif
endif

include $(NSBUILD)/Makefile.subdir


#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
# Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
#
# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Look at Makefile.config for configuration options.
#
# Tested on unix platforms (building for GTK and cross-compiling for RO) and
# on RO (building for RO).
#
# To clean, invoke as above, with the 'clean' target
#
# To build developer Doxygen generated documentation, invoke as above,
# with the 'docs' target:
#   make docs
#

all: all-program

# Determine host type
# NOTE: HOST determination on RISC OS could fail because of missing bug fixes
#	in UnixLib which only got addressed in UnixLib 5 / GCCSDK 4.
#	When you don't have 'uname' available, you will see:
#	  File 'uname' not found
#	When you do and using a 'uname' compiled with a buggy UnixLib, you
#	will see the following printed on screen:
#	  RISC OS
#	In both cases HOST make variable is empty and we recover from that by
#	assuming we're building on RISC OS.
#	In case you don't see anything printed (including the warning), you
#	have an up-to-date RISC OS build system. ;-)
HOST := $(shell uname -s)

# Sanitise host
# TODO: Ideally, we want the equivalent of s/[^A-Za-z0-9]/_/g here
HOST := $(subst .,_,$(subst -,_,$(subst /,_,$(HOST))))

ifeq ($(HOST),)
  HOST := riscos
  $(warning Build platform determination failed but that's a known problem for RISC OS so we're assuming a native RISC OS build.)
else
  ifeq ($(HOST),RISC OS)
    # Fixup uname -s returning "RISC OS"
    HOST := riscos
  endif
endif

ifeq ($(HOST),riscos)
  # Build happening on RO platform, default target is RO backend
  ifeq ($(TARGET),)
    TARGET := riscos
  endif
else
  ifeq ($(HOST),BeOS)
    HOST := beos
  endif
  ifeq ($(HOST),Haiku)
    # Haiku implements the BeOS API
    HOST := beos
  endif
  ifeq ($(HOST),beos)
    # Build happening on BeOS platform, default target is BeOS backend
    ifeq ($(TARGET),)
      TARGET := beos
    endif
  else
    ifeq ($(HOST),AmigaOS)
      HOST := amiga
      ifeq ($(TARGET),)
        TARGET := amiga
      endif
    else
      ifeq ($(HOST),Darwin)
        HOST := macosx
        ifeq ($(TARGET),)
          TARGET := cocoa
        endif
      endif  
      ifeq ($(HOST),FreeMiNT)
        HOST := mint
      endif
      ifeq ($(HOST),mint)
        ifeq ($(TARGET),)
          TARGET := atari
        endif
      endif
      ifeq ($(findstring MINGW,$(HOST)),MINGW)
        # MSYS' uname reports the likes of "MINGW32_NT-6.0"
        HOST := windows
      endif
      ifeq ($(HOST),windows)
        ifeq ($(TARGET),)
          TARGET := windows
        endif
      endif

      # Default target is GTK backend
      ifeq ($(TARGET),)
        TARGET := gtk
      endif
    endif
  endif
endif
SUBTARGET =
RESOURCES =

ifneq ($(TARGET),riscos)
  ifneq ($(TARGET),gtk)
    ifneq ($(TARGET),beos)
      ifneq ($(findstring amiga,$(TARGET)),amiga)
        ifneq ($(TARGET),framebuffer)
          ifneq ($(TARGET),windows)
            ifneq ($(TARGET),atari)
              ifneq ($(TARGET),cocoa)
                ifneq ($(TARGET),monkey)
                  $(error Unknown TARGET "$(TARGET)", should either be "riscos", "gtk", "beos", "amiga", "framebuffer", "windows", "atari" or "cocoa" or "monkey")
                endif
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch
STRIP=strip
SPLIT_MESSAGES=$(PERL) utils/split-messages.pl

# Override this only if the host compiler is called something different
HOST_CC := gcc

ifeq ($(TARGET),riscos)
  ifeq ($(HOST),riscos)
    # Build for RO on RO
    GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
    CCRES := ccres
    TPLEXT :=
    MAKERUN := makerun
    SQUEEZE := squeeze
    RUNEXT :=
    CC := gcc
    CXX := g++
    EXEEXT :=
    PKG_CONFIG :=
  else
    # Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
    # either using GCCSDK 4 - ELF)
    ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/env),)
        GCCSDK_INSTALL_ENV := /opt/netsurf/arm-unknown-riscos/env
      else
        GCCSDK_INSTALL_ENV := /home/riscos/env
      endif
    endif

    ifeq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/cross/bin),)
        GCCSDK_INSTALL_CROSSBIN := /opt/netsurf/arm-unknown-riscos/cross/bin
      else
        GCCSDK_INSTALL_CROSSBIN := /home/riscos/cross/bin
      endif
    endif

    CCRES := $(GCCSDK_INSTALL_CROSSBIN)/ccres
    TPLEXT := ,fec
    MAKERUN := $(GCCSDK_INSTALL_CROSSBIN)/makerun
    SQUEEZE := $(GCCSDK_INSTALL_CROSSBIN)/squeeze
    RUNEXT := ,feb
    CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
    ifneq (,$(findstring arm-unknown-riscos-gcc,$(CC)))
      SUBTARGET := -elf
      EXEEXT := ,e1f
      ELF2AIF := $(GCCSDK_INSTALL_CROSSBIN)/elf2aif
    else
     SUBTARGET := -aof
     EXEEXT := ,ff8
    endif
    CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
    PKG_CONFIG := $(GCCSDK_INSTALL_ENV)/ro-pkg-config
  endif
else
  ifeq ($(TARGET),beos)
    # Building for BeOS/Haiku
    #ifeq ($(HOST),beos)
      # Build for BeOS on BeOS
      GCCSDK_INSTALL_ENV := /boot/develop
      CC := gcc
      CXX := g++
      EXEEXT :=
      PKG_CONFIG :=
    #endif
  else
    ifeq ($(TARGET),windows)
      ifneq ($(HOST),windows)
        # Set Mingw defaults
        GCCSDK_INSTALL_ENV ?= /opt/netsurf/i686-w64-mingw32/env
        GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/i686-w64-mingw32/cross/bin

        CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
        WINDRES := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*windres)

        PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
      else
        # Building on Windows
        CC := gcc
        PKG_CONFIG :=
      endif
    else
      ifeq ($(findstring amiga,$(TARGET)),amiga)
        ifeq ($(findstring amiga,$(HOST)),amiga)
          PKG_CONFIG := pkg-config
        else
          ifeq ($(TARGET),amigaos3)
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-unknown-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-unknown-amigaos/cross/bin

            SUBTARGET = os3
          else
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/ppc-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/ppc-amigaos/cross/bin
          endif

          override TARGET := amiga

          CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)

          PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
        endif
      else
        ifeq ($(TARGET),cocoa)
          PKG_CONFIG := PKG_CONFIG_PATH="$(PKG_CONFIG_PATH):/usr/local/lib/pkgconfig" pkg-config
        else
          ifeq ($(TARGET),atari)
            ifeq ($(HOST),atari)
              PKG_CONFIG := pkg-config
            else
              ifeq ($(HOST),mint)
                PKG_CONFIG := pkg-config
              else
                GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-atari-mint/env
                GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-atari-mint/cross/bin

                CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)

                PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
              endif
            endif
          else
	    ifeq ($(TARGET),monkey)
              ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
                PKG_CONFIG := pkg-config
              else
                PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config                
              endif

              ifneq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
                CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
                CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
              endif
	    else
              ifeq ($(findstring framebuffer,$(TARGET)),framebuffer)
                ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
                  PKG_CONFIG := pkg-config
                else
                  PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config                
                endif

                ifneq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
                  CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
                  CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
                endif
              else
                # All native targets (GTK)
                PKG_CONFIG := pkg-config
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

# compiler versioning to adjust warning flags
CC_VERSION := $(shell $(CC) -dumpversion)
CC_MAJOR := $(word 1,$(subst ., ,$(CC_VERSION)))
CC_MINOR := $(word 2,$(subst ., ,$(CC_VERSION)))
define cc_ver_ge
$(shell expr $(CC_MAJOR) \>= $(1) \& $(CC_MINOR) \>= $(2))
endef

# CCACHE
ifeq ($(origin CCACHE),undefined)
  CCACHE=$(word 1,$(shell ccache -V 2>/dev/null))
endif
CC := $(CCACHE) $(CC)

# Target paths
OBJROOT = build-$(HOST)-$(TARGET)$(SUBTARGET)
DEPROOT := $(OBJROOT)/deps
TOOLROOT := $(OBJROOT)/tools


# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Parameters to add to CFLAGS
# 3: Parameters to add to LDFLAGS
# 4: Human-readable name for the feature
define feature_enabled
  ifeq ($$(NETSURF_USE_$(1)),YES)
    CFLAGS += $(2)
    LDFLAGS += $(3)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	enabled       (NETSURF_USE_$(1) := YES))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(4)	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES or NO)
  endif
endef

# Extend flags with appropriate values from pkg-config for enabled features
#
# 1: pkg-config required modules for feature
# 2: Human-readable name for the feature
define pkg_config_find_and_add
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  PKG_CONFIG_$(1)_EXISTS := $$(shell $$(PKG_CONFIG) --exists $(1) && echo yes)

  ifeq ($$(PKG_CONFIG_$(1)_EXISTS),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(1))
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(1))
      ifneq ($(MAKECMDGOALS),clean)
        $$(info PKG.CNFG: $(2) ($(1))	enabled)
      endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info PKG.CNFG: $(2) ($(1))	failed)
      $$(error Unable to find library for: $(2) ($(1)))
    endif
  endif
endef

# Extend flags with appropriate values from pkg-config for enabled features
#
# 1: Feature name (ie, NETSURF_USE_RSVG -> RSVG)
# 2: pkg-config required modules for feature
# 3: Human-readable name for the feature
define pkg_config_find_and_add_enabled
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  NETSURF_FEATURE_$(1)_AVAILABLE := $$(shell $$(PKG_CONFIG) --exists $(2) && echo yes)

  ifeq ($$(NETSURF_USE_$(1)),YES)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	enabled       (NETSURF_USE_$(1) := YES))
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	failed        (NETSURF_USE_$(1) := YES))
        $$(error Unable to find library for: $(3) ($(2)))
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),AUTO)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-enabled  (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := YES
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-disabled (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := NO
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
      $$(error NETSURF_USE_$(1) must be YES, NO, or AUTO)
    endif
  endif
endef

# ----------------------------------------------------------------------------
# General flag setup
# ----------------------------------------------------------------------------

# Set up the WARNFLAGS here so that they can be overridden in the Makefile.config
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Wuninitialized
ifneq ($(CC_MAJOR),2)
  WARNFLAGS += -Wno-unused-parameter 
endif
# deal with lots of unwanted warnings from javascript
ifeq ($(call cc_ver_ge,4,6),1)
	WARNFLAGS += -Wno-unused-but-set-variable
endif

# Pull in the configuration
include Makefile.defaults

$(eval $(call feature_enabled,JPEG,-DWITH_JPEG,-ljpeg,JPEG (libjpeg)))

$(eval $(call feature_enabled,HARU_PDF,-DWITH_PDF_EXPORT,-lhpdf -lpng,PDF export (haru)))
$(eval $(call feature_enabled,LIBICONV_PLUG,-DLIBICONV_PLUG,,glibc internal iconv))

# common libraries without pkg-config support
LDFLAGS += -lz

# add top level and build directory to include search path
CFLAGS += -I. -I$(OBJROOT)

# export the user agent format
CFLAGS += -DNETSURF_UA_FORMAT_STRING=\"$(NETSURF_UA_FORMAT_STRING)\"

# set the default homepage to use
CFLAGS += -DNETSURF_HOMEPAGE=\"$(NETSURF_HOMEPAGE)\"

# ----------------------------------------------------------------------------
# General make rules
# ----------------------------------------------------------------------------

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

$(TOOLROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(TOOLROOT)"
	$(Q)$(MKDIR) $(TOOLROOT)
	$(Q)$(TOUCH) $(TOOLROOT)/created

CLEANS := clean-target clean-testament

POSTEXES :=

# ----------------------------------------------------------------------------
# Target specific setup
# ----------------------------------------------------------------------------

include $(TARGET)/Makefile.target

# ----------------------------------------------------------------------------
# General source file setup
# ----------------------------------------------------------------------------

# Content sources
include content/Makefile

# Content fetchers sources
include content/fetchers/Makefile

# CSS sources
include css/Makefile

# render sources
include render/Makefile

# utility sources
include utils/Makefile

# http utility sources
include utils/http/Makefile

# Desktop sources
include desktop/Makefile

# Javascript source
include javascript/Makefile

# Image content handler sources
include image/Makefile

# PDF saving sources
include desktop/save_pdf/Makefile

# S_COMMON are sources common to all builds
S_COMMON := $(S_CONTENT) $(S_FETCHERS) $(S_CSS)	$(S_RENDER) $(S_UTILS) \
	$(S_HTTP) $(S_DESKTOP) $(S_JAVASCRIPT)


# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# Force exapnsion of source file list
SOURCES := $(SOURCES)

ifeq ($(SOURCES),)
$(error Unable to build NetSurf, could not determine set of sources to build)
endif

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.m,%.o,$(patsubst %.s,%.o,$(SOURCES))))))))

$(EXETARGET): $(OBJECTS) $(RESOURCES)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CXX) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif
ifeq ($(TARGET),windows)
	$(Q)$(TOUCH) windows/res/preferences
endif
ifeq ($(TARGET),gtk)
	$(Q)$(TOUCH) gtk/res/toolbarIndices
endif
ifeq ($(NETSURF_STRIP_BINARY),YES)
	$(VQ)echo "   STRIP: $(EXETARGET)"
	$(Q)$(STRIP) $(EXETARGET)
endif
ifeq ($(TARGET),beos)
	$(VQ)echo "    XRES: $(EXETARGET)"
	$(Q)$(BEOS_XRES) -o $(EXETARGET) $(RSRC_BEOS)
	$(VQ)echo "  SETVER: $(EXETARGET)"
	$(Q)$(BEOS_SETVER) $(EXETARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "NetSurf $(VERSION_FULL)" \
                -long "NetSurf $(VERSION_FULL) © 2003 - 2013 The NetSurf Developers"
	$(VQ)echo " MIMESET: $(EXETARGET)"
	$(Q)$(BEOS_MIMESET) $(EXETARGET)
endif

ifeq ($(TARGET),beos)
$(RDEF_IMP_BEOS): $(RDEP_BEOS)
	$(VQ)echo "     GEN: $@"
	$(Q)n=5000; for f in $^; do echo "resource($$n,\"$${f#beos/res/}\") #'data' import \"$${f#beos/}\";"; n=$$(($$n+1)); done > $@

$(RSRC_BEOS): $(RDEF_BEOS) $(RDEF_IMP_BEOS)
	$(VQ)echo "      RC: $<"
	$(Q)$(BEOS_RC) -I beos -o $@ $^
endif

ifeq ($(TARGET),riscos)
  # Native RO build is different as 1) it can't do piping and 2) ccres on
  # RO does not understand Unix filespec
  ifeq ($(HOST),riscos)
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) -o processed_template $(2)
	$$(Q)$$(CCRES) processed_template $$(subst /,.,$$@)
	$$(Q)$(RM) processed_template
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  else
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)mkdir -p !NetSurf/Resources/$(1)
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) $(2) | $$(CCRES) - $$@
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  endif

clean-templates:
	$(VQ)echo "   CLEAN: $(CLEAN_TEMPLATES)"
	$(Q)$(RM) $(CLEAN_TEMPLATES)
CLEANS += clean-templates

$(eval $(foreach TPL,$(TPL_RISCOS), \
	$(call compile_template,$(notdir $(TPL)),$(TPL))))
endif

clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)
	$(call clean_install_messages, !NetSurf/Resources)

clean-testament:
	$(VQ)echo "   CLEAN: testament.h"
	$(Q)$(RM) $(OBJROOT)/testament.h

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir

all-program: $(EXETARGET) post-exe
	$(call split_install_messages, any, !NetSurf/Resources)

.PHONY: testament
testament $(OBJROOT)/testament.h:
	$(Q)$(PERL) utils/git-testament.pl $(CURDIR) $(OBJROOT)/testament.h

post-exe: $(POSTEXES)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) Makefile.config

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
ifeq ($(CC_MAJOR),2)
# simpler deps tracking for gcc2...
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef
else
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef
endif

define compile_target_cpp
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CXX) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "ASSEMBLE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(ASFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.d)),$(subst /,_,$(SOURCE:.cpp=.o)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.d)),$(subst /,_,$(SOURCE:.m=.o)))))

# Cannot currently generate dep files for S files because they're objasm
# when we move to gas format, we will be able to.

#$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
#	$(call dependency_generate_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.d)),$(subst /,_,$(SOURCE:.s=.o)))))

ifneq ($(MAKECMDGOALS),clean)
-include $(sort $(addprefix $(DEPROOT)/,$(DEPFILES)))
-include $(D_JSAPI_BINDING)
endif

# And rules to build the objects themselves...

$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call compile_target_cpp,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.o)),$(subst /,_,$(SOURCE:.cpp=.d)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.o)),$(subst /,_,$(SOURCE:.m=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

.PHONY: all clean docs install package-$(TARGET) package install-$(TARGET)

clean: $(CLEANS)

# Target builds a distribution package
package: all-program package-$(TARGET)

FAT_LANGUAGES=de en fr it nl
# 1 = front end name (gtk, ro, ami, etc)
# 2 = Destination directory (where resources being installed, creates en/Messages etc)
# 3 = suffix after language name 
define split_install_messages
	$(foreach LANG, $(FAT_LANGUAGES), @echo MSGSPLIT: $(1)/$(LANG) to $(2)
		$(Q)mkdir -p $(2)/$(LANG)$(3)
		$(Q)$(SPLIT_MESSAGES) -l $(LANG) -p $(1) -f messages resources/FatMessages | gzip -9n > $(2)$(3)/$(LANG)/Messages
	)
endef

# Clean Message target
# 1 = Destination directory (where resources being installed, creates en/Messages etc)
# 2 = suffix after language name
define clean_install_messages
	$(foreach LANG, $(FAT_LANGUAGES), @echo MSGCLEAN: $(LANG) in $(1)
		$(Q)$(RM) -f $(1)$(2)/$(LANG)/Messages
	)
endef

.PHONY: messages-split-tfx messages-fetch-tfx messages-import-tfx

# split fat messages into properties files suitable for uploading to transifex
messages-split-tfx:
	for splitlang in $(FAT_LANGUAGES);do perl ./utils/split-messages.pl -l $${splitlang} -f transifex -p any -o Messages.any.$${splitlang}.tfx resources/FatMessages;done

# download property files from transifex
messages-fetch-tfx:
	for splitlang in $(FAT_LANGUAGES);do $(RM) Messages.any.$${splitlang}.tfx ; perl ./utils/fetch-transifex.pl -w insecure -l $${splitlang} -o Messages.any.$${splitlang}.tfx ;done

# merge property files into fat messages
messages-import-tfx: messages-fetch-tfx
	for tfxlang in $(FAT_LANGUAGES);do perl ./utils/import-messages.pl -l $${tfxlang} -p any -f transifex -o resources/FatMessages -i resources/FatMessages -I Messages.any.$${tfxlang}.tfx ; $(RM) Messages.any.$${tfxlang}.tfx; done

# Target installs executable on the host system 
install: all-program install-$(TARGET)

docs:
	doxygen Docs/Doxyfile

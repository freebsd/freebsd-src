#
# Device Tree Compiler
#

#
# Version information will be constructed in this order:
# EXTRAVERSION might be "-rc", for example.
# LOCAL_VERSION is likely from command line.
# CONFIG_LOCALVERSION from some future config system.
#
VERSION = 1
PATCHLEVEL = 4
SUBLEVEL = 6
EXTRAVERSION =
LOCAL_VERSION =
CONFIG_LOCALVERSION =

CPPFLAGS = -I libfdt -I .
WARNINGS = -Wall -Wpointer-arith -Wcast-qual -Wnested-externs \
	-Wstrict-prototypes -Wmissing-prototypes -Wredundant-decls -Wshadow
CFLAGS = -g -Os $(SHAREDLIB_CFLAGS) -Werror $(WARNINGS)

BISON = bison
LEX = flex
SWIG = swig
PKG_CONFIG ?= pkg-config

INSTALL = /usr/bin/install
DESTDIR =
PREFIX = $(HOME)
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

HOSTOS := $(shell uname -s | tr '[:upper:]' '[:lower:]' | \
	    sed -e 's/\(cygwin\|msys\).*/\1/')

ifeq ($(HOSTOS),darwin)
SHAREDLIB_EXT     = dylib
SHAREDLIB_CFLAGS  = -fPIC
SHAREDLIB_LDFLAGS = -fPIC -dynamiclib -Wl,-install_name -Wl,
else ifeq ($(HOSTOS),$(filter $(HOSTOS),msys cygwin))
SHAREDLIB_EXT     = so
SHAREDLIB_CFLAGS  =
SHAREDLIB_LDFLAGS = -shared -Wl,--version-script=$(LIBFDT_version) -Wl,-soname,
else
SHAREDLIB_EXT     = so
SHAREDLIB_CFLAGS  = -fPIC
SHAREDLIB_LDFLAGS = -fPIC -shared -Wl,--version-script=$(LIBFDT_version) -Wl,-soname,
endif

#
# Overall rules
#
ifdef V
VECHO = :
else
VECHO = echo "	"
ARFLAGS = rc
.SILENT:
endif

NODEPTARGETS = clean
ifeq ($(MAKECMDGOALS),)
DEPTARGETS = all
else
DEPTARGETS = $(filter-out $(NODEPTARGETS),$(MAKECMDGOALS))
endif

#
# Rules for versioning
#

DTC_VERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
VERSION_FILE = version_gen.h

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)

nullstring :=
space	:= $(nullstring) # end of line

localver_config = $(subst $(space),, $(string) \
			      $(patsubst "%",%,$(CONFIG_LOCALVERSION)))

localver_cmd = $(subst $(space),, $(string) \
			      $(patsubst "%",%,$(LOCALVERSION)))

localver_scm = $(shell $(CONFIG_SHELL) ./scripts/setlocalversion)
localver_full  = $(localver_config)$(localver_cmd)$(localver_scm)

dtc_version = $(DTC_VERSION)$(localver_full)

# Contents of the generated version file.
define filechk_version
	(echo "#define DTC_VERSION \"DTC $(dtc_version)\""; )
endef

define filechk
	set -e;					\
	echo '	CHK $@';			\
	mkdir -p $(dir $@);			\
	$(filechk_$(1)) < $< > $@.tmp;		\
	if [ -r $@ ] && cmp -s $@ $@.tmp; then	\
		rm -f $@.tmp;			\
	else					\
		echo '	UPD $@';		\
		mv -f $@.tmp $@;		\
	fi;
endef


include Makefile.convert-dtsv0
include Makefile.dtc
include Makefile.utils

BIN += convert-dtsv0
BIN += dtc
BIN += fdtdump
BIN += fdtget
BIN += fdtput
BIN += fdtoverlay

SCRIPTS = dtdiff

all: $(BIN) libfdt

# We need both Python and swig to build pylibfdt.
.PHONY: maybe_pylibfdt
maybe_pylibfdt: FORCE
	if $(PKG_CONFIG) --cflags python2 >/dev/null 2>&1; then \
		if which swig >/dev/null 2>&1; then \
			can_build=yes; \
		fi; \
	fi; \
	if [ "$$can_build" = "yes" ]; then \
		$(MAKE) pylibfdt; \
	else \
		echo "## Skipping pylibfdt (install python dev and swig to build)"; \
	fi

ifeq ($(NO_PYTHON),)
all: maybe_pylibfdt
endif


ifneq ($(DEPTARGETS),)
-include $(DTC_OBJS:%.o=%.d)
-include $(CONVERT_OBJS:%.o=%.d)
-include $(FDTDUMP_OBJS:%.o=%.d)
-include $(FDTGET_OBJS:%.o=%.d)
-include $(FDTPUT_OBJS:%.o=%.d)
-include $(FDTOVERLAY_OBJS:%.o=%.d)
endif



#
# Rules for libfdt
#
LIBFDT_objdir = libfdt
LIBFDT_srcdir = libfdt
LIBFDT_archive = $(LIBFDT_objdir)/libfdt.a
LIBFDT_lib = $(LIBFDT_objdir)/libfdt-$(DTC_VERSION).$(SHAREDLIB_EXT)
LIBFDT_include = $(addprefix $(LIBFDT_srcdir)/,$(LIBFDT_INCLUDES))
LIBFDT_version = $(addprefix $(LIBFDT_srcdir)/,$(LIBFDT_VERSION))

include $(LIBFDT_srcdir)/Makefile.libfdt

.PHONY: libfdt
libfdt: $(LIBFDT_archive) $(LIBFDT_lib)

$(LIBFDT_archive): $(addprefix $(LIBFDT_objdir)/,$(LIBFDT_OBJS))
$(LIBFDT_lib): $(addprefix $(LIBFDT_objdir)/,$(LIBFDT_OBJS))

libfdt_clean:
	@$(VECHO) CLEAN "(libfdt)"
	rm -f $(addprefix $(LIBFDT_objdir)/,$(STD_CLEANFILES))
	rm -f $(LIBFDT_objdir)/*.so

ifneq ($(DEPTARGETS),)
-include $(LIBFDT_OBJS:%.o=$(LIBFDT_objdir)/%.d)
endif

# This stops make from generating the lex and bison output during
# auto-dependency computation, but throwing them away as an
# intermediate target and building them again "for real"
.SECONDARY: $(DTC_GEN_SRCS) $(CONVERT_GEN_SRCS)

install-bin: all $(SCRIPTS)
	@$(VECHO) INSTALL-BIN
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) $(BIN) $(SCRIPTS) $(DESTDIR)$(BINDIR)

install-lib: all
	@$(VECHO) INSTALL-LIB
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) $(LIBFDT_lib) $(DESTDIR)$(LIBDIR)
	ln -sf $(notdir $(LIBFDT_lib)) $(DESTDIR)$(LIBDIR)/$(LIBFDT_soname)
	ln -sf $(LIBFDT_soname) $(DESTDIR)$(LIBDIR)/libfdt.$(SHAREDLIB_EXT)
	$(INSTALL) -m 644 $(LIBFDT_archive) $(DESTDIR)$(LIBDIR)

install-includes:
	@$(VECHO) INSTALL-INC
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 644 $(LIBFDT_include) $(DESTDIR)$(INCLUDEDIR)

install: install-bin install-lib install-includes

ifeq ($(NO_PYTHON),)
install: install_pylibfdt
endif

$(VERSION_FILE): Makefile FORCE
	$(call filechk,version)


dtc: $(DTC_OBJS)

convert-dtsv0: $(CONVERT_OBJS)
	@$(VECHO) LD $@
	$(LINK.c) -o $@ $^

fdtdump:	$(FDTDUMP_OBJS)

fdtget:	$(FDTGET_OBJS) $(LIBFDT_archive)

fdtput:	$(FDTPUT_OBJS) $(LIBFDT_archive)

fdtoverlay: $(FDTOVERLAY_OBJS) $(LIBFDT_archive)

dist:
	git archive --format=tar --prefix=dtc-$(dtc_version)/ HEAD \
		> ../dtc-$(dtc_version).tar
	cat ../dtc-$(dtc_version).tar | \
		gzip -9 > ../dtc-$(dtc_version).tar.gz


#
# Rules for pylibfdt
#
PYLIBFDT_srcdir = pylibfdt
PYLIBFDT_objdir = pylibfdt

include $(PYLIBFDT_srcdir)/Makefile.pylibfdt

.PHONY: pylibfdt
pylibfdt: $(PYLIBFDT_objdir)/_libfdt.so

pylibfdt_clean:
	@$(VECHO) CLEAN "(pylibfdt)"
	rm -f $(addprefix $(PYLIBFDT_objdir)/,$(PYLIBFDT_cleanfiles))

#
# Release signing and uploading
# This is for maintainer convenience, don't try this at home.
#
ifeq ($(MAINTAINER),y)
GPG = gpg2
KUP = kup
KUPDIR = /pub/software/utils/dtc

kup: dist
	$(GPG) --detach-sign --armor -o ../dtc-$(dtc_version).tar.sign \
		../dtc-$(dtc_version).tar
	$(KUP) put ../dtc-$(dtc_version).tar.gz ../dtc-$(dtc_version).tar.sign \
		$(KUPDIR)/dtc-$(dtc_version).tar.gz
endif

tags: FORCE
	rm -f tags
	find . \( -name tests -type d -prune \) -o \
	       \( ! -name '*.tab.[ch]' ! -name '*.lex.c' \
	       -name '*.[chly]' -type f -print \) | xargs ctags -a

#
# Testsuite rules
#
TESTS_PREFIX=tests/

TESTS_BIN += dtc
TESTS_BIN += convert-dtsv0
TESTS_BIN += fdtput
TESTS_BIN += fdtget
TESTS_BIN += fdtdump
TESTS_BIN += fdtoverlay
ifeq ($(NO_PYTHON),)
TESTS_PYLIBFDT += maybe_pylibfdt
endif

include tests/Makefile.tests

#
# Clean rules
#
STD_CLEANFILES = *~ *.o *.$(SHAREDLIB_EXT) *.d *.a *.i *.s core a.out vgcore.* \
	*.tab.[ch] *.lex.c *.output

clean: libfdt_clean pylibfdt_clean tests_clean
	@$(VECHO) CLEAN
	rm -f $(STD_CLEANFILES)
	rm -f $(VERSION_FILE)
	rm -f $(BIN)
	rm -f dtc-*.tar dtc-*.tar.sign dtc-*.tar.asc

#
# Generic compile rules
#
%: %.o
	@$(VECHO) LD $@
	$(LINK.c) -o $@ $^

%.o: %.c
	@$(VECHO) CC $@
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

%.o: %.S
	@$(VECHO) AS $@
	$(CC) $(CPPFLAGS) $(AFLAGS) -D__ASSEMBLY__ -o $@ -c $<

%.d: %.c
	@$(VECHO) DEP $<
	$(CC) $(CPPFLAGS) -MM -MG -MT "$*.o $@" $< > $@

%.d: %.S
	@$(VECHO) DEP $<
	$(CC) $(CPPFLAGS) -MM -MG -MT "$*.o $@" $< > $@

%.i:	%.c
	@$(VECHO) CPP $@
	$(CC) $(CPPFLAGS) -E $< > $@

%.s:	%.c
	@$(VECHO) CC -S $@
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -S $<

%.a:
	@$(VECHO) AR $@
	$(AR) $(ARFLAGS) $@ $^

$(LIBFDT_lib):
	@$(VECHO) LD $@
	$(CC) $(LDFLAGS) $(SHAREDLIB_LDFLAGS)$(LIBFDT_soname) -o $(LIBFDT_lib) $^

%.lex.c: %.l
	@$(VECHO) LEX $@
	$(LEX) -o$@ $<

%.tab.c %.tab.h %.output: %.y
	@$(VECHO) BISON $@
	$(BISON) -d $<

FORCE:

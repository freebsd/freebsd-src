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
PATCHLEVEL = 2
SUBLEVEL = 0
EXTRAVERSION =
LOCAL_VERSION =
CONFIG_LOCALVERSION =

CPPFLAGS = -I libfdt
CFLAGS = -Wall -g -Os -fPIC -Wpointer-arith -Wcast-qual

BISON = bison
LEX = flex

INSTALL = /usr/bin/install
DESTDIR =
PREFIX = $(HOME)
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

HOSTOS := $(shell uname -s | tr '[:upper:]' '[:lower:]' | \
	    sed -e 's/\(cygwin\).*/cygwin/')

ifeq ($(HOSTOS),darwin)
SHAREDLIB_EXT=dylib
SHAREDLIB_LINK_OPTIONS=-dynamiclib -Wl,-install_name -Wl,
else
SHAREDLIB_EXT=so
SHAREDLIB_LINK_OPTIONS=-shared -Wl,--version-script=$(LIBFDT_version) -Wl,-soname,
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
include Makefile.ftdump

BIN += convert-dtsv0
BIN += dtc
BIN += ftdump


all: $(BIN) libfdt


ifneq ($(DEPTARGETS),)
-include $(DTC_OBJS:%.o=%.d)
-include $(CONVERT_OBJS:%.o=%.d)
-include $(FTDUMP_OBJS:%.o=%.d)
endif



#
# Rules for libfdt
#
LIBFDT_objdir = libfdt
LIBFDT_srcdir = libfdt
LIBFDT_archive = $(LIBFDT_objdir)/libfdt.a
LIBFDT_lib = $(LIBFDT_objdir)/libfdt.$(SHAREDLIB_EXT)
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

install: all
	@$(VECHO) INSTALL
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) $(BIN) $(DESTDIR)$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) $(LIBFDT_lib) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 644 $(LIBFDT_archive) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 644 $(LIBFDT_include) $(DESTDIR)$(INCLUDEDIR)

$(VERSION_FILE): Makefile FORCE
	$(call filechk,version)


dtc: $(DTC_OBJS)

convert-dtsv0: $(CONVERT_OBJS)
	@$(VECHO) LD $@
	$(LINK.c) -o $@ $^

ftdump:	$(FTDUMP_OBJS)


#
# Testsuite rules
#
TESTS_PREFIX=tests/
include tests/Makefile.tests

#
# Clean rules
#
STD_CLEANFILES = *~ *.o *.so *.d *.a *.i *.s core a.out vgcore.* \
	*.tab.[ch] *.lex.c *.output

clean: libfdt_clean tests_clean
	@$(VECHO) CLEAN
	rm -f $(STD_CLEANFILES)
	rm -f $(VERSION_FILE)
	rm -f $(BIN)

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
	$(CC) $(LDFLAGS) -fPIC $(SHAREDLIB_LINK_OPTIONS)$(notdir $@) -o $(LIBFDT_objdir)/libfdt-$(DTC_VERSION).$(SHAREDLIB_EXT) $^
	ln -sf libfdt-$(DTC_VERSION).$(SHAREDLIB_EXT) $(LIBFDT_objdir)/libfdt.$(SHAREDLIB_EXT)

%.lex.c: %.l
	@$(VECHO) LEX $@
	$(LEX) -o$@ $<

%.tab.c %.tab.h %.output: %.y
	@$(VECHO) BISON $@
	$(BISON) -d $<

FORCE:

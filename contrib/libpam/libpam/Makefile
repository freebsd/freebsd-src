#
# $Id: Makefile,v 1.4 2001/02/10 07:17:53 agmorgan Exp $
#
#

include ../Make.Rules

# need to tell libpam about the default directory for PAMs
MOREFLAGS=-D"DEFAULT_MODULE_PATH=\"$(SECUREDIR)/\""

ifeq ($(WITH_LIBDEBUG),yes)
 LIBNAME=libpamd
else
 LIBNAME=libpam
endif
VERSION=.$(MAJOR_REL)
MODIFICATION=.$(MINOR_REL)

# ---------------------------------------------

dummy: ../Make.Rules all

# ---------------------------------------------

CFLAGS += $(DYNAMIC) $(STATIC) $(MOREFLAGS) \
  -DLIBPAM_VERSION_MAJOR=$(MAJOR_REL) \
  -DLIBPAM_VERSION_MINOR=$(MINOR_REL)

# dynamic library names

LIBPAM = $(LIBNAME).$(DYNTYPE)
LIBPAMNAME = $(LIBPAM)$(VERSION)
LIBPAMFULL = $(LIBPAMNAME)$(MODIFICATION)

# static library name

LIBPAMSTATIC = $(LIBNAME).a

ifdef STATIC
@echo Did you mean to set STATIC\?
MODULES = $(shell cat ../modules/_static_module_objects)
STATICOBJ = pam_static.o
else
MODULES =
endif

ifdef MEMORY_DEBUG
EXTRAS += pam_malloc.o
endif

LIBOBJECTS = pam_item.o pam_strerror.o pam_end.o pam_start.o pam_data.o \
		pam_delay.o pam_dispatch.o pam_handlers.o pam_misc.o \
		pam_account.o pam_auth.o pam_session.o pam_password.o \
		pam_env.o pam_log.o $(EXTRAS)

ifeq ($(DYNAMIC_LIBPAM),yes)
# libpam.so needs -ldl, too.
DLIBOBJECTS = $(addprefix dynamic/,$(LIBOBJECTS) $(STATICOBJ))
ifeq ($(STATICOBJ),yes)
dynamic/pam_static.o: pam_static.c ../modules/_static_module_objects
	$(CC) $(CFLAGS) -c pam_static.c -o $@
endif
endif

ifeq ($(STATIC_LIBPAM),yes)
SLIBOBJECTS = $(addprefix static/,$(LIBOBJECTS) $(STATICOBJ))
ifdef STATICOBJ
static/pam_static.o: pam_static.c ../modules/_static_module_objects
	$(CC) $(CFLAGS) -c pam_static.c -o $@
endif
endif

# ---------------------------------------------
## rules

all: dirs $(LIBPAM) $(LIBPAMSTATIC) ../Make.Rules

dirs:
ifeq ($(DYNAMIC_LIBPAM),yes)
	$(MKDIR) dynamic
endif
ifeq ($(STATIC_LIBPAM),yes)
	$(MKDIR) static
endif

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

static/%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

$(LIBPAM): $(DLIBOBJECTS)
ifeq ($(DYNAMIC_LIBPAM),yes)
    ifeq ($(USESONAME),yes)
	$(LD_L) $(SOSWITCH) $(LIBPAMNAME) -o $@ $(DLIBOBJECTS) \
		$(MODULES) $(LINKLIBS)
    else
	$(LD_L) -o $@ $(DLIBOBJECTS) $(MODULES) $(LINKLIBS)
    endif
    ifeq ($(NEEDSONAME),yes)
	rm -f $(LIBPAMFULL)
	ln -sf $(LIBPAM) $(LIBPAMFULL)
	rm -f $(LIBPAMNAME)
	ln -sf $(LIBPAM) $(LIBPAMNAME)
    endif
endif

$(LIBPAMSTATIC): $(SLIBOBJECTS)
ifeq ($(STATIC_LIBPAM),yes)
	ar cr $@ $(SLIBOBJECTS) $(MODULES)
	$(RANLIB) $@
endif

install: all
	$(MKDIR) $(FAKEROOT)$(INCLUDED) $(FAKEROOT)$(libdir)
	$(INSTALL) -m 644 include/security/pam_appl.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/pam_modules.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_macros.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_types.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_compat.h $(FAKEROOT)$(INCLUDED)
ifdef MEMORY_DEBUG
	$(INSTALL) -m 644 include/security/pam_malloc.h $(FAKEROOT)$(INCLUDED)
endif
ifeq ($(DYNAMIC_LIBPAM),yes)
	$(INSTALL) -m $(SHLIBMODE) $(LIBPAM) $(FAKEROOT)$(libdir)/$(LIBPAMFULL)
	$(LDCONFIG)
  ifneq ($(DYNTYPE),"sl")
	( cd $(FAKEROOT)$(libdir) ; rm -f $(LIBPAM) ; \
	  ln -sf $(LIBPAMNAME) $(LIBPAM) )
  endif
endif
ifeq ($(STATIC_LIBPAM),yes)
	$(INSTALL) -m 644 $(LIBPAMSTATIC) $(FAKEROOT)$(libdir)
endif

remove:
	rm -f $(FAKEROOT)$(INCLUDED)/_pam_types.h
	rm -f $(FAKEROOT)$(INCLUDED)/_pam_macros.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_appl.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_modules.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_malloc.h
	rm -f $(FAKEROOT)$(libdir)/$(LIBPAM).*
	rm -f $(FAKEROOT)$(libdir)/$(LIBPAM)
	$(LDCONFIG)
	rm -f $(FAKEROOT)$(libdir)/$(LIBPAMSTATIC)

clean:
	rm -f a.out core *~ static/*.o dynamic/*.o
	rm -f *.a *.o *.so ./include/security/*~
	if [ -d dynamic ]; then rmdir dynamic ; fi
	if [ -d static ]; then rmdir static ; fi

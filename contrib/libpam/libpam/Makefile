#
# $Id: Makefile,v 1.19 1997/04/05 06:58:43 morgan Exp morgan $
# $FreeBSD$
#
# $Log: Makefile,v $
# Revision 1.19  1997/04/05 06:58:43  morgan
# fakeroot
#
# Revision 1.18  1997/02/15 15:56:09  morgan
# inherit major and minor numbers
#
# Revision 1.17  1997/01/04 20:03:09  morgan
# update for .55
#
# Revision 1.16  1996/12/01 03:14:13  morgan
# update for .54
#
# Revision 1.15  1996/11/10 20:07:51  morgan
# updated for .53
#
# Revision 1.14  1996/09/05 06:06:53  morgan
# added local flag for locking, slight reorganization too.
#

# need to tell libpam about the default directory for PAMs
MOREFLAGS=-D"DEFAULT_MODULE_PATH=\"$(SECUREDIR)/\""

# you may uncomment the following to build libpam in modified ways

# lots of debugging information goes to /tmp/pam-debug.log
#MOREFLAGS += -D"DEBUG"

# pay attention to locked /etc/pam.conf or /etc/pam.d/* files
#MOREFLAGS += -D"PAM_LOCKING"

# read both the /etc/pam.d/ and pam.conf files specific to the deisred service
#MOREFLAGS += -D"PAM_READ_BOTH_CONFS"

# make a kludge attempt to be compatible with the old pam_strerror
# calling convention
#MOREFLAGS += -D"UGLY_HACK_FOR_PRIOR_BEHAVIOR_SUPPORT"

ifeq ($(DEBUG_REL),yes)
 LIBNAME=libpamd
else
 LIBNAME=libpam
endif
VERSION=.$(MAJOR_REL)
MODIFICATION=.$(MINOR_REL)

# ---------------------------------------------

dummy:
	@echo "*** This is not a top-level Makefile!"

# ---------------------------------------------

CFLAGS += $(DYNAMIC) $(STATIC) $(MOREFLAGS)

# dynamic library names

LIBPAM = $(LIBNAME).$(DYNTYPE)
LIBPAMNAME = $(LIBPAM)$(VERSION)
LIBPAMFULL = $(LIBPAMNAME)$(MODIFICATION)

# static library name

LIBPAMSTATIC = $(LIBNAME).a

ifdef STATIC
MODULES = $(shell cat ../modules/_static_module_objects)
STATICOBJ = pam_static.o
endif

ifdef MEMORY_DEBUG
EXTRAS += pam_malloc.o
endif

LIBOBJECTS = pam_item.o pam_strerror.o pam_end.o pam_start.o pam_data.o \
		pam_delay.o pam_dispatch.o pam_handlers.o pam_misc.o \
		pam_account.o pam_auth.o pam_session.o pam_password.o \
		pam_env.o pam_log.o $(EXTRAS)

ifdef DYNAMIC_LIBPAM
DLIBOBJECTS = $(addprefix dynamic/,$(LIBOBJECTS) $(STATICOBJ))
ifdef STATICOBJ
dynamic/pam_static.o: pam_static.c ../modules/_static_module_objects
	$(CC) $(CFLAGS) -c pam_static.c -o $@
endif
endif

ifdef STATIC_LIBPAM
SLIBOBJECTS = $(addprefix static/,$(LIBOBJECTS) $(STATICOBJ))
ifdef STATICOBJ
static/pam_static.o: pam_static.c ../modules/_static_module_objects
	$(CC) $(CFLAGS) -c pam_static.c -o $@
endif
endif

# ---------------------------------------------
## rules

all: dirs $(LIBPAM) $(LIBPAMSTATIC)

dirs:
ifdef DYNAMIC_LIBPAM
	mkdir -p dynamic
endif
ifdef STATIC_LIBPAM
	mkdir -p static
endif

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

static/%.o : %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

$(LIBPAM): $(DLIBOBJECTS)
ifdef DYNAMIC_LIBPAM
    ifeq ($(USESONAME),yes)
	$(LD_L) $(SOSWITCH) $(LIBPAMNAME) -o $@ $(DLIBOBJECTS) $(MODULES)
    else
	$(LD_L) -o $@ $(DLIBOBJECTS) $(MODULES)
    endif
    ifeq ($(NEEDSONAME),yes)
	rm -f $(LIBPAMFULL)
	ln -s $(LIBPAM) $(LIBPAMFULL)
	rm -f $(LIBPAMNAME)
	ln -s $(LIBPAM) $(LIBPAMNAME)
    endif
endif

$(LIBPAMSTATIC): $(SLIBOBJECTS)
ifdef STATIC_LIBPAM
	$(AR) $@ $(SLIBOBJECTS) $(MODULES)
	$(RANLIB) $@
endif

install: all
	$(MKDIR) $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/pam_appl.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/pam_modules.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_macros.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_types.h $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/security/_pam_compat.h $(FAKEROOT)$(INCLUDED)
ifdef MEMORY_DEBUG
	$(INSTALL) -m 644 include/security/pam_malloc.h $(FAKEROOT)$(INCLUDED)
endif
ifdef DYNAMIC_LIBPAM
	$(INSTALL) -m $(SHLIBMODE) $(LIBPAM) $(FAKEROOT)$(LIBDIR)/$(LIBPAMFULL)
	$(LDCONFIG)
  ifneq ($(DYNTYPE),"sl")
	( cd $(FAKEROOT)$(LIBDIR) ; rm -f $(LIBPAM) ; ln -s $(LIBPAMNAME) $(LIBPAM) )
  endif
endif
ifdef STATIC_LIBPAM
	$(INSTALL) -m 644 $(LIBPAMSTATIC) $(FAKEROOT)$(LIBDIR)
endif

remove:
	rm -f $(FAKEROOT)$(INCLUDED)/_pam_types.h
	rm -f $(FAKEROOT)$(INCLUDED)/_pam_macros.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_appl.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_modules.h
	rm -f $(FAKEROOT)$(INCLUDED)/pam_malloc.h
	rm -f $(FAKEROOT)$(LIBDIR)/$(LIBPAM).*
	rm -f $(FAKEROOT)$(LIBDIR)/$(LIBPAM)
	$(LDCONFIG)
	rm -f $(FAKEROOT)$(LIBDIR)/$(LIBPAMSTATIC)

clean:
	rm -f a.out core *~ static/*.o dynamic/*.o

extraclean: clean
	rm -f *.a *.out *.o *.so ./include/security/*~
	if [ -d dynamic ]; then rmdir dynamic ; fi
	if [ -d static ]; then rmdir static ; fi

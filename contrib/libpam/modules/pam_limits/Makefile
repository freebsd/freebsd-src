#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# Created by Cristian Gafton <gafton@redhat.com> 1996/09/10
#

ifeq ($(OS),linux)
ifeq ($(HAVE_PWDBLIB),yes)
TITLE=pam_limits
CONFD=$(CONFIGED)/security
export CONFD
CONFILE=$(CONFD)/limits.conf
export CONFILE

CFLAGS+=-DLIMITS_FILE=\"$(CONFILE)\"

#

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBOBJS = $(addprefix static/,$(LIBOBJ))

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

static/%.o : %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@


ifdef DYNAMIC
LIBSHARED = $(TITLE).so
endif

ifdef STATIC
LIBSTATIC = lib$(TITLE).o
endif

####################### don't edit below #######################

dummy:

	@echo "**** This is not a top-level Makefile "
	exit

all: dirs $(LIBSHARED) $(LIBSTATIC) register

dirs:
ifdef DYNAMIC
	$(MKDIR) ./dynamic
endif
ifdef STATIC
	$(MKDIR) ./static
endif

register:
ifdef STATIC
	( cd .. ; ./register_static $(TITLE) $(TITLE)/$(LIBSTATIC) )
endif

ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)

$(LIBSHARED):	$(LIBOBJD)
	$(LD_D) -o $@ $(LIBOBJD) -lpwdb
endif

ifdef STATIC
$(LIBOBJS): $(LIBSRC)

$(LIBSTATIC): $(LIBOBJS)
	$(LD) -r -o $@ $(LIBOBJS)
endif

install: all
ifdef DYNAMIC
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
	$(INSTALL) -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif
	$(MKDIR) $(FAKEROOT)$(SCONFIGED)
	bash -f ./install_conf

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(TITLE).so

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) core *~ *.so

extraclean: clean
	rm -f *.a *.o *.so *.bak dynamic/* static/*

.c.o:	
	$(CC) $(CFLAGS) -c $<

else
include ../dont_makefile
endif
else
include ../dont_makefile
endif

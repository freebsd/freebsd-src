#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# Created by Cristian Gafton <gafton@sorosis.ro> 1996/09/10
#

ifeq ($(HAVE_PWDBLIB),yes)

TITLE=pam_wheel
CFLAGS += -DHAVE_PWDBLIB

#

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
#LIBOBJS = $(addprefix static/,$(LIBOBJ))

EXTRALS = -lpwdb

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

#static/%.o : %.c
#	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@


ifdef DYNAMIC
LIBSHARED = $(TITLE).so
endif

#ifdef STATIC
#LIBSTATIC = lib$(TITLE).o
#endif

####################### don't edit below #######################

dummy:

	@echo "**** This is not a top-level Makefile "
	exit

all: dirs $(LIBSHARED) $(LIBSTATIC) register

dirs:
ifdef DYNAMIC
	$(MKDIR) ./dynamic
endif
#ifdef STATIC
#	$(MKDIR) ./static
#endif

register:
#ifdef STATIC
#	( cd .. ; ./register_static $(TITLE) $(TITLE)/$(LIBSTATIC) )
#endif

ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)

$(LIBSHARED):	$(LIBOBJD)
	$(LD_D) -o $@ $(LIBOBJD) $(EXTRALS)
endif

#ifdef STATIC
#$(LIBOBJS): $(LIBSRC)
#
#$(LIBSTATIC): $(LIBOBJS)
#	$(LD) -r -o $@ $(LIBOBJS) $(EXTRALS)
#endif

install: all
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
	$(INSTALL) -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif

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

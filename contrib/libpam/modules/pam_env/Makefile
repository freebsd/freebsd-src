#
# $Id: Makefile,v 1.1 1997/04/05 06:42:35 morgan Exp morgan $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# $Log: Makefile,v $
# Revision 1.1  1997/04/05 06:42:35  morgan
# Initial revision
#
# Revision 1.1  1997/01/04 20:32:52  morgan
# Initial revision
#
# Created by Andrew Morgan <morgan@parc.power.net> 1996/12/8
# Adaptations by Dave Kinclea and Cristian Gafton
#

TITLE=pam_env

CONFD=$(CONFIGED)/security
export CONFD
CONFILE=$(CONFD)/pam_env.conf
export CONFILE

#ifeq ($(HAVE_PWDBLIB),yes)
#CFLAGS += -DWANT_PWDB
#EXTRALIB = -lpwdb
#endif

#

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBOBJS = $(addprefix static/,$(LIBOBJ))

ifdef DYNAMIC
LIBSHARED = $(TITLE).so
endif

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

ifdef STATIC
LIBSTATIC = lib$(TITLE).o
endif

static/%.o : %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

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
	$(LD_D) -o $@ $(LIBOBJD) $(EXTRALIB)
endif

ifdef STATIC
$(LIBOBJS): $(LIBSRC)

$(LIBSTATIC): $(LIBOBJS)
	$(LD) -r -o $@ $(LIBOBJS) $(EXTRALIB)
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
	rm -f $(LIBOBJD) $(LIBOBJS) core *~

extraclean: clean
	rm -f *.a *.o *.so *.bak dynamic/* static/*

.c.o:	
	$(CC) $(CFLAGS) -c $<


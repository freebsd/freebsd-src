#
# $Id: Makefile,v 1.6 1997/04/05 06:22:32 morgan Exp morgan $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# $Log: Makefile,v $
# Revision 1.6  1997/04/05 06:22:32  morgan
# fakeroot
#
# Revision 1.5  1997/02/15 19:16:16  morgan
# fixed email
#
# Revision 1.4  1996/11/10 20:18:21  morgan
# cross platform support
#
# Revision 1.3  1996/09/05 06:27:37  morgan
# ld --> gcc
#
# Revision 1.2  1996/08/09 05:48:19  morgan
# inherit installation files from parent
#
# Revision 1.1  1996/07/07 23:42:48  morgan
# Initial revision
#
# Revision 1.1  1996/06/24 05:48:49  morgan
# Initial revision
#
#
# Created by Andrew Morgan <morgan@parc.power.net> 1996/6/11
#

TITLE=pam_time
CONFD=$(CONFIGED)/security
export CONFD
CONFILE=$(CONFD)/time.conf
export CONFILE

#

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBOBJS = $(addprefix static/,$(LIBOBJ))

DEFS=-DCONFILE=\"$(CONFILE)\"

CFLAGS += $(DEFS)

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
		$(LD_D) -o $@ $(LIBOBJD)
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
	rm -f $(FAKEROOT)$(CONFILE)

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) core *~
	rm -f ./.ignore_age

extraclean: clean
	rm -f *.a *.o *.so *.bak

.c.o:	
	$(CC) $(CFLAGS) -c $<


#
# $Id: Makefile,v 1.10 1997/04/05 06:41:09 morgan Exp $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# $Log: Makefile,v $
# Revision 1.10  1997/04/05 06:41:09  morgan
# fakeroot
#
# Revision 1.9  1997/02/15 18:58:48  morgan
# fixed bash syntax
#
# Revision 1.8  1997/01/04 20:24:29  morgan
# don't compile on solaris, make -> $(MAKE)
#
# Revision 1.7  1996/11/10 20:12:09  morgan
# cross platform support
#
# Created by Andrew Morgan <morgan@parc.power.net> 1996/3/11
#

ifeq ($(OS),solaris)

include ../dont_makefile

else

TITLE=pam_filter
FILTERS=upperLOWER
FILTERSDIR=$(SUPLEMENTED)/pam_filter
export FILTERSDIR

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

#
# this is where we compile this module
#

all: dirs $(LIBSHARED) $(LIBSTATIC) register filters

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

filters:
	@for i in $(FILTERS) ; do \
		if [ -d $$i ]; then \
		   $(MAKE) -C $$i all ; \
		fi ; \
	done


ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)
endif

ifdef DYNAMIC
$(LIBSHARED):	$(LIBOBJD)
		$(LD_D) -o $@ $(LIBOBJD)
endif

ifdef STATIC
$(LIBOBJS): $(LIBSRC)
endif

ifdef STATIC
$(LIBSTATIC): $(LIBOBJS)
	$(LD) -r -o $@ $(LIBOBJS)
endif

install: all
	@for i in $(FILTERS) ; do \
		if [ -d $$i ]; then \
		   $(MAKE) -C $$i install ; \
		fi ; \
	done
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
	$(INSTALL) -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif
	$(MKDIR) $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 644 include/pam_filter.h $(FAKEROOT)$(INCLUDED)

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(TITLE).so
	rm -f $(FAKEROOT)$(INCLUDED)/pam_filter.h
	@for i in $(FILTERS) ; do \
		if [ -d $$i ]; then \
		   $(MAKE) -C $$i remove ; \
		fi ; \
	done

lclean:
	rm -f $(LIBSHARED) $(LIBOBJD) $(LIBOBJS) core *~

clean: lclean
	@for i in $(FILTERS) ; do \
		if [ -d $$i ]; then \
		   $(MAKE) -C $$i clean ; \
		fi ; \
	done

extraclean: lclean
	@rm -f *.a *.o *.so *.bak
	for i in $(FILTERS) ; do \
		if [ -d $$i ]; then \
		   $(MAKE) -C $$i extraclean ; \
		fi ; \
	done

.c.o:	
	$(CC) $(CFLAGS) -c $<

endif

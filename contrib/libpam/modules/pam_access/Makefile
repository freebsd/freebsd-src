# $Id: Makefile,v 1.1 1997/06/23 00:39:42 morgan Exp morgan $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# $Log: Makefile,v $
# Revision 1.1  1997/06/23 00:39:42  morgan
# Initial revision
#
#

TITLE=pam_access
CONFD=$(CONFIGED)/security
export CONFD
CONFILE=$(CONFD)/access.conf
export CONFILE

# Convenient defaults for compiling independently of the full source
# tree.
ifndef FULL_LINUX_PAM_SOURCE_TREE
export DYNAMIC=-DPAM_DYNAMIC
export CC=gcc
export CFLAGS=-O2 -Dlinux -DLINUX_PAM \
       -ansi -D_POSIX_SOURCE -Wall -Wwrite-strings \
       -Wpointer-arith -Wcast-qual -Wcast-align -Wtraditional \
       -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline \
       -Wshadow -pedantic -fPIC
export MKDIR=mkdir -p
export LD_D=gcc -shared -Xlinker -x
endif

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
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
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

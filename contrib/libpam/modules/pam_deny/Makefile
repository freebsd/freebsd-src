#
# $Id: Makefile,v 1.7 1997/04/05 06:43:41 morgan Exp morgan $
# $FreeBSD$
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# $Log: Makefile,v $
# Revision 1.7  1997/04/05 06:43:41  morgan
# full-source-tree and fakeroot
#
# Revision 1.6  1997/02/15 19:04:27  morgan
# fixed email
#
# Revision 1.5  1996/11/10 20:11:48  morgan
# crossplatform support
#
# Revision 1.4  1996/09/05 06:50:12  morgan
# ld --> gcc
#
# Revision 1.3  1996/05/26 15:48:38  morgan
# make dynamic and static dirs
#
# Revision 1.2  1996/05/26 04:00:16  morgan
# changes for automated static/dynamic modules
#
# Revision 1.1  1996/03/16 17:47:36  morgan
# Initial revision
#
#
# Created by Andrew Morgan <morgan@parc.power.net> 1996/3/11
#

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

#

TITLE=pam_deny

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

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(TITLE).so

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) core *~

extraclean: clean
	rm -f *.a *.o *.so *.bak

.c.o:	
	$(CC) $(CFLAGS) -c $<


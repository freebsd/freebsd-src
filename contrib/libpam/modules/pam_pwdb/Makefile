# $Id: Makefile,v 1.7 1997/04/05 06:28:50 morgan Exp morgan $
#
# This Makefile controls a build process of the pam_unix module
# for Linux-PAM. You should not modify this Makefile.
#
# rewritten to compile new module Andrew Morgan
# <morgan@parc.power.net> 1996/11/6
#

#
# Note, the STATIC module is commented out because it doesn't work.
# please fix!
#

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
export HAVE_PWDBLIB=yes
endif

ifeq ($(HAVE_PWDBLIB),yes)

TITLE=pam_pwdb
CHKPWD=pwdb_chkpwd

# compilation flags
EXTRAS=
# extra object files
PLUS=
# extra files that may be needed to be created
CREATE=

# NOTE: this module links dynamically to the libpwdb library.
EXTRALS += -lpwdb
EXTRAS += -DCHKPWD_HELPER=\"$(SUPLEMENTED)/$(CHKPWD)\"

########################### don't edit below ##########################

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
#LIBOBJS = $(addprefix static/,$(LIBOBJ))
LIBDEPS = pam_unix_acct.-c pam_unix_auth.-c pam_unix_passwd.-c \
	pam_unix_sess.-c pam_unix_pwupd.-c support.-c bigcrypt.-c

PLUS += md5.o md5_crypt.o
CFLAGS += $(EXTRAS)

ifdef DYNAMIC
LIBSHARED = $(TITLE).so
endif
#ifdef STATIC
#LIBSTATIC = lib$(TITLE).o
#endif

all: info dirs $(PLUS) $(LIBSHARED) $(LIBSTATIC) register $(CHKPWD)

dynamic/$(LIBOBJ) : $(LIBSRC) $(LIBDEPS)
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

#static/$(LIBOBJ) : $(LIBSRC) $(LIBDEPS)
#	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

info:
	@echo
	@echo "*** Building PAM_pwdb module..."
	@echo

$(CHKPWD): pwdb_chkpwd.o md5.o md5_crypt.o
	$(CC) -o $(CHKPWD) $^ -lpwdb

pwdb_chkpwd.o: pwdb_chkpwd.c pam_unix_md.-c bigcrypt.-c

dirs:
ifdef DYNAMIC
	@$(MKDIR) ./dynamic
endif
#ifdef STATIC
#	@$(MKDIR) ./static
#endif

register:
#ifdef STATIC
#	( cd .. ; ./register_static $(TITLE) $(TITLE)/$(LIBSTATIC) )
#endif

ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)

$(LIBSHARED):	$(LIBOBJD)
	$(LD_D) -o $@ $(LIBOBJD) $(PLUS) $(EXTRALS)
endif

#ifdef STATIC
#$(LIBOBJS): $(LIBSRC)
#
#$(LIBSTATIC): $(LIBOBJS)
#	$(LD) -r -o $@ $(LIBOBJS) $(PLUS) $(EXTRALS)
#endif

install: all
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
	$(INSTALL) -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif
	$(MKDIR) $(FAKEROOT)$(SUPLEMENTED)
	$(INSTALL) -m 4555 -o root -g root $(CHKPWD) $(FAKEROOT)$(SUPLEMENTED)

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(TITLE).so
	rm -f $(FAKEROOT)$(SUPLEMENTED)/$(CHKPWD)

clean:
	rm -f $(CHKPWD) $(LIBOBJD) $(LIBOBJS) $(MOREDELS) core *~ *.o *.so

extraclean: clean
	rm -f *.a *.o *.so *.bak

else

include ../dont_makefile

endif

#####################################################################
# $Log: Makefile,v $
# Revision 1.7  1997/04/05 06:28:50  morgan
# fakeroot
#
# Revision 1.6  1997/02/15 17:25:32  morgan
# update for .56 . extra commands for new helper binary
#
# Revision 1.5  1997/01/04 20:39:08  morgan
# conditional on having libpwdb
#
# Revision 1.4  1996/12/01 03:02:03  morgan
# changed banner, removed linking libraries
#
# Revision 1.3  1996/11/10 20:14:42  morgan
# cross platform support
#
# Revision 1.2  1996/09/05 06:36:49  morgan
# options added and use of LD altered
#
# Revision 1.1  1996/08/29 13:23:29  morgan
# Initial revision
#
#

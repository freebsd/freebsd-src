# $Header$
#
# This Makefile controls a build process of the pam_unix modules
# for Linux-PAM. You should not modify this Makefile.
#
# $Log$
# Revision 1.1.1.2  1998/06/03 03:43:56  adam
# Import from archive
#
# Revision 1.3  1998/05/31 23:48:13  adam
# Link crypt library as necessary.
#
# Revision 1.3  1997/04/05 06:20:58  morgan
# fakeroot and also lockpwdf is in libc now
#
# Revision 1.2  1996/11/10 20:18:59  morgan
# cross platform support
#
# Revision 1.1  1996/11/09 19:44:16  morgan
# Initial revision
#
#

########################################################################
# some options... uncomment to take effect
########################################################################

# do you want shadow?
USE_SHADOW=-D"HAVE_SHADOW_H"

# do you want cracklib?
ifeq ($(HAVE_CRACKLIB),yes)
USE_CRACKLIB=-D"USE_CRACKLIB"
endif

# do you want to use lckpwdf?
USE_LCKPWDF=-D"USE_LCKPWDF"

# do you need to include the locking functions in the source?
#NEED_LCKPWDF=-D"NEED_LCKPWDF"

########################################################################

CFLAGS += $(USE_SHADOW) $(USE_CRACKLIB) $(USE_LCKPWDF) $(NEED_LCKPWDF)

ifdef DYNAMIC
LIBSESSSH = pam_unix_session.so
LIBAUTHSH = pam_unix_auth.so
LIBPASSWDSH = pam_unix_passwd.so
LIBACCOUNT = pam_unix_acct.so
endif

ifdef STATIC
LIBSTATIC = libpam_unix.o
endif

ifdef USE_CRACKLIB
CRACKLIB = -lcrack
endif

LIBAUTHOBJ = pam_unix_auth.o support.o
LIBAUTHSRC = pam_unix_auth.c support.c
LIBSESSOBJ = pam_unix_sess.o
LIBSESSSRC = pam_unix_sess.c
LIBPASSWDSRC = pam_unix_passwd.c
LIBPASSWDOBJ = pam_unix_passwd.o
LIBACCOUNTSRC = pam_unix_acct.c
LIBACCOUNTOBJ = pam_unix_acct.o
LIBOBJ = $(LIBAUTHOBJ) $(LIBSESSOBJ) $(LIBPASSWDOBJ) $(LIBACCOUNTOBJ)
LIBSRC = $(LIBAUTHSRC) $(LIBSESSSRC) $(LIBPASSWDSRC) $(LIBACCOUNTSRC)

LIBSHARED = $(LIBSESSSH) $(LIBAUTHSH) $(LIBPASSWDSH) $(LIBACCOUNT)

LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBOBJS = $(addprefix static/,$(LIBOBJ))

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) -c $< -o $@

static/%.o: %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) -c $< -o $@


########################### don't edit below #######################

dummy:

	@echo "**** This is not a top-level Makefile "
	exit

info:
	@echo
	@echo "*** Building pam-unix(alpha) module of the framework..."
	@echo

all: dirs info $(LIBSHARED) $(LIBSTATIC) register

dirs:
ifdef DYNAMIC
	mkdir -p ./dynamic
endif
ifdef STATIC
	mkdir -p ./static
endif

register:
ifdef STATIC
	( cd .. ; \
	  ./register_static pam_unix_auth  pam_unix/$(LIBSTATIC) ; \
	  ./register_static pam_unix_acct  "" ; \
	)
endif

ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)

$(LIBAUTHSH):	$(LIBAUTHSRC) $(LIBOBJD)
		$(LD_D) -o $@ $(addprefix dynamic/,$(LIBAUTHOBJ)) -lcrypt

$(LIBSESSSH):	$(LIBSESSSRC) $(LIBOBJD)
		$(LD_D) -o $@ $(addprefix dynamic/,$(LIBSESSOBJ))

$(LIBPASSWDSH):	$(LIBPASSWDSRC) $(LIBOBJD)
		$(LD_D) -o $@ $(addprefix dynamic/,$(LIBPASSWDOBJ)) $(CRACKLIB) -lcrypt

$(LIBACCOUNT):	$(LIBACCOUNTSRC) $(LIBOBJD)
		$(LD_D) -o $@ $(addprefix dynamic/,$(LIBACCOUNTOBJ))
endif


ifdef STATIC
$(LIBOBJS): $(LIBSRC)

$(LIBSTATIC): $(LIBOBJS)
	$(LD) -r -o $@ $(LIBOBJS)
endif

install: all
	mkdir -p $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
	install -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif

remove:
	cd $(FAKEROOT)$(SECUREDIR) && rm -f $(LIBSHARED)

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) a.out core *~

extraclean: clean
	rm -f *.a *.out *.o *.so *.bak

.c.o:	
	$(CC) -c $(CFLAGS) $<


# $Id: Makefile,v 1.3 2001/02/11 06:33:53 agmorgan Exp $
#
# This Makefile controls a build process of the pam_unix modules
# for Linux-PAM. You should not modify this Makefile.
#

include ../../Make.Rules

########################################################################
# some options... uncomment to take effect
########################################################################

# Unless someone wants to work out how to make this work with the new
# autoconf stuff, you should use a separate module for this type of thing
# pam_cracklib perhaps..?
# do you want cracklib?
#ifeq ($(HAVE_CRACKLIB),yes)
#USE_CRACKLIB=-D"USE_CRACKLIB"
#endif

# do you want to use lckpwdf?
ifeq ($(WITH_LCKPWDF),yes)
USE_LCKPWDF=-D"USE_LCKPWDF"
# do you need to include the locking functions in the source?
ifeq ($(HAVE_LCKPWDF),no)
  NEED_LCKPWDF=-D"NEED_LCKPWDF"
endif
endif

ifeq ($(HAVE_LIBNSL),yes)
  LIBNSL = -lnsl
endif

ifeq ($(HAVE_LIBCRYPT),yes)
  LIBCRYPT=-lcrypt
endif

CHKPWD=unix_chkpwd

EXTRAS += -DCHKPWD_HELPER=\"$(SUPLEMENTED)/$(CHKPWD)\"

########################################################################

CFLAGS += $(USE_CRACKLIB) $(USE_LCKPWDF) $(NEED_LCKPWDF) $(EXTRAS)
LDLIBS = $(EXTRALS)

ifdef USE_CRACKLIB
CRACKLIB = -lcrack
endif


LIBOBJ = pam_unix_auth.o pam_unix_acct.o pam_unix_sess.o pam_unix_passwd.o \
		support.o
LIBSRC = pam_unix_auth.c pam_unix_acct.c pam_unix_sess.c pam_unix_passwd.c \
		support.c
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBOBJS = $(addprefix static/,$(LIBOBJ))

PLUS = md5_good.o md5_broken.o md5_crypt_good.o md5_crypt_broken.o \
		yppasswd_xdr.o bigcrypt.o

ifdef DYNAMIC
LIBSHARED = pam_unix.so
endif
ifdef STATIC
LIBSTATIC = libpam_unix.o
endif


########################### don't edit below #######################

all: dirs info $(PLUS) $(LIBSHARED) $(LIBSTATIC) $(CHKPWD) register

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

static/%.o: %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

dummy:
	@echo "**** This is not a top-level Makefile "
	exit

info:
	@echo
	@echo "*** Building pam-unix module of the framework..."
	@echo

dirs:
ifdef DYNAMIC
	mkdir -p ./dynamic
endif
ifdef STATIC
	mkdir -p ./static
endif

register:
ifdef STATIC
	( cd .. ; ./register_static pam_unix_auth pam_unix/$(LIBSTATIC) ; \
		./register_static pam_unix_acct  "" ; \
		./register_static pam_unix_session "" ; \
		./register_static pam_unix_passwd "" ; \
	)
endif

ifdef DYNAMIC
$(LIBOBJD): $(LIBSRC)

$(LIBSHARED):	$(LIBOBJD)
	$(LD_D) -o $@ $(LIBOBJD) $(PLUS) $(CRACKLIB) $(LDLIBS) $(LIBNSL) $(LIBCRYPT)
endif

ifdef STATIC
$(LIBOBJS): $(LIBSRC)

$(LIBSTATIC): $(LIBOBJS)
	$(LD) -r -o $@ $(LIBOBJS) $(PLUS) $(CRACKLIB) $(LDLIBS) $(LIBNSL) $(LIBCRYPT)
endif

$(CHKPWD): unix_chkpwd.o md5_good.o md5_broken.o \
		md5_crypt_good.o md5_crypt_broken.o \
		bigcrypt.o
	$(CC) -o $(CHKPWD) $^ $(LDLIBS) $(LIBCRYPT)

unix_chkpwd.o: unix_chkpwd.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

md5_good.o: md5.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHIGHFIRST -D'MD5Name(x)=Good##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_broken.o: md5.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Broken##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_crypt_good.o: md5_crypt.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Good##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_crypt_broken.o: md5_crypt.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Broken##x' \
		$(TARGET_ARCH) -c $< -o $@

install: all
	mkdir -p $(FAKEROOT)$(SECUREDIR)
ifdef DYNAMIC
	install -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
	for x in pam_unix_auth pam_unix_acct pam_unix_passwd pam_unix_session;\
		do ln -sf $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)/$$x.so ; done
endif
	$(MKDIR) $(FAKEROOT)$(SUPLEMENTED)
	install -m 4555 $(CHKPWD) $(FAKEROOT)$(SUPLEMENTED)

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(LIBSHARED)
	for x in pam_unix_auth pam_unix_acct pam_unix_passwd pam_unix_session;\
		do rm -f $(FAKEROOT)$(SECUREDIR)/$$x.so ; done
	rm -f $(FAKEROOT)$(SUPLEMENTED)/$(CHKPWD)

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) $(CHKPWD) *.o *.so core
	rm -f *~ *.a *.out *.bak
	rm -rf dynamic static

.c.o:	
	$(CC) -c $(CFLAGS) $<


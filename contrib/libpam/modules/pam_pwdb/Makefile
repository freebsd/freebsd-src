# $Id: Makefile,v 1.2 2000/11/19 23:54:04 agmorgan Exp $
#
# This Makefile controls a build process of the pam_unix module
# for Linux-PAM. You should not modify this Makefile.
#
# rewritten to compile new module Andrew Morgan
# <morgan@parc.power.net> 1996/11/6
#

include ../../Make.Rules

ifeq ($(HAVE_LIBPWDB),yes)

EXTRALS += -lpwdb 
EXTRAS += -DCHKPWD_HELPER=\"$(SUPLEMENTED)/$(CHKPWD)\"

ifeq ($(HAVE_LIBCRYPT),yes)
  EXTRALS += -lcrypt
endif

TITLE=pam_pwdb
CHKPWD=pwdb_chkpwd

LIBSRC = $(TITLE).c
LIBOBJ = $(TITLE).o
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
#LIBOBJS = $(addprefix static/,$(LIBOBJ))
LIBDEPS = pam_unix_acct.-c pam_unix_auth.-c pam_unix_passwd.-c \
	pam_unix_sess.-c pam_unix_pwupd.-c support.-c bigcrypt.-c

PLUS += md5_good.o md5_broken.o md5_crypt_good.o md5_crypt_broken.o
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

$(CHKPWD): pwdb_chkpwd.o md5_good.o md5_broken.o \
           md5_crypt_good.o md5_crypt_broken.o
	$(CC) -o $(CHKPWD) $^ -lpwdb

pwdb_chkpwd.o: pwdb_chkpwd.c pam_unix_md.-c bigcrypt.-c

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
	$(INSTALL) -m 4555 $(CHKPWD) $(FAKEROOT)$(SUPLEMENTED)

remove:
	rm -f $(FAKEROOT)$(SECUREDIR)/$(TITLE).so
	rm -f $(FAKEROOT)$(SUPLEMENTED)/$(CHKPWD)

clean:
	rm -f $(CHKPWD) $(LIBOBJD) $(LIBOBJS) $(MOREDELS) core *~ *.o *.so
	rm -f *.a *.o *.so *.bak
	rm -fr dynamic static

else

include ../dont_makefile

endif

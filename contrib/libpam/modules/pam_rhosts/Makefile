# This Makefile controls a build process of the pam_rhosts modules
# for Linux-PAM. You should not modify this Makefile.

LIBAUTHOBJ = pam_rhosts_auth.o
LIBAUTHSRC = pam_rhosts_auth.c
LIBSESSOBJ =
LIBSESSSRC =
LIBPASSWDSRC =
LIBPASSWDOBJ =
LIBOBJ = $(LIBAUTHOBJ) $(LIBSESSOBJ) $(LIBPASSWDOBJ)
LIBSRC = $(LIBAUTHSRC) $(LIBSESSSRC) $(LIBPASSWDSRC)

ifdef STATIC
LIBSTATIC = libpam_rhosts.o
LIBOBJS = $(addprefix static/,$(LIBOBJ))
endif

ifdef DYNAMIC
LIBSESSSH =
LIBAUTHSH = pam_rhosts_auth.so
LIBPASSWDSH =
LIBOBJD = $(addprefix dynamic/,$(LIBOBJ))
LIBSHARED = $(LIBSESSSH) $(LIBAUTHSH) $(LIBPASSWDSH)
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
	( cd .. ; \
	  ./register_static pam_rhosts_auth pam_rhosts/libpam_rhosts.o )
endif

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

#.c.o:	
#	$(CC) -c $(CFLAGS) $<

install: all
ifdef DYNAMIC
	$(MKDIR) $(FAKEROOT)$(SECUREDIR)
	$(INSTALL) -m $(SHLIBMODE) $(LIBSHARED) $(FAKEROOT)$(SECUREDIR)
endif

# tidy up

remove:
	cd $(FAKEROOT)$(SECUREDIR) && rm -f $(LIBSHARED)

clean:
	rm -f $(LIBOBJD) $(LIBOBJS) a.out core *~

extraclean:
	rm -f *.a *.out *.o *.so *.bak dynamic/* static/*

dynamic/%.o : %.c
	$(CC) $(CFLAGS) $(DYNAMIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

static/%.o : %.c
	$(CC) $(CFLAGS) $(STATIC) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
#AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_LIBRARIES = @MAKE_LIBRSAREF@
EXTRA_LIBRARIES = librsaref.a
CLEANFILES = $(EXTRA_LIBRARIES)

# NOTES:
# don't use RSAREF's global.h - we use ours.
# We already have a copy of des.h

librsaref_a_SOURCES = \
	desc.c \
	digit.c \
	digit.h \
	md2.h \
	md2c.c \
	md5.h \
	md5c.c \
	nn.c \
	nn.h \
	prime.c \
	prime.h \
	r_dh.c \
	r_encode.c \
	r_enhanc.c \
	r_keygen.c \
	r_random.c \
	r_random.h \
	r_stdlib.c \
	rsa.c \
	rsa.h \
	rsaref.h

BUILT_SOURCES = $(librsaref_a_SOURCES)
INCLUDES = -I$(top_srcdir)/include
ETAGS_ARGS = Makefile.am

#EXTRA_DIST = 

$(librsaref_a_SOURCES): stamp-rsaref
	for i in $(librsaref_a_SOURCES); do \
	    case "@MAKE_LIBRSAREF@" in \
	     '') touch $$i ;; \
	     *) cmp -s $(srcdir)/$$i $(srcdir)/../rsaref2/source/$$i 2>/dev/null \
		|| cp $(srcdir)/../rsaref2/source/$$i $(srcdir)/$$i ;; \
	    esac ; \
	done

stamp-rsaref:
	touch stamp-rsaref

#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
#AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_LIBRARIES = @MAKE_LIBRSAREF@
EXTRA_LIBRARIES = librsaref.a
CLEANFILES = $(EXTRA_LIBRARIES)

# NOTES:
# don't use RSAREF's global.h - we use ours.

@isRSAREF_TRUE@foo = digit.c digit.h
@isRSAEURO_TRUE@foo = md4c.c shsc.c

nodist_librsaref_a_SOURCES = \
	desc.c \
	md2c.c \
	md5c.c \
	nn.c \
	prime.c \
	r_dh.c \
	r_encode.c \
	r_enhanc.c \
	r_keygen.c \
	r_random.c \
	r_stdlib.c \
	rsa.c \
	des.h \
	md2.h \
	md5.h \
	nn.h \
	prime.h \
	r_random.h \
	rsa.h \
	rsaref.h $(foo)

librsaref_a_LIBADD = @RSAOBJS@
librsaref_a_DEPENDENCIES = $(librsaref_a_LIBADD)

BUILT_SOURCES = $(nodist_librsaref_a_SOURCES)
INCLUDES = -I$(top_srcdir)/include
ETAGS_ARGS = Makefile.am

DISTCLEANFILES = $(nodist_librsaref_a_SOURCES) stamp-rsaref

#EXTRA_DIST = 

$(nodist_librsaref_a_SOURCES): stamp-rsaref

stamp-rsaref:
	@rm -f stamp-rsaref stamp-rsarefT
	@echo timestamp > stamp-rsarefT 2> /dev/null
	for i in $(nodist_librsaref_a_SOURCES); do \
	    case "@MAKE_LIBRSAREF@" in \
	     '') touch $$i ;; \
	     *) case "$$i" in \
		 *.h) r_dst=$(srcdir)/$$i ;; \
		 *)   r_dst=$$i ;; \
		esac ; \
	 	cmp -s $${r_dst} $(srcdir)/../@RSADIR@/source/$$i 2>/dev/null \
			|| cp $(srcdir)/../@RSADIR@/source/$$i $${r_dst} ;; \
	    esac ; \
	done
	@echo timestamp > stamp-rsarefT 2> /dev/null
	@mv stamp-rsarefT stamp-rsaref

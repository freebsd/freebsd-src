#
# $Id: Makefile,v 1.3 1994/08/09 18:52:52 csgr Exp $
#

LCRYPTBASE=     libcrypt
LCRYPTSO=       $(LCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)


LDCRYPTBASE=    libdescrypt
LDCRYPTSO=      $(LDCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)

LIB=	descrypt
SRCS=	crypt.c

#MAN3=   crypt.3
#MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3
#MLINKS+=crypt.3 des_cipher.3 crypt.3 des_setkey.3



.include <bsd.lib.mk>

test:
	cd test ; make test ; make clean


# We only install the links if they do not already exist.
# This may have to be revised
afterinstall:
.if defined(SHLIB_MAJOR) && ! defined(NOSHARED)
	@if [ ! -e $(LCRYPTSO) ]; then \
	cd  $(DESTDIR)/$(LIBDIR); \
	rm -f $(LCRYPTSO); \
	ln -s $(LDCRYPTSO) $(LCRYPTSO); \
	fi
.endif
	@if [ ! -e $(LCRYPTBASE).a ]; then \
	cd  $(DESTDIR)/$(LIBDIR); \
	rm -f $(LCRYPTBASE).a $(LCRYPTBASE)_p.a; \
	ln -s $(LDCRYPTBASE).a libcrypt.a; \
	ln -s $(LDCRYPTBASE)_p.a libcrypt_p.a; \
	fi

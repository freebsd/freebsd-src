#
# $Id: Makefile,v 1.6 1994/08/26 19:03:23 wollman Exp $
#

LCRYPTBASE=     libcrypt
LCRYPTSO=       $(LCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)


LDCRYPTBASE=    libdescrypt
LDCRYPTSO=      $(LDCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)

LIB=	descrypt
SRCS=	crypt.c
PRECIOUSLIB=	yes

#MAN3=   crypt.3
#MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3
#MLINKS+=crypt.3 des_cipher.3 crypt.3 des_setkey.3




test:
	cd test ; make test ; make clean


.include <bsd.lib.mk>

# We only install the links if they do not already exist.
# This may have to be revised
afterinstall:
.if !defined(NOPIC) && defined(SHLIB_MAJOR)
	@cd $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTSO) ]; then \
	rm -f $(LCRYPTSO); \
	ln -s $(LDCRYPTSO) $(LCRYPTSO); \
	fi
.endif
	@cd  $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTBASE).a ]; then \
	rm -f $(LCRYPTBASE).a; \
	ln -s $(LDCRYPTBASE).a libcrypt.a; \
	fi
.if !defined(NOPROFILE)
	@cd  $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTBASE)_p.a ]; then \
	rm -f $(LCRYPTBASE)_p.a; \
	ln -s $(LDCRYPTBASE)_p.a libcrypt_p.a; \
	fi
.endif



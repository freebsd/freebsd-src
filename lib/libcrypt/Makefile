#
# $Id: Makefile,v 1.2 1994/08/09 18:49:04 csgr Exp $
#

LCRYPTBASE= 	libcrypt
LCRYPTSO=	$(LCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)


LSCRYPTBASE= 	libscrypt
LSCRYPTSO=	$(LSCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)

# called libscrypt - for scramble crypt!
LIB=	scrypt
SRCS=	crypt.c

# We only install the links if they do not already exist.
# This may have to be revised
afterinstall:
.if defined(SHLIB_MAJOR) && !defined(NOSHARED)
	@if [ ! -e $(LCRYPTSO) ]; then \
	cd  $(DESTDIR)/$(LIBDIR); \
	rm -f $(LCRYPTSO); \
	ln -s $(LSCRYPTSO) $(LCRYPTSO); \
	fi
.endif
	@if [ ! -e $(LCRYPTBASE).a ]; then \
	cd  $(DESTDIR)/$(LIBDIR); \
	rm -f $(LCRYPTBASE).a $(LCRYPTBASE)_p.a; \
	ln -s $(LSCRYPTBASE).a libcrypt.a; \
	ln -s $(LSCRYPTBASE)_p.a libcrypt_p.a; \
	fi

.include <bsd.lib.mk>

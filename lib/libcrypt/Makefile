#
# $Id: Makefile,v 1.3 1994/08/12 21:12:37 csgr Exp $
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
.if !defined(NOPIC) 
	@cd $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTSO) ]; then \
	rm -f $(LCRYPTSO); \
	ln -s $(LSCRYPTSO) $(LCRYPTSO); \
	fi
.endif
	@cd  $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTBASE).a ]; then \
	rm -f $(LCRYPTBASE).a; \
	ln -s $(LSCRYPTBASE).a libcrypt.a; \
	fi
.if !defined(NOPROFILE)
	@cd  $(DESTDIR)/$(LIBDIR); \
	if [ ! -e $(LCRYPTBASE)_p.a ]; then \
	rm -f $(LCRYPTBASE)_p.a; \
	ln -s $(LSCRYPTBASE)_p.a libcrypt_p.a; \
	fi
.endif

.include <bsd.lib.mk>

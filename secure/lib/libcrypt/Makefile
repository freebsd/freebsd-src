#
# $FreeBSD$
#

LCRYPTBASE=     libcrypt
LCRYPTSO=       $(LCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)

LDCRYPTBASE=    libdescrypt
LDCRYPTSO=      $(LDCRYPTBASE).so.$(SHLIB_MAJOR).$(SHLIB_MINOR)

.PATH:	${.CURDIR}/../../../lib/libmd

LIB=	descrypt
SRCS=	crypt.c crypt-md5.c md5c.c
MAN3=	crypt.3
CFLAGS+=	-I${.CURDIR}/../../../lib/libmd -Wall
PRECIOUSLIB=	yes

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

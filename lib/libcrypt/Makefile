#
# $FreeBSD$
#

SHLIB_MAJOR=	2
LIB=		scrypt

LCRYPTBASE=	libcrypt
LSCRYPTBASE=	lib${LIB}

LCRYPTSO=	${LCRYPTBASE}.so.${SHLIB_MAJOR}
LSCRYPTSO=	${LSCRYPTBASE}.so.${SHLIB_MAJOR}

.if ${OBJFORMAT} == elf
SONAME=		${LCRYPTSO}
.endif

.PATH:		${.CURDIR}/../libmd
SRCS=		crypt.c crypt-md5.c md5c.c misc.c
STATICSRCS=	md5c.c
STATICOBJS=	${STATICSRCS:S/.c/.o/g}
MAN=		crypt.3
MLINKS=		crypt.3 crypt_get_format.3 crypt.3 crypt_set_format.3
CFLAGS+=	-I${.CURDIR}/../libmd
CFLAGS+=	-DLIBC_SCCS -Wall
# And the auth_getval() code and support.
.PATH:		${.CURDIR}/../libutil
SRCS+=		auth.c property.c
.for sym in MD5Init MD5Final MD5Update MD5Pad auth_getval \
		property_find properties_read properties_free
CFLAGS+=	-D${sym}=__${sym}
.endfor
PRECIOUSLIB=	yes

# Include this early to pick up the definitions of SHLIB_MAJOR and
# SHLIB_MINOR which are used in the existence tests.
.include "${.CURDIR}/../Makefile.inc"

# We only install the links if they do not already exist.
# This may have to be revised
.if !exists(${DESTDIR}${LIBDIR}/${LCRYPTBASE}.a)
SYMLINKS+=	${LSCRYPTBASE}.a ${LIBDIR}/${LCRYPTBASE}.a
.endif
.if !defined(NOPROFILE) && !exists(${DESTDIR}${LIBDIR}/${LCRYPTBASE}_p.a)
SYMLINKS+=	${LSCRYPTBASE}_p.a ${LIBDIR}/${LCRYPTBASE}_p.a
.endif
.if !defined(NOPIC) && !exists(${DESTDIR}${SHLIBDIR}/${LCRYPTSO})
SYMLINKS+=	${LSCRYPTSO} ${SHLIBDIR}/${LCRYPTSO}
.endif
.if !defined(NOPIC) && ${OBJFORMAT} == elf && \
    !exists(${DESTDIR}${SHLIBDIR}/${LCRYPTBASE}.so)
SYMLINKS+=	${LSCRYPTBASE}.so ${SHLIBDIR}/${LCRYPTBASE}.so
.endif

.include <bsd.lib.mk>

afterinstall:
.if !defined(NOPIC)
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f ${LCRYPTSO}; \
	ln -sf ${LSCRYPTSO} ${LCRYPTSO};
.endif
.if !defined(NOPIC) && ${OBJFORMAT} == elf
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f ${LCRYPTBASE}.so; \
	ln -sf ${LSCRYPTBASE}.so libcrypt.so
.endif
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f ${LCRYPTBASE}.a; \
	ln -sf ${LSCRYPTBASE}.a libcrypt.a
.if !defined(NOPROFILE)
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f ${LCRYPTBASE}_p.a; \
	ln -sf ${LSCRYPTBASE}_p.a libcrypt_p.a
.endif

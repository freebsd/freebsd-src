#
# $Id: Makefile,v 1.10 1997/09/05 12:12:35 peter Exp $
#

LCRYPTBASE= 	libcrypt
LSCRYPTBASE= 	libscrypt

.if ${BINFORMAT} != elf
LCRYPTSO=	${LCRYPTBASE}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
LSCRYPTSO=	${LSCRYPTBASE}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.else
LCRYPTSO=	${LCRYPTBASE}.so.${SHLIB_MAJOR}
LSCRYPTSO=	${LSCRYPTBASE}.so.${SHLIB_MAJOR}
.endif

# called libscrypt - for scramble crypt!
.PATH:	${.CURDIR}/../libmd
LIB=	scrypt
SRCS=	crypt.c md5c.c
CFLAGS+=	-I${.CURDIR}/../libmd -DLIBC_SCCS

.if ${BINFORMAT} == elf
SONAME=	${LCRYPTBASE}.so.${SHLIB_MAJOR}
.endif

# We only install the links if they do not already exist.
# This may have to be revised

afterinstall:
.if !defined(NOPIC)
	@cd ${DESTDIR}${SHLIBDIR}; \
	if [ ! -e ${LCRYPTSO} ]; then \
	rm -f ${LCRYPTSO}; \
	ln -s ${LSCRYPTSO} ${LCRYPTSO}; \
	fi
.endif
.if !defined(NOPIC) && ${BINFORMAT} == elf
	@cd  ${DESTDIR}${LIBDIR}; \
	if [ ! -e ${LCRYPTBASE}.so ]; then \
	rm -f ${LCRYPTBASE}.so; \
	ln -s ${LSCRYPTBASE}.so libcrypt.so; \
	fi
.endif
	@cd  ${DESTDIR}${LIBDIR}; \
	if [ ! -e ${LCRYPTBASE}.a ]; then \
	rm -f ${LCRYPTBASE}.a; \
	ln -s ${LSCRYPTBASE}.a libcrypt.a; \
	fi
.if !defined(NOPROFILE)
	@cd  ${DESTDIR}${LIBDIR}; \
	if [ ! -e ${LCRYPTBASE}_p.a ]; then \
	rm -f ${LCRYPTBASE}_p.a; \
	ln -s ${LSCRYPTBASE}_p.a libcrypt_p.a; \
	fi
.endif

.include <bsd.lib.mk>

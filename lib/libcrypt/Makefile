#
# $Id: Makefile,v 1.17 1999/01/24 07:48:30 markm Exp $
#

LCRYPTBASE= 	libcrypt
LSCRYPTBASE= 	libscrypt

.if ${OBJFORMAT} != elf
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

.if ${OBJFORMAT} == elf
SONAME=	${LCRYPTBASE}.so.${SHLIB_MAJOR}
.endif

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

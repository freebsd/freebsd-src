#
# $Id: Makefile,v 1.14 1998/09/02 15:09:15 bde Exp $
#

SHLIB_MAJOR=	3
.PATH:		${.CURDIR}/../../lib/libmd
SRCS=		crypt.c crypt-md5.c md5c.c crypt-shs.c shs.c
MAN3=		crypt.3 shs.3 descrypt.3
CFLAGS+=	-I${.CURDIR}/../libmd -DLIBC_SCCS
DESCRYPT=	${.CURDIR}/../../secure/lib/libcrypt/crypt-des.c

## build exportable crypt or des crypt?
.if exists(${DESCRYPT}) && !defined(NOCRYPT) && !defined(NOSECURE)
.PATH:		${.CURDIR}/../../secure/lib/libcrypt
CIPHERTYPE=	des
SRCS+=		crypt-des.c
CFLAGS+=	-I${.CURDIR} -DDES_CRYPT
.else
CIPHERTYPE=	exp
.endif

LIB=${CIPHERTYPE}crypt
LCRYPTBASE=	libcrypt
LSCRYPTBASE=	lib${LIB}

.if ${OBJFORMAT} != elf
LCRYPTSO=	${LCRYPTBASE}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
LSCRYPTSO=	${LSCRYPTBASE}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.else
SONAME=		${LCRYPTBASE}.so.${SHLIB_MAJOR}
LCRYPTSO=	${LCRYPTBASE}.so.${SHLIB_MAJOR}
LSCRYPTSO=	${LSCRYPTBASE}.so.${SHLIB_MAJOR}
.endif

afterinstall:
.if !defined(NOPIC)
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f ${LCRYPTSO}; \
	ln -s ${LSCRYPTSO} ${LCRYPTSO}; \
	rm -f ${LCRYPTBASE}.so.2; \
	ln -s ${LSCRYPTSO} ${LCRYPTBASE}.so.2
.endif
.if !defined(NOPIC) && ${OBJFORMAT} == elf
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f ${LCRYPTBASE}.so; \
	ln -s ${LSCRYPTBASE}.so libcrypt.so
.endif
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f ${LCRYPTBASE}.a; \
	ln -s ${LSCRYPTBASE}.a libcrypt.a
.if !defined(NOPROFILE)
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f ${LCRYPTBASE}_p.a; \
	ln -s ${LSCRYPTBASE}_p.a libcrypt_p.a
.endif

.include <bsd.lib.mk>

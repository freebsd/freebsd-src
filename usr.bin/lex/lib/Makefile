# $Id$

LIB=    ln
SRCS=   libmain.c libyywrap.c
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a \

.if !defined(NOPROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
.endif

.if !defined(NOSHARED) 
LINKS+= ${LIBDIR}/libln.so.$(SHLIB_MAJOR).$(SHLIB_MINOR) \
		${LIBDIR}/libl.so.$(SHLIB_MAJOR).$(SHLIB_MINOR)
.endif

#This is where we get our SHLIB_MAJOR and SHLIB_MINOR
.include "${.CURDIR}/../../../lib/Makefile.inc"

.include <bsd.lib.mk>


# $FreeBSD$

EXPAT=		${.CURDIR}/../../contrib/expat

LIB=		bsdxml
SHLIBDIR?=	/lib
SHLIB_MAJOR=	2
SRCS=		xmlparse.c xmlrole.c xmltok.c
INCS=		bsdxml.h
MAN=		libbsdxml.3

.PATH:		${EXPAT}/lib

CFLAGS+= 	-I${.CURDIR}
CLEANFILES=	bsdxml.h

# OK, so it is not entirely unadultered: we ammend the COPYING
# to point people to the right place, get rid of some VMS stuff
# and use FreeBSD style indempotency #ifndefs.
#
bsdxml.h: expat.h
	unifdef -U__VMS < ${.ALLSRC} | \
	sed -e 's/XmlParse_INCLUDED/_BSD_XML_H_/' \
	    -e 's/COPYING/src\/contrib\/expat\/COPYING/' \
		> ${.TARGET}

.include <bsd.lib.mk>

# $FreeBSD$

EXPAT=		${.CURDIR}/../../contrib/expat

LIB=		bsdxml
SHLIBDIR?=	/lib
SHLIB_MAJOR=	4
SRCS=		xmlparse.c xmlrole.c xmltok.c
INCS=		bsdxml.h bsdxml_external.h
MAN=		libbsdxml.3

.PATH:		${EXPAT}/lib

CFLAGS+= 	-I${.CURDIR} -DHAVE_EXPAT_CONFIG_H
CLEANFILES=	bsdxml.h bsdxml_external.h

WARNS?=		2

# OK, so it is not entirely unadultered: we ammend the COPYING
# to point people to the right place, get rid of some VMS stuff
# and use FreeBSD style indempotency #ifndefs. We also want to
# point it at the new bsdxml_external.h rather than the old
# expat_external.h file.
#
bsdxml.h: expat.h
	unifdef -U__VMS < ${.ALLSRC} | \
	sed -e 's/XmlParse_INCLUDED/_BSD_XML_H_/' \
	    -e 's/COPYING/src\/contrib\/expat\/COPYING/' \
	    -e 's/expat_external/bsdxml_external/' \
		> ${.TARGET}

bsdxml_external.h: expat_external.h
	cp -f ${.ALLSRC} ${.TARGET}

.include <bsd.lib.mk>

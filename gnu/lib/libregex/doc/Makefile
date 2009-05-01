# $FreeBSD: src/gnu/lib/libregex/doc/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $

INFO = regex
INFOSECTION= "Programming & development tools."
INFOENTRY_regex= "* Regex: (regex).		Regular expression library."

CLEANFILES += regex.texi

regex.texi: xregex.texi ../regex.h
	awk -f ${.CURDIR}/include.awk -v source=${.CURDIR}/../regex.h \
	  < ${.CURDIR}/xregex.texi \
          | expand >$@

.include <bsd.info.mk>

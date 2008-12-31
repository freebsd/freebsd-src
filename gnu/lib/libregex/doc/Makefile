# $FreeBSD: src/gnu/lib/libregex/doc/Makefile,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $

INFO = regex
INFOSECTION= "Programming & development tools."
INFOENTRY_regex= "* Regex: (regex).		Regular expression library."

CLEANFILES += regex.texi

regex.texi: xregex.texi ../regex.h
	awk -f ${.CURDIR}/include.awk -v source=${.CURDIR}/../regex.h \
	  < ${.CURDIR}/xregex.texi \
          | expand >$@

.include <bsd.info.mk>

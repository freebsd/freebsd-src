INFO = regex
CLEANFILES += regex.texi

regex.texi: xregex.texi ../regex.h
	awk -f ${.CURDIR}/include.awk -vsource=${.CURDIR}/../regex.h \
	  < ${.CURDIR}/xregex.texi \
          | expand >$@

.include <bsd.info.mk>

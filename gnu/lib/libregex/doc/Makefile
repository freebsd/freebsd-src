INFO = regex
CLEANFILES += regex.texi

regex.texi: xregex.texi
	awk -f ${.CURDIR}/include.awk -vsource=${.CURDIR}/../regex.h \
	  < ${.CURDIR}/xregex.texi \
          | expand >$@

.include <bsd.info.mk>

DB=	freebsd-tips

BLDS=	${DB:S/$/.dat/}
FILES=	${DB} ${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in ${DB}
$f.dat: $f
	PATH=$$PATH:/usr/bin:${FORTUNE_OBJ}/strfile \
	    strfile -Cs ${.ALLSRC} ${.TARGET}
.endfor

.include <bsd.prog.mk>

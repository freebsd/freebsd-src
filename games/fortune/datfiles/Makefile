#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD$

FILES=	fortunes fortunes2 freebsd-tips murphy startrek zippy
BLDS=	fortunes.dat fortunes2.dat murphy.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat freebsd-tips.dat

# Pass all new entries by ${MAINTAINER} to preserve some semblance of
# humor in the fortune files.  What's funny to you on 6 beers may not
# be funny to anyone else.
MAINTAINER=	jkh

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# THREE LINES AND UNCOMMENT THE FOURTH LINE.

# THE THREE LINES:
FILES+=	fortunes2-o limerick murphy-o gerrold.limerick
BLDS+=	fortunes2-o.dat limerick.dat murphy-o.dat gerrold.limerick.dat
TYPE=	real

# THE FOURTH LINE:
#TYPE=	fake

FILES+=	${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in fortunes fortunes2 fortunes2-o freebsd-tips gerrold.limerick limerick murphy murphy-o startrek zippy
$f.dat: $f
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Cs ${.ALLSRC} ${.TARGET}
.endfor

fortunes-o.dat: fortunes-o
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Csx ${.ALLSRC} ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	LC_ALL=C tr a-zA-Z n-za-mN-ZA-M < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>

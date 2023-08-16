
SUBDIR=	tinfo ncurses form menu panel

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Ntinfo*}
SUBDIR_DEPEND_${subdir}=	tinfo
.endfor

.include <bsd.subdir.mk>

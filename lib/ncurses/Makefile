# $FreeBSD$

SUBDIR=	ncurses form menu panel

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Nncurses*}
SUBDIR_DEPEND_${subdir}=	ncurses
.endfor

.include <bsd.subdir.mk>

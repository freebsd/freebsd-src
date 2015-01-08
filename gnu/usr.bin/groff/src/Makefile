# $FreeBSD$

SUBDIR=		libs devices preproc roff utils

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Nlibs}
SUBDIR_DEPEND_${subdir}=	libs
.endfor

.include <bsd.subdir.mk>

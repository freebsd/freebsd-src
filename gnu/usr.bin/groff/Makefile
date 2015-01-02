# $FreeBSD$

SUBDIR=		contrib font man src tmac

.for subdir in ${SUBDIR:Nsrc}
SUBDIR_DEPEND_${subdir}=	src
.endfor

.include <bsd.subdir.mk>

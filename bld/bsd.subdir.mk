# $FreeBSD$

DIRDEP?=

.for _F in ${SUBDIR}
.if empty(.SRCREL)
DIRDEP+= ${_F}
.else
DIRDEP+= ${.SRCREL}/${_F}
.endif
.endfor

.if ${__MKLVL__} != 1
# We do everything by dependencies.
all	: .PHONY
.endif

.include <bsd.dirdep.mk>

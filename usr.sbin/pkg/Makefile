# $FreeBSD$

_BRANCH!=	${MAKE} -C ${SRCTOP}/release -V BRANCH
BRANCH?=	${_BRANCH}
.if ${BRANCH:MCURRENT} != ""
PKGCONFBRANCH?=	latest
.else
. if ${BRANCH:MBETA*} || ${BRANCH:MRC*} || ${BRANCH:MRELEASE*}
PKGCONFBRANCH?=	quarterly
. else
.  if ${MACHINE} != "amd64" && ${MACHINE} != "i386"
PKGCONFBRANCH?=	quarterly
.  else
PKGCONFBRANCH?=	latest
.  endif
. endif
.endif
CONFS=		FreeBSD.conf.${PKGCONFBRANCH}
CONFSNAME=	FreeBSD.conf
CONFSDIR=	/etc/pkg
CONFSMODE=	644
PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c
MAN=	pkg.7

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:	${SRCTOP}/contrib/libucl/include
LIBADD=	archive fetch ucl sbuf crypto ssl

.include <bsd.prog.mk>

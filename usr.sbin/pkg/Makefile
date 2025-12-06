PACKAGE=	pkg-bootstrap

.MAKEFLAGS: -W
_BRANCH!=	${MAKE} -C ${SRCTOP}/release -V BRANCH
BRANCH?=	${_BRANCH}
.if ${BRANCH:MCURRENT} != ""
PKGCONFBRANCH?=	latest
.else
. if ${BRANCH:MBETA*} || ${BRANCH:MRC*} || ${BRANCH:MRELEASE*}
PKGCONFBRANCH?=	quarterly-release
. else
.  if ${MACHINE} != "amd64" && ${MACHINE} != "i386" && ${MACHINE} != "arm64"
PKGCONFBRANCH?=	quarterly
.  else
PKGCONFBRANCH?=	latest
.  endif
. endif
.endif
PKGCONF?=	FreeBSD.conf.${PKGCONFBRANCH}
CONFS=		${PKGCONF}
CONFSNAME_${PKGCONF}=	${PKGCONF:C/\.conf.+$/.conf/}
CONFSDIR=	/etc/pkg
CONFSMODE=	644
PROG=	pkg
SRCS=	pkg.c rsa.c dns_utils.c config.c ecc.c hash.c
MAN=	pkg.7

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:	${SRCTOP}/contrib/libucl/include
LIBADD=	archive der fetch pkgecc ucl crypto ssl util md

CFLAGS+=-I${SRCTOP}/contrib/libder/libder
CFLAGS+=-I${SRCTOP}/crypto/libecc/include

.include <bsd.prog.mk>

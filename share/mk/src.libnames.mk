# $FreeBSD$
#
# The include file <src.libnames.mk> define library names suitable
# for INTERNALLIB and PRIVATELIB definition

.if !target(__<bsd.init.mk>__)
.error src.libnames.mk cannot be included directly.
.endif

ROOTOBJDIR=	${.OBJDIR:S/${.CURDIR}//}${.MAKE.MAKEFILES:M*/src.libnames.mk:H:H:H}

LIBATF_CDIR=	${ROOTOBJDIR}/lib/atf/libatf-c
LDATF_C?=	${LIBATF_CDIR}/libatf-c.so
LIBATF_C?=	${LIBATF_CDIR}/libatf-c.a

LIBATF_CXXDIR=	${ROOTOBJDIR}/lib/atf/libatf-c++
LDATF_CXX?=	${LIBATF_CXXDIR}/libatf-c++.so
LIBATF_CXX?=	${LIBATF_CXXDIR}/libatf-c++.a

LIBHEIMIPCCDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimipcc
LDHEIMIPCC?=	${LIBHEIMIPCCDIR}/libheimipcc.so
LIBHEIMIPCC?=	${LIBHEIMIPCCDIR}/libheimipcc.a

LIBHEIMIPCSDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimipcs
LDHEIMIPCS?=	${LIBHEIMIPCSDIR}/libheimipcs.so
LIBHEIMIPCS?=	${LIBHEIMIPCSDIR}/libheimipcs.a

LIBLDNSDIR=	${ROOTOBJDIR}/lib/libldns
LDLDNS?=	${LIBLDNSDIR}/libldns.so
LIBLDNS?=	${LIBLDNSDIR}/libldns.a

LIBSSHDIR=	${ROOTOBJDIR}/secure/lib/libssh
LDSSH?=		${LIBSSHDIR}/libssh.so
LIBSSH?=	${LIBSSHDIR}/libssh.a

LIBUNBOUNDDIR=	${ROOTOBJDIR}/lib/libunbound
LDUNBOUND?=	${LIBUNBOUNDDIR}/libunbound.so
LIBUNBOUND?=	${LIBUNBOUNDDIR}/libunbound.a

LIBUCLDIR=	${ROOTOBJDIR}/lib/libucl
LDUCL?=		${LIBUCLDIR}/libucl.so
LIBUCL?=	${LIBUCLDIR}/libucl.a

LIBREADLINEDIR=	${ROOTOBJDIR}/gnu/lib/libreadline/readline
LDREADLINE?=	${LIBREADLINEDIR}/libreadline.a
LIBREADLINE?=	${LIBREADLINEDIR}/libreadline.a

LIBOHASHDIR=	${ROOTOBJDIR}/lib/libohash
LDOHASH?=	${LIBOHASHDIR}/libohash.a
LIBOHASH?=	${LIBOHASHDIR}/libohash.a

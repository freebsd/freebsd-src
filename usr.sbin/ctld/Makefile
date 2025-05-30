.include <src.opts.mk>

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

PACKAGE=	ctl
PROG_CXX=	ctld
SRCS=		ctld.cc conf.cc discovery.cc isns.cc kernel.cc
SRCS+=		login.cc parse.y token.l y.tab.h uclparse.cc
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/sys
CFLAGS+=	-I${SRCTOP}/sys/cam/ctl
CFLAGS+=	-I${SRCTOP}/sys/dev/iscsi
CFLAGS+=	-I${SRCTOP}/lib/libiscsiutil
#CFLAGS+=	-DICL_KERNEL_PROXY
NO_WCAST_ALIGN=
CXXWARNFLAGS.gcc= -Wno-shadow
MAN=		ctld.8 ctl.conf.5

LIBADD=		bsdxml iscsiutil md sbuf util ucl m nv

YFLAGS+=	-v
CLEANFILES=	y.tab.c y.tab.h y.output

NO_WMISSING_VARIABLE_DECLARATIONS=

.if ${MK_ISCSI} != "no"
CFLAGS+=	-DWANT_ISCSI
.endif

.include <bsd.prog.mk>

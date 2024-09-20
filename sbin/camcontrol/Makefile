.include <src.opts.mk>

PACKAGE=runtime
PROG=	camcontrol
SRCS=	camcontrol.c
SRCS+=	attrib.c
SRCS+=	depop.c
SRCS+=	epc.c
SRCS+=	fwdownload.c
SRCS+=	modeedit.c
SRCS+=	persist.c
SRCS+=	progress.c
SRCS+=	timestamp.c
SRCS+=	util.c
SRCS+=	zone.c
.PATH:	${SRCTOP}/sbin/nvmecontrol
CFLAGS+= -I${SRCTOP}/sbin/nvmecontrol
SRCS+=	identify_ext.c
SRCS+=	nc_util.c
.PATH:	${SRCTOP}/sys/dev/nvme
SRCS+=	nvme_util.c
# This is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif
CFLAGS+= -I${SRCTOP}/lib/libnvmf
LIBADD=	cam nvmf sbuf util
MAN=	camcontrol.8

.include <bsd.prog.mk>

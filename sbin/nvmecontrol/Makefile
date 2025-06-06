.include <src.opts.mk>

PACKAGE=nvme-tools
PROG=	nvmecontrol
SRCS+=	comnd.c
SRCS+=	connect.c
SRCS+=	devlist.c
SRCS+=	disconnect.c
SRCS+=	discover.c
SRCS+=	fabrics.c
SRCS+=	firmware.c
SRCS+=	format.c
SRCS+=	identify.c
SRCS+=	identify_ext.c
SRCS+=	logpage.c
SRCS+=	nc_util.c
SRCS+=	ns.c
SRCS+=	nsid.c
SRCS+=	nvme_util.c
SRCS+=	nvmecontrol.c
SRCS+=	passthru.c
SRCS+=	perftest.c
SRCS+=	power.c
SRCS+=	reconnect.c
SRCS+=	reset.c
SRCS+=	resv.c
SRCS+=	sanitize.c
SRCS+=  selftest.c
SRCS+=	telemetry.c
CFLAGS+= -I${SRCTOP}/lib/libnvmf
MAN=	nvmecontrol.8
LDFLAGS+= -rdynamic
LIBADD+= nvmf sbuf util
SUBDIR=	modules
HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.PATH:	${SRCTOP}/sys/dev/nvme

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

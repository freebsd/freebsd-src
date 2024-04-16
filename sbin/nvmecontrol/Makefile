.include <src.opts.mk>

PACKAGE=nvme-tools
PROG=	nvmecontrol
SRCS+=	comnd.c
SRCS+=	devlist.c
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
SRCS+=	reset.c
SRCS+=	resv.c
SRCS+=	sanitize.c
SRCS+=  selftest.c
MAN=	nvmecontrol.8
LDFLAGS+= -rdynamic
LIBADD+= util
SUBDIR=	modules
HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.PATH:	${SRCTOP}/sys/dev/nvme

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

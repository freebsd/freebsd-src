.PATH:	${SRCTOP}/sys/dev/bnxt/bnxt_re

KMOD=bnxt_re
SRCS	+= ib_verbs.c ib_verbs.h
SRCS	+= qplib_fp.c qplib_fp.h
SRCS	+= qplib_sp.c qplib_sp.h
SRCS	+= qplib_res.c qplib_res.h
SRCS	+= qplib_rcfw.c qplib_rcfw.h
SRCS	+= stats.c stats.h
SRCS	+= main.c bnxt_re.h
SRCS	+= opt_inet.h opt_inet6.h opt_ratelimit.h
SRCS	+= ${LINUXKPI_GENSRCS}

CFLAGS+= -I${SRCTOP}/sys/dev/bnxt/bnxt_en
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}

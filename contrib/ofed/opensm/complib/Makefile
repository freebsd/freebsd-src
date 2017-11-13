# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		osmcomp
SHLIB_MAJOR=    3
MK_PROFILE=     no

SRCS= \
cl_complib.c \
cl_dispatcher.c \
cl_event.c \
cl_event_wheel.c \
cl_list.c \
cl_log.c \
cl_map.c \
cl_nodenamemap.c \
cl_pool.c \
cl_ptr_vector.c \
cl_spinlock.c \
cl_statustext.c \
cl_thread.c \
cl_threadpool.c \
cl_timer.c \
cl_vector.c \
ib_statustext.c

CFLAGS+=	-I${.CURDIR}/.. -DHAVE_CONFIG_H=1
CFLAGS+=	-I${INCLUDEDIR}/infiniband
LDADD=		pthread
VERSION_MAP=	${.CURDIR}/libosmcomp.map

.include <bsd.lib.mk>

# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

COMPLIBDIR= 	${.CURDIR}/../../management/opensm/complib

.PATH: ${COMPLIBDIR}

LIB=		osmcomp
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	cl_complib.c cl_dispatcher.c cl_event.c cl_event_wheel.c cl_list.c
SRCS+=	cl_log.c cl_map.c cl_pool.c cl_ptr_vector.c cl_spinlock.c
SRCS+=	cl_statustext.c cl_thread.c cl_threadpool.c cl_timer.c cl_vector.c
SRCS+=	ib_statustext.c cl_nodenamemap.c

CFLAGS+= -I${.CURDIR} -I/usr/include/infiniband

VERSION_MAP= ${COMPLIBDIR}/libosmcomp.map

.include <bsd.lib.mk>

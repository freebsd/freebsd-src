# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

.PATH: ${COMPLIBDIR}

LIB=		osmcomp
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	cl_complib.c cl_dispatcher.c cl_event.c cl_event_wheel.c cl_list.c
SRCS+=	cl_log.c cl_map.c cl_pool.c cl_ptr_vector.c cl_spinlock.c
SRCS+=	cl_statustext.c cl_thread.c cl_threadpool.c cl_timer.c cl_vector.c
SRCS+=	ib_statustext.c cl_nodenamemap.c

LIBADD=	pthread

VERSION_MAP= ${COMPLIBDIR}/libosmcomp.map

.include <bsd.lib.mk>

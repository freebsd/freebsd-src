# $FreeBSD: src/gnu/lib/libgcc_r/Makefile,v 1.4.4.1 2000/11/01 01:20:37 jdp Exp $

LIB=	gcc_r
CFLAGS+=-D_PTHREADS
CFLAGS+=-D'__GTHREAD_MUTEX_INIT_FUNCTION(m)=pthread_mutex_init(m, NULL)'

.include "../libgcc/Makefile"

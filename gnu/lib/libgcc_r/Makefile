# $FreeBSD: src/gnu/lib/libgcc_r/Makefile,v 1.5.26.1 2008/10/02 02:57:24 kensmith Exp $

LIB=	gcc_r
CFLAGS+=-D_PTHREADS
CFLAGS+=-D'__GTHREAD_MUTEX_INIT_FUNCTION(m)=pthread_mutex_init(m, NULL)'

.include "../libgcc/Makefile"

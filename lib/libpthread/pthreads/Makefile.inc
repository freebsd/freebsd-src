#	from: @(#)Makefile.inc	5.6 (Berkeley) 6/4/91

# pthread sources
.PATH: ${.CURDIR}/pthreads

SRCS+= cond.c fd.c fd_kern.c fd_pipe.c file.c globals.c malloc.c mutex.c \
       pthread.c pthread_attr.c queue.c signal.c \
       pthread_join.c pthread_detach.c sleep.c

MAN2+=

MAN3+=


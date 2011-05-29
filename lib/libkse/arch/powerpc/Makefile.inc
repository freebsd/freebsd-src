# $FreeBSD$

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	enter_uts.S context.S pthread_md.c

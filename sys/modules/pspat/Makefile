# $FreeBSD$

.PATH:  ${SRCTOP}/sys/net/pspat
KMOD=   pspat
SRCS=   pspat.c
SRCS+= mailbox.c pspat_arbiter.c
SRCS+= pspat_client.c pspat_dispatcher.c
SRCS+= pspat_opts.c

.include <bsd.kmod.mk>

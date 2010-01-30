# $FreeBSD$

LIB=	capsicum
DPADD=	${LIBSBUF}
LDADD=	-lsbuf

SRCS=					\
	libcapsicum.c			\
	libcapsicum_sandbox.c		\
	libcapsicum_sandbox_io.c	\
	libcapsicum_host.c		\
	libcapsicum_host_io.c		\
	libcapsicum_fdlist.c

INCS=	libcapsicum.h

CFLAGS+=	-I.

SHLIB_MAJOR=	1

WARNS?=	6

MAN=	libcapsicum.3
MAN+=	libcapsicum_host.3
MAN+=	libcapsicum_sandbox.3
MLINKS=	libcapsicum.3 lc_limitfd.3		\
	libcapsicum_host.3 lch_autosandbox_isenabled.3	\
	libcapsicum_host.3 lch_start.3	\
	libcapsicum_host.3 lch_startfd.3	\
	libcapsicum_host.3 lch_stop.3		\
	libcapsicum_host.3 lch_getsock.3	\
	libcapsicum_host.3 lch_getpid.3	\
	libcapsicum_host.3 lch_getprocdesc.3	\
	libcapsicum_host.3 lch_recv.3		\
	libcapsicum_host.3 lch_recv_rights.3	\
	libcapsicum_host.3 lch_rpc.3		\
	libcapsicum_host.3 lch_rpc_rights.3	\
	libcapsicum_host.3 lch_send.3		\
	libcapsicum_host.3 lch_send_rights.3	\
	libcapsicum_sandbox.3 lcs_get.3	\
	libcapsicum_sandbox.3 lcs_getsock.3	\
	libcapsicum_sandbox.3 lcs_recv.3	\
	libcapsicum_sandbox.3 lcs_recv_rights.3	\
	libcapsicum_sandbox.3 lcs_recvrpc.3	\
	libcapsicum_sandbox.3 lcs_recvrpc_rights.3	\
	libcapsicum_sandbox.3 lcs_send.3	\
	libcapsicum_sandbox.3 lcs_send_rights.3	\
	libcapsicum_sandbox.3 lcs_sendrpc.3	\
	libcapsicum_sandbox.3 lcs_sendrpc_rights.3

.include <bsd.lib.mk>

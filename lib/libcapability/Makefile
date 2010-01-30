# $FreeBSD$

LIB=	capability
DPADD=	${LIBSBUF}
LDADD=	-lsbuf

SRCS=					\
	libcapability.c			\
	libcapability_sandbox.c		\
	libcapability_sandbox_io.c	\
	libcapability_host.c		\
	libcapability_host_io.c		\
	libcapability_registry.c

INCS=	libcapability.h

CFLAGS+=	-I.

SHLIB_MAJOR=	1

WARNS?=	6

MAN=	libcapability.3
MAN+=	libcapability_host.3
MAN+=	libcapability_sandbox.3
MLINKS=	libcapability.3 lc_limitfd.3		\
	libcapability_host.3 lch_autosandbox_isenabled.3	\
	libcapability_host.3 lch_start.3	\
	libcapability_host.3 lch_startfd.3	\
	libcapability_host.3 lch_stop.3		\
	libcapability_host.3 lch_getsock.3	\
	libcapability_host.3 lch_getpid.3	\
	libcapability_host.3 lch_getprocdesc.3	\
	libcapability_host.3 lch_recv.3		\
	libcapability_host.3 lch_recv_rights.3	\
	libcapability_host.3 lch_rpc.3		\
	libcapability_host.3 lch_rpc_rights.3	\
	libcapability_host.3 lch_send.3		\
	libcapability_host.3 lch_send_rights.3	\
	libcapability_sandbox.3 lcs_get.3	\
	libcapability_sandbox.3 lcs_getsock.3	\
	libcapability_sandbox.3 lcs_recv.3	\
	libcapability_sandbox.3 lcs_recv_rights.3	\
	libcapability_sandbox.3 lcs_recvrpc.3	\
	libcapability_sandbox.3 lcs_recvrpc_rights.3	\
	libcapability_sandbox.3 lcs_send.3	\
	libcapability_sandbox.3 lcs_send_rights.3	\
	libcapability_sandbox.3 lcs_sendrpc.3	\
	libcapability_sandbox.3 lcs_sendrpc_rights.3

.include <bsd.lib.mk>

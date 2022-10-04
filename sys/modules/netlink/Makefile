.PATH:	${SRCTOP}/sys/netlink
KMOD=	netlink

SRCS =	netlink_module.c netlink_domain.c netlink_io.c \
	netlink_message_parser.c netlink_message_writer.c  netlink_generic.c \
	netlink_route.c route/iface.c route/iface_drivers.c route/neigh.c \
	route/nexthop.c route/route.c
SRCS+=	opt_inet.h opt_inet6.h opt_route.h

EXPORT_SYMS=
EXPORT_SYMS+=	nlmsg_get_chain_writer
EXPORT_SYMS+=	nlmsg_refill_buffer
EXPORT_SYMS+=	nlmsg_end
EXPORT_SYMS+=	nlmsg_flush

EXPORT_SYMS= YES

.include <bsd.kmod.mk>

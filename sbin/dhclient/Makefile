# Makefile for ISC DHCP client.
# $FreeBSD$

SUBDIR= common dst minires omapip client


# Our directory structure confuses crunchgen, so we must explicitly
# list the requisite object files when building crunched binaries for
# release floppies.

.ifdef(RELEASE_CRUNCH)

# client
OBJS=  client/clparse.o client/dhclient.o

# common
OBJS+=	client/alloc.o client/bpf.o client/comapi.o client/conflex.o \
	client/ctrace.o client/discover.o client/dispatch.o client/dlpi.o \
	client/dns.o client/ethernet.o client/execute.o client/fddi.o \
	client/icmp.o client/inet.o client/lpf.o client/memory.o \
	client/nit.o client/options.o client/packet.o client/parse.o \
	client/print.o client/raw.o client/resolv.o client/socket.o \
	client/tables.o client/tr.o client/tree.o client/upf.o

# dst
OBJS+=	dst/base64.o dst/dst_api.o dst/dst_support.o dst/hmac_link.o \
	dst/md5_dgst.o dst/prandom.o

# minires
OBJS+=	minires/ns_date.o minires/ns_name.o minires/ns_parse%FIXED.o \
	minires/ns_samedomain.o minires/ns_sign.o minires/ns_verify.o \
	minires/res_comp.o minires/res_findzonecut.o minires/res_init.o	\
	minires/res_mkquery.o minires/res_mkupdate.o minires/res_query.o \
	minires/res_send.o minires/res_sendsigned.o minires/res_update.o

# omapi
OBJS+=	omapip/alloc.o omapip/array.o omapip/auth.o omapip/buffer.o \
	omapip/connection.o omapip/convert.o omapip/dispatch.o \
	omapip/errwarn.o omapip/generic.o omapip/handle.o omapip/hash.o \
	omapip/inet_addr.o omapip/listener.o omapip/message.o \
	omapip/mrtrace.o omapip/protocol.o omapip/result.o \
	omapip/support.o omapip/toisc.o omapip/trace.o

# src/release/Makefile (release.4) tries to invokes this makefile with
# the target set to the list of all objects defined above.  This
# top-level Makefile doesn't actually know how to build those objects
# though, so we must run ``make all'' instead when we are asked to
# generate an individual object file.

${OBJS}: all

.endif

.include <bsd.subdir.mk>

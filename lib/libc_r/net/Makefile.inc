#	@(#)Makefile.inc	8.2 (Berkeley) 9/5/93

# machine-independent net sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/net ${.CURDIR}/../libc/net

SRCS+=	addr2ascii.c ascii2addr.c \
	gethostbydns.c gethostbyht.c gethostbynis.c gethostnamadr.c \
	getnetbydns.c getnetbyht.c getnetbynis.c getnetnamadr.c \
	getproto.c getprotoent.c getprotoname.c getservbyname.c \
	getservbyport.c getservent.c herror.c inet_addr.c inet_lnaof.c \
	inet_makeaddr.c inet_netof.c inet_network.c inet_ntoa.c inet_ntop.c \
	inet_pton.c linkaddr.c map_v4v6.c nsap_addr.c \
	rcmd.c recv.c res_comp.c res_data.c res_debug.c \
	res_init.c res_mkquery.c res_query.c res_send.c send.c ether_addr.c \

# iso_addr.c 
# ns_addr.c ns_ntoa.c

# machine-dependent net sources
.include "${.CURDIR}/../libc/${MACHINE}/net/Makefile.inc"

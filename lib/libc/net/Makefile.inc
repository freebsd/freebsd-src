#	from @(#)Makefile.inc	8.2 (Berkeley) 9/5/93
#	$Id: Makefile.inc,v 1.24 1997/10/15 16:15:41 bde Exp $

# machine-independent net sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/net ${.CURDIR}/../libc/net

SRCS+=	addr2ascii.c ascii2addr.c base64.c ether_addr.c \
	gethostbydns.c gethostbyht.c gethostbynis.c gethostnamadr.c \
	getnetbydns.c getnetbyht.c getnetbynis.c getnetnamadr.c \
	getproto.c getprotoent.c getprotoname.c getservbyname.c \
	getservbyport.c getservent.c herror.c inet_addr.c inet_lnaof.c \
	inet_makeaddr.c inet_net_ntop.c inet_net_pton.c inet_neta.c \
	inet_netof.c inet_network.c inet_ntoa.c inet_ntop.c \
	inet_pton.c linkaddr.c map_v4v6.c ns_addr.c ns_ntoa.c nsap_addr.c \
	rcmd.c recv.c res_comp.c res_data.c res_debug.c \
	res_init.c res_mkquery.c res_query.c res_send.c res_stubs.c \
	send.c
# not supported: iso_addr.c 

# machine-dependent net sources
.include "${.CURDIR}/../libc/${MACHINE}/net/Makefile.inc"

.if ${LIB} == "c"
MAN3+=	addr2ascii.3 byteorder.3 ethers.3 gethostbyname.3 \
	getnetent.3 getprotoent.3 getservent.3 \
	inet.3 linkaddr.3 rcmd.3 resolver.3
# not installed: iso_addr.3 ns.3

MLINKS+=addr2ascii.3 ascii2addr.3
MLINKS+=byteorder.3 htonl.3 byteorder.3 htons.3 byteorder.3 ntohl.3 \
	byteorder.3 ntohs.3
MLINKS+=ethers.3 ether_aton.3 ethers.3 ether_hostton.3 ethers.3 ether_line.3 \
	ethers.3 ether_ntoa.3 ethers.3 ether_ntohost.3
MLINKS+=gethostbyname.3 endhostent.3 gethostbyname.3 gethostbyaddr.3 \
	gethostbyname.3 gethostbyname2.3 gethostbyname.3 gethostent.3 \
	gethostbyname.3 herror.3 gethostbyname.3 hstrerror.3 \
	gethostbyname.3 sethostent.3
MLINKS+=getnetent.3 endnetent.3 getnetent.3 getnetbyaddr.3 \
	getnetent.3 getnetbyname.3 getnetent.3 setnetent.3
MLINKS+=getprotoent.3 endprotoent.3 getprotoent.3 getprotobyname.3 \
	getprotoent.3 getprotobynumber.3 getprotoent.3 setprotoent.3
MLINKS+=getservent.3 endservent.3 getservent.3 getservbyname.3 \
	getservent.3 getservbyport.3 getservent.3 setservent.3
MLINKS+=inet.3 addr.3 inet.3 inet_addr.3 inet.3 inet_aton.3 \
	inet.3 inet_lnaof.3 inet.3 inet_makeaddr.3 inet.3 inet_netof.3 \
	inet.3 inet_network.3 inet.3 inet_ntoa.3 inet.3 network.3 \
	inet.3 ntoa.3
MLINKS+=linkaddr.3 link_addr.3 linkaddr.3 link_ntoa.3 
#MLINKS+=ns.3 ns_addr.3 ns.3 ns_ntoa.3
MLINKS+=rcmd.3 iruserok.3 rcmd.3 rresvport.3 rcmd.3 ruserok.3
MLINKS+=resolver.3 dn_comp.3 resolver.3 dn_expand.3 resolver.3 res_init.3 \
	resolver.3 res_mkquery.3 resolver.3 res_query.3 \
	resolver.3 res_search.3 resolver.3 res_send.3
.endif

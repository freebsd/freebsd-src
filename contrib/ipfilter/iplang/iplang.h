/*
 * Copyright (C) 1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
typedef	struct iface {
	int	if_MTU;
	char	*if_name;
	struct	in_addr	if_addr;
	struct	ether_addr	if_eaddr;
	struct	iface *if_next;
	int	if_fd;
} iface_t;


typedef	struct	send	{
	struct	iface	*snd_if;
	struct	in_addr	snd_gw;
} send_t;


typedef	struct	arp	{
	struct	in_addr	arp_addr;
	struct	ether_addr	arp_eaddr;
	struct	arp *arp_next;
} arp_t;


typedef	struct	aniphdr	{
	union	{
		ip_t		*ahu_ip;
		char		*ahu_data;
		tcphdr_t	*ahu_tcp;
		udphdr_t	*ahu_udp;
		icmphdr_t	*ahu_icmp;
	} ah_un;
	int	ah_optlen;
	int	ah_lastopt;
	int	ah_p;
	size_t	ah_len;
	struct	aniphdr	*ah_next;
	struct	aniphdr	*ah_prev;
} aniphdr_t;

#define	ah_ip	ah_un.ahu_ip
#define	ah_data	ah_un.ahu_data
#define	ah_tcp	ah_un.ahu_tcp
#define	ah_udp	ah_un.ahu_udp
#define	ah_icmp	ah_un.ahu_icmp

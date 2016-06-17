/*
 * IP_VS        ftp application module
 *
 * Version:	$Id: ip_vs_ftp.c,v 1.12 2002/08/10 04:32:35 wensong Exp $
 *
 * Authors:	Wensong Zhang <wensong@linuxvirtualserver.org>
 *
 * Changes:
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * Most code here is taken from ip_masq_ftp.c in kernel 2.2. The difference
 * is that ip_vs_ftp module handles the reverse direction to ip_masq_ftp.
 *
 *		IP_MASQ_FTP ftp masquerading module
 *
 * Version:	@(#)ip_masq_ftp.c 0.04   02/05/96
 *
 * Author:	Wouter Gadeyne
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <net/protocol.h>
#include <net/tcp.h>

#include <net/ip_vs.h>


#define SERVER_STRING "227 Entering Passive Mode ("
#define CLIENT_STRING "PORT "


/*
 * List of ports (up to IP_VS_APP_MAX_PORTS) to be handled by helper
 * First port is set to the default port.
 */
static int ports[IP_VS_APP_MAX_PORTS] = {21, 0};
struct ip_vs_app *incarnations[IP_VS_APP_MAX_PORTS];

/*
 *	Debug level
 */
#ifdef CONFIG_IP_VS_DEBUG
static int debug=0;
MODULE_PARM(debug, "i");
#endif

MODULE_PARM(ports, "1-" __MODULE_STRING(IP_VS_APP_MAX_PORTS) "i");

/*	Dummy variable */
static int ip_vs_ftp_pasv;


static int
ip_vs_ftp_init_conn(struct ip_vs_app *vapp, struct ip_vs_conn *cp)
{
	return 0;
}


static int
ip_vs_ftp_done_conn(struct ip_vs_app *vapp, struct ip_vs_conn *cp)
{
	return 0;
}


/*
 * Get <addr,port> from the string "xxx.xxx.xxx.xxx,ppp,ppp", started
 * with the "pattern" and terminated with the "term" character.
 * <addr,port> is in network order.
 */
static int ip_vs_ftp_get_addrport(char *data, char *data_limit,
				  const char *pattern, size_t plen, char term,
				  __u32 *addr, __u16 *port,
				  char **start, char **end)
{
	unsigned char p[6];
	int i = 0;

	if (data_limit - data < plen) {
		/* check if there is partial match */
		if (strnicmp(data, pattern, data_limit - data) == 0)
			return -1;
		else
			return 0;
	}

	if (strnicmp(data, pattern, plen) != 0) {
		return 0;
	}
	*start = data + plen;

	for (data = *start; *data != term; data++) {
		if (data == data_limit)
			return -1;
	}
	*end = data;

	memset(p, 0, sizeof(p));
	for (data = *start; data != *end; data++) {
		if (*data >= '0' && *data <= '9') {
			p[i] = p[i]*10 + *data - '0';
		} else if (*data == ',' && i < 5) {
			i++;
		} else {
			/* unexpected character */
			return -1;
		}
	}

	if (i != 5)
		return -1;

	*addr = (p[3]<<24) | (p[2]<<16) | (p[1]<<8) | p[0];
	*port = (p[5]<<8) | p[4];
	return 1;
}


/*
 * Look at outgoing ftp packets to catch the response to a PASV command
 * from the server (inside-to-outside).
 * When we see one, we build a connection entry with the client address,
 * client port 0 (unknown at the moment), the server address and the
 * server port.  Mark the current connection entry as a control channel
 * of the new entry. All this work is just to make the data connection
 * can be scheduled to the right server later.
 *
 * The outgoing packet should be something like
 *   "227 Entering Passive Mode (xxx,xxx,xxx,xxx,ppp,ppp)".
 * xxx,xxx,xxx,xxx is the server address, ppp,ppp is the server port number.
 */
static int ip_vs_ftp_out(struct ip_vs_app *vapp,
			 struct ip_vs_conn *cp, struct sk_buff *skb)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_limit;
	char *start, *end;
	__u32 from;
	__u16 port;
	struct ip_vs_conn *n_cp;
	char buf[24];		/* xxx.xxx.xxx.xxx,ppp,ppp\000 */
	unsigned buf_len;
	int diff;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_S_ESTABLISHED)
		return 0;

	if (cp->app_data == &ip_vs_ftp_pasv) {
		iph = skb->nh.iph;
		th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);
		data = (char *)th + (th->doff << 2);
		data_limit = skb->tail;

		if (ip_vs_ftp_get_addrport(data, data_limit,
					   SERVER_STRING,
					   sizeof(SERVER_STRING)-1, ')',
					   &from, &port,
					   &start, &end) != 1)
			return 0;

		IP_VS_DBG(1-debug, "PASV response (%u.%u.%u.%u:%d) -> "
			  "%u.%u.%u.%u:%d detected\n",
			  NIPQUAD(from), ntohs(port), NIPQUAD(cp->caddr), 0);

		/*
		 * Now update or create an connection entry for it
		 */
		n_cp = ip_vs_conn_out_get(iph->protocol, from, port,
					  cp->caddr, 0);
		if (!n_cp) {
			n_cp = ip_vs_conn_new(IPPROTO_TCP,
					      cp->caddr, 0,
					      cp->vaddr, port,
					      from, port,
					      IP_VS_CONN_F_NO_CPORT,
					      cp->dest);
			if (!n_cp)
				return 0;

			/* add its controller */
			ip_vs_control_add(n_cp, cp);

			/* increase dest's inactive connection counter */
			if (cp->dest)
				atomic_inc(&cp->dest->inactconns);
		}

		/*
		 * Replace the old passive address with the new one
		 */
		from = n_cp->vaddr;
		port = n_cp->vport;
		sprintf(buf,"%d,%d,%d,%d,%d,%d", NIPQUAD(from),
			port&255, port>>8&255);
		buf_len = strlen(buf);

		/*
		 * Calculate required delta-offset to keep TCP happy
		 */
		diff = buf_len - (end-start);

		if (diff == 0) {
			/* simply replace it with new passive address */
			memcpy(start, buf, buf_len);
		} else {
			/* fixme: return value isn't checked here */
			ip_vs_skb_replace(skb, GFP_ATOMIC, start,
					  end-start, buf, buf_len);
		}

		cp->app_data = NULL;
		ip_vs_conn_listen(n_cp);
		ip_vs_conn_put(n_cp);
		return diff;
	}
	return 0;
}


/*
 * Look at incoming ftp packets to catch the PASV/PORT command
 * (outside-to-inside).
 *
 * The incoming packet having the PORT command should be something like
 *      "PORT xxx,xxx,xxx,xxx,ppp,ppp\n".
 * xxx,xxx,xxx,xxx is the client address, ppp,ppp is the client port number.
 * In this case, we create a connection entry using the client address and
 * port, so that the active ftp data connection from the server can reach
 * the client.
 */
static int ip_vs_ftp_in(struct ip_vs_app *vapp,
			struct ip_vs_conn *cp, struct sk_buff *skb)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_start, *data_limit;
	char *start, *end;
	__u32 to;
	__u16 port;
	struct ip_vs_conn *n_cp;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_S_ESTABLISHED)
		return 0;

	/*
	 * Detecting whether it is passive
	 */
	iph = skb->nh.iph;
	th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);

	/* Since there may be OPTIONS in the TCP packet and the HLEN is
	   the length of the header in 32-bit multiples, it is accurate
	   to calculate data address by th+HLEN*4 */
	data = data_start = (char *)th + (th->doff << 2);
	data_limit = skb->tail;

	while (data <= data_limit - 6) {
		if (strnicmp(data, "PASV\r\n", 6) == 0) {
			IP_VS_DBG(1-debug, "got PASV at %d of %d\n",
				  data - data_start,
				  data_limit - data_start);
			cp->app_data = &ip_vs_ftp_pasv;
			return 0;
		}
		data++;
	}

	/*
	 * To support virtual FTP server, the scenerio is as follows:
	 *       FTP client ----> Load Balancer ----> FTP server
	 * First detect the port number in the application data,
	 * then create a new connection entry for the coming data
	 * connection.
	 */
	if (ip_vs_ftp_get_addrport(data_start, data_limit,
				   CLIENT_STRING, sizeof(CLIENT_STRING)-1,
				   '\r', &to, &port,
				   &start, &end) != 1)
		return 0;

	IP_VS_DBG(1-debug, "PORT %u.%u.%u.%u:%d detected\n",
		  NIPQUAD(to), ntohs(port));

	/*
	 * Now update or create a connection entry for it
	 */
	IP_VS_DBG(1-debug, "protocol %s %u.%u.%u.%u:%d %u.%u.%u.%u:%d\n",
		  ip_vs_proto_name(iph->protocol),
		  NIPQUAD(to), ntohs(port),
		  NIPQUAD(cp->vaddr), ntohs(cp->vport) - 1);

	n_cp = ip_vs_conn_in_get(iph->protocol,
				 to, port,
				 cp->vaddr, htons(ntohs(cp->vport)-1));
	if (!n_cp) {
		n_cp = ip_vs_conn_new(IPPROTO_TCP,
				      to, port,
				      cp->vaddr, htons(ntohs(cp->vport)-1),
				      cp->daddr, htons(ntohs(cp->dport)-1),
				      0,
				      cp->dest);
		if (!n_cp)
			return 0;

		/* add its controller */
		ip_vs_control_add(n_cp, cp);

		/* increase dest's inactive connection counter */
		if (cp->dest)
			atomic_inc(&cp->dest->inactconns);
	}

	/*
	 *	Move tunnel to listen state
	 */
	ip_vs_conn_listen(n_cp);
	ip_vs_conn_put(n_cp);

	/* no diff required for incoming packets */
	return 0;
}


static struct ip_vs_app ip_vs_ftp = {
	{0},			/* n_list */
	"ftp",			/* name */
	0,                      /* type */
	THIS_MODULE,            /* this module */
	ip_vs_ftp_init_conn,    /* ip_vs_init_conn */
	ip_vs_ftp_done_conn,    /* ip_vs_done_conn */
	ip_vs_ftp_out,          /* pkt_out */
	ip_vs_ftp_in,           /* pkt_in */
};


/*
 *	ip_vs_ftp initialization
 */
static int __init ip_vs_ftp_init(void)
{
	int i, j;

	for (i=0; i<IP_VS_APP_MAX_PORTS; i++) {
		if (ports[i]) {
			if (!(incarnations[i] =
			     kmalloc(sizeof(struct ip_vs_app), GFP_KERNEL)))
				return -ENOMEM;

			memcpy(incarnations[i], &ip_vs_ftp,
			       sizeof(struct ip_vs_app));
			if ((j = register_ip_vs_app(incarnations[i],
						    IPPROTO_TCP,
						    ports[i]))) {
				return j;
			}
			IP_VS_DBG(1-debug,
				  "Ftp: loaded support on port[%d] = %d\n",
				  i, ports[i]);
		} else {
			/* To be safe, force the incarnation table entry
			   to be NULL */
			incarnations[i] = NULL;
		}
	}
	return 0;
}


/*
 *	ip_vs_ftp finish.
 */
static void __exit ip_vs_ftp_exit(void)
{
	int i, j, k;

	k=0;
	for (i=0; i<IP_VS_APP_MAX_PORTS; i++) {
		if (incarnations[i]) {
			if ((j = unregister_ip_vs_app(incarnations[i]))) {
				k = j;
			} else {
				kfree(incarnations[i]);
				incarnations[i] = NULL;
				IP_VS_DBG(1-debug, "Ftp: unloaded support on port[%d] = %d\n",
					  i, ports[i]);
			}
		}
	}
}


module_init(ip_vs_ftp_init);
module_exit(ip_vs_ftp_exit);
MODULE_LICENSE("GPL");

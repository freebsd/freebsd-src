/* $FreeBSD$ */
/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifndef SOLARIS
#define SOLARIS (defined(__SVR4) || defined(__svr4__)) && defined(sun)
#endif

#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__svr4__)
# if (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
#else
# include <sys/filio.h>
# include <sys/byteorder.h>
#endif
#if !defined(__SVR4) && !defined(__GNUC__)
# include <strings.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/tcp_fsm.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifndef linux
# include <sys/protosw.h>
# include <netinet/ip_var.h>
#endif

#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#include <ctype.h>
#include <syslog.h>

#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_state.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipmon.c	1.21 6/5/96 (C)1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipmon.c,v 2.12.2.36 2002/08/22 15:12:23 darrenr Exp $";
#endif


#if	defined(sun) && !defined(SOLARIS2)
#define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
#define	STRERROR(x)	strerror(x)
#endif


struct	flags {
	int	value;
	char	flag;
};


typedef	struct	icmp_subtype {
	int	ist_val;
	char	*ist_name;
} icmp_subtype_t;

typedef	struct	icmp_type {
	int	it_val;
	struct	icmp_subtype *it_subtable;
	size_t	it_stsize;
	char	*it_name;
} icmp_type_t;


#define	IST_SZ(x)	(sizeof(x)/sizeof(icmp_subtype_t))


struct	flags	tcpfl[] = {
	{ TH_ACK, 'A' },
	{ TH_RST, 'R' },
	{ TH_SYN, 'S' },
	{ TH_FIN, 'F' },
	{ TH_URG, 'U' },
	{ TH_PUSH,'P' },
	{ TH_ECN, 'E' },
	{ TH_CWR, 'C' },
	{ 0, '\0' }
};

#if SOLARIS
static	char	*pidfile = "/etc/opt/ipf/ipmon.pid";
#else
# if BSD >= 199306
static	char	*pidfile = "/var/run/ipmon.pid";
# else
static	char	*pidfile = "/etc/ipmon.pid";
# endif
#endif

static	char	line[2048];
static	int	opts = 0;
static	FILE	*newlog = NULL;
static	char	*logfile = NULL;
static	int	donehup = 0;
static	void	usage __P((char *));
static	void	handlehup __P((int));
static	void	flushlogs __P((char *, FILE *));
static	void	print_log __P((int, FILE *, char *, int));
static	void	print_ipflog __P((FILE *, char *, int));
static	void	print_natlog __P((FILE *, char *, int));
static	void	print_statelog __P((FILE *, char *, int));
static	void	dumphex __P((FILE *, u_char *, int));
static	int	read_log __P((int, int *, char *, int));
static	void	write_pid __P((char *));
static	char	*icmpname __P((u_int, u_int));
static	char	*icmpname6 __P((u_int, u_int));
static	icmp_type_t *find_icmptype __P((int, icmp_type_t *, size_t));
static	icmp_subtype_t *find_icmpsubtype __P((int, icmp_subtype_t *, size_t));

char	*hostname __P((int, int, u_32_t *));
char	*portname __P((int, char *, u_int));
int	main __P((int, char *[]));

static	void	logopts __P((int, char *));
static	void	init_tabs __P((void));
static	char	*getproto __P((u_int));

static	char	**protocols = NULL;
static	char	**udp_ports = NULL;
static	char	**tcp_ports = NULL;

#define	OPT_SYSLOG	0x001
#define	OPT_RESOLVE	0x002
#define	OPT_HEXBODY	0x004
#define	OPT_VERBOSE	0x008
#define	OPT_HEXHDR	0x010
#define	OPT_TAIL	0x020
#define	OPT_NAT		0x080
#define	OPT_STATE	0x100
#define	OPT_FILTER	0x200
#define	OPT_PORTNUM	0x400
#define	OPT_LOGALL	(OPT_NAT|OPT_STATE|OPT_FILTER)
#define	OPT_LOGBODY	0x800

#define	HOSTNAME_V4(a,b)	hostname((a), 4, (u_32_t *)&(b))

#ifndef	LOGFAC
#define	LOGFAC	LOG_LOCAL0
#endif


static icmp_subtype_t icmpunreachnames[] = {
	{ ICMP_UNREACH_NET,		"net" },
	{ ICMP_UNREACH_HOST,		"host" },
	{ ICMP_UNREACH_PROTOCOL,	"protocol" },
	{ ICMP_UNREACH_PORT,		"port" },
	{ ICMP_UNREACH_NEEDFRAG,	"needfrag" },
	{ ICMP_UNREACH_SRCFAIL,		"srcfail" },
	{ ICMP_UNREACH_NET_UNKNOWN,	"net_unknown" },
	{ ICMP_UNREACH_HOST_UNKNOWN,	"host_unknown" },
	{ ICMP_UNREACH_NET,		"isolated" },
	{ ICMP_UNREACH_NET_PROHIB,	"net_prohib" },
	{ ICMP_UNREACH_NET_PROHIB,	"host_prohib" },
	{ ICMP_UNREACH_TOSNET,		"tosnet" },
	{ ICMP_UNREACH_TOSHOST,		"toshost" },
	{ ICMP_UNREACH_ADMIN_PROHIBIT,	"admin_prohibit" },
	{ -2,				NULL }
};

static icmp_subtype_t redirectnames[] = {
	{ ICMP_REDIRECT_NET,		"net" },
	{ ICMP_REDIRECT_HOST,		"host" },
	{ ICMP_REDIRECT_TOSNET,		"tosnet" },
	{ ICMP_REDIRECT_TOSHOST,	"toshost" },
	{ -2,				NULL }
};

static icmp_subtype_t timxceednames[] = {
	{ ICMP_TIMXCEED_INTRANS,	"transit" },
	{ ICMP_TIMXCEED_REASS,		"reassem" },
	{ -2,				NULL }
};

static icmp_subtype_t paramnames[] = {
	{ ICMP_PARAMPROB_ERRATPTR,	"errata_pointer" },
	{ ICMP_PARAMPROB_OPTABSENT,	"optmissing" },
	{ ICMP_PARAMPROB_LENGTH,	"length" },
	{ -2,				NULL }
};

static icmp_type_t icmptypes[] = {
	{ ICMP_ECHOREPLY,	NULL,	0,		"echoreply" },
	{ -1,			NULL,	0,		NULL },
	{ -1,			NULL,	0,		NULL },
	{ ICMP_UNREACH,		icmpunreachnames,
				IST_SZ(icmpunreachnames),"unreach" },
	{ ICMP_SOURCEQUENCH,	NULL,	0,		"sourcequench" },
	{ ICMP_REDIRECT,	redirectnames,
				IST_SZ(redirectnames),	"redirect" },
	{ -1,			NULL,	0,		NULL },
	{ -1,			NULL,	0,		NULL },
	{ ICMP_ECHO,		NULL,	0,		"echo" },
	{ ICMP_ROUTERADVERT,	NULL,	0,		"routeradvert" },
	{ ICMP_ROUTERSOLICIT,	NULL,	0,		"routersolicit" },
	{ ICMP_TIMXCEED,	timxceednames,
				IST_SZ(timxceednames),	"timxceed" },
	{ ICMP_PARAMPROB,	paramnames,
				IST_SZ(paramnames),	"paramprob" },
	{ ICMP_TSTAMP,		NULL,	0,		"timestamp" },
	{ ICMP_TSTAMPREPLY,	NULL,	0,		"timestampreply" },
	{ ICMP_IREQ,		NULL,	0,		"inforeq" },
	{ ICMP_IREQREPLY,	NULL,	0,		"inforeply" },
	{ ICMP_MASKREQ,		NULL,	0,		"maskreq" },
	{ ICMP_MASKREPLY,	NULL,	0,		"maskreply" },
	{ -2,			NULL,	0,		NULL }
};

static icmp_subtype_t icmpredirect6[] = {
	{ ICMP6_DST_UNREACH_NOROUTE,		"noroute" },
	{ ICMP6_DST_UNREACH_ADMIN,		"admin" },
	{ ICMP6_DST_UNREACH_NOTNEIGHBOR,	"neighbour" },
	{ ICMP6_DST_UNREACH_ADDR,		"address" },
	{ ICMP6_DST_UNREACH_NOPORT,		"noport" },
	{ -2,					NULL }
};

static icmp_subtype_t icmptimexceed6[] = {
	{ ICMP6_TIME_EXCEED_TRANSIT,		"intransit" },
	{ ICMP6_TIME_EXCEED_REASSEMBLY,		"reassem" },
	{ -2,					NULL }
};

static icmp_subtype_t icmpparamprob6[] = {
	{ ICMP6_PARAMPROB_HEADER,		"header" },
	{ ICMP6_PARAMPROB_NEXTHEADER,		"nextheader" },
	{ ICMP6_PARAMPROB_OPTION,		"option" },
	{ -2,					NULL }
};

static icmp_subtype_t icmpquerysubject6[] = {
	{ ICMP6_NI_SUBJ_IPV6,			"ipv6" },
	{ ICMP6_NI_SUBJ_FQDN,			"fqdn" },
	{ ICMP6_NI_SUBJ_IPV4,			"ipv4" },
	{ -2,					NULL },
};

static icmp_subtype_t icmpnodeinfo6[] = {
	{ ICMP6_NI_SUCCESS,			"success" },
	{ ICMP6_NI_REFUSED,			"refused" },
	{ ICMP6_NI_UNKNOWN,			"unknown" },
	{ -2,					NULL }
};

static icmp_subtype_t icmprenumber6[] = {
	{ ICMP6_ROUTER_RENUMBERING_COMMAND,		"command" },
	{ ICMP6_ROUTER_RENUMBERING_RESULT,		"result" },
	{ ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET,	"seqnum_reset" },
	{ -2,						NULL }
};

static icmp_type_t icmptypes6[] = {
	{ 0,			NULL,	0,		NULL },
	{ ICMP6_DST_UNREACH,	icmpredirect6,
			IST_SZ(icmpredirect6),		"unreach" },
	{ ICMP6_PACKET_TOO_BIG,	NULL,	0,		"toobig" },
	{ ICMP6_TIME_EXCEEDED,	icmptimexceed6,
			IST_SZ(icmptimexceed6),		"timxceed" },
	{ ICMP6_PARAM_PROB,	icmpparamprob6,
			IST_SZ(icmpparamprob6),		"paramprob" },
	{ ICMP6_ECHO_REQUEST,	NULL,	0,		"echo" },
	{ ICMP6_ECHO_REPLY,	NULL,	0,		"echoreply" },
	{ ICMP6_MEMBERSHIP_QUERY, icmpquerysubject6,
			IST_SZ(icmpquerysubject6),	"groupmemberquery" },
	{ ICMP6_MEMBERSHIP_REPORT,NULL,	0,		"groupmemberreport" },
	{ ICMP6_MEMBERSHIP_REDUCTION,NULL,	0,	"groupmemberterm" },
	{ ND_ROUTER_SOLICIT,	NULL,	0,		"routersolicit" },
	{ ND_ROUTER_ADVERT,	NULL,	0,		"routeradvert" },
	{ ND_NEIGHBOR_SOLICIT,	NULL,	0,		"neighborsolicit" },
	{ ND_NEIGHBOR_ADVERT,	NULL,	0,		"neighboradvert" },
	{ ND_REDIRECT,		NULL,	0,		"redirect" },
	{ ICMP6_ROUTER_RENUMBERING,	icmprenumber6,
			IST_SZ(icmprenumber6),		"routerrenumber" },
	{ ICMP6_WRUREQUEST,	NULL,	0,		"whoareyourequest" },
	{ ICMP6_WRUREPLY,	NULL,	0,		"whoareyoureply" },
	{ ICMP6_FQDN_QUERY,	NULL,	0,		"fqdnquery" },
	{ ICMP6_FQDN_REPLY,	NULL,	0,		"fqdnreply" },
	{ ICMP6_NI_QUERY,	icmpnodeinfo6,
			IST_SZ(icmpnodeinfo6),		"nodeinforequest" },
	{ ICMP6_NI_REPLY,	NULL,	0,		"nodeinforeply" },
	{ MLD6_MTRACE_RESP,	NULL,	0,		"mtraceresponse" },
	{ MLD6_MTRACE,		NULL,	0,		"mtracerequest" },
	{ -2,			NULL,	0,		NULL }
};

static icmp_subtype_t *find_icmpsubtype(type, table, tablesz)
int type;
icmp_subtype_t *table;
size_t tablesz;
{
	icmp_subtype_t *ist;
	int i;

	if (tablesz < 2)
		return NULL;

	if ((type < 0) || (type > table[tablesz - 2].ist_val))
		return NULL;

	i = type;
	if (table[type].ist_val == type)
		return table + type;

	for (i = 0, ist = table; ist->ist_val != -2; i++, ist++)
		if (ist->ist_val == type)
			return ist;
	return NULL;
}


static icmp_type_t *find_icmptype(type, table, tablesz)
int type;
icmp_type_t *table;
size_t tablesz;
{
	icmp_type_t *it;
	int i;

	if (tablesz < 2)
		return NULL;

	if ((type < 0) || (type > table[tablesz - 2].it_val))
		return NULL;

	i = type;
	if (table[type].it_val == type)
		return table + type;

	for (i = 0, it = table; it->it_val != -2; i++, it++)
		if (it->it_val == type)
			return it;
	return NULL;
}


static void handlehup(sig)
int sig;
{
	FILE	*fp;

	signal(SIGHUP, handlehup);
	if (logfile && (fp = fopen(logfile, "a")))
		newlog = fp;
	init_tabs();
	donehup = 1;
}


static void init_tabs()
{
	struct	protoent	*p;
	struct	servent	*s;
	char	*name, **tab;
	int	port;

	if (protocols != NULL) {
		free(protocols);
		protocols = NULL;
	}
	protocols = (char **)malloc(256 * sizeof(*protocols));
	if (protocols != NULL) {
		bzero((char *)protocols, 256 * sizeof(*protocols));

		setprotoent(1);
		while ((p = getprotoent()) != NULL)
			if (p->p_proto >= 0 && p->p_proto <= 255 &&
			    p->p_name != NULL && protocols[p->p_proto] == NULL)
				protocols[p->p_proto] = strdup(p->p_name);
		endprotoent();
	}

	if (udp_ports != NULL) {
		free(udp_ports);
		udp_ports = NULL;
	}
	udp_ports = (char **)malloc(65536 * sizeof(*udp_ports));
	if (udp_ports != NULL)
		bzero((char *)udp_ports, 65536 * sizeof(*udp_ports));

	if (tcp_ports != NULL) {
		free(tcp_ports);
		tcp_ports = NULL;
	}
	tcp_ports = (char **)malloc(65536 * sizeof(*tcp_ports));
	if (tcp_ports != NULL)
		bzero((char *)tcp_ports, 65536 * sizeof(*tcp_ports));

	setservent(1);
	while ((s = getservent()) != NULL) {
		if (s->s_proto == NULL)
			continue;
		else if (!strcmp(s->s_proto, "tcp")) {
			port = ntohs(s->s_port);
			name = s->s_name;
			tab = tcp_ports;
		} else if (!strcmp(s->s_proto, "udp")) {
			port = ntohs(s->s_port);
			name = s->s_name;
			tab = udp_ports;
		} else
			continue;
		if ((port < 0 || port > 65535) || (name == NULL))
			continue;
		tab[port] = strdup(name);
	}
	endservent();
}


static char *getproto(p)
u_int p;
{
	static char pnum[4];
	char *s;

	p &= 0xff;
	s = protocols ? protocols[p] : NULL;
	if (s == NULL) {
		sprintf(pnum, "%u", p);
		s = pnum;
	}
	return s;
}


static int read_log(fd, lenp, buf, bufsize)
int fd, bufsize, *lenp;
char *buf;
{
	int	nr;

	nr = read(fd, buf, bufsize);
	if (!nr)
		return 2;
	if ((nr < 0) && (errno != EINTR))
		return -1;
	*lenp = nr;
	return 0;
}


char	*hostname(res, v, ip)
int	res, v;
u_32_t	*ip;
{
# define MAX_INETA	16
	static char hname[MAXHOSTNAMELEN + MAX_INETA + 3];
#ifdef	USE_INET6
	static char hostbuf[MAXHOSTNAMELEN+1];
#endif
	struct hostent *hp;
	struct in_addr ipa;

	if (v == 4) {
		ipa.s_addr = *ip;
		if (!res)
			return inet_ntoa(ipa);
		hp = gethostbyaddr((char *)ip, sizeof(*ip), AF_INET);
		if (!hp)
			return inet_ntoa(ipa);
		sprintf(hname, "%.*s[%s]", MAXHOSTNAMELEN, hp->h_name,
			inet_ntoa(ipa));
		return hname;
	}
#ifdef	USE_INET6
	(void) inet_ntop(AF_INET6, ip, hostbuf, sizeof(hostbuf) - 1);
	hostbuf[MAXHOSTNAMELEN] = '\0';
	return hostbuf;
#else
	return "IPv6";
#endif
}


char	*portname(res, proto, port)
int	res;
char	*proto;
u_int	port;
{
	static	char	pname[8];
	char	*s;

	port = ntohs(port);
	port &= 0xffff;
	(void) sprintf(pname, "%u", port);
	if (!res || (opts & OPT_PORTNUM))
		return pname;
	s = NULL;
	if (!strcmp(proto, "tcp"))
		s = tcp_ports[port];
	else if (!strcmp(proto, "udp"))
		s = udp_ports[port];
	if (s == NULL)
		s = pname;
	return s;
}


static	char	*icmpname(type, code)
u_int	type;
u_int	code;
{
	static char name[80];
	icmp_subtype_t *ist;
	icmp_type_t *it;
	char *s;

	s = NULL;
	it = find_icmptype(type, icmptypes, sizeof(icmptypes) / sizeof(*it));
	if (it != NULL)
		s = it->it_name;

	if (s == NULL)
		sprintf(name, "icmptype(%d)/", type);
	else
		sprintf(name, "%s/", s);

	ist = NULL;
	if (it != NULL && it->it_subtable != NULL)
		ist = find_icmpsubtype(code, it->it_subtable, it->it_stsize);

	if (ist != NULL && ist->ist_name != NULL)
		strcat(name, ist->ist_name);
	else
		sprintf(name + strlen(name), "%d", code);

	return name;
}

static	char	*icmpname6(type, code)
u_int	type;
u_int	code;
{
	static char name[80];
	icmp_subtype_t *ist;
	icmp_type_t *it;
	char *s;

	s = NULL;
	it = find_icmptype(type, icmptypes6, sizeof(icmptypes6) / sizeof(*it));
	if (it != NULL)
		s = it->it_name;

	if (s == NULL)
		sprintf(name, "icmpv6type(%d)/", type);
	else
		sprintf(name, "%s/", s);

	ist = NULL;
	if (it != NULL && it->it_subtable != NULL)
		ist = find_icmpsubtype(code, it->it_subtable, it->it_stsize);

	if (ist != NULL && ist->ist_name != NULL)
		strcat(name, ist->ist_name);
	else
		sprintf(name + strlen(name), "%d", code);

	return name;
}


static	void	dumphex(log, buf, len)
FILE	*log;
u_char	*buf;
int	len;
{
	char	line[80];
	int	i, j, k;
	u_char	*s = buf, *t = (u_char *)line;

	if (len == 0 || buf == 0)
		return;
	*line = '\0';

	for (i = len, j = 0; i; i--, j++, s++) {
		if (j && !(j & 0xf)) {
			*t++ = '\n';
			*t = '\0';
			if (!(opts & OPT_SYSLOG))
				fputs(line, log);
			else
				syslog(LOG_INFO, "%s", line);
			t = (u_char *)line;
			*t = '\0';
		}
		sprintf((char *)t, "%02x", *s & 0xff);
		t += 2;
		if (!((j + 1) & 0xf)) {
			s -= 15;
			sprintf((char *)t, "        ");
			t += 8;
			for (k = 16; k; k--, s++)
				*t++ = (isprint(*s) ? *s : '.');
			s--;
		}
			
		if ((j + 1) & 0xf)
			*t++ = ' ';;
	}

	if (j & 0xf) {
		for (k = 16 - (j & 0xf); k; k--) {
			*t++ = ' ';
			*t++ = ' ';
			*t++ = ' ';
		}
		sprintf((char *)t, "       ");
		t += 7;
		s -= j & 0xf;
		for (k = j & 0xf; k; k--, s++)
			*t++ = (isprint(*s) ? *s : '.');
		*t++ = '\n';
		*t = '\0';
	}
	if (!(opts & OPT_SYSLOG)) {
		fputs(line, log);
		fflush(log);
	} else
		syslog(LOG_INFO, "%s", line);
}

static	void	print_natlog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	natlog	*nl;
	iplog_t	*ipl = (iplog_t *)buf;
	char	*t = line;
	struct	tm	*tm;
	int	res, i, len;
	char	*proto;

	nl = (struct natlog *)((char *)ipl + IPLOG_SIZE);
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&ipl->ipl_sec);
	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld @%hd ", ipl->ipl_usec, nl->nl_rule + 1);
	t += strlen(t);

	if (nl->nl_type == NL_NEWMAP)
		strcpy(t, "NAT:MAP ");
	else if (nl->nl_type == NL_NEWRDR)
		strcpy(t, "NAT:RDR ");
	else if (nl->nl_type == NL_EXPIRE)
		strcpy(t, "NAT:EXPIRE ");
	else if (nl->nl_type == NL_FLUSH)
		strcpy(t, "NAT:FLUSH ");
	else if (nl->nl_type == NL_NEWBIMAP)
		strcpy(t, "NAT:BIMAP ");
	else if (nl->nl_type == NL_NEWBLOCK)
		strcpy(t, "NAT:MAPBLOCK ");
	else
		sprintf(t, "Type: %d ", nl->nl_type);
	t += strlen(t);

	proto = getproto(nl->nl_p);

	(void) sprintf(t, "%s,%s <- -> ", HOSTNAME_V4(res, nl->nl_inip),
		portname(res, proto, (u_int)nl->nl_inport));
	t += strlen(t);
	(void) sprintf(t, "%s,%s ", HOSTNAME_V4(res, nl->nl_outip),
		portname(res, proto, (u_int)nl->nl_outport));
	t += strlen(t);
	(void) sprintf(t, "[%s,%s]", HOSTNAME_V4(res, nl->nl_origip),
		portname(res, proto, (u_int)nl->nl_origport));
	t += strlen(t);
	if (nl->nl_type == NL_EXPIRE) {
#ifdef	USE_QUAD_T
		(void) sprintf(t, " Pkts %qd Bytes %qd",
				(long long)nl->nl_pkts,
				(long long)nl->nl_bytes);
#else
		(void) sprintf(t, " Pkts %ld Bytes %ld",
				nl->nl_pkts, nl->nl_bytes);
#endif
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(LOG_INFO, "%s", line);
	else
		(void) fprintf(log, "%s", line);
}


static	void	print_statelog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	ipslog *sl;
	iplog_t	*ipl = (iplog_t *)buf;
	char	*t = line, *proto;
	struct	tm	*tm;
	int	res, i, len;

	sl = (struct ipslog *)((char *)ipl + IPLOG_SIZE);
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&ipl->ipl_sec);
	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld ", ipl->ipl_usec);
	t += strlen(t);

	if (sl->isl_type == ISL_NEW)
		strcpy(t, "STATE:NEW ");
	else if (sl->isl_type == ISL_EXPIRE) {
		if ((sl->isl_p == IPPROTO_TCP) &&
		    (sl->isl_state[0] > TCPS_ESTABLISHED ||
		     sl->isl_state[1] > TCPS_ESTABLISHED))
			strcpy(t, "STATE:CLOSE ");
		else
			strcpy(t, "STATE:EXPIRE ");
	} else if (sl->isl_type == ISL_FLUSH)
		strcpy(t, "STATE:FLUSH ");
	else if (sl->isl_type == ISL_REMOVE)
		strcpy(t, "STATE:REMOVE ");
	else
		sprintf(t, "Type: %d ", sl->isl_type);
	t += strlen(t);

	proto = getproto(sl->isl_p);

	if (sl->isl_p == IPPROTO_TCP || sl->isl_p == IPPROTO_UDP) {
		(void) sprintf(t, "%s,%s -> ",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_src),
			portname(res, proto, (u_int)sl->isl_sport));
		t += strlen(t);
		(void) sprintf(t, "%s,%s PR %s",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_dst),
			portname(res, proto, (u_int)sl->isl_dport), proto);
	} else if (sl->isl_p == IPPROTO_ICMP) {
		(void) sprintf(t, "%s -> ", hostname(res, sl->isl_v,
						     (u_32_t *)&sl->isl_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp %d",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_dst),
			sl->isl_itype);
	} else if (sl->isl_p == IPPROTO_ICMPV6) {
		(void) sprintf(t, "%s -> ", hostname(res, sl->isl_v,
						     (u_32_t *)&sl->isl_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmpv6 %d",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_dst),
			sl->isl_itype);
	}
	t += strlen(t);
	if (sl->isl_type != ISL_NEW) {
#ifdef	USE_QUAD_T
		(void) sprintf(t, " Pkts %qd Bytes %qd",
				(long long)sl->isl_pkts,
				(long long)sl->isl_bytes);
#else
		(void) sprintf(t, " Pkts %ld Bytes %ld",
				sl->isl_pkts, sl->isl_bytes);
#endif
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(LOG_INFO, "%s", line);
	else
		(void) fprintf(log, "%s", line);
}


static	void	print_log(logtype, log, buf, blen)
FILE	*log;
char	*buf;
int	logtype, blen;
{
	iplog_t	*ipl;
	char *bp = NULL, *bpo = NULL;
	int psize;

	while (blen > 0) {
		ipl = (iplog_t *)buf;
		if ((u_long)ipl & (sizeof(long)-1)) {
			if (bp)
				bpo = bp;
			bp = (char *)malloc(blen);
			bcopy((char *)ipl, bp, blen);
			if (bpo) {
				free(bpo);
				bpo = NULL;
			}
			buf = bp;
			continue;
		}
		if (ipl->ipl_magic != IPL_MAGIC) {
			/* invalid data or out of sync */
			break;
		}
		psize = ipl->ipl_dsize;
		switch (logtype)
		{
		case IPL_LOGIPF :
			print_ipflog(log, buf, psize);
			break;
		case IPL_LOGNAT :
			print_natlog(log, buf, psize);
			break;
		case IPL_LOGSTATE :
			print_statelog(log, buf, psize);
			break;
		}

		blen -= psize;
		buf += psize;
	}
	if (bp)
		free(bp);
	return;
}


static	void	print_ipflog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	tcphdr_t	*tp;
	struct	icmp	*ic;
	struct	icmp	*icmp;
	struct	tm	*tm;
	char	*t, *proto;
	int	i, v, lvl, res, len, off, plen, ipoff;
	ip_t	*ipc, *ip;
	u_short	hl, p;
	ipflog_t *ipf;
	iplog_t	*ipl;
	u_32_t	*s, *d;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif

	ipl = (iplog_t *)buf;
	ipf = (ipflog_t *)((char *)buf + IPLOG_SIZE);
	ip = (ip_t *)((char *)ipf + sizeof(*ipf));
	v = ip->ip_v;
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	t = line;
	*t = '\0';
	tm = localtime((time_t *)&ipl->ipl_sec);
#ifdef	linux
	if (v == 4)
		ip->ip_len = ntohs(ip->ip_len);
#endif

	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld ", ipl->ipl_usec);
	t += strlen(t);
	if (ipl->ipl_count > 1) {
		(void) sprintf(t, "%dx ", ipl->ipl_count);
		t += strlen(t);
	}
#if (SOLARIS || \
	(defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))) || defined(linux)
	{
	char	ifname[sizeof(ipf->fl_ifname) + 1];

	strncpy(ifname, (char *)ipf->fl_ifname, sizeof(ipf->fl_ifname));
	ifname[sizeof(ipf->fl_ifname)] = '\0';
	(void) sprintf(t, "%s", ifname);
	t += strlen(t);
# if SOLARIS
	if (isalpha(*(t - 1))) {
		sprintf(t, "%d", ipf->fl_unit);
		t += strlen(t);
	}
# endif
	}
#else
	for (len = 0; len < 3; len++)
		if (ipf->fl_ifname[len] == '\0')
			break;
	if (ipf->fl_ifname[len])
		len++;
	(void) sprintf(t, "%*.*s%u", len, len, ipf->fl_ifname, ipf->fl_unit);
	t += strlen(t);
#endif
	if (ipf->fl_group == 0xffffffff)
		strcat(t, " @-1:");
	else
		(void) sprintf(t, " @%u:", ipf->fl_group);
	t += strlen(t);
	if (ipf->fl_rule == 0xffffffff)
		strcat(t, "-1 ");
	else
		(void) sprintf(t, "%u ", ipf->fl_rule + 1);
	t += strlen(t);

 	if (ipf->fl_flags & FF_SHORT) {
		*t++ = 'S';
		lvl = LOG_ERR;
	} else if (ipf->fl_flags & FR_PASS) {
		if (ipf->fl_flags & FR_LOG)
			*t++ = 'p';
		else
			*t++ = 'P';
		lvl = LOG_NOTICE;
	} else if (ipf->fl_flags & FR_BLOCK) {
		if (ipf->fl_flags & FR_LOG)
			*t++ = 'b';
		else
			*t++ = 'B';
		lvl = LOG_WARNING;
	} else if (ipf->fl_flags & FF_LOGNOMATCH) {
		*t++ = 'n';
		lvl = LOG_NOTICE;
	} else {
		*t++ = 'L';
		lvl = LOG_INFO;
	}
	if (ipf->fl_loglevel != 0xffff)
		lvl = ipf->fl_loglevel;
	*t++ = ' ';
	*t = '\0';

	if (v == 6) {
#ifdef	USE_INET6
		off = 0;
		ipoff = 0;
		hl = sizeof(ip6_t);
		ip6 = (ip6_t *)ip;
		p = (u_short)ip6->ip6_nxt;
		s = (u_32_t *)&ip6->ip6_src;
		d = (u_32_t *)&ip6->ip6_dst;
		plen = ntohs(ip6->ip6_plen);
#else
		sprintf(t, "ipv6");
		goto printipflog;
#endif
	} else if (v == 4) {
		hl = (ip->ip_hl << 2);
		ipoff = ip->ip_off;
		off = ipoff & IP_OFFMASK;
		p = (u_short)ip->ip_p;
		s = (u_32_t *)&ip->ip_src;
		d = (u_32_t *)&ip->ip_dst;
		plen = ip->ip_len;
	} else {
		goto printipflog;
	}
	proto = getproto(p);

	if ((p == IPPROTO_TCP || p == IPPROTO_UDP) && !off) {
		tp = (tcphdr_t *)((char *)ip + hl);
		if (!(ipf->fl_flags & FF_SHORT)) {
			(void) sprintf(t, "%s,%s -> ", hostname(res, v, s),
				portname(res, proto, (u_int)tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, "%s,%s PR %s len %hu %hu",
				hostname(res, v, d),
				portname(res, proto, (u_int)tp->th_dport),
				proto, hl, plen);
			t += strlen(t);

			if (p == IPPROTO_TCP) {
				*t++ = ' ';
				*t++ = '-';
				for (i = 0; tcpfl[i].value; i++)
					if (tp->th_flags & tcpfl[i].value)
						*t++ = tcpfl[i].flag;
				if (opts & OPT_VERBOSE) {
					(void) sprintf(t, " %lu %lu %hu",
						(u_long)(ntohl(tp->th_seq)),
						(u_long)(ntohl(tp->th_ack)),
						ntohs(tp->th_win));
					t += strlen(t);
				}
			}
			*t = '\0';
		} else {
			(void) sprintf(t, "%s -> ", hostname(res, v, s));
			t += strlen(t);
			(void) sprintf(t, "%s PR %s len %hu %hu",
				hostname(res, v, d), proto, hl, plen);
		}
	} else if ((p == IPPROTO_ICMPV6) && !off && (v == 6)) {
		ic = (struct icmp *)((char *)ip + hl);
		(void) sprintf(t, "%s -> ", hostname(res, v, s));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmpv6 len %hu %hu icmpv6 %s",
			hostname(res, v, d), hl, plen,
			icmpname6(ic->icmp_type, ic->icmp_code));
	} else if ((p == IPPROTO_ICMP) && !off && (v == 4)) {
		ic = (struct icmp *)((char *)ip + hl);
		(void) sprintf(t, "%s -> ", hostname(res, v, s));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp len %hu %hu icmp %s",
			hostname(res, v, d), hl, plen,
			icmpname(ic->icmp_type, ic->icmp_code));
		if (ic->icmp_type == ICMP_UNREACH ||
		    ic->icmp_type == ICMP_SOURCEQUENCH ||
		    ic->icmp_type == ICMP_PARAMPROB ||
		    ic->icmp_type == ICMP_REDIRECT ||
		    ic->icmp_type == ICMP_TIMXCEED) {
			ipc = &ic->icmp_ip;
			i = ntohs(ipc->ip_len);
			ipoff = ntohs(ipc->ip_off);
			proto = getproto(ipc->ip_p);

			if (!(ipoff & IP_OFFMASK) &&
			    ((ipc->ip_p == IPPROTO_TCP) ||
			     (ipc->ip_p == IPPROTO_UDP))) {
				tp = (tcphdr_t *)((char *)ipc + hl);
				t += strlen(t);
				(void) sprintf(t, " for %s,%s -",
					HOSTNAME_V4(res, ipc->ip_src),
					portname(res, proto,
						 (u_int)tp->th_sport));
				t += strlen(t);
				(void) sprintf(t, " %s,%s PR %s len %hu %hu",
					HOSTNAME_V4(res, ipc->ip_dst),
					portname(res, proto,
						 (u_int)tp->th_dport),
					proto, ipc->ip_hl << 2, i);
			} else if (!(ipoff & IP_OFFMASK) &&
				   (ipc->ip_p == IPPROTO_ICMP)) {
				icmp = (icmphdr_t *)((char *)ipc + hl);

				t += strlen(t);
				(void) sprintf(t, " for %s -",
					HOSTNAME_V4(res, ipc->ip_src));
				t += strlen(t);
				(void) sprintf(t,
					" %s PR icmp len %hu %hu icmp %d/%d",
					HOSTNAME_V4(res, ipc->ip_dst),
					ipc->ip_hl << 2, i,
					icmp->icmp_type, icmp->icmp_code);

			} else {
				t += strlen(t);
				(void) sprintf(t, " for %s -",
						HOSTNAME_V4(res, ipc->ip_src));
				t += strlen(t);
				(void) sprintf(t, " %s PR %s len %hu (%hu)",
					HOSTNAME_V4(res, ipc->ip_dst), proto,
					ipc->ip_hl << 2, i);
				t += strlen(t);
				if (ipoff & IP_OFFMASK) {
					(void) sprintf(t, " frag %s%s%hu@%hu",
						ipoff & IP_MF ? "+" : "",
						ipoff & IP_DF ? "-" : "",
						i - (ipc->ip_hl<<2),
						(ipoff & IP_OFFMASK) << 3);
				}
			}
		}
	} else {
		(void) sprintf(t, "%s -> ", hostname(res, v, s));
		t += strlen(t);
		(void) sprintf(t, "%s PR %s len %hu (%hu)",
			hostname(res, v, d), proto, hl, plen);
		t += strlen(t);
		if (off & IP_OFFMASK)
			(void) sprintf(t, " frag %s%s%hu@%hu",
				ipoff & IP_MF ? "+" : "",
				ipoff & IP_DF ? "-" : "",
				plen - hl, (off & IP_OFFMASK) << 3);
	}
	t += strlen(t);

	if (ipf->fl_flags & FR_KEEPSTATE) {
		(void) strcpy(t, " K-S");
		t += strlen(t);
	}

	if (ipf->fl_flags & FR_KEEPFRAG) {
		(void) strcpy(t, " K-F");
		t += strlen(t);
	}

	if (ipf->fl_dir == 0)
		strcpy(t, " IN");
	else if (ipf->fl_dir == 1)
		strcpy(t, " OUT");
	t += strlen(t);
printipflog:
	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(lvl, "%s", line);
	else
		(void) fprintf(log, "%s", line);
	if (opts & OPT_HEXHDR)
		dumphex(log, (u_char *)buf, sizeof(iplog_t) + sizeof(*ipf));
	if (opts & OPT_HEXBODY)
		dumphex(log, (u_char *)ip, ipf->fl_plen + ipf->fl_hlen);
	else if ((opts & OPT_LOGBODY) && (ipf->fl_flags & FR_LOGBODY))
		dumphex(log, (u_char *)ip + ipf->fl_hlen, ipf->fl_plen);
}


static void usage(prog)
char *prog;
{
	fprintf(stderr, "%s: [-NFhstvxX] [-f <logfile>]\n", prog);
	exit(1);
}


static void write_pid(file)
char *file;
{
	FILE *fp = NULL;
	int fd;

	if ((fd = open(file, O_CREAT|O_TRUNC|O_WRONLY, 0644)) >= 0)
		fp = fdopen(fd, "w");
	if (!fp) {
		close(fd);
		fprintf(stderr, "unable to open/create pid file: %s\n", file);
		return;
	}
	fprintf(fp, "%d", getpid());
	fclose(fp);
	close(fd);
}


static void flushlogs(file, log)
char *file;
FILE *log;
{
	int	fd, flushed = 0;

	if ((fd = open(file, O_RDWR)) == -1) {
		(void) fprintf(stderr, "%s: open: %s\n",
			       file, STRERROR(errno));
		exit(1);
	}

	if (ioctl(fd, SIOCIPFFB, &flushed) == 0) {
		printf("%d bytes flushed from log buffer\n",
			flushed);
		fflush(stdout);
	} else
		perror("SIOCIPFFB");
	(void) close(fd);

	if (flushed) {
		if (opts & OPT_SYSLOG)
			syslog(LOG_INFO, "%d bytes flushed from log\n",
				flushed);
		else if (log != stdout)
			fprintf(log, "%d bytes flushed from log\n", flushed);
	}
}


static void logopts(turnon, options)
int turnon;
char *options;
{
	int flags = 0;
	char *s;

	for (s = options; *s; s++)
	{
		switch (*s)
		{
		case 'N' :
			flags |= OPT_NAT;
			break;
		case 'S' :
			flags |= OPT_STATE;
			break;
		case 'I' :
			flags |= OPT_FILTER;
			break;
		default :
			fprintf(stderr, "Unknown log option %c\n", *s);
			exit(1);
		}
	}

	if (turnon)
		opts |= flags;
	else
		opts &= ~(flags);
}


int main(argc, argv)
int argc;
char *argv[];
{
	int	fdt[3], devices = 0, make_daemon = 0;
	char	buf[IPLLOGSIZE], *iplfile[3], *s;
	int	fd[3], doread, n, i;
	extern	char	*optarg;
	extern	int	optind;
	int	regular[3], c;
	FILE	*log = stdout;
	struct	stat	sb;
	size_t	nr, tr;

	fd[0] = fd[1] = fd[2] = -1;
	fdt[0] = fdt[1] = fdt[2] = -1;
	iplfile[0] = IPL_NAME;
	iplfile[1] = IPNAT_NAME;
	iplfile[2] = IPSTATE_NAME;

	while ((c = getopt(argc, argv, "?abDf:FhnN:o:O:pP:sS:tvxX")) != -1)
		switch (c)
		{
		case 'a' :
			opts |= OPT_LOGALL;
			fdt[0] = IPL_LOGIPF;
			fdt[1] = IPL_LOGNAT;
			fdt[2] = IPL_LOGSTATE;
			break;
		case 'b' :
			opts |= OPT_LOGBODY;
			break;
		case 'D' :
			make_daemon = 1;
			break;
		case 'f' : case 'I' :
			opts |= OPT_FILTER;
			fdt[0] = IPL_LOGIPF;
			iplfile[0] = optarg;
			break;
		case 'F' :
			flushlogs(iplfile[0], log);
			flushlogs(iplfile[1], log);
			flushlogs(iplfile[2], log);
			break;
		case 'n' :
			opts |= OPT_RESOLVE;
			break;
		case 'N' :
			opts |= OPT_NAT;
			fdt[1] = IPL_LOGNAT;
			iplfile[1] = optarg;
			break;
		case 'o' : case 'O' :
			logopts(c == 'o', optarg);
			fdt[0] = fdt[1] = fdt[2] = -1;
			if (opts & OPT_FILTER)
				fdt[0] = IPL_LOGIPF;
			if (opts & OPT_NAT)
				fdt[1] = IPL_LOGNAT;
			if (opts & OPT_STATE)
				fdt[2] = IPL_LOGSTATE;
			break;
		case 'p' :
			opts |= OPT_PORTNUM;
			break;
		case 'P' :
			pidfile = optarg;
			break;
		case 's' :
			s = strrchr(argv[0], '/');
			if (s == NULL)
				s = argv[0];
			else
				s++;
			openlog(s, LOG_NDELAY|LOG_PID, LOGFAC);
			opts |= OPT_SYSLOG;
			log = NULL;
			break;
		case 'S' :
			opts |= OPT_STATE;
			fdt[2] = IPL_LOGSTATE;
			iplfile[2] = optarg;
			break;
		case 't' :
			opts |= OPT_TAIL;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'x' :
			opts |= OPT_HEXBODY;
			break;
		case 'X' :
			opts |= OPT_HEXHDR;
			break;
		default :
		case 'h' :
		case '?' :
			usage(argv[0]);
		}

	init_tabs();

	/*
	 * Default action is to only open the filter log file.
	 */
	if ((fdt[0] == -1) && (fdt[1] == -1) && (fdt[2] == -1))
		fdt[0] = IPL_LOGIPF;

	for (i = 0; i < 3; i++) {
		if (fdt[i] == -1)
			continue;
		if (!strcmp(iplfile[i], "-"))
			fd[i] = 0;
		else {
			if ((fd[i] = open(iplfile[i], O_RDONLY)) == -1) {
				(void) fprintf(stderr,
					       "%s: open: %s\n", iplfile[i],
					       STRERROR(errno));
				exit(1);
				/* NOTREACHED */
			}
			if (fstat(fd[i], &sb) == -1) {
				(void) fprintf(stderr, "%d: fstat: %s\n",
					       fd[i], STRERROR(errno));
				exit(1);
				/* NOTREACHED */
			}
			if (!(regular[i] = !S_ISCHR(sb.st_mode)))
				devices++;
		}
	}

	if (!(opts & OPT_SYSLOG)) {
		logfile = argv[optind];
		log = logfile ? fopen(logfile, "a") : stdout;
		if (log == NULL) {
			(void) fprintf(stderr, "%s: fopen: %s\n",
				       argv[optind], STRERROR(errno));
			exit(1);
			/* NOTREACHED */
		}
		setvbuf(log, NULL, _IONBF, 0);
	} else
		log = NULL;

	if (make_daemon && ((log != stdout) || (opts & OPT_SYSLOG))) {
#if BSD
		daemon(0, !(opts & OPT_SYSLOG));
#else
		int pid;
		if ((pid = fork()) > 0)
			exit(0);
		if (pid < 0) {
			(void) fprintf(stderr, "%s: fork() failed: %s\n",
				       argv[0], STRERROR(errno));
			exit(1);
			/* NOTREACHED */
		}
		setsid();
		if ((opts & OPT_SYSLOG))
			close(2);
#endif /* !BSD */
		close(0);
		close(1);
	}
	write_pid(pidfile);

	signal(SIGHUP, handlehup);

	for (doread = 1; doread; ) {
		nr = 0;

		for (i = 0; i < 3; i++) {
			tr = 0;
			if (fdt[i] == -1)
				continue;
			if (!regular[i]) {
				if (ioctl(fd[i], FIONREAD, &tr) == -1) {
					if (opts & OPT_SYSLOG)
						syslog(LOG_CRIT,
						       "ioctl(FIONREAD): %m");
					else
						perror("ioctl(FIONREAD)");
					exit(1);
					/* NOTREACHED */
				}
			} else {
				tr = (lseek(fd[i], 0, SEEK_CUR) < sb.st_size);
				if (!tr && !(opts & OPT_TAIL))
					doread = 0;
			}
			if (!tr)
				continue;
			nr += tr;

			tr = read_log(fd[i], &n, buf, sizeof(buf));
			if (donehup) {
				donehup = 0;
				if (newlog) {
					fclose(log);
					log = newlog;
					newlog = NULL;
				}
			}

			switch (tr)
			{
			case -1 :
				if (opts & OPT_SYSLOG)
					syslog(LOG_CRIT, "read: %m\n");
				else
					perror("read");
				doread = 0;
				break;
			case 1 :
				if (opts & OPT_SYSLOG)
					syslog(LOG_CRIT, "aborting logging\n");
				else
					fprintf(log, "aborting logging\n");
				doread = 0;
				break;
			case 2 :
				break;
			case 0 :
				if (n > 0) {
					print_log(fdt[i], log, buf, n);
					if (!(opts & OPT_SYSLOG))
						fflush(log);
				}
				break;
			}
		}
		if (!nr && ((opts & OPT_TAIL) || devices))
			sleep(1);
	}
	exit(0);
	/* NOTREACHED */
}

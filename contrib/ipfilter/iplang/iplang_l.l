%e 1500
%p 4000
%a 4000
%o 6000
%{
/*
 * Copyright (C) 1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * $Id: iplang_l.l,v 2.0.2.15.2.2 1997/12/10 09:54:15 darrenr Exp $
 */
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#if defined(__SVR4) || defined(__sysv__)
#include <sys/stream.h>
#endif
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include "y.tab.h"
#include "ip_compat.h"
#include "ipf.h"

#ifndef	__P
# ifdef __STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

extern int opts;

int lineNum = 0, proto = 0, oldproto = 0, next = -1, laststate = 0;
int *prstack = NULL, numpr = 0, state = 0, token = 0;

void    yyerror __P((char *));
void	push_proto __P((void));
void	pop_proto __P((void));
int	next_state __P((int, int));
int	next_item __P((int));
int	save_token __P((void));
void	swallow __P((void));
int	yylex __P((void));
%}

%%
[ \t\r]	;
\n	{ lineNum++; swallow(); }
interface |
iface	{ return next_state(IL_INTERFACE, -1); }
name |
ifname	{ return next_state(IL_IFNAME, IL_TOKEN); }
router	{ return next_state(IL_DEFROUTER, IL_TOKEN); }
mtu 	{ return next_state(IL_MTU, IL_NUMBER); }
eaddr	{ return next_state(IL_EADDR, IL_TOKEN); }
v4addr	{ return next_state(IL_V4ADDR, IL_TOKEN); }
ipv4	{ return next_state(IL_IPV4, -1); }
v	{ return next_state(IL_V4V, IL_TOKEN); }
proto	{ return next_state(IL_V4PROTO, IL_TOKEN); }
hl	{ return next_state(IL_V4HL, IL_TOKEN); }
id	{ return next_state(IL_V4ID, IL_TOKEN); }
ttl	{ return next_state(IL_V4TTL, IL_TOKEN); }
tos	{ return next_state(IL_V4TOS, IL_TOKEN); }
src	{ return next_state(IL_V4SRC, IL_TOKEN); }
dst	{ return next_state(IL_V4DST, IL_TOKEN); }
opt	{ return next_state(IL_OPT, -1); }
len	{ return next_state(IL_LEN, IL_TOKEN); }
off	{ return next_state(IL_OFF, IL_TOKEN); }
sum	{ return next_state(IL_SUM, IL_TOKEN); }
tcp	{ return next_state(IL_TCP, -1); }
sport	{ return next_state(IL_SPORT, IL_TOKEN); }
dport	{ return next_state(IL_DPORT, IL_TOKEN); }
seq	{ return next_state(IL_TCPSEQ, IL_TOKEN); }
ack	{ return next_state(IL_TCPACK, IL_TOKEN); }
flags	{ return next_state(IL_TCPFL, IL_TOKEN); }
urp	{ return next_state(IL_TCPURP, IL_TOKEN); }
win	{ return next_state(IL_TCPWIN, IL_TOKEN); }
udp	{ return next_state(IL_UDP, -1); }
send	{ return next_state(IL_SEND, -1); }
via	{ return next_state(IL_VIA, IL_TOKEN); }
arp	{ return next_state(IL_ARP, -1); }
data	{ return next_state(IL_DATA, -1); }
value	{ return next_state(IL_DVALUE, IL_TOKEN); }
file	{ return next_state(IL_DFILE, IL_TOKEN); }
nop	{ return next_state(IL_IPO_NOP, -1); }
eol	{ return next_state(IL_IPO_EOL, -1); }
rr	{ return next_state(IL_IPO_RR, -1);  }
zsu	{ return next_state(IL_IPO_ZSU, -1); }
mtup	{ return next_state(IL_IPO_MTUP, -1); }
mtur	{ return next_state(IL_IPO_MTUR, -1); }
encode	{ return next_state(IL_IPO_ENCODE, -1); }
ts	{ return next_state(IL_IPO_TS, -1); }
tr	{ return next_state(IL_IPO_TR, -1); }
sec	{ return next_state(IL_IPO_SEC, -1); }
secclass	{ return next_state(IL_IPO_SECCLASS, IL_TOKEN); }
lsrr	{ return next_state(IL_IPO_LSRR, -1); }
esec	{ return next_state(IL_IPO_ESEC, -1); }
cipso	{ return next_state(IL_IPO_CIPSO, -1); }
satid	{ return next_state(IL_IPO_SATID, -1); }
ssrr	{ return next_state(IL_IPO_SSRR, -1); }
addext	{ return next_state(IL_IPO_ADDEXT, -1); }
visa	{ return next_state(IL_IPO_VISA, -1); }
imitd	{ return next_state(IL_IPO_IMITD, -1); }
eip	{ return next_state(IL_IPO_EIP, -1); }
finn	{ return next_state(IL_IPO_FINN, -1); }
mss	{ return next_state(IL_TCPO_MSS, IL_TOKEN); }
wscale	{ return next_state(IL_TCPO_MSS, IL_TOKEN); }
reserv-4	{ return next_state(IL_IPS_RESERV4, -1); }
topsecret	{ return next_state(IL_IPS_TOPSECRET, -1); }
secret		{ return next_state(IL_IPS_SECRET, -1); }
reserv-3	{ return next_state(IL_IPS_RESERV3, -1); }
confid		{ return next_state(IL_IPS_CONFID, -1); }
unclass		{ return next_state(IL_IPS_UNCLASS, -1); }
reserv-2	{ return next_state(IL_IPS_RESERV2, -1); }
reserv-1	{ return next_state(IL_IPS_RESERV1, -1); }
icmp		{ return next_state(IL_ICMP, -1); }
type		{ return next_state(IL_ICMPTYPE, -1); }
code		{ return next_state(IL_ICMPCODE, -1); }
echorep		{ return next_state(IL_ICMP_ECHOREPLY, -1); }
unreach		{ return next_state(IL_ICMP_UNREACH, -1); }
squench		{ return next_state(IL_ICMP_SOURCEQUENCH, -1); }
redir		{ return next_state(IL_ICMP_REDIRECT, -1); }
echo		{ return next_state(IL_ICMP_ECHO, -1); }
routerad	{ return next_state(IL_ICMP_ROUTERADVERT, -1); }
routersol	{ return next_state(IL_ICMP_ROUTERSOLICIT, -1); }
timex		{ return next_state(IL_ICMP_TIMXCEED, -1); }
paramprob	{ return next_state(IL_ICMP_PARAMPROB, -1); }
timest		{ return next_state(IL_ICMP_TSTAMP, -1); }
timestrep	{ return next_state(IL_ICMP_TSTAMPREPLY, -1); }
inforeq		{ return next_state(IL_ICMP_IREQ, -1); }
inforep		{ return next_state(IL_ICMP_IREQREPLY, -1); }
maskreq		{ return next_state(IL_ICMP_MASKREQ, -1); }
maskrep		{ return next_state(IL_ICMP_MASKREPLY, -1); }
net-unr		{ return next_state(IL_ICMP_UNREACH_NET, -1); }
host-unr	{ return next_state(IL_ICMP_UNREACH_HOST, -1); }
proto-unr	{ return next_state(IL_ICMP_UNREACH_PROTOCOL, -1); }
port-unr	{ return next_state(IL_ICMP_UNREACH_PORT, -1); }
needfrag	{ return next_state(IL_ICMP_UNREACH_NEEDFRAG, -1); }
srcfail		{ return next_state(IL_ICMP_UNREACH_SRCFAIL, -1); }
net-unk		{ return next_state(IL_ICMP_UNREACH_NET_UNKNOWN, -1); }
host-unk	{ return next_state(IL_ICMP_UNREACH_HOST_UNKNOWN, -1); }
isolate		{ return next_state(IL_ICMP_UNREACH_ISOLATED, -1); }
net-prohib	{ return next_state(IL_ICMP_UNREACH_NET_PROHIB, -1); }
host-prohib	{ return next_state(IL_ICMP_UNREACH_HOST_PROHIB, -1); }
net-tos		{ return next_state(IL_ICMP_UNREACH_TOSNET, -1); }
host-tos	{ return next_state(IL_ICMP_UNREACH_TOSHOST, -1); }
filter-prohib	{ return next_state(IL_ICMP_UNREACH_FILTER_PROHIB, -1); }
host-preced	{ return next_state(IL_ICMP_UNREACH_HOST_PRECEDENCE, -1); }
cutoff-preced	{ return next_state(IL_ICMP_UNREACH_PRECEDENCE_CUTOFF, -1); }
net-redir	{ return next_state(IL_ICMP_REDIRECT_NET, -1); }
host-redir	{ return next_state(IL_ICMP_REDIRECT_HOST, -1); }
tos-net-redir	{ return next_state(IL_ICMP_REDIRECT_TOSNET, -1); }
tos-host-redir	{ return next_state(IL_ICMP_REDIRECT_TOSHOST, -1); }
intrans		{ return next_state(IL_ICMP_TIMXCEED_INTRANS, -1); }
reass		{ return next_state(IL_ICMP_TIMXCEED_REASS, -1); }
optabsent	{ return next_state(IL_ICMP_PARAMPROB_OPTABSENT, -1); }
otime		{ return next_state(IL_ICMP_OTIME, -1); }
rtime		{ return next_state(IL_ICMP_RTIME, -1); }
ttime		{ return next_state(IL_ICMP_TTIME, -1); }
icmpseq		{ return next_state(IL_ICMP_SEQ, -1); }
icmpid		{ return next_state(IL_ICMP_SEQ, -1); }
\377		{ return 0; }		/* EOF */
\{	{ push_proto(); return next_item('{'); }
\}	{ pop_proto(); return next_item('}'); }
\.	{ return next_item(IL_DOT); }
;	{ return next_item(';'); }
[0-9]+	{ return next_item(IL_NUMBER); }
[0-9a-fA-F]	{ return next_item(IL_HEXDIGIT); }
:	{ return next_item(IL_COLON); }
#[^\n]*	{ return next_item(IL_COMMENT); }
[^ {}\n\t;]*	{ return next_item(IL_TOKEN); }
\"[^\"]*\"	{ return next_item(IL_TOKEN); }
%%
void    yyerror(msg)
char    *msg;
{
	fprintf(stderr, "%s error at \"%s\", line %d\n", msg, yytext,
		lineNum + 1);
	exit(1);
}


void push_proto()
{
	numpr++;
	if (!prstack)
		prstack = (int *)malloc(sizeof(int));
	else
		prstack = (int *)realloc((char *)prstack, numpr * sizeof(int));
	prstack[numpr - 1] = oldproto;
}


void pop_proto()
{
	numpr--;
	proto = prstack[numpr];
	if (!numpr) {
		free(prstack);
		prstack = NULL;
		return;
	}
	prstack = (int *)realloc((char *)prstack, numpr * sizeof(int));
}


int save_token()
{

	yylval.str = strdup(yytext);
	return IL_TOKEN;
}


int next_item(nstate)
int nstate;
{
	if (next == IL_TOKEN) {
		next = -1;
		return save_token();
	}
	next = -1;
	if (nstate == IL_NUMBER)
		yylval.num = atoi(yytext);
	token++;
	return nstate;
}


int next_state(nstate, fornext)
int nstate, fornext;
{
	token++;

	if (next == IL_TOKEN) {
		next = -1;
		return save_token();
	}

	next = fornext;

	switch (nstate)
	{
	case IL_IPV4 :
	case IL_TCP :
	case IL_UDP :
	case IL_ICMP :
	case IL_DATA :
	case IL_INTERFACE :
	case IL_ARP :
		oldproto = proto;
		proto = nstate;
		break;
	case IL_SUM :
		if (proto == IL_IPV4)
			nstate = IL_V4SUM;
		else if (proto == IL_TCP)
			nstate = IL_TCPSUM;
		else if (proto == IL_UDP)
			nstate = IL_UDPSUM;
		break;
	case IL_OPT :
		if (proto == IL_IPV4)
			nstate = IL_V4OPT;
		else if (proto == IL_TCP)
			nstate = IL_TCPOPT;
		break;
	case IL_IPO_NOP :
		if (proto == IL_TCP)
			nstate = IL_TCPO_NOP;
		break;
	case IL_IPO_EOL :
		if (proto == IL_TCP)
			nstate = IL_TCPO_EOL;
		break;
	case IL_IPO_TS :
		if (proto == IL_TCP)
			nstate = IL_TCPO_TS;
		break;
	case IL_OFF :
		if (proto == IL_IPV4)
			nstate = IL_V4OFF;
		else if (proto == IL_TCP)
			nstate = IL_TCPOFF;
		break;
	case IL_LEN :
		if (proto == IL_IPV4)
			nstate = IL_V4LEN;
		else if (proto == IL_UDP)
			nstate = IL_UDPLEN;
		break;
	}
	return nstate;
}


void swallow()
{
	int c = input();

	if (c == '#') {
		while ((c != '\n') && (c != EOF))
			c = input();
	}
	unput(c);
}

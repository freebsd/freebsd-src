%{
/*
 * Configuration file parser for mrouted.
 *
 * Written by Bill Fenner, NRL, 1994
 *
 * $Id: cfparse.y,v 3.6 1995/06/25 18:49:46 fenner Exp $
 */
#include <stdio.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "defs.h"

/*
 * Local function declarations
 */
static void		fatal __P((char *fmt, ...));
static void		warn __P((char *fmt, ...));
static void		yyerror __P((char *s));
static char *		next_word __P((void));
static int		yylex __P((void));
static u_int32		valid_if __P((char *s));
static struct ifreq *	ifconfaddr __P((struct ifconf *ifcp, u_int32 a));
int			yyparse __P((void));

static FILE *f;

extern int udp_socket;
char *configfilename = _PATH_MROUTED_CONF;

extern int cache_lifetime;
extern int max_prune_lifetime;

static int lineno;
static struct ifreq ifbuf[32];
static struct ifconf ifc;

static struct uvif *v;

static int order;

struct addrmask {
	u_int32	addr;
	int	mask;
};

struct boundnam {
	char		*name;
	struct addrmask	 bound;
};

#define MAXBOUNDS 20

struct boundnam boundlist[MAXBOUNDS];	/* Max. of 20 named boundaries */
int numbounds = 0;			/* Number of named boundaries */

%}

%union
{
	int num;
	char *ptr;
	struct addrmask addrmask;
	u_int32 addr;
};

%token CACHE_LIFETIME PRUNING
%token PHYINT TUNNEL NAME
%token DISABLE METRIC THRESHOLD RATE_LIMIT SRCRT BOUNDARY NETMASK ALTNET
%token <num> BOOLEAN
%token <num> NUMBER
%token <ptr> STRING
%token <addrmask> ADDRMASK
%token <addr> ADDR

%type <addr> interface
%type <addrmask> bound boundary addrmask

%start conf

%%

conf	: stmts
	;

stmts	: /* Empty */
	| stmts stmt
	;

stmt	: error
	| PHYINT interface 		{

			vifi_t vifi;

			if (order)
			    fatal("phyints must appear before tunnels");

			for (vifi = 0, v = uvifs;
			     vifi < numvifs;
			     ++vifi, ++v)
			    if (!(v->uv_flags & VIFF_TUNNEL) &&
				$2 == v->uv_lcl_addr)
				break;
			
			if (vifi == numvifs)
			    fatal("%s is not a configured interface",
				inet_fmt($2,s1));

			/*log(LOG_INFO, 0, "phyint: %x\n", v);*/
					}
		ifmods
	| TUNNEL interface ADDR		{

			struct ifreq *ifr;
			struct ifreq ffr;
			vifi_t vifi;

			order++;

			ifr = ifconfaddr(&ifc, $2);
			if (ifr == 0)
			    fatal("Tunnel local address %s is not mine",
				inet_fmt($2, s1));

			strncpy(ffr.ifr_name, ifr->ifr_name, IFNAMSIZ);
			if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ffr)<0)
			    fatal("ioctl SIOCGIFFLAGS on %s",ffr.ifr_name);
			if (ffr.ifr_flags & IFF_LOOPBACK)
			    fatal("Tunnel local address %s is a loopback interface",
				inet_fmt($2, s1));

			if (ifconfaddr(&ifc, $3) != 0)
			    fatal("Tunnel remote address %s is one of mine",
				inet_fmt($3, s1));

			for (vifi = 0, v = uvifs;
			     vifi < numvifs;
			     ++vifi, ++v)
			    if (v->uv_flags & VIFF_TUNNEL) {
				if ($3 == v->uv_rmt_addr)
				    fatal("Duplicate tunnel to %s",
					inet_fmt($3, s1));
			    } else if (!(v->uv_flags & VIFF_DISABLED)) {
				if (($3 & v->uv_subnetmask) == v->uv_subnet)
				    fatal("Unnecessary tunnel to %s",
					inet_fmt($3,s1));
			    }

			if (numvifs == MAXVIFS)
			    fatal("too many vifs");

			v = &uvifs[numvifs];
			v->uv_flags	= VIFF_TUNNEL;
			v->uv_metric	= DEFAULT_METRIC;
			v->uv_rate_limit= DEFAULT_TUN_RATE_LIMIT;
			v->uv_threshold	= DEFAULT_THRESHOLD;
			v->uv_lcl_addr	= $2;
			v->uv_rmt_addr	= $3;
			v->uv_subnet	= 0;
			v->uv_subnetmask= 0;
			v->uv_subnetbcast= 0;
			strncpy(v->uv_name, ffr.ifr_name, IFNAMSIZ);
			v->uv_groups	= NULL;
			v->uv_neighbors	= NULL;
			v->uv_acl	= NULL;
			v->uv_addrs	= NULL;

			if (!(ffr.ifr_flags & IFF_UP)) {
			    v->uv_flags |= VIFF_DOWN;
			    vifs_down = TRUE;
			}
			/*log(LOG_INFO, 0, "tunnel: %x\n", v);*/
					}
		tunnelmods
					{
			log(LOG_INFO, 0,
			    "installing tunnel from %s to %s as vif #%u - rate=%d",
			    inet_fmt($2, s1), inet_fmt($3, s2),
			    numvifs, v->uv_rate_limit);

			++numvifs;
					}
	| PRUNING BOOLEAN	    { pruning = $2; }
	| CACHE_LIFETIME NUMBER     { cache_lifetime = $2;
				      max_prune_lifetime = cache_lifetime * 2;
				    }
	| NAME STRING boundary	    { if (numbounds >= MAXBOUNDS) {
					fatal("Too many named boundaries (max %d)", MAXBOUNDS);
				      }

				      boundlist[numbounds].name = malloc(strlen($2) + 1);
				      strcpy(boundlist[numbounds].name, $2);
				      boundlist[numbounds++].bound = $3;
				    }
	;

tunnelmods	: /* empty */
	| tunnelmods /*{ log(LOG_INFO, 0, "tunnelmod: %x", v); }*/ tunnelmod
	;

tunnelmod	: mod
	| SRCRT			{ fatal("Source-route tunnels not supported"); }
	;

ifmods	: /* empty */
	| ifmods /*{ log(LOG_INFO, 0, "ifmod: %x", v); }*/ ifmod
	;

ifmod	: mod
	| DISABLE		{ v->uv_flags |= VIFF_DISABLED; }
	| NETMASK ADDR		{
				  u_int32 subnet, mask;

				  mask = $2;
				  subnet = v->uv_lcl_addr & mask;
				  if (!inet_valid_subnet(subnet, mask))
					fatal("Invalid netmask");
				  v->uv_subnet = subnet;
				  v->uv_subnetmask = mask;
				  v->uv_subnetbcast = subnet | ~mask;
				}
	| ALTNET addrmask	{

		    struct phaddr *ph;

		    ph = (struct phaddr *)malloc(sizeof(struct phaddr));
		    if (ph == NULL)
			fatal("out of memory");
		    if ($2.mask) {
			VAL_TO_MASK(ph->pa_subnetmask, $2.mask);
		    } else
			ph->pa_subnetmask = v->uv_subnetmask;
		    ph->pa_subnet = $2.addr & ph->pa_subnetmask;
		    ph->pa_subnetbcast = ph->pa_subnet | ~ph->pa_subnetmask;
		    if ($2.addr & ~ph->pa_subnetmask)
			warn("Extra subnet %s/%d has host bits set",
				inet_fmt($2.addr,s1), $2.mask);
		    ph->pa_next = v->uv_addrs;
		    v->uv_addrs = ph;

				}
	;

mod	: THRESHOLD NUMBER	{ if ($2 < 1 || $2 > 255)
				    fatal("Invalid threshold %d",$2);
				  v->uv_threshold = $2;
				}
	| THRESHOLD		{

		    warn("Expected number after threshold keyword");

				}
	| METRIC NUMBER		{ if ($2 < 1 || $2 > UNREACHABLE)
				    fatal("Invalid metric %d",$2);
				  v->uv_metric = $2;
				}
	| METRIC		{

		    warn("Expected number after metric keyword");

				}
	| RATE_LIMIT NUMBER	{ if ($2 > MAX_RATE_LIMIT)
				    fatal("Invalid rate_limit %d",$2);
				  v->uv_rate_limit = $2;
				}
	| RATE_LIMIT		{

		    warn("Expected number after rate_limit keyword");

				}
	| BOUNDARY bound	{

		    struct vif_acl *v_acl;

		    v_acl = (struct vif_acl *)malloc(sizeof(struct vif_acl));
		    if (v_acl == NULL)
			fatal("out of memory");
		    VAL_TO_MASK(v_acl->acl_mask, $2.mask);
		    v_acl->acl_addr = $2.addr & v_acl->acl_mask;
		    if ($2.addr & ~v_acl->acl_mask)
			warn("Boundary spec %s/%d has host bits set",
				inet_fmt($2.addr,s1),$2.mask);
		    v_acl->acl_next = v->uv_acl;
		    v->uv_acl = v_acl;

				}
	| BOUNDARY		{

		    warn("Expected boundary spec after boundary keyword");

				}
	;

interface	: ADDR		{ $$ = $1; }
	| STRING		{
				  $$ = valid_if($1);
				  if ($$ == 0)
					fatal("Invalid interface name %s",$1);
				}
	;

bound	: boundary		{ $$ = $1; }
	| STRING		{ int i;

				  for (i=0; i < numbounds; i++) {
				    if (!strcmp(boundlist[i].name, $1)) {
					$$ = boundlist[i].bound;
					break;
				    }
				  }
				  if (i == numbounds) {
				    fatal("Invalid boundary name %s",$1);
				  }
				}
	;

boundary	: ADDRMASK	{

			if ((ntohl($1.addr) & 0xff000000) != 0xef000000) {
			    fatal("Boundaries must be 239.x.x.x, not %s/%d",
				inet_fmt($1.addr, s1), $1.mask);
			}
			$$ = $1;

				}
	;

addrmask	: ADDRMASK	{ $$ = $1; }
	| ADDR			{ $$.addr = $1; $$.mask = 0; }
	;
%%
#ifdef __STDC__
static void
fatal(char *fmt, ...)
{
	va_list ap;
	char buf[200];

	va_start(ap, fmt);
#else
/*VARARGS1*/
static void
fatal(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list ap;
	char buf[200];

	va_start(ap);
#endif
	vsprintf(buf, fmt, ap);
	va_end(ap);

	log(LOG_ERR,0,"%s: %s near line %d", configfilename, buf, lineno);
}

#ifdef __STDC__
static void
warn(char *fmt, ...)
{
	va_list ap;
	char buf[200];

	va_start(ap, fmt);
#else
/*VARARGS1*/
static void
warn(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list ap;
	char buf[200];

	va_start(ap);
#endif
	vsprintf(buf, fmt, ap);
	va_end(ap);

	log(LOG_WARNING,0,"%s: %s near line %d", configfilename, buf, lineno);
}

static void
yyerror(s)
char *s;
{
	log(LOG_ERR, 0, "%s: %s near line %d", configfilename, s, lineno);
}

static char *
next_word()
{
	static char buf[1024];
	static char *p=NULL;
	extern FILE *f;
	char *q;

	while (1) {
	    if (!p || !*p) {
		lineno++;
		if (fgets(buf, sizeof(buf), f) == NULL)
		    return NULL;
		p = buf;
	    }
	    while (*p && (*p == ' ' || *p == '\t'))	/* skip whitespace */
		p++;
	    if (*p == '#') {
		p = NULL;		/* skip comments */
		continue;
	    }
	    q = p;
	    while (*p && *p != ' ' && *p != '\t' && *p != '\n')
		p++;		/* find next whitespace */
	    *p++ = '\0';	/* null-terminate string */

	    if (!*q) {
		p = NULL;
		continue;	/* if 0-length string, read another line */
	    }

	    return q;
	}
}

static int
yylex()
{
	int n;
	u_int32 addr;
	char *q;

	if ((q = next_word()) == NULL) {
		return 0;
	}

	if (!strcmp(q,"cache_lifetime"))
		return CACHE_LIFETIME;
	if (!strcmp(q,"pruning"))
		return PRUNING;
	if (!strcmp(q,"phyint"))
		return PHYINT;
	if (!strcmp(q,"tunnel"))
		return TUNNEL;
	if (!strcmp(q,"disable"))
		return DISABLE;
	if (!strcmp(q,"metric"))
		return METRIC;
	if (!strcmp(q,"threshold"))
		return THRESHOLD;
	if (!strcmp(q,"rate_limit"))
		return RATE_LIMIT;
	if (!strcmp(q,"srcrt") || !strcmp(q,"sourceroute"))
		return SRCRT;
	if (!strcmp(q,"boundary"))
		return BOUNDARY;
	if (!strcmp(q,"netmask"))
		return NETMASK;
	if (!strcmp(q,"name"))
		return NAME;
	if (!strcmp(q,"altnet"))
		return ALTNET;
	if (!strcmp(q,"on") || !strcmp(q,"yes")) {
		yylval.num = 1;
		return BOOLEAN;
	}
	if (!strcmp(q,"off") || !strcmp(q,"no")) {
		yylval.num = 0;
		return BOOLEAN;
	}
	if (sscanf(q,"%[.0-9]/%d%c",s1,&n,s2) == 2) {
		if ((addr = inet_parse(s1)) != 0xffffffff) {
			yylval.addrmask.mask = n;
			yylval.addrmask.addr = addr;
			return ADDRMASK;
		}
		/* fall through to returning STRING */
	}
	if (sscanf(q,"%[.0-9]%c",s1,s2) == 1) {
		if ((addr = inet_parse(s1)) != 0xffffffff &&
		    inet_valid_host(addr)) { 
			yylval.addr = addr;
			return ADDR;
		}
	}
	if (sscanf(q,"0x%8x%c",&n,s1) == 1) {
		yylval.addr = n;
		return ADDR;
	}
	if (sscanf(q,"%d%c",&n,s1) == 1) {
		yylval.num = n;
		return NUMBER;
	}
	yylval.ptr = q;
	return STRING;
}

void
config_vifs_from_file()
{
	extern FILE *f;

	order = 0;
	numbounds = 0;
	lineno = 0;

	if ((f = fopen(configfilename, "r")) == NULL) {
	    if (errno != ENOENT)
		log(LOG_ERR, errno, "can't open %s", configfilename);
	    return;
	}

	ifc.ifc_buf = (char *)ifbuf;
	ifc.ifc_len = sizeof(ifbuf);
	if (ioctl(udp_socket, SIOCGIFCONF, (char *)&ifc) < 0)
	    log(LOG_ERR, errno, "ioctl SIOCGIFCONF");

	yyparse();

	fclose(f);
}

static u_int32
valid_if(s)
char *s;
{
	register vifi_t vifi;
	register struct uvif *v;

	for (vifi=0, v=uvifs; vifi<numvifs; vifi++, v++)
	    if (!strcmp(v->uv_name, s))
		return v->uv_lcl_addr;

	return 0;
}

static struct ifreq *
ifconfaddr(ifcp, a)
    struct ifconf *ifcp;
    u_int32 a;
{
    int n;
    struct ifreq *ifrp = (struct ifreq *)ifcp->ifc_buf;
    struct ifreq *ifend = (struct ifreq *)((char *)ifrp + ifcp->ifc_len);

    while (ifrp < ifend) {
	    if (ifrp->ifr_addr.sa_family == AF_INET &&
		((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr == a)
		    return (ifrp);
#if (defined(BSD) && (BSD >= 199006))
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			++ifrp;
		else
			ifrp = (struct ifreq *)((char *)ifrp + n);
#else
		++ifrp;
#endif
    }
    return (0);
}

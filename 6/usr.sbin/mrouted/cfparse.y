%{
/*
 * Configuration file parser for mrouted.
 *
 * Written by Bill Fenner, NRL, 1994
 *
 * $FreeBSD$
 * cfparse.y,v 3.8.4.30 1998/03/01 01:48:58 fenner Exp
 */
#include <stdio.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "defs.h"
#include <netdb.h>
#include <ifaddrs.h>

/*
 * Local function declarations
 */
static void		fatal __P((char *fmt, ...)) __printflike(1, 2);
static void		warn __P((char *fmt, ...)) __printflike(1, 2);
static void		yyerror __P((char *s));
static char *		next_word __P((void));
static int		yylex __P((void));
static u_int32		valid_if __P((char *s));
static const char *	ifconfaddr(u_int32_t a);
int			yyparse __P((void));

static FILE *f;

char *configfilename = _PATH_MROUTED_CONF;

extern int cache_lifetime;
extern int prune_lifetime;

/* imported from config.c, with slight memory leak */
extern struct ifconf ifc;

int allow_black_holes = 0;

static int lineno;

static struct uvif *v;

static int order, state;
static int noflood = 0;
static int rexmit = VIFF_REXMIT_PRUNES;

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
	struct vf_element *filterelem;
};

%token CACHE_LIFETIME PRUNE_LIFETIME PRUNING BLACK_HOLE NOFLOOD
%token PHYINT TUNNEL NAME
%token DISABLE IGMPV1 SRCRT BESIDE
%token METRIC THRESHOLD RATE_LIMIT BOUNDARY NETMASK ALTNET ADVERT_METRIC
%token FILTER ACCEPT DENY EXACT BIDIR REXMIT_PRUNES REXMIT_PRUNES2
%token PASSIVE ALLOW_NONPRUNERS
%token NOTRANSIT BLASTER FORCE_LEAF
%token PRUNE_LIFETIME2 NOFLOOD2
%token SYSNAM SYSCONTACT SYSVERSION SYSLOCATION
%token <num> BOOLEAN
%token <num> NUMBER
%token <ptr> STRING
%token <addrmask> ADDRMASK
%token <addr> ADDR

%type <addr> interface addrname
%type <addrmask> bound boundary addrmask
%type <filterelem> filter filtlist filtelement filtelem

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

			state++;

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

					}
		ifmods
	| TUNNEL interface addrname	{
			const char *ifname;
			struct ifreq ffr;
			vifi_t vifi;

			order++;

			ifname = ifconfaddr($2);
			if (ifname == 0)
			    fatal("Tunnel local address %s is not mine",
				inet_fmt($2, s1));

			if (((ntohl($2) & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT) ==
				IN_LOOPBACKNET)
			    fatal("Tunnel local address %s is a loopback address",
				inet_fmt($2, s1));

			if (ifconfaddr($3) != 0)
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
				    fatal("Unnecessary tunnel to %s, same subnet as vif %d (%s)",
					inet_fmt($3,s1), vifi, v->uv_name);
			    }

			if (numvifs == MAXVIFS)
			    fatal("too many vifs");

			strlcpy(ffr.ifr_name, ifname, sizeof(ffr.ifr_name));
			if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ffr)<0)
			    fatal("ioctl SIOCGIFFLAGS on %s", ffr.ifr_name);

			v = &uvifs[numvifs];
			zero_vif(v, 1);
			v->uv_flags	= VIFF_TUNNEL | rexmit | noflood;
			v->uv_flags |= VIFF_OTUNNEL; /*XXX*/
			v->uv_lcl_addr	= $2;
			v->uv_rmt_addr	= $3;
			v->uv_dst_addr	= $3;
			strlcpy(v->uv_name, ffr.ifr_name, sizeof(v->uv_name));

			if (!(ffr.ifr_flags & IFF_UP)) {
			    v->uv_flags |= VIFF_DOWN;
			    vifs_down = TRUE;
			}
					}
		tunnelmods
					{

	if (!(v->uv_flags & VIFF_OTUNNEL)) {
	    init_ipip_on_vif(v);
	}

	log(LOG_INFO, 0,
	    "installing tunnel from %s to %s as vif #%u - rate=%d",
	    inet_fmt($2, s1), inet_fmt($3, s2),
	    numvifs, v->uv_rate_limit);

	++numvifs;

					}
	| CACHE_LIFETIME NUMBER     {

			if ($2 < MIN_CACHE_LIFETIME) {
			    warn("cache_lifetime %d must be at least %d",
					    $2, MIN_CACHE_LIFETIME);
			} else {
			    cache_lifetime = $2;
			}

				    }
	| PRUNE_LIFETIME NUMBER	    {

			if ($2 < MIN_PRUNE_LIFETIME) {
			    warn("prune_lifetime %d must be at least %d",
					    $2, MIN_PRUNE_LIFETIME);
			} else {
			    prune_lifetime = $2;
			}

				    }
	| PRUNING BOOLEAN	    {

			if ($2 != 1) {
			    warn("Disabling pruning is no longer supported");
			}

				    }
	| BLACK_HOLE		    {
#ifdef ALLOW_BLACK_HOLES
					allow_black_holes = 1;
#endif
				    }
	/*
	 * Turn off initial flooding (until subordinateness is learned
	 * via route exchange) on all phyints and set the default for
	 * all further tunnels.
	 */
	| NOFLOOD		    {

			vifi_t vifi;

			noflood = VIFF_NOFLOOD;
			for (vifi = 0, v = uvifs;
			     vifi < numvifs;
			     ++vifi, ++v)
				v->uv_flags |= VIFF_NOFLOOD;

				    }
	/*
	 * Turn on prune retransmission on all interfaces.
	 * Tunnels default to retransmitting, so this just
	 * needs to turn on phyints.
	 */
	| REXMIT_PRUNES		    {

			vifi_t vifi;

			for (vifi = 0, v = uvifs;
			     vifi < numvifs;
			     ++vifi, ++v)
				v->uv_flags |= VIFF_REXMIT_PRUNES;

				    }
	/*
	 * If true, do as above.  If false, no need to turn
	 * it off for phyints since they default to not
	 * rexmit; need to set flag to not rexmit on tunnels.
	 */
	| REXMIT_PRUNES BOOLEAN {

		    if ($2) {
			vifi_t vifi;

			for (vifi = 0, v = uvifs;
			     vifi < numvifs;
			     ++vifi, ++v)
				v->uv_flags |= VIFF_REXMIT_PRUNES;
		    } else {
			rexmit = 0;
		    }

				}
	| NAME STRING boundary	    { if (numbounds >= MAXBOUNDS) {
					fatal("Too many named boundaries (max %d)", MAXBOUNDS);
				      }

				      boundlist[numbounds].name = malloc(strlen($2) + 1);
				      strcpy(boundlist[numbounds].name, $2);
				      boundlist[numbounds++].bound = $3;
				    }
	| SYSNAM STRING    {
#ifdef SNMP
			    set_sysName($2);
#endif /* SNMP */
			    }
	| SYSCONTACT STRING {
#ifdef SNMP
			    set_sysContact($2);
#endif /* SNMP */
			    }
        | SYSVERSION STRING {
#ifdef SNMP
			    set_sysVersion($2);
#endif /* SNMP */
			    }
	| SYSLOCATION STRING {
#ifdef SNMP
			    set_sysLocation($2);
#endif /* SNMP */
			    }
	;

tunnelmods	: /* empty */
	| tunnelmods tunnelmod
	;

tunnelmod	: mod
	| BESIDE		{ v->uv_flags |= VIFF_OTUNNEL; }
	| BESIDE BOOLEAN	{

		    if ($2) {
			v->uv_flags |= VIFF_OTUNNEL;
		    } else {
			v->uv_flags &= ~VIFF_OTUNNEL;
		    }

				}
	| SRCRT			{ fatal("Source-route tunnels not supported"); }
	;

ifmods	: /* empty */
	| ifmods ifmod
	;

ifmod	: mod
	| DISABLE		{ v->uv_flags |= VIFF_DISABLED; }
	| IGMPV1		{ v->uv_flags |= VIFF_IGMPV1; }
	| NETMASK addrname	{
				  u_int32 subnet, mask;

				  mask = $2;
				  subnet = v->uv_lcl_addr & mask;
				  if (!inet_valid_subnet(subnet, mask))
					fatal("Invalid netmask");
				  v->uv_subnet = subnet;
				  v->uv_subnetmask = mask;
				  v->uv_subnetbcast = subnet | ~mask;
				}
	| NETMASK		{

		    warn("Expected address after netmask keyword, ignored");

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
	| ALTNET		{

		    warn("Expected address after altnet keyword, ignored");

				}
	| FORCE_LEAF		{

		    v->uv_flags |= VIFF_FORCE_LEAF;

				}
	| FORCE_LEAF BOOLEAN	{

		    if ($2) {
		        v->uv_flags |= VIFF_FORCE_LEAF;
		    } else {
		        v->uv_flags &= ~VIFF_FORCE_LEAF;
		    }

				}
	;

mod	: THRESHOLD NUMBER	{ if ($2 < 1 || $2 > 255)
				    fatal("Invalid threshold %d",$2);
				  v->uv_threshold = $2;
				}
	| THRESHOLD		{

		    warn("Expected number after threshold keyword, ignored");

				}
	| METRIC NUMBER		{ if ($2 < 1 || $2 > UNREACHABLE)
				    fatal("Invalid metric %d",$2);
				  v->uv_metric = $2;
				}
	| METRIC		{

		    warn("Expected number after metric keyword, ignored");

				}
	| ADVERT_METRIC NUMBER	{ if ($2 < 0 || $2 > UNREACHABLE - 1)
				    fatal("Invalid advert_metric %d", $2);
				  v->uv_admetric = $2;
				}
	| ADVERT_METRIC		{

		    warn("Expected number after advert_metric keyword, ignored");

				}
	| RATE_LIMIT NUMBER	{ if ($2 > MAX_RATE_LIMIT)
				    fatal("Invalid rate_limit %d",$2);
				  v->uv_rate_limit = $2;
				}
	| RATE_LIMIT		{

		    warn("Expected number after rate_limit keyword, ignored");

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

		warn("Expected boundary spec after boundary keyword, ignored");

				}
	| REXMIT_PRUNES2	{

		    v->uv_flags |= VIFF_REXMIT_PRUNES;

				}
	| REXMIT_PRUNES2 BOOLEAN {

		    if ($2) {
			v->uv_flags |= VIFF_REXMIT_PRUNES;
		    } else {
			v->uv_flags &= ~VIFF_REXMIT_PRUNES;
		    }

				}
	| PASSIVE		{

		    v->uv_flags |= VIFF_PASSIVE;

				}
	| NOFLOOD2		{

		    v->uv_flags |= VIFF_NOFLOOD;

				}
	| NOTRANSIT		{

		    v->uv_flags |= VIFF_NOTRANSIT;

				}
	| BLASTER		{

		    v->uv_flags |= VIFF_BLASTER;
		    blaster_alloc(v - uvifs);

				}
	| ALLOW_NONPRUNERS	{

		    v->uv_flags |= VIFF_ALLOW_NONPRUNERS;

				}
	| PRUNE_LIFETIME2 NUMBER {

			if ($2 < MIN_PRUNE_LIFETIME) {
			    warn("prune_lifetime %d must be at least %d",
					    $2, MIN_PRUNE_LIFETIME);
			} else {
			    v->uv_prune_lifetime = $2;
			}

				}
	| ACCEPT filter		{

		    if (v->uv_filter == NULL) {
			struct vif_filter *v_filter;

			v_filter = (struct vif_filter *)malloc(sizeof(struct vif_filter));
			if (v_filter == NULL)
			    fatal("out of memory");
			v_filter->vf_flags = 0;
			v_filter->vf_type = VFT_ACCEPT;
			v_filter->vf_filter = $2;
			v->uv_filter = v_filter;
		    } else if (v->uv_filter->vf_type != VFT_ACCEPT) {
			fatal("can't accept and deny");
		    } else {
			struct vf_element *p;

			p = v->uv_filter->vf_filter;
			while (p->vfe_next)
			    p = p->vfe_next;
			p->vfe_next = $2;
		    }

				}
	| ACCEPT		{

		warn("Expected filter spec after accept keyword, ignored");

				}
	| DENY filter		{

		    if (v->uv_filter == NULL) {
			struct vif_filter *v_filter;

			v_filter = (struct vif_filter *)malloc(sizeof(struct vif_filter));
			if (v_filter == NULL)
			    fatal("out of memory");
			v_filter->vf_flags = 0;
			v_filter->vf_type = VFT_DENY;
			v_filter->vf_filter = $2;
			v->uv_filter = v_filter;
		    } else if (v->uv_filter->vf_type != VFT_DENY) {
			fatal("can't accept and deny");
		    } else {
			struct vf_element *p;

			p = v->uv_filter->vf_filter;
			while (p->vfe_next)
			    p = p->vfe_next;
			p->vfe_next = $2;
		    }

				}
	| DENY			{

		warn("Expected filter spec after deny keyword, ignored");

				}
	| BIDIR			{

		    if (v->uv_filter == NULL) {
			fatal("bidir goes after filters");
		    }
		    v->uv_filter->vf_flags |= VFF_BIDIR;

				}
	;

interface	: ADDR		{ $$ = $1; }
	| STRING		{
				  $$ = valid_if($1);
				  if ($$ == 0)
					fatal("Invalid interface name %s",$1);
				}
	;

addrname	: ADDR		{ $$ = $1; }
	| STRING		{ struct hostent *hp;

				  if ((hp = gethostbyname($1)) == NULL ||
					hp->h_length != sizeof($$))
				    fatal("No such host %s", $1);

				  if (hp->h_addr_list[1])
				    fatal("Hostname %s does not %s",
					$1, "map to a unique address");

				  bcopy(hp->h_addr_list[0], &$$,
					    hp->h_length);
				}

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

#ifdef ALLOW_BLACK_HOLES
			if (!allow_black_holes)
#endif
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

filter	:	filtlist	{ $$ = $1; }
	| STRING		{ fatal("named filters no implemented yet"); }
	;

filtlist	: filtelement	{ $$ = $1; }
	| filtelement filtlist	{ $1->vfe_next = $2; $$ = $1; }
	;

filtelement	: filtelem	{ $$ = $1; }
	| filtelem EXACT	{ $1->vfe_flags |= VFEF_EXACT; $$ = $1; }
	;

filtelem	: ADDRMASK	{

			struct vf_element *vfe;

			vfe = (struct vf_element *)malloc(sizeof(struct vf_element));
			if (vfe == NULL)
			    fatal("out of memory");

			vfe->vfe_addr = $1.addr;
			VAL_TO_MASK(vfe->vfe_mask, $1.mask);
			vfe->vfe_flags = 0;
			vfe->vfe_next = NULL;

			$$ = vfe;

				}
%%
#ifdef __STDC__
static void
fatal(char *fmt, ...)
{
	va_list ap;
	char buf[MAXHOSTNAMELEN + 100];

	va_start(ap, fmt);
#else
/*VARARGS1*/
static void
fatal(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list ap;
	char buf[MAXHOSTNAMELEN + 100];

	va_start(ap);
#endif
	vsnprintf(buf, sizeof(buf), fmt, ap);
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
	vsnprintf(buf, sizeof(buf), fmt, ap);
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
#ifdef SNMP
       if (*p == '"') {
          p++;
	       while (*p && *p != '"' && *p != '\n')
		      p++;		/* find next whitespace */
          if (*p == '"')
             p++;
       } else
#endif
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

/*
 * List of keywords.  Must have an empty record at the end to terminate
 * list.  If a second value is specified, the first is used at the beginning
 * of the file and the second is used while parsing interfaces (e.g. after
 * the first "phyint" or "tunnel" keyword).
 */
static struct keyword {
	char	*word;
	int	val1;
	int	val2;
} words[] = {
	{ "cache_lifetime",	CACHE_LIFETIME },
	{ "prune_lifetime",	PRUNE_LIFETIME,	PRUNE_LIFETIME2 },
	{ "pruning",		PRUNING },
	{ "phyint",		PHYINT },
	{ "tunnel",		TUNNEL },
	{ "disable",		DISABLE },
	{ "metric",		METRIC },
	{ "advert_metric",	ADVERT_METRIC },
	{ "threshold",		THRESHOLD },
	{ "rate_limit",		RATE_LIMIT },
	{ "force_leaf",		FORCE_LEAF },
	{ "srcrt",		SRCRT },
	{ "sourceroute",	SRCRT },
	{ "boundary",		BOUNDARY },
	{ "netmask",		NETMASK },
	{ "igmpv1",		IGMPV1 },
	{ "altnet",		ALTNET },
	{ "name",		NAME },
	{ "accept",		ACCEPT },
	{ "deny",		DENY },
	{ "exact",		EXACT },
	{ "bidir",		BIDIR },
	{ "allow_nonpruners",	ALLOW_NONPRUNERS },
#ifdef ALLOW_BLACK_HOLES
	{ "allow_black_holes",	BLACK_HOLE },
#endif
	{ "noflood",		NOFLOOD, NOFLOOD2},
	{ "notransit",		NOTRANSIT },
	{ "blaster",		BLASTER },
	{ "rexmit_prunes",	REXMIT_PRUNES, REXMIT_PRUNES2 },
	{ "passive",		PASSIVE },
	{ "beside",		BESIDE },
#ifdef SNMP
	{ "sysName",		SYSNAM },
	{ "sysContact",		SYSCONTACT },
	{ "sysVersion",		SYSVERSION },
	{ "sysLocation",	SYSLOCATION },
#endif
	{ NULL,			0 }
};


static int
yylex()
{
	int n;
	u_int32 addr;
	char *q;
	struct keyword *w;

	if ((q = next_word()) == NULL) {
		return 0;
	}

	for (w = words; w->word; w++)
		if (!strcmp(q, w->word))
		    return (state && w->val2) ? w->val2 : w->val1;

	if (!strcmp(q,"on") || !strcmp(q,"yes")) {
		yylval.num = 1;
		return BOOLEAN;
	}
	if (!strcmp(q,"off") || !strcmp(q,"no")) {
		yylval.num = 0;
		return BOOLEAN;
	}
	if (!strcmp(q,"default")) {
		yylval.addrmask.mask = 0;
		yylval.addrmask.addr = 0;
		return ADDRMASK;
	}
	if (sscanf(q,"%[.0-9]/%d%c",s1,&n,s2) == 2) {
		if ((addr = inet_parse(s1,1)) != 0xffffffff) {
			yylval.addrmask.mask = n;
			yylval.addrmask.addr = addr;
			return ADDRMASK;
		}
		/* fall through to returning STRING */
	}
	if (sscanf(q,"%[.0-9]%c",s1,s2) == 1) {
		if ((addr = inet_parse(s1,4)) != 0xffffffff &&
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
#ifdef SNMP
   if (*q=='"') {
      if (q[ strlen(q)-1 ]=='"')
         q[ strlen(q)-1 ]='\0'; /* trash trailing quote */
      yylval.ptr = q+1;
      return STRING;
   }
#endif
	yylval.ptr = q;
	return STRING;
}

void
config_vifs_from_file()
{
	order = 0;
	state = 0;
	numbounds = 0;
	lineno = 0;

	if ((f = fopen(configfilename, "r")) == NULL) {
	    if (errno != ENOENT)
		log(LOG_ERR, errno, "can't open %s", configfilename);
	    return;
	}

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

static const char *
ifconfaddr(u_int32_t a)
{
    static char ifname[IFNAMSIZ];
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) != 0)
	return (NULL);

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	if (ifa->ifa_addr->sa_family == AF_INET &&
	    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == a) {
	    strlcpy(ifname, ifa->ifa_name, sizeof(ifname));
	    freeifaddrs(ifap);
	    return (ifname);
	}
    }

    freeifaddrs(ifap);
    return (NULL);
}

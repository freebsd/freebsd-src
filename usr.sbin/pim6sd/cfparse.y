/*
 * Copyright (C) 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/cfparse.y,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */
%{
#include <sys/types.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <string.h>
#include <syslog.h>

#include "defs.h"
#include "rp.h"
#include "vif.h"

#include "var.h"
#include "vmbuf.h"
#include "cfparse.h"
#include "debug.h"
#include "pimd.h"
#include "timer.h"
#include "inet6.h"

#define set_param(var,val,p) \
	do {\
		if ((var) != -1) {\
			yywarn("%s doubly defined(ignore %d)", (p), (val));\
		}\
		else {\
			(var) = val;\
		}\
	} while(0)

struct in6_prefix {
	struct in6_addr paddr;
	int plen;
};

struct attr_list {
	struct attr_list *next;
	int type;
	union {
		unsigned int flags;
		double number;
		struct in6_prefix prefix;
	}attru;
};

enum {IFA_FLAG, IFA_PREFERENCE, IFA_METRIC, RPA_PRIORITY, RPA_TIME,
      BSRA_PRIORITY, BSRA_TIME, BSRA_MASKLEN, IN6_PREFIX, THRESA_RATE,
      THRESA_INTERVAL};

static int strict;		/* flag if the grammer check is strict */
static struct attr_list *rp_attr, *bsr_attr, *grp_prefix, *regthres_attr,
	*datathres_attr;
static char *cand_rp_ifname, *cand_bsr_ifname;
static int srcmetric, srcpref, helloperiod, jpperiod, granularity,
	datatimo, regsuptimo, probetime, asserttimo;
static double helloperiod_coef, jpperiod_coef;

static int debugonly;

extern int yylex __P((void));
%}

%union {
	unsigned long num;
	double fl;
	vchar_t val;
	struct attr_list *attr;
}

%token EOS
%token LOGGING LOGLEV NOLOGLEV
%token YES NO
%token REVERSELOOKUP
%token PHYINT IFNAME DISABLE PREFERENCE METRIC NOLISTENER
%token GRPPFX
%token CANDRP CANDBSR TIME PRIORITY MASKLEN
%token NUMBER STRING SLASH
%token REGTHRES DATATHRES RATE INTERVAL
%token SRCMETRIC SRCPREF HELLOPERIOD GRANULARITY JPPERIOD
%token DATATIME REGSUPTIME PROBETIME ASSERTTIME

%type <num> LOGLEV NOLOGLEV
%type <fl> NUMBER
%type <val> STRING IFNAME
%type <attr> if_attributes rp_substatement rp_attributes
%type <attr> bsr_substatement bsr_attributes thres_attributes

%%
statements:
		/* empty */
	|	statements statement
	;

statement:
		logging_statement
	|	reverselookup_statement
	|	phyint_statement
	|	candrp_statement
	|	candbsr_statement
	|	grppfx_statement
	|	regthres_statement
	|	datathres_statement
	|	param_statement
	;

/* logging */
logging_statement:
	LOGGING log_specs EOS
	;

log_specs:
		/* empty */
	|	log_specs LOGLEV {debug |= $2;}
	|	log_specs NOLOGLEV {debug &= ~($2);}
	;

/* reverselookup */
reverselookup_statement:
		REVERSELOOKUP YES EOS { numerichost = FALSE; }
	|	REVERSELOOKUP NO EOS { numerichost = TRUE; }
	;

/* phyint */
phyint_statement:
	PHYINT IFNAME if_attributes EOS {
		struct uvif *v;

		v = find_vif($2.v);
		free($2.v);	/* XXX */
		if (v == NULL) {
			yywarn("unknown interface: %s", $2.v);
			free_attr_list($3);
			if (strict)
				return(-1);
		}
		else {
			struct attr_list *p;

			for (p = (struct attr_list *)v->config_attr;
			     p && p->next; p = p->next)
				;
			if (p)
				p->next = (void *)$3;
			else
				v->config_attr = (void *)$3;
		}
	}
	;

if_attributes:
		{ $$ = NULL; }
	|	if_attributes DISABLE
		{
			if (($$ = add_attribute_flag($1, IFA_FLAG,
						     VIFF_DISABLED)) == NULL)
				return(-1);
		}
	|	if_attributes NOLISTENER
		{
			if (($$ = add_attribute_flag($1, IFA_FLAG,
						     VIFF_NOLISTENER)) == NULL)
				return(-1);
		}
	|	if_attributes PREFERENCE NUMBER
		{
			if (($$ = add_attribute_num($1, IFA_PREFERENCE, $3))
			    == NULL)
				return(-1);
		}
	|	if_attributes METRIC NUMBER
		{
			if (($$ = add_attribute_num($1, IFA_METRIC, $3))
			    == NULL)
				return(-1);
		}
	;

/* cand_rp */
candrp_statement:
	CANDRP rp_substatement EOS {
		if (cand_rp_flag == TRUE) {
			yywarn("cand_rp doubly defined");
			free_attr_list($2);
			if (strict)
				return(-1);
		}
		else {
			cand_rp_flag = TRUE;
			rp_attr = $2;
		}
	}
	;
/* XXX: intermediate rule to avoid shift-reduce conflict */
rp_substatement:
		IFNAME rp_attributes
		{
			if (cand_rp_ifname) {
				yywarn("ifname for cand_rp doubly defined");
				if (strict)
					return(-1);
			}
			else
				cand_rp_ifname = $1.v;
			$$ = $2;
		}
	|	rp_attributes
	;
rp_attributes:
		{ $$ = NULL; }
	|	rp_attributes PRIORITY NUMBER
		{
			if (($$ = add_attribute_num($1, RPA_PRIORITY, $3))
			    == NULL)
				return(-1);
		}
	|	rp_attributes TIME NUMBER
		{
			if (($$ = add_attribute_num($1, RPA_TIME, $3))
			    == NULL)
				return(-1);
		}
	;

/* cand_bootstrap_router */
candbsr_statement:
	CANDBSR bsr_substatement  EOS {
		if (cand_bsr_flag == TRUE) {
			yywarn("cand_bsr doubly defined");
			free_attr_list($2);
			if (strict)
				return(-1);
		}
		else {
			cand_bsr_flag = TRUE;
			bsr_attr = $2;
		}
	}
	;
/* XXX: intermediate rule to avoid shift-reduce conflict */
bsr_substatement:
		IFNAME bsr_attributes
		{
			if (cand_bsr_ifname) {
				yywarn("ifname for cand_bsr doubly defined");
				if (strict)
					return(-1);
			}
			else
				cand_bsr_ifname = $1.v;
			$$ = $2;
		}
	|	bsr_attributes
	;

bsr_attributes:
		{ $$ = NULL; }
	|	bsr_attributes PRIORITY NUMBER
		{
			if (($$ = add_attribute_num($1, BSRA_PRIORITY, $3))
			    == NULL)
				return(-1);
		}
	|	bsr_attributes TIME NUMBER
		{
			if (($$ = add_attribute_num($1, BSRA_TIME, $3))
			    == NULL)
				return(-1);
		}
	|	bsr_attributes MASKLEN NUMBER
		{
			int masklen = $3;

			if (masklen < 0 || masklen > 128)
				yywarn("invalid mask length: %d (ignored)",
				       masklen);
			else if (($$ = add_attribute_num($1, BSRA_MASKLEN,
							 masklen))
				 == NULL)
				return(-1);
		}
	;

/* group_prefix <group-addr>/<prefix_len> */
grppfx_statement:
	GRPPFX STRING SLASH NUMBER EOS {
		struct in6_prefix prefix;
		int prefixok = 1;

		if (inet_pton(AF_INET6, $2.v, &prefix.paddr) != 1) {
			yywarn("invalid IPv6 address: %s (ignored)", $2);
			prefixok = 0;
		}
		free($2.v);	/* XXX: which was allocated dynamically */

		prefix.plen = $4;
		if (prefix.plen < 0 || prefix.plen > 128) {
			yywarn("invalid prefix length: %d (ignored)",
			       prefix.plen);
			prefixok = 0;
		}
		if (IN6_IS_ADDR_MC_NODELOCAL(&prefix.paddr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&prefix.paddr)) {
			yywarn("group prefix (%s/%d) has a narrow scope "
			       "(ignored)",
			       inet6_fmt(&prefix.paddr), prefix.plen);
			prefixok = 0;
		}

		if (prefixok) {
			struct attr_list *new;

			if ((new = malloc(sizeof(*new))) == NULL) {
				yyerror("malloc failed");
				return(NULL);
			}
			memset(new, 0, sizeof(*new));

			new->type = IN6_PREFIX;
			new->attru.prefix = prefix;
			new->next = grp_prefix;
 
			grp_prefix = new;
		}
	}
	;

/*
 * switch_register_threshold [rate <number> interval <number>]
 * Operation: reads and assigns the switch to the spt threshold
 * due to registers for the router, if used as RP.
 * Maybe extended to support different thresholds for different
 * groups(prefixes).
 */
regthres_statement:
	REGTHRES thres_attributes EOS {
		if (regthres_attr) {
			yywarn("switch_register_threshold doubly defined");
			free_attr_list($2);
			if (strict)
				return(-1);
		}
		else
			regthres_attr = $2;
	}
	;

thres_attributes:
		{ $$ = NULL; }
	|	thres_attributes RATE NUMBER
		{
			if (($$ = add_attribute_num($1, THRESA_RATE, $3))
			    == NULL)
				return(-1);
		}
	|	thres_attributes INTERVAL NUMBER
		{
			if (($$ = add_attribute_num($1, THRESA_INTERVAL, $3))
			    == NULL)
				return(-1);
		}

/*  
 * switch_data_threshold [rate <number> interval <number>]
 * Operation: reads and assigns the switch to the spt threshold due to
 * data packets, if used as DR.
 */
datathres_statement:
	DATATHRES thres_attributes EOS {
		if (datathres_attr) {
			yywarn("switch_data_threshold doubly defined");
			free_attr_list($2);
			if (strict)
				return(-1);
		}
		else
			datathres_attr = $2;
	}
	;

param_statement:
		SRCMETRIC NUMBER EOS
		{
			set_param(srcmetric, $2, "default_source_metric");
		}
	|	SRCPREF NUMBER EOS
		{
			set_param(srcpref, $2, "default_source_preference");
		}
	|	HELLOPERIOD NUMBER EOS
		{
			set_param(helloperiod, $2, "hello_period");
		}
	|	HELLOPERIOD NUMBER NUMBER EOS
		{
			set_param(helloperiod, $2, "hello_period");
			set_param(helloperiod_coef, $3, "hello_period(coef)");
		}
	|	JPPERIOD NUMBER EOS
		{
			set_param(jpperiod, $2, "join_prune_period");
		}
	|	JPPERIOD NUMBER NUMBER EOS
		{
			set_param(jpperiod, $2, "join_prune_period");
			set_param(jpperiod_coef, $3, "join_prune_period(coef)");
		}
	|	GRANULARITY NUMBER EOS
		{
			set_param(granularity, $2, "granularity");
		}
	|	DATATIME NUMBER EOS
		{
			set_param(datatimo, $2, "data_timeout");
		}
	|	REGSUPTIME NUMBER EOS
		{
			set_param(regsuptimo, $2, "register_suppression_timeout");
		}
	|	PROBETIME NUMBER EOS
		{
			set_param(probetime, $2, "probe_time");
		}
	|	ASSERTTIME NUMBER EOS
		{
			set_param(asserttimo, $2, "assert_timeout");
		}
	;
%%

static struct attr_list *add_attribute_flag __P((struct attr_list *, int,
	unsigned int));
static struct attr_list *add_attribute_num __P((struct attr_list *, int,
	double));
static void free_attr_list __P((struct attr_list *));
static int param_config __P((void));
static int phyint_config __P((void));
static int rp_config __P((void));
static int bsr_config __P((void));
static int grp_prefix_config __P((void));
static int regthres_config __P((void));
static int datathres_config __P((void));

static struct attr_list *
add_attribute_flag(list, type, flag)
	struct attr_list *list;
	int type;
	unsigned int flag;
{
	struct attr_list *p;
	
	if ((p = malloc(sizeof(*p))) == NULL) {
		yyerror("malloc failed");
		return(NULL);
	}
	memset((void *)p, 0, sizeof(*p));
	p->type = type;
	p->attru.flags = flag;
	p->next = list;

	return(p);
}

/* XXX: too many dup code... */
static struct attr_list *
add_attribute_num(list, type, num)
	struct attr_list *list;
	int type;
	double num;
{
	struct attr_list *p;
	
	if ((p = malloc(sizeof(*p))) == NULL) {
		yyerror("malloc failed");
		return(NULL);
	}
	memset((void *)p, 0, sizeof(*p));
	p->type = type;
	p->attru.number = num;
	p->next = list;

	return(p);
}

static void
free_attr_list(list)
	struct attr_list *list;
{
	struct attr_list *p, *next;

	for(p = list; p; p = next) {
		next = p->next;
		free(p);
	}
}

static int
param_config()
{
	struct uvif *v;
	vifi_t vifi;

	/* at first, set the default values to all the undefined variables */
	if (srcmetric == -1) srcmetric = DEFAULT_LOCAL_METRIC;
	if (srcpref == -1) srcpref = DEFAULT_LOCAL_PREF;
	if (helloperiod == -1) helloperiod = PIM_TIMER_HELLO_PERIOD;
	if (helloperiod_coef == -1) helloperiod_coef = 3.5;
	if (jpperiod == -1) jpperiod = PIM_JOIN_PRUNE_PERIOD;
	if (jpperiod_coef == -1) jpperiod_coef = 3.5;
	if (granularity == -1) granularity = DEFAULT_TIMER_INTERVAL;
	if (datatimo == -1) datatimo = PIM_DATA_TIMEOUT;
	if (regsuptimo == -1) regsuptimo = PIM_REGISTER_SUPPRESSION_TIMEOUT;
	if (probetime == -1) probetime = PIM_REGISTER_PROBE_TIME;
	if (asserttimo == -1) asserttimo = PIM_ASSERT_TIMEOUT;

	/* set protocol parameters using the configuration variables */
	for (vifi = 0, v = uvifs; vifi < MAXVIFS; ++vifi, ++v) {
		v->uv_local_metric = srcmetric;
		v->uv_local_pref = srcpref;
	}
	pim_hello_period = helloperiod;
	pim_hello_holdtime = helloperiod * helloperiod_coef;
	pim_join_prune_period = jpperiod;
	pim_join_prune_holdtime = jpperiod * jpperiod_coef;
	timer_interval = granularity;
	pim_data_timeout = datatimo;
	pim_register_suppression_timeout = regsuptimo;
	pim_register_probe_time = probetime;
	pim_assert_timeout = asserttimo;

	IF_DEBUG(DEBUG_PIM_HELLO) {
		log(LOG_DEBUG, 0, "pim_hello_period set to: %u",
		    pim_hello_period);
		log(LOG_DEBUG, 0, "pim_hello_holdtime set to: %u",
		    pim_hello_holdtime);
	}

	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE) {
		log(LOG_DEBUG,0 , "pim_join_prune_period set to: %u",
		    pim_join_prune_period);
		log(LOG_DEBUG, 0, "pim_join_prune_holdtime set to: %u",
		    pim_join_prune_holdtime);
	}
	IF_DEBUG(DEBUG_TIMER) {
		log(LOG_DEBUG,0 , "timer interval set to: %u", timer_interval);
	}
	IF_DEBUG(DEBUG_PIM_TIMER) {
		log(LOG_DEBUG,0 , "PIM data timeout set to: %u",
		    pim_data_timeout);
	}
	IF_DEBUG(DEBUG_PIM_REGISTER) {
		log(LOG_DEBUG, 0,
		    "PIM register suppression timeout set to: %u",
		    pim_register_suppression_timeout);
		log(LOG_DEBUG, 0, "PIM register probe time set to: %u",
		    pim_register_probe_time);
	}
	IF_DEBUG(DEBUG_PIM_ASSERT) {
		log(LOG_DEBUG, 0,
		    "PIM assert timeout set to: %u",
		    pim_assert_timeout);
	}
	return(0);
}

static int
phyint_config()
{
	struct uvif *v;
	vifi_t vifi;
	struct attr_list *al;
	
	for (vifi = 0, v = uvifs; vifi < numvifs ; ++vifi , ++v) {
		for (al = (struct attr_list *)v->config_attr; al; al = al->next) {
			switch(al->type) {
			case IFA_FLAG:
				v->uv_flags |= al->attru.flags;
				break;
			case IFA_PREFERENCE:
				if (al->attru.number < 1 ||
				    al->attru.number > 255)
					yywarn("invalid phyint preference(%d)",
					       (int)al->attru.number);
				else {
					v->uv_local_pref = al->attru.number;
					IF_DEBUG(DEBUG_ASSERT)
						log(LOG_DEBUG, 0,
						    "default localpref for %s "
						    "is %d",
						    v->uv_name,
						    v->uv_local_pref);
				}
				break;
			case IFA_METRIC:
				if (al->attru.number < 1 ||
				    al->attru.number > 1024)
					yywarn("invalid metric(%d)",
					       al->attru.number);
				else {
					v->uv_metric = al->attru.number;
					IF_DEBUG(DEBUG_ASSERT)
						log(LOG_DEBUG, 0,
						    "default local metric for %s "
						    "is %d",
						    v->uv_name,
						    v->uv_metric);
				}
				break;
			}
		}
	}

	return(0);
}

static int
rp_config()
{
	struct sockaddr_in6 *sa6_rp = NULL;
	struct attr_list *al;
	u_int8 *data_ptr;

	/* initialization by default values */
	my_cand_rp_adv_period = PIM_DEFAULT_CAND_RP_ADV_PERIOD;
	my_cand_rp_priority = PIM_DEFAULT_CAND_RP_PRIORITY;

	if (cand_rp_ifname) {
		sa6_rp = local_iface(cand_rp_ifname);
		if (!sa6_rp)
			log(LOG_WARNING, 0,
			    "cand_rp '%s' is not configured. "
			    "take the max local address the router..",
			    cand_rp_ifname);
	}

	for (al = rp_attr; al; al = al->next) {
		switch(al->type) {
		case RPA_PRIORITY:
			if (al->attru.number < 0)
				my_cand_rp_priority =
					PIM_DEFAULT_CAND_RP_PRIORITY;
			else
				my_cand_rp_priority = al->attru.number;
			break;
		case RPA_TIME:
			if (al->attru.number < 10)
				my_cand_rp_adv_period = 10;
			else if (al->attru.number > PIM_DEFAULT_CAND_RP_ADV_PERIOD)
				my_cand_rp_adv_period =
					PIM_DEFAULT_CAND_RP_ADV_PERIOD;
			else
				my_cand_rp_adv_period = al->attru.number;
			break;
		default:
			yywarn("unknown attribute(%d) for RP", al->type);
			break;
		}
	}

	if (!sa6_rp)
		sa6_rp = max_global_address(); /* this MUST suceed */
	my_cand_rp_address = *sa6_rp;

	/*
	 * initialize related parameters
	 */

	/*
	 * Note that sizeof(pim6_enocd_uni_addr_t) might be larger than
	 * the length of the Encoded-Unicast-address field(18 byte) due to
	 * some padding put in the compiler. However, it doesn't matter
	 * since we use the space just as a buffer(i.e not as the message).
	 */
	cand_rp_adv_message.buffer = (u_int8 *)malloc(4 +
						      sizeof(pim6_encod_uni_addr_t) +
						      255*sizeof(pim6_encod_grp_addr_t));
	if(cand_rp_adv_message.buffer == NULL)
		log(LOG_ERR, 0, "Candrpadv Buffer allocation");

	cand_rp_adv_message.prefix_cnt_ptr = cand_rp_adv_message.buffer;

	/*
	 * By default, if no group_prefix configured, then prefix_cnt == 0
	 * implies group_prefix = ff00::/8 and masklen = 8.
	 */
	*cand_rp_adv_message.prefix_cnt_ptr = 0;
	cand_rp_adv_message.insert_data_ptr = cand_rp_adv_message.buffer;

	/* TODO: XXX: HARDCODING!!! */
	cand_rp_adv_message.insert_data_ptr += (4 + 18);
	cand_rp_adv_message.message_size =
		cand_rp_adv_message.insert_data_ptr - cand_rp_adv_message.buffer;

	my_cand_rp_holdtime = 2.5 * my_cand_rp_adv_period;

	/* TODO: HARDCODING! */
	data_ptr = cand_rp_adv_message.buffer + 1;	/* WARNING */
	PUT_BYTE(my_cand_rp_priority,data_ptr);
	PUT_HOSTSHORT(my_cand_rp_holdtime, data_ptr);
	PUT_EUADDR6(my_cand_rp_address.sin6_addr,data_ptr);
	IF_DEBUG(DEBUG_PIM_CAND_RP) {
		log(LOG_DEBUG, 0,
		    "Local Cand-RP address is : %s",
		    inet6_fmt(&my_cand_rp_address.sin6_addr));
		log(LOG_DEBUG, 0,
		    "Local Cand-RP priority is : %u",my_cand_rp_priority);
		log(LOG_DEBUG, 0,
		    "Local Cand-RP advertisement period is : %u sec.",
		    my_cand_rp_adv_period);
	}

	return(0);
}

static int
bsr_config()
{
	struct sockaddr_in6 *sa6_bsr = NULL;
	struct attr_list *al;
	int my_bsr_hash_masklen;

	/* initialization by default values */
	my_bsr_period = PIM_DEFAULT_BOOTSTRAP_PERIOD;
	my_bsr_priority = PIM_DEFAULT_BSR_PRIORITY;
	my_bsr_hash_masklen = RP_DEFAULT_IPV6_HASHMASKLEN;

	if (cand_bsr_ifname) {
		sa6_bsr = local_iface(cand_bsr_ifname);
		if (!sa6_bsr)
			log(LOG_WARNING, 0,
			    "bsr '%s' is not configured. "
			    "take the max local address the router..",
			    cand_bsr_ifname);
	}

	for (al = bsr_attr; al; al = al->next) {
		switch(al->type) {
		case BSRA_PRIORITY:
			if (al->attru.number >= 0)
				my_bsr_priority = al->attru.number;
			break;
		case BSRA_MASKLEN:
			/* validation has been done. */
			my_bsr_hash_masklen = al->attru.number;
			break;
		case BSRA_TIME:
			if (al->attru.number < 10)
				my_bsr_period = 10;
			else if (al->attru.number > PIM_DEFAULT_BOOTSTRAP_PERIOD)
				my_bsr_period =
					PIM_DEFAULT_BOOTSTRAP_PERIOD;
			else
				my_bsr_period = al->attru.number;
			break;
		default:
			yywarn("unknown attribute(%d) for BSR", al->type);
			break;
		}
	}

	if (!sa6_bsr)
		sa6_bsr = max_global_address(); /* this MUST suceed */
	my_bsr_address = *sa6_bsr;
	MASKLEN_TO_MASK6(my_bsr_hash_masklen, my_bsr_hash_mask);

	IF_DEBUG(DEBUG_PIM_BOOTSTRAP) {
		log(LOG_DEBUG, 0, "Local BSR address: %s",
		    inet6_fmt(&my_bsr_address.sin6_addr));
		log(LOG_DEBUG, 0, "Local BSR priority : %u", my_bsr_priority);
		log(LOG_DEBUG, 0, "Local BSR period is : %u sec.",
		    my_bsr_period);
		log(LOG_DEBUG, 0, "Local BSR hash mask length: %d",
		    my_bsr_hash_masklen);
	}

	return(0);
}

static int
grp_prefix_config()
{
	struct attr_list *pl;

	if (cand_rp_flag != TRUE) {
		log(LOG_WARNING, 0,
		    "group_prefix was specified without cand_rp(ignored)");
		return(0);
	}

	for (pl = grp_prefix; pl; pl = pl->next) {
		if (!IN6_IS_ADDR_MULTICAST(&pl->attru.prefix.paddr)) {
			log(LOG_WARNING, 0,
			    "Config error: %s is not a mulicast address(ignored)",
			    inet6_fmt(&pl->attru.prefix.paddr));
			continue;
		}

		if (!(~(*cand_rp_adv_message.prefix_cnt_ptr))) {
			log(LOG_WARNING, 0,
			    "Too many group_prefix configured. Truncating...");
			break;
		}

		/* validation for plen has almost done */
		if (pl->attru.prefix.plen < PIM_GROUP_PREFIX_DEFAULT_MASKLEN)
			pl->attru.prefix.plen = PIM_GROUP_PREFIX_DEFAULT_MASKLEN;

		PUT_EGADDR6(pl->attru.prefix.paddr,
			    (u_int8)pl->attru.prefix.plen, 0,
			    cand_rp_adv_message.insert_data_ptr);
		(*cand_rp_adv_message.prefix_cnt_ptr)++;
	}

	/* finally, adjust the data size */
	cand_rp_adv_message.message_size =
		cand_rp_adv_message.insert_data_ptr - cand_rp_adv_message.buffer;

	return(0);
}

static int
regthres_config()
{
	struct attr_list *al;
	int rate = -1;
	int interval = -1;

	if (cand_rp_flag != TRUE) {
		log(LOG_WARNING, 0,
		    "register_threshold was specified without cand_rp");
	}

	for (al = regthres_attr; al; al = al->next) {
		switch(al->type) {
		case THRESA_RATE:
			if (al->attru.number < 0)
				yywarn("invalid regthres rate: %d(ignored)",
				       al->attru.number);
			else if (rate != -1)
				yywarn("regthres rate is doubly defined(ignored)");
			else
				rate = al->attru.number;
			break;
		case THRESA_INTERVAL:
			if (al->attru.number < 0)
				yywarn("invalid regthres interval: %d(ignored)",
				       al->attru.number);
			else if (interval != -1)
				yywarn("regthres interval is doubly defined(ignored)");
			else
				interval = al->attru.number;
			break;
		default:
			yywarn("unknown attribute(%d) for regthres", al->type);
			break;
		}
	}

	/* set default values if not specified */
	if (rate == -1)
		rate = PIM_DEFAULT_REG_RATE;
	if (interval == -1)
		interval = PIM_DEFAULT_REG_RATE_INTERVAL;

	pim_reg_rate_bytes = (rate * interval ) /10;
	pim_reg_rate_check_interval = interval;

	return(0);
}

static int
datathres_config()
{
	struct attr_list *al;
	int rate = -1;
	int interval = -1;

	for (al = datathres_attr; al; al = al->next) {
		switch(al->type) {
		case THRESA_RATE:
			if (al->attru.number < 0)
				yywarn("invalid datathres rate: %d(ignored)",
				       al->attru.number);
			else if (rate != -1)
				yywarn("datathres rate is doubly defined(ignored)");
			else
				rate = al->attru.number;
			break;
		case THRESA_INTERVAL:
			if (al->attru.number < 0)
				yywarn("invalid datathres interval: %d(ignored)",
				       al->attru.number);
			else if (interval != -1)
				yywarn("datathres interval is doubly defined(ignored)");
			else
				interval = al->attru.number;
			break;
		default:
			yywarn("unknown attribute(%d) for datathres", al->type);
			break;
		}
	}

	/* set default values if not specified */
	if (rate == -1)
		rate = PIM_DEFAULT_DATA_RATE;
	if (interval == -1)
		interval = PIM_DEFAULT_DATA_RATE_INTERVAL;

	pim_data_rate_bytes = (rate * interval ) /10;
	pim_data_rate_check_interval = interval;

	return(0);
}

int
cf_post_config()
{
	struct uvif *v;
	vifi_t vifi;

	if (debugonly)
		goto cleanup;

	param_config();		/* must be called before phyint_conifg() */

	phyint_config();

	if (cand_bsr_flag == TRUE)
		bsr_config();

	if (cand_rp_flag == TRUE)
		rp_config();

	if (grp_prefix)		/* this must be called after rp_config() */
		grp_prefix_config();

	if (cand_rp_flag == TRUE)
		regthres_config();

	datathres_config();

	IF_DEBUG(DEBUG_SWITCH) {
		log(LOG_DEBUG, 0, "reg_rate_limit set to %u (bits/s)",
		    pim_reg_rate_bytes);
		log(LOG_DEBUG, 0, "reg_rate_interval set to  %u s.",
		    pim_reg_rate_check_interval);
		log(LOG_DEBUG, 0, "data_rate_limit set to %u (bits/s)",
		    pim_data_rate_bytes);
		log(LOG_DEBUG, 0, "data_rate_interval set to %u s.",
		    pim_data_rate_check_interval);
	}

  cleanup:
	/* cleanup temporary variables */
	if (cand_rp_ifname) free(cand_rp_ifname);
	if (cand_bsr_ifname) free(cand_bsr_ifname);
	if (rp_attr) free_attr_list(rp_attr);
	if (bsr_attr) free_attr_list(bsr_attr);
	if (grp_prefix) free_attr_list(grp_prefix);
	if (regthres_attr) free_attr_list(regthres_attr);
	if (datathres_attr) free_attr_list(datathres_attr);
	for (vifi = 0, v = uvifs; vifi < numvifs ; ++vifi , ++v)
		free_attr_list((struct attr_list *)v->config_attr);

	return(0);
}

/* initialize all the temporary variables */
void
cf_init(s, d)
{
	struct uvif *v;
	vifi_t vifi;

	strict = s;
	debugonly = d;

	debug = 0;

	rp_attr = bsr_attr = grp_prefix = regthres_attr	= datathres_attr = NULL;

	cand_rp_ifname = cand_bsr_ifname = NULL;

	srcmetric = srcpref = helloperiod = jpperiod = jpperiod_coef
		= granularity = datatimo = regsuptimo = probetime
		= asserttimo = -1;
	helloperiod_coef = jpperiod_coef = -1;

	for (vifi = 0, v = uvifs; vifi < numvifs ; ++vifi , ++v)
		v->config_attr = NULL;
}

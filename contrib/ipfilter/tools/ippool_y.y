/*	$FreeBSD$	*/

%{
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#if defined(BSD) && (BSD >= 199306)
# include <sys/cdefs.h>
#endif
#include <sys/ioctl.h>

#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>

#include "ipf.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_pool.h"
#include "netinet/ip_htable.h"
#include "ippool_l.h"
#include "kmem.h"

#define	YYDEBUG	1

extern	int	yyparse __P((void));
extern	int	yydebug;
extern	FILE	*yyin;

static	iphtable_t	ipht;
static	iphtent_t	iphte;
static	ip_pool_t	iplo;
static	ioctlfunc_t	poolioctl = NULL;
static	char		poolname[FR_GROUPLEN];

%}

%union	{
	char	*str;
	u_32_t	num;
	struct	in_addr	addr;
	struct	alist_s	*alist;
	struct	in_addr	adrmsk[2];
	iphtent_t	*ipe;
	ip_pool_node_t	*ipp;
	union	i6addr	ip6;
}

%token  <num>   YY_NUMBER YY_HEX
%token  <str>   YY_STR
%token	  YY_COMMENT 
%token	  YY_CMP_EQ YY_CMP_NE YY_CMP_LE YY_CMP_GE YY_CMP_LT YY_CMP_GT
%token	  YY_RANGE_OUT YY_RANGE_IN
%token  <ip6>   YY_IPV6

%token	IPT_IPF IPT_NAT IPT_COUNT IPT_AUTH IPT_IN IPT_OUT
%token	IPT_TABLE IPT_GROUPMAP IPT_HASH
%token	IPT_ROLE IPT_TYPE IPT_TREE
%token	IPT_GROUP IPT_SIZE IPT_SEED IPT_NUM IPT_NAME
%type	<num> role table inout
%type	<ipp> ipftree range addrlist
%type	<adrmsk> addrmask
%type	<ipe> ipfgroup ipfhash hashlist hashentry
%type	<ipe> groupentry setgrouplist grouplist
%type	<addr> ipaddr mask ipv4
%type	<str> number setgroup

%%
file:	line
	| assign
	| file line
	| file assign
	;

line:	table role ipftree eol		{ iplo.ipo_unit = $2;
					  iplo.ipo_list = $3;
					  load_pool(&iplo, poolioctl);
					  resetlexer();
					}
	| table role ipfhash eol	{ ipht.iph_unit = $2;
					  ipht.iph_type = IPHASH_LOOKUP;
					  load_hash(&ipht, $3, poolioctl);
					  resetlexer();
					}
	| groupmap role number ipfgroup eol
					{ ipht.iph_unit = $2;
					  strncpy(ipht.iph_name, $3,
						  sizeof(ipht.iph_name));
					  ipht.iph_type = IPHASH_GROUPMAP;
					  load_hash(&ipht, $4, poolioctl);
					  resetlexer();
					}
	| YY_COMMENT
	;

eol:	';'
	;

assign:	YY_STR assigning YY_STR ';'	{ set_variable($1, $3);
					  resetlexer();
					  free($1);
					  free($3);
					}
	;

assigning:
	'='				{ yyvarnext = 1; }
	;

table:	IPT_TABLE		{ bzero((char *)&ipht, sizeof(ipht));
				  bzero((char *)&iphte, sizeof(iphte));
				  bzero((char *)&iplo, sizeof(iplo));
				  *ipht.iph_name = '\0';
				  iplo.ipo_flags = IPHASH_ANON;
				  iplo.ipo_name[0] = '\0';
				}
	;

groupmap:
	IPT_GROUPMAP inout	{ bzero((char *)&ipht, sizeof(ipht));
				  bzero((char *)&iphte, sizeof(iphte));
				  *ipht.iph_name = '\0';
				  ipht.iph_unit = IPHASH_GROUPMAP;
				  ipht.iph_flags = $2;
				}
	;

inout:	IPT_IN				{ $$ = FR_INQUE; }
	| IPT_OUT			{ $$ = FR_OUTQUE; }
	;
role:
	IPT_ROLE '=' IPT_IPF		{ $$ = IPL_LOGIPF; }
	| IPT_ROLE '=' IPT_NAT		{ $$ = IPL_LOGNAT; }
	| IPT_ROLE '=' IPT_AUTH		{ $$ = IPL_LOGAUTH; }
	| IPT_ROLE '=' IPT_COUNT	{ $$ = IPL_LOGCOUNT; }
	;

ipftree:
	IPT_TYPE '=' IPT_TREE number start addrlist end
					{ strncpy(iplo.ipo_name, $4,
						  sizeof(iplo.ipo_name));
					  $$ = $6;
					}
	;

ipfhash:
	IPT_TYPE '=' IPT_HASH number hashopts start hashlist end
					{ strncpy(ipht.iph_name, $4,
						  sizeof(ipht.iph_name));
					  $$ = $7;
					}
	;

ipfgroup:
	setgroup hashopts start grouplist end
					{ iphtent_t *e;
					  for (e = $4; e != NULL;
					       e = e->ipe_next)
						if (e->ipe_group[0] == '\0')
							strncpy(e->ipe_group,
								$1,
								FR_GROUPLEN);
					  $$ = $4;
					}
	| hashopts start setgrouplist end		{ $$ = $3; }
	;

number:	IPT_NUM '=' YY_NUMBER			{ sprintf(poolname, "%u", $3);
						  $$ = poolname;
						}
	| IPT_NAME '=' YY_STR			{ $$ = $3; }
	|					{ $$ = ""; }
	;

setgroup:
	IPT_GROUP '=' YY_STR		{ char tmp[FR_GROUPLEN+1];
					  strncpy(tmp, $3, FR_GROUPLEN);
					  $$ = strdup(tmp);
					}
	| IPT_GROUP '=' YY_NUMBER	{ char tmp[FR_GROUPLEN+1];
					  sprintf(tmp, "%u", $3);
					  $$ = strdup(tmp);
					}
	;

hashopts:
	| size
	| seed
	| size seed
	;

addrlist:
	next				{ $$ = NULL; }
	| range next addrlist		{ $1->ipn_next = $3; $$ = $1; }
	| range next			{ $$ = $1; }
	;

grouplist:
	next				{ $$ = NULL; }
	| groupentry next grouplist	{ $$ = $1; $1->ipe_next = $3; }
	| addrmask next grouplist	{ $$ = calloc(1, sizeof(iphtent_t));
					  bcopy((char *)&($1[0]),
						(char *)&($$->ipe_addr),
						sizeof($$->ipe_addr));
					  bcopy((char *)&($1[1]),
						(char *)&($$->ipe_mask),
						sizeof($$->ipe_mask));
					  $$->ipe_next = $3;
					}
	| groupentry next		{ $$ = $1; }
	| addrmask next			{ $$ = calloc(1, sizeof(iphtent_t));
					  bcopy((char *)&($1[0]),
						(char *)&($$->ipe_addr),
						sizeof($$->ipe_addr));
					  bcopy((char *)&($1[1]),
						(char *)&($$->ipe_mask),
						sizeof($$->ipe_mask));
					}
	;

setgrouplist:
	next				{ $$ = NULL; }
	| groupentry next		{ $$ = $1; }
	| groupentry next setgrouplist	{ $1->ipe_next = $3; $$ = $1; }
	;

groupentry:
	addrmask ',' setgroup		{ $$ = calloc(1, sizeof(iphtent_t));
					  bcopy((char *)&($1[0]),
						(char *)&($$->ipe_addr),
						sizeof($$->ipe_addr));
					  bcopy((char *)&($1[1]),
						(char *)&($$->ipe_mask),
						sizeof($$->ipe_mask));
					  strncpy($$->ipe_group, $3,
						  FR_GROUPLEN);
					  free($3);
					}
	;

range:	addrmask	{ $$ = calloc(1, sizeof(*$$));
			  $$->ipn_info = 0;
			  $$->ipn_addr.adf_len = sizeof($$->ipn_addr);
			  $$->ipn_addr.adf_addr.in4.s_addr = $1[0].s_addr;
			  $$->ipn_mask.adf_len = sizeof($$->ipn_mask);
			  $$->ipn_mask.adf_addr.in4.s_addr = $1[1].s_addr;
			}
	| '!' addrmask	{ $$ = calloc(1, sizeof(*$$));
			  $$->ipn_info = 1;
			  $$->ipn_addr.adf_len = sizeof($$->ipn_addr);
			  $$->ipn_addr.adf_addr.in4.s_addr = $2[0].s_addr;
			  $$->ipn_mask.adf_len = sizeof($$->ipn_mask);
			  $$->ipn_mask.adf_addr.in4.s_addr = $2[1].s_addr;
			}

hashlist:
	next				{ $$ = NULL; }
	| hashentry next		{ $$ = $1; }
	| hashentry next hashlist	{ $1->ipe_next = $3; $$ = $1; }
	;

hashentry:
	addrmask 			{ $$ = calloc(1, sizeof(iphtent_t));
					  bcopy((char *)&($1[0]),
						(char *)&($$->ipe_addr),
						sizeof($$->ipe_addr));
					  bcopy((char *)&($1[1]),
						(char *)&($$->ipe_mask),
						sizeof($$->ipe_mask));
					}
	;

addrmask:
	ipaddr '/' mask		{ $$[0] = $1; $$[1].s_addr = $3.s_addr;
				  yyexpectaddr = 0;
				}
	| ipaddr		{ $$[0] = $1; $$[1].s_addr = 0xffffffff;
				  yyexpectaddr = 0;
				}
	;

ipaddr:	ipv4			{ $$ = $1; }
	| YY_NUMBER		{ $$.s_addr = htonl($1); }
	| YY_STR		{ if (gethost($1, &($$.s_addr)) == -1)
					yyerror("Unknown hostname");
				}
	;

mask:	YY_NUMBER		{ ntomask(4, $1, (u_32_t *)&$$.s_addr); }
	| ipv4			{ $$ = $1; }
	;

start:	'{'			{ yyexpectaddr = 1; }
	;

end:	'}'			{ yyexpectaddr = 0; }
	;

next:	';'			{ yyexpectaddr = 1; }
	;

size:	IPT_SIZE '=' YY_NUMBER	{ ipht.iph_size = $3; }
	;

seed:	IPT_SEED '=' YY_NUMBER	{ ipht.iph_seed = $3; }
	;

ipv4:	YY_NUMBER '.' YY_NUMBER '.' YY_NUMBER '.' YY_NUMBER
		{ if ($1 > 255 || $3 > 255 || $5 > 255 || $7 > 255) {
			yyerror("Invalid octet string for IP address");
			return 0;
		  }
		  $$.s_addr = ($1 << 24) | ($3 << 16) | ($5 << 8) | $7;
		  $$.s_addr = htonl($$.s_addr);
		}
	;
%%
static	wordtab_t	yywords[] = {
	{ "auth",	IPT_AUTH },
	{ "count",	IPT_COUNT },
	{ "group",	IPT_GROUP },
	{ "group-map",	IPT_GROUPMAP },
	{ "hash",	IPT_HASH },
	{ "in",		IPT_IN },
	{ "ipf",	IPT_IPF },
	{ "name",	IPT_NAME },
	{ "nat",	IPT_NAT },
	{ "number",	IPT_NUM },
	{ "out",	IPT_OUT },
	{ "role",	IPT_ROLE },
	{ "seed",	IPT_SEED },
	{ "size",	IPT_SIZE },
	{ "table",	IPT_TABLE },
	{ "tree",	IPT_TREE },
	{ "type",	IPT_TYPE },
	{ NULL,		0 }
};


int ippool_parsefile(fd, filename, iocfunc)
int fd;
char *filename;
ioctlfunc_t iocfunc;
{
	FILE *fp = NULL;
	char *s;

	yylineNum = 1;
	(void) yysettab(yywords);

	s = getenv("YYDEBUG");
	if (s)
		yydebug = atoi(s);
	else
		yydebug = 0;

	if (strcmp(filename, "-")) {
		fp = fopen(filename, "r");
		if (!fp) {
			fprintf(stderr, "fopen(%s) failed: %s\n", filename,
				STRERROR(errno));
			return -1;
		}
	} else
		fp = stdin;

	while (ippool_parsesome(fd, fp, iocfunc) == 1)
		;
	if (fp != NULL)
		fclose(fp);
	return 0;
}


int ippool_parsesome(fd, fp, iocfunc)
int fd;
FILE *fp;
ioctlfunc_t iocfunc;
{
	char *s;
	int i;

	poolioctl = iocfunc;

	if (feof(fp))
		return 0;
	i = fgetc(fp);
	if (i == EOF)
		return 0;
	if (ungetc(i, fp) == EOF)
		return 0;
	if (feof(fp))
		return 0;
	s = getenv("YYDEBUG");
	if (s)
		yydebug = atoi(s);
	else
		yydebug = 0;

	yyin = fp;
	yyparse();
	return 1;
}

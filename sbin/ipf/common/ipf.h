/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ipf.h	1.12 6/5/96
 * $Id$
 */

#ifndef	__IPF_H__
#define	__IPF_H__


#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
/*
 * This is a workaround for <sys/uio.h> troubles on FreeBSD, HPUX, OpenBSD.
 * Needed here because on some systems <sys/uio.h> gets included by things
 * like <sys/socket.h>
 */
#ifndef _KERNEL
# define ADD_KERNEL
# define _KERNEL
# define KERNEL
#endif
#include <sys/uio.h>
#ifdef ADD_KERNEL
# undef _KERNEL
# undef KERNEL
#endif
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
# include <netinet/tcp.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#if !defined(__SVR4) && !defined(__svr4__) && defined(sun)
# include <strings.h>
#endif
#include <string.h>
#include <unistd.h>

#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_pool.h"
#include "netinet/ip_scan.h"
#include "netinet/ip_htable.h"
#include "netinet/ip_sync.h"
#include "netinet/ip_dstlist.h"

#include "opts.h"

#ifndef __P
#  define	__P(x)	x
#endif

#ifndef	U_32_T
# define	U_32_T	1
# if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
    defined(__sgi)
typedef	u_int32_t	u_32_t;
# else
#  if defined(__alpha__) || defined(__alpha) || defined(_LP64)
typedef unsigned int	u_32_t;
#  else
#   if SOLARIS2 >= 6
typedef uint32_t	u_32_t;
#   else
typedef unsigned int	u_32_t;
#   endif
#  endif
# endif /* __NetBSD__ || __OpenBSD__ || __FreeBSD__ || __sgi */
#endif /* U_32_T */

#ifndef	MAXHOSTNAMELEN
# define	MAXHOSTNAMELEN	256
#endif

#define	MAX_ICMPCODE	16
#define	MAX_ICMPTYPE	19

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf
#define FORMAT_IF(_a)	(_a != NULL ? _a : "(null)")


struct	ipopt_names	{
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};


typedef struct  alist_s {
	struct	alist_s	*al_next;
	int		al_not;
	int		al_family;
	i6addr_t	al_i6addr;
	i6addr_t	al_i6mask;
} alist_t;

#define	al_addr	al_i6addr.in4_addr
#define	al_mask	al_i6mask.in4_addr
#define	al_1	al_addr
#define	al_2	al_mask


typedef struct  plist_s {
	struct	plist_s	*pl_next;
	int		pl_compare;
	u_short		pl_port1;
	u_short		pl_port2;
} plist_t;


typedef	struct	{
	u_short	fb_c;
	u_char	fb_t;
	u_char	fb_f;
	u_32_t	fb_k;
} fakebpf_t;


typedef struct  {
	char	*it_name;
	int	it_v4;
	int	it_v6;
} icmptype_t;


typedef	struct	wordtab {
	char	*w_word;
	int	w_value;
} wordtab_t;


typedef	struct	namelist {
	struct namelist	*na_next;
	char		*na_name;
	int		na_value;
} namelist_t;


typedef	struct	proxyrule {
	struct	proxyrule	*pr_next;
	char			*pr_proxy;
	char			*pr_conf;
	namelist_t		*pr_names;
	int			pr_proto;
} proxyrule_t;


#if defined(__NetBSD__) || defined(__FreeBSD__) || \
	SOLARIS
# include <stdarg.h>
typedef	int	(* ioctlfunc_t)(int, ioctlcmd_t, ...);
#else
typedef	int	(* ioctlfunc_t)(dev_t, ioctlcmd_t, void *);
#endif
typedef	int	(* addfunc_t)(int, ioctlfunc_t, void *);
typedef	int	(* copyfunc_t)(void *, void *, size_t);


extern	char	thishost[MAXHOSTNAMELEN];
extern	char	flagset[];
extern	u_char	flags[];
extern	struct ipopt_names ionames[];
extern	struct ipopt_names secclass[];
extern	char	*icmpcodes[MAX_ICMPCODE + 1];
extern	char	*icmptypes[MAX_ICMPTYPE + 1];
extern	int	use_inet6;
extern	int	lineNum;
extern	int	debuglevel;
extern	struct ipopt_names v6ionames[];
extern	icmptype_t icmptypelist[];
extern	wordtab_t statefields[];
extern	wordtab_t natfields[];
extern	wordtab_t poolfields[];


extern int addicmp(char ***, struct frentry *, int);
extern int addipopt(char *, struct ipopt_names *, int, char *);
extern int addkeep(char ***, struct frentry *, int);
extern alist_t *alist_new(int, char *);
extern void alist_free(alist_t *);
extern void assigndefined(char *);
extern void binprint(void *, size_t);
extern u_32_t buildopts(char *, char *, int);
extern int checkrev(char *);
extern int connecttcp(char *, int);
extern int count6bits(u_32_t *);
extern int count4bits(u_32_t);
extern char *fac_toname(int);
extern int fac_findname(char *);
extern const char *familyname(const int);
extern void fill6bits(int, u_int *);
extern wordtab_t *findword(wordtab_t *, char *);
extern int ftov(int);
extern char *ipf_geterror(int, ioctlfunc_t *);
extern int genmask(int, char *, i6addr_t *);
extern int gethost(int, char *, i6addr_t *);
extern int geticmptype(int, char *);
extern int getport(struct frentry *, char *, u_short *, char *);
extern int getportproto(char *, int);
extern int getproto(char *);
extern char *getnattype(struct nat *);
extern char *getsumd(u_32_t);
extern u_32_t getoptbyname(char *);
extern u_32_t getoptbyvalue(int);
extern u_32_t getv6optbyname(char *);
extern u_32_t getv6optbyvalue(int);
extern char *icmptypename(int, int);
extern void initparse(void);
extern void ipf_dotuning(int, char *, ioctlfunc_t);
extern int ipf_addrule(int, ioctlfunc_t, void *);
extern void ipf_mutex_clean(void);
extern int ipf_parsefile(int, addfunc_t, ioctlfunc_t *, char *);
extern int ipf_parsesome(int, addfunc_t, ioctlfunc_t *, FILE *);
extern void ipf_perror(int, char *);
extern int ipf_perror_fd( int, ioctlfunc_t, char *);
extern void ipf_rwlock_clean(void);
extern char *ipf_strerror(int);
extern void ipferror(int, char *);
extern int ipmon_parsefile(char *);
extern int ipmon_parsesome(FILE *);
extern int ipnat_addrule(int, ioctlfunc_t, void *);
extern int ipnat_parsefile(int, addfunc_t, ioctlfunc_t, char *);
extern int ipnat_parsesome(int, addfunc_t, ioctlfunc_t, FILE *);
extern int ippool_parsefile(int, char *, ioctlfunc_t);
extern int ippool_parsesome(int, FILE *, ioctlfunc_t);
extern int kmemcpywrap(void *, void *, size_t);
extern char *kvatoname(ipfunc_t, ioctlfunc_t);
extern int load_dstlist(struct ippool_dst *, ioctlfunc_t,
			     ipf_dstnode_t *);
extern int load_dstlistnode(int, char *, struct ipf_dstnode *,
				 ioctlfunc_t);
extern alist_t *load_file(char *);
extern int load_hash(struct iphtable_s *, struct iphtent_s *,
			  ioctlfunc_t);
extern int load_hashnode(int, char *, struct iphtent_s *, int,
			      ioctlfunc_t);
extern alist_t *load_http(char *);
extern int load_pool(struct ip_pool_s *list, ioctlfunc_t);
extern int load_poolnode(int, char *, ip_pool_node_t *, int, ioctlfunc_t);
extern alist_t *load_url(char *);
extern alist_t *make_range(int, struct in_addr, struct in_addr);
extern void mb_hexdump(mb_t *, FILE *);
extern ipfunc_t nametokva(char *, ioctlfunc_t);
extern void nat_setgroupmap(struct ipnat *);
extern int ntomask(int, int, u_32_t *);
extern u_32_t optname(char ***, u_short *, int);
extern wordtab_t *parsefields(wordtab_t *, char *);
extern int *parseipfexpr(char *, char **);
extern int parsewhoisline(char *, addrfamily_t *, addrfamily_t *);
extern void pool_close(void);
extern int pool_fd(void);
extern int pool_ioctl(ioctlfunc_t, ioctlcmd_t, void *);
extern int pool_open(void);
extern char *portname(int, int);
extern int pri_findname(char *);
extern char *pri_toname(int);
extern void print_toif(int, char *, char *, struct frdest *);
extern void printaps(ap_session_t *, int, int);
extern void printaddr(int, int, char *, int, u_32_t *, u_32_t *);
extern void printbuf(char *, int, int);
extern void printfieldhdr(wordtab_t *, wordtab_t *);
extern void printfr(struct frentry *, ioctlfunc_t);
extern struct iphtable_s *printhash(struct iphtable_s *, copyfunc_t,
					 char *, int, wordtab_t *);
extern struct iphtable_s *printhash_live(iphtable_t *, int, char *,
					      int, wordtab_t *);
extern ippool_dst_t *printdstl_live(ippool_dst_t *, int, char *,
					 int, wordtab_t *);
extern void printhashdata(iphtable_t *, int);
extern struct iphtent_s *printhashnode(struct iphtable_s *,
					    struct iphtent_s *,
					    copyfunc_t, int, wordtab_t *);
extern void printhost(int, u_32_t *);
extern void printhostmask(int, u_32_t *, u_32_t *);
extern void printip(int, u_32_t *);
extern void printlog(struct frentry *);
extern void printlookup(char *, i6addr_t *addr, i6addr_t *mask);
extern void printmask(int, u_32_t *);
extern void printnataddr(int, char *, nat_addr_t *, int);
extern void printnatfield(nat_t *, int);
extern void printnatside(char *, nat_stat_side_t *);
extern void printpacket(int, mb_t *);
extern void printpacket6(int, mb_t *);
extern struct ippool_dst *printdstlist(struct ippool_dst *, copyfunc_t,
					    char *, int, ipf_dstnode_t *,
					    wordtab_t *);
extern void printdstlistdata(ippool_dst_t *, int);
extern ipf_dstnode_t *printdstlistnode(ipf_dstnode_t *, copyfunc_t,
					    int, wordtab_t *);
extern void printdstlistpolicy(ippool_policy_t);
extern struct ip_pool_s *printpool(struct ip_pool_s *, copyfunc_t,
					char *, int, wordtab_t *);
extern struct ip_pool_s *printpool_live(struct ip_pool_s *, int,
					     char *, int, wordtab_t *);
extern void printpooldata(ip_pool_t *, int);
extern void printpoolfield(void *, int, int);
extern struct ip_pool_node *printpoolnode(struct ip_pool_node *,
					       int, wordtab_t *);
extern void printproto(struct protoent *, int, struct ipnat *);
extern void printportcmp(int, struct frpcmp *);
extern void printstatefield(ipstate_t *, int);
extern void printtqtable(ipftq_t *);
extern void printtunable(ipftune_t *);
extern void printunit(int);
extern void optprint(u_short *, u_long, u_long);
#ifdef	USE_INET6
extern void optprintv6(u_short *, u_long, u_long);
#endif
extern int remove_hash(struct iphtable_s *, ioctlfunc_t);
extern int remove_hashnode(int, char *, struct iphtent_s *, ioctlfunc_t);
extern int remove_pool(ip_pool_t *, ioctlfunc_t);
extern int remove_poolnode(int, char *, ip_pool_node_t *, ioctlfunc_t);
extern u_char tcpflags(char *);
extern void printc(struct frentry *);
extern void printC(int);
extern void emit(int, int, void *, struct frentry *);
extern u_char secbit(int);
extern u_char seclevel(char *);
extern void printfraginfo(char *, struct ipfr *);
extern void printifname(char *, char *, void *);
extern char *hostname(int, void *);
extern struct ipstate *printstate(struct ipstate *, int, u_long);
extern void printsbuf(char *);
extern void printnat(struct ipnat *, int);
extern void printactiveaddress(int, char *, i6addr_t *, char *);
extern void printactivenat(struct nat *, int, u_long);
extern void printhostmap(struct hostmap *, u_int);
extern void printtcpflags(u_32_t, u_32_t);
extern void printipfexpr(int *);
extern void printstatefield(ipstate_t *, int);
extern void printstatefieldhdr(int);
extern int sendtrap_v1_0(int, char *, char *, int, time_t);
extern int sendtrap_v2_0(int, char *, char *, int);
extern int vtof(int);

extern void set_variable(char *, char *);
extern char *get_variable(char *, char **, int);
extern void resetlexer(void);

extern void debug(int, char *, ...);
extern void verbose(int, char *, ...);
extern void ipfkdebug(char *, ...);
extern void ipfkverbose(char *, ...);

#if SOLARIS
extern int gethostname(char *, int );
extern void sync(void);
#endif

#endif /* __IPF_H__ */

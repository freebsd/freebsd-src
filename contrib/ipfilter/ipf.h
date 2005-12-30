/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ipf.h	1.12 6/5/96
 * $Id: ipf.h,v 2.71.2.7 2005/06/12 07:18:31 darrenr Exp $
 */

#ifndef	__IPF_H__
#define	__IPF_H__

#if defined(__osf__)
# define radix_mask ipf_radix_mask
# define radix_node ipf_radix_node
# define radix_node_head ipf_radix_node_head
#endif

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
#ifdef __OpenBSD__
struct file;
#endif
#include <sys/uio.h>
#ifdef ADD_KERNEL
# undef _KERNEL
# undef KERNEL
#endif
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#ifndef	TCP_PAWS_IDLE	/* IRIX */
# include <netinet/tcp.h>
#endif
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

#include "opts.h"

#ifndef __P
# ifdef __STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif
#ifndef __STDC__
# undef		const
# define	const
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


struct	ipopt_names	{
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};


typedef struct  alist_s {
	struct	alist_s	*al_next;
	int		al_not;
	i6addr_t	al_i6addr;
	i6addr_t	al_i6mask;
} alist_t;

#define	al_addr	al_i6addr.in4_addr
#define	al_mask	al_i6mask.in4_addr
#define	al_1	al_addr
#define	al_2	al_mask


typedef	struct	{
	u_short	fb_c;
	u_char	fb_t;
	u_char	fb_f;
	u_32_t	fb_k;
} fakebpf_t;


#if defined(__NetBSD__) || defined(__OpenBSD__) || \
        (_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000) || \
	SOLARIS || defined(__sgi) || defined(__osf__) || defined(linux)
# include <stdarg.h>
typedef	int	(* ioctlfunc_t) __P((int, ioctlcmd_t, ...));
#else
typedef	int	(* ioctlfunc_t) __P((dev_t, ioctlcmd_t, void *));
#endif
typedef	void	(* addfunc_t) __P((int, ioctlfunc_t, void *));
typedef	int	(* copyfunc_t) __P((void *, void *, size_t));


/*
 * SunOS4
 */
#if defined(sun) && !defined(__SVR4) && !defined(__svr4__)
extern	int	ioctl __P((int, int, void *));
#endif

extern	char	thishost[];
extern	char	flagset[];
extern	u_char	flags[];
extern	struct ipopt_names ionames[];
extern	struct ipopt_names secclass[];
extern	char	*icmpcodes[MAX_ICMPCODE + 1];
extern	char	*icmptypes[MAX_ICMPTYPE + 1];
extern	int	use_inet6;
extern	int	lineNum;
extern	struct ipopt_names v6ionames[];


extern int addicmp __P((char ***, struct frentry *, int));
extern int addipopt __P((char *, struct ipopt_names *, int, char *));
extern int addkeep __P((char ***, struct frentry *, int));
extern int bcopywrap __P((void *, void *, size_t));
extern void binprint __P((void *, size_t));
extern void initparse __P((void));
extern u_32_t buildopts __P((char *, char *, int));
extern int checkrev __P((char *));
extern int count6bits __P((u_32_t *));
extern int count4bits __P((u_32_t));
extern int extras __P((char ***, struct frentry *, int));
extern char *fac_toname __P((int));
extern int fac_findname __P((char *));
extern void fill6bits __P((int, u_int *));
extern int gethost __P((char *, u_32_t *));
extern int getport __P((struct frentry *, char *, u_short *));
extern int getportproto __P((char *, int));
extern int getproto __P((char *));
extern char *getline __P((char *, size_t, FILE *, int *));
extern int genmask __P((char *, u_32_t *));
extern char *getnattype __P((struct ipnat *));
extern char *getsumd __P((u_32_t));
extern u_32_t getoptbyname __P((char *));
extern u_32_t getoptbyvalue __P((int));
extern u_32_t getv6optbyname __P((char *));
extern u_32_t getv6optbyvalue __P((int));
extern void hexdump __P((FILE *, void *, int, int));
extern int hostmask __P((char ***, char *, char *, u_32_t *, u_32_t *, int));
extern int hostnum __P((u_32_t *, char *, int, char *));
extern int icmpcode __P((char *));
extern int icmpidnum __P((char *, u_short *, int));
extern void initparse __P((void));
extern void ipf_dotuning __P((int, char *, ioctlfunc_t)); 
extern void ipf_addrule __P((int, ioctlfunc_t, void *));
extern int ipf_parsefile __P((int, addfunc_t, ioctlfunc_t *, char *));
extern int ipf_parsesome __P((int, addfunc_t, ioctlfunc_t *, FILE *));
extern int ipmon_parsefile __P((char *));
extern int ipmon_parsesome __P((FILE *));
extern void ipnat_addrule __P((int, ioctlfunc_t, void *));
extern int ipnat_parsefile __P((int, addfunc_t, ioctlfunc_t, char *));
extern int ipnat_parsesome __P((int, addfunc_t, ioctlfunc_t, FILE *));
extern int ippool_parsefile __P((int, char *, ioctlfunc_t));
extern int ippool_parsesome __P((int, FILE *, ioctlfunc_t));
extern int kmemcpywrap __P((void *, void *, size_t));
extern char *kvatoname __P((ipfunc_t, ioctlfunc_t));
extern int load_hash __P((struct iphtable_s *, struct iphtent_s *,
			  ioctlfunc_t));
extern int load_hashnode __P((int, char *, struct iphtent_s *, ioctlfunc_t));
extern int load_pool __P((struct ip_pool_s *list, ioctlfunc_t));
extern int load_poolnode __P((int, char *, ip_pool_node_t *, ioctlfunc_t));
extern int loglevel __P((char **, u_int *, int));
extern alist_t *make_range __P((int, struct in_addr, struct in_addr));
extern ipfunc_t nametokva __P((char *, ioctlfunc_t));
extern ipnat_t *natparse __P((char *, int));
extern void natparsefile __P((int, char *, int));
extern void nat_setgroupmap __P((struct ipnat *));
extern int ntomask __P((int, int, u_32_t *));
extern u_32_t optname __P((char ***, u_short *, int));
extern struct frentry *parse __P((char *, int));
extern char *portname __P((int, int));
extern int portnum __P((char *, char *, u_short *, int));
extern int ports __P((char ***, char *, u_short *, int *, u_short *, int));
extern int pri_findname __P((char *));
extern char *pri_toname __P((int));
extern void print_toif __P((char *, struct frdest *));
extern void printaps __P((ap_session_t *, int));
extern void printbuf __P((char *, int, int));
extern void printfr __P((struct frentry *, ioctlfunc_t));
extern void printtunable __P((ipftune_t *));
extern struct iphtable_s *printhash __P((struct iphtable_s *, copyfunc_t,
					 char *, int));
extern struct iphtent_s *printhashnode __P((struct iphtable_s *,
					    struct iphtent_s *,
					    copyfunc_t, int));
extern void printhostmask __P((int, u_32_t *, u_32_t *));
extern void printip __P((u_32_t *));
extern void printlog __P((struct frentry *));
extern void printlookup __P((i6addr_t *addr, i6addr_t *mask));
extern void printmask __P((u_32_t *));
extern void printpacket __P((struct ip *));
extern void printpacket6 __P((struct ip *));
extern struct ip_pool_s *printpool __P((struct ip_pool_s *, copyfunc_t,
					char *, int));
extern struct ip_pool_node *printpoolnode __P((struct ip_pool_node *, int));
extern void printproto __P((struct protoent *, int, struct ipnat *));
extern void printportcmp __P((int, struct frpcmp *));
extern void optprint __P((u_short *, u_long, u_long));
#ifdef	USE_INET6
extern void optprintv6 __P((u_short *, u_long, u_long));
#endif
extern int ratoi __P((char *, int *, int, int));
extern int ratoui __P((char *, u_int *, u_int, u_int));
extern int remove_hash __P((struct iphtable_s *, ioctlfunc_t));
extern int remove_hashnode __P((int, char *, struct iphtent_s *, ioctlfunc_t));
extern int remove_pool __P((ip_pool_t *, ioctlfunc_t));
extern int remove_poolnode __P((int, char *, ip_pool_node_t *, ioctlfunc_t));
extern u_char tcp_flags __P((char *, u_char *, int));
extern u_char tcpflags __P((char *));
extern int to_interface __P((struct frdest *, char *, int));
extern void printc __P((struct frentry *));
extern void printC __P((int));
extern void emit __P((int, int, void *, struct frentry *));
extern u_char secbit __P((int));
extern u_char seclevel __P((char *));
extern void printfraginfo __P((char *, struct ipfr *));
extern void printifname __P((char *, char *, void *));
extern char *hostname __P((int, void *));
extern struct ipstate *printstate __P((struct ipstate *, int, u_long));
extern void printsbuf __P((char *));
extern void printnat __P((struct ipnat *, int));
extern void printactivenat __P((struct nat *, int));
extern void printhostmap __P((struct hostmap *, u_int));
extern void printpacket __P((struct ip *));

extern void set_variable __P((char *, char *));
extern char *get_variable __P((char *, char **, int));
extern void resetlexer __P((void));

#if SOLARIS
extern int gethostname __P((char *, int ));
extern void sync __P((void));
#endif

#endif /* __IPF_H__ */

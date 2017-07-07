/*! \file resolv.h
 *  WSHelper DNS/Hesiod Library header
 *	This file contains the function declaration for:\n
 *	res_init()		\n
 *	res_search()	\n
 *  dn_comp()		\n
 *	rdn_expand()	\n \n
 *	and unsupported functions: \n
 *	res_setopts()		\n
 *	res_getopts()		\n
 *	res_querydomain()	\n
 *	res_mkquery()		\n
 *	res_send()			\n
*/

#ifndef _RESOLV_H_
#define _RESOLV_H_

#include <windows.h>
#ifndef MAXDNAME
#include <arpa/nameser.h>
#endif

/*! \def MAXNS
 *	max # name servers we'll track
 */
#define MAXNS                   3

/*! \def MAXDFLSRCH
 *	# default domain levels to try
 */
#define MAXDFLSRCH              3

/*!	\def MAXDNSRCH
 *	max # domains in search path
 */
#define MAXDNSRCH               6

/*! \def LOCALDOMAINPARTS
 *	min levels in name that is "local"
 */
#define LOCALDOMAINPARTS        2

/*! \def RES_TIMEOUT
 *	min. seconds between retries
 */
#define RES_TIMEOUT             5

/*! \def MAXMXRECS
 *	number of records in the preference array in the MX record
 */
#define MAXMXRECS               8

/*! \struct mxent
 *	structure to hold the MX record
 */
struct mxent {
	/*! number of records in the preference field */
    int numrecs;
	/*! holds a 16 bit integer which specifies the preference given to this RR */
    u_short pref[MAXMXRECS];
	/*! a host willing to act as a mail exchange */
    char ** hostname;
};


/*! \struct state
 * This structure holds the state for the resolver query
 */
struct state {
	/*! retransmition time interval */
    int     retrans;
	/*! number of times to retransmit */
    int     retry;
	/*! field option flags - see below. */
    long    options;
	 /*! field number of name servers */
    int     nscount;
	/*! address of name server */
    struct  sockaddr_in nsaddr_list[MAXNS];
#define nsaddr  nsaddr_list[0]
	/*! current packet id */
    u_short id;
	/*! field default domain */
    char    defdname[MAXDNAME];
	/*! field components of domain to search */
    char    *dnsrch[MAXDNSRCH+1];
};

/*! \def RES_INIT
 *	resolver option: address initialized
 */
#define RES_INIT        0x0001

/*! \def RES_DEBUG
 *	resolver option: print debug messages
 */
#define RES_DEBUG       0x0002

/*! \def RES_AAONLY
 *	resolver option: authoritative answers only
 */
#define RES_AAONLY      0x0004

/*! \def RES_USEVC
 *	resolver option: use virtual circuit
 */
#define RES_USEVC       0x0008

/*! \def RES_PRIMARY
 *	resolver option: query primary server only
 */
#define RES_PRIMARY     0x0010

/*! \def RES_IGNTC
 *	resolver option: ignore trucation errors
 */
#define RES_IGNTC       0x0020

/*! \def RES_RECURSE
 *	resolver option: recursion desired
 */
#define RES_RECURSE     0x0040

/*! \def RES_DEFNAMES
 *	resolver option: use default domain name
 */
#define RES_DEFNAMES    0x0080

/*! \def RES_STAYOPEN
 *	resolver option: Keep TCP socket ope
 */
#define RES_STAYOPEN    0x0100

/*! \def RES_DNSRCH
 *	resolver option: search up local domain tree
 */
#define RES_DNSRCH      0x0200

/*! \def RES_DEFAULT
 *	resolver option: Default RES options (RES_RECURSE + RES_DEFNAMES + RES_DNSRCH)
 */
#define RES_DEFAULT     (RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)

extern struct state _res;

#include <stdio.h>

/* Private routines shared between libc/net, named, nslookup and others. */
#define fp_query        __fp_query
#define hostalias       __hostalias
#define putlong         __putlong
#define putshort        __putshort
#define p_class         __p_class
#define p_time          __p_time
#define p_type          __p_type


#ifdef __cplusplus
extern "C" {
#endif

/*! \fn int WINAPI res_init()
 *	\brief retrieves the default domain name and search order. It will look to see if an environment variable LOCALDOMAIN is defined. Otherwise,
 *	the domain associated with the local host is used. Otherwise, it will try to find the domain name from the registry
 *
 *  defined in res_init.c
 *
 *	\retval		The return value is 0 if the operation was successful.  Otherwise the value -1 is returned.
 */
int  WINAPI res_init();


/*! \fn	int WINAPI res_search(const char* name, int qclass, int type, u_char* answer, int anslen)
 *	\brief a generic query interface to the DNS name space. The query is performed with the dnsapi and
 *	the answer buffer is populated based on the returned RR set.
 *
 *	defined in res_quer.c

 *	\param[in]	name	domain name
 *	\param[in]	qclass  class of query(such as DNS_CLASS_INTERNET, DNS_CLASS_CSNET, DNS_CLASS_CHAOS,
 *						DNS_CLASS_HESIOD. Defined in windns.h)
 *	\param[in]	type	type of query(such as DNS_TYPE_A, DNS_TYPE_NS, DNS_TYPE_MX, DNS_TYPE_SRV. Defined in
 *						windns.h)
 *	\param[in]	answer  buffer to put answer in
 *	\param[in]	anslen	size of the answer buffer. compare the anslen with the return value, if the return
 *						value is bigger than anslen, it means the answer buffer doesn't contain the complete
 *						response. You will need to call this function again with a bigger answer buffer if
 *						you care about the complete response
 *
 *	\retval		return the size of the response on success, -1 on error
 *
 */
int  WINAPI res_search(const char  *name,
                                       int qclass, int type,
                                       u_char  *answer, int anslen);

/*! \fn	int WINAPI dn_comp(const u_char* exp_dn, u_char* comp_dn, int length, u_char** dnptrs, u_char** lastdnptr)
 *	\brief Compress domain name 'exp_dn' into 'comp_dn'
 *
 *	defined in res_comp.c
 *
 *	\param[in]	exp_dn	name to compress
 *	\param[in, out]	comp_dn		result of the compression
 *	\param[in]	length			the size of the array pointed to by 'comp_dn'.
 *	\param[in, out]	dnptrs		a list of pointers to previous compressed names. dnptrs[0]
 *								is a pointer to the beginning of the message. The list ends with NULL.
 *	\param[in]	lastdnptr		a pointer to the end of the arrary pointed to by 'dnptrs'. Side effect
 *								is to update the list of pointers for labels inserted into the
 *								message as we compress the name. If 'dnptr' is NULL, we don't try to
 *								compress names. If 'lastdnptr' is NULL, we don't update the list.
 *	\retval						Return the size of the compressed name or -1
 */
int  WINAPI dn_comp(const u_char *exp_dn,
                                    u_char  *comp_dn,
                                    int length, u_char  **dnptrs,
                                    u_char  * *lastdnptr);

/*! \fn int WINAPI rdn_expand(const u_char  *msg, const u_char  *eomorig, const u_char  *comp_dn, u_char  *exp_dn,
                              int length);
 *	\brief	replacement for dn_expand called rdn_expand. Older versions of the DLL used to this as dn_expand
 *			but this has caused some conflict with more recent versions of the MSDEV libraries. rdn_expand()
 *			expands the compressed domain name comp_dn to a full domain name.  Expanded names are converted to upper case.
 *
 *	defined in res_comp.c
 *
 *	\param[in]		msg			msg is a pointer to the  beginning  of  the  message
 *	\param[in]		eomorig
 *	\param[in]		comp_dn		the compressed domain name.
 *	\param[in, out]	exp_dn		a pointer to the result buffer
 *	\param[in]		length		size of the result in expn_dn
 *	\retval						the size of compressed name is returned or -1 if there was an error.
*/
int  WINAPI rdn_expand(const u_char  *msg,
                                      const u_char  *eomorig,
                                      const u_char  *comp_dn,
                                      u_char  *exp_dn,
                                      int length);
/* Microsoft includes an implementation of dn_expand() in winsock */
/* Make sure we do not use it.  jaltman@columbia.edu              */
#define dn_expand(a,b,c,d,e) rdn_expand(a,b,c,d,e)


/*! \fn void WINAPI res_setopts(long opts)
 *		unsupported
*/
void WINAPI res_setopts(long opts);

/*! \fn long WINAPI res_getopts(void)
 *		unsupported
*/
long WINAPI res_getopts(void);

/*! \fn int  WINAPI res_mkquery(int op, const char *dname, int qclass, int type, const char  *data, int datalen,
 *					const struct rrec  *newrr, char  *buf, int buflen)
 *		unsupported
 */
int  WINAPI res_mkquery(int op, const char *dname,
                                        int qclass, int type,
                                        const char  *data, int datalen,
                                        const struct rrec  *newrr,
                                        char  *buf, int buflen);

/*! \fn int  WINAPI res_send(const char  *msg, int msglen, char  *answer, int anslen)
 *		unsupported
*/
int  WINAPI res_send(const char  *msg, int msglen,
                                     char  *answer, int anslen);

/*! \fn int  WINAPI res_querydomain(const char  *name, const char  *domain, int qclass, int type,
									u_char  *answer, int anslen);
*		unsupported
*/
int  WINAPI res_querydomain(const char  *name,
                                            const char  *domain,
                                            int qclass, int type,
											u_char  *answer, int anslen);


#ifdef __cplusplus
}
#endif

#endif /* !_RESOLV_H_ */

/**************************************************************************
 * ns_validate.c (was security.c in original ISI contribution)
 * author: anant kumar
 * contributed: March 17, 1993
 *
 * implements validation procedure for RR's received from a server as a
 * response to a query.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <resolv.h>

#include "named.h"

#ifdef VALIDATE

static int		isvalid __P((struct namebuf *, int, int, char *, int)),
			check_addr_ns __P((struct databuf **,
					   struct sockaddr_in *,
					   char *)),
			check_in_tables __P((struct databuf **,
					     struct sockaddr_in *,
					     char *));
#if 0
static void		stick_in_queue __P((char *, int, int, char *));
#endif

static NAMEADDR		nameaddrlist[MAXNAMECACHE];
static int		firstNA = 0,
			lastNA = 0;

static TO_Validate	*validateQ, *currentVQ;
static int		VQcount;

/*****************************************************************
 * validate() is called from dovalidate(). it takes as parameters, 
 * the domain name sought, the class, type etc. of record, the server
 * that gave us the answer and the data it gave us
 *
 * it returns VALID if it is able to validate the record, INVALID if it cannot.
 * furtehr VALID is split into VALID_CACHE if we need to cache this record
 * since the domainname is not something we are authoritative for and
 * VALID_NO_CACHE if the name is something we are authoritative for.
 *
 * pseudocode for function validate is as follows:
 * validate(domain, qdomain, server, type, class, data, dlen, rcode) {
 *
 *       if (dname or a higher level name not found in cache)
 *          return INVALID;
 *       if (NS records for "domain" found in cache){
 *
 *           if (we are authoritative)  /findns() returned NXDOMAIN;/
 *              if (we did not have an exact match on names)
 *                 =>the name does not exist in our database
 *                 => data is bad: return INVALID
 *              if (data agrees with what we have)
 *                return VALID_NO_CACHE;
 *              else return INVALID;
 *    
 *          if (we are not authoritative) /findns() returned OK;/       
 *	    if (domain lives below the qdomain)
 *		return VALID_CACHE;
 *          if (address records for NS's found in cache){
 *                       if ("server" = one of the addresses){
 *                               return VALID_CACHE;
 *                       }else{
 *                          stick in queue of "to_validate" data;
 *                          return (INVALID);
 *                       }
 *          else return INVALID;
 *
 * This performs the validation procedure described above. Checks
 * for the longest component of the dname that has a NS record
 * associated with it. At any stage, if no data is found, it implies
 * that the name is bad (has an unknown domain identifier) thus, we
 * return INVALID.
 * If address of one of these servers matches the address of the server
 * that returned us this data, we are happy!
 *
 * since findns will set needs_prime_cache if np = NULL is passed, we always
 * reset it. will let ns_req do it when we are searching for ns records to
 * query someone. hence in all the three cases of switch(findns())
 *                                 we have needs_prime_cache = 0;
 *****************************************************************************/
int
validate(dname, qdomain, server, type, class, data, dlen
#ifdef NCACHE
	 ,rcode
#endif
	 )
	char *dname, *qdomain;
	struct sockaddr_in *server;
	int type, class;
	char *data;
	int dlen;
#ifdef NCACHE
	int rcode;
#endif
{
	struct namebuf *np, *dnamep;
	struct hashbuf *htp;
	struct databuf *nsp[NSMAX];
	int count;
	const char *fname;
	int exactmatch = 0;
	struct fwdinfo *fwd;

#ifdef	DATUMREFCNT
	nsp[0] = NULL;
#endif
	dprintf(3, (ddt,
		    "validate(), d:%s, s:[%s], t:%d, c:%d\n",
		    dname, inet_ntoa(server->sin_addr), type, class));

	/* everything from forwarders is the GOSPEL */
	for (fwd = fwdtab; fwd != NULL; fwd = fwd->next) {
		if (server->sin_addr.s_addr == fwd->fwdaddr.sin_addr.s_addr)
			return (VALID_CACHE);
	}

	htp = hashtab;
	if (priming && (dname[0] == '\0'))
		np = NULL;
	else
		np = nlookup(dname, &htp, &fname, 0);
    
	/* we were able to locate namebufs for this domain, or a parent domain,
	 * or ??? */

	if (np == NULL)
		fname = "";
	dprintf(5, (ddt,
		    "validate:namebuf found np:%#lx, d:\"%s\", f:\"%s\"\n",
		    (u_long)np, dname, fname));
	/* save the namebuf if we were able to locate the exact dname */
	if (!strcasecmp(dname, fname)) {
		dnamep = np;
		exactmatch = 1;
	}
	switch (findns(&np, class, nsp, &count, 0)) {
	case NXDOMAIN:
		/** we are authoritative for this domain, lookup name 
		 * in our zone data, if it matches, return valid.
		 * in either case, do not cache
		 **/
		dprintf(5, (ddt, "validate: auth data found\n"));
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		if (needs_prime_cache)
			needs_prime_cache = 0;

#ifdef NCACHE
		if (rcode == NXDOMAIN) {
			/* If we had an exactmatch on the name, we found the
			 * name in our authority database, so this couldn't 
			 * have been a bad name. INVALID data, say so
			 */
			if (exactmatch)
				return (INVALID);
			else
				/* we did not have an exactmatch, the data is
				 * good, we do not NCACHE stuff we are
				 * authoritative for, though.
				 */
				return (VALID_NO_CACHE);
		}
#endif
		if (!strcasecmp(dname, np->n_dname)) {
      
			/* if the name we seek is the same as that we have ns
			 * records for, compare the data we have to see if it
			 * matches. if it does, return valid_no_cache, if it
			 * doesn't, invalid.
			 */
			if (isvalid(np, type, class, data, dlen))
				return (VALID_NO_CACHE);
			else
				return (INVALID);
		}
    
		/* we found ns records in a higher level, if we were unable to
		 * locate the exact name earlier, it means we are
		 * authoritative for this domain but do not have records for
		 * this name. this name is obviously invalid
		 */
		if (!exactmatch)
			return (INVALID);
    
		/* we found the exact name earlier and we are obviously
		 * authoritative so check for data records and see if any
		 * match.
		 */
		if (isvalid(dnamep, type, class, data, dlen))
			return (VALID_NO_CACHE);
		else
			return (INVALID);
  
	case SERVFAIL:/* could not find name server records*/
		/* stick_in_queue(dname, type, class, data); */
		if (needs_prime_cache)
			needs_prime_cache = 0;
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (INVALID);
    
	case OK: /*proceed */
		dprintf(5, (ddt, "validate:found ns records\n"));
		if (needs_prime_cache)
			needs_prime_cache = 0;
		if (samedomain(dname, qdomain) ||
		    check_addr_ns(nsp, server, dname)) {
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (VALID_CACHE);
		}
		/* server is not one of those we know of */
		/* stick_in_queue(dname, type, class, data); */
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (INVALID);
	default:
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (INVALID);
	} /*switch*/

} /*validate*/

/***********************************************************************
 * validate rr returned by somebody against your own database, if you are 
 * authoritative for the information. if you have a record that matches,
 * return 1, else return 0. validate() above will use this and determine
 * if the record should be returned/discarded.
 ***********************************************************************/
static int
isvalid(np, type, class, data, dlen)
	struct namebuf *np;
	int type, class;
	char *data;
	int dlen;
{
	register struct databuf *dp;
  
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!wanted(dp, class, type)) {
			if ((type == T_CNAME) && (class == dp->d_class)) { 
				/* if a cname exists, any other will not */
				return (0);
				/* we come here only for zone info,
				 * so -ve $ed info can't be
				 */
			}
			continue;
		}
		/* type and class match, if i get here 
		 * let's now compare the data section, per RR type
		 */  

		/* unless, of course, the data was negative, in which case
		 * we should return FAILURE since we should not have found
		 * data here.
		 */
		if ((data == NULL) || (dlen == 0))
			return (0);
		
		/* XXX:	why aren't we just calling db_cmp()? */

	        switch (type) {
			char *td;
			u_char *tdp;
			int x;

			case T_A:
			case T_WKS:
			case T_HINFO:
			case T_UINFO:
			case T_UID:
			case T_GID:
			case T_TXT:
			case T_X25:
			case T_ISDN:
			case T_LOC:
#ifdef ALLOW_T_UNSPEC
			case T_UNSPEC:
#endif
			       x = memcmp(dp->d_data, data, dlen);
			       dprintf(3, (ddt, "type = %d, GOOD = %d\n",
					   type, x));
			       if (x == 0)
				       return (1);
			       else
				       break;

			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
			case T_NS:
			case T_PTR:
			       x = strncasecmp((char *)dp->d_data, data, dlen);
			       dprintf(3, (ddt, "type = %d, GOOD = %d\n",
					   type, x));
			       if (x == 0)
				       return (1);
			       else
				      break; 

			case T_MINFO:
			case T_SOA:
			case T_RP:
				/* compare first string */
				x = strncasecmp((char *)dp->d_data, data,
						strlen(data) + 1);
				if (x != 0) 
					break;

				/* move to second string */
				td = data + (strlen(data) + 1);
				tdp = dp->d_data + 
					(strlen((char *)dp->d_data)+1);

				/* compare second string */
				x = strncasecmp(td, (char *)tdp, 
						strlen((char *)td+1));
				if (x != 0)
					break;

				/* move beyond second string, to
				 * set of words in SOA.
				 * RP and MINFO stuff really
				 * ends here
				 */

				td = td + strlen((char *)td) + 1;
				tdp = tdp + strlen((char *)tdp) + 1;
				if (type == T_SOA) {
					x = memcmp(td, (char *)tdp,
						   5*INT32SZ);
					if (x != 0)
						break;
				}
				
				/* everything was equal, wow!
				 * so return a success
				 */
			        return (1);

			case T_MX:
			case T_AFSDB:
			case T_RT:
				x = memcmp(dp->d_data, data,
					   INT16SZ);
				if (x != 0)
					break;
				td = data + INT16SZ;
				tdp = dp->d_data + INT16SZ;
				x = strncasecmp(td, (char *)tdp, 
						strlen((char *)td) + 1);
				if (x != 0)
					break;
				return (1);

			case T_PX:
				x = memcmp(dp->d_data, data,
					INT16SZ);
				if (x != 0)
					break;
				td = data + INT16SZ;
				tdp = dp->d_data + INT16SZ;

				/* compare first string */
				x = strncasecmp(td, (char *)tdp,
						strlen((char *)td) + 1);
				if (x != 0)
					break;
				td += (strlen(td) + 1);
				tdp += (strlen((char *)tdp) + 1);
				
				/* compare second string */
				x = strncasecmp(td, (char *)tdp,
						strlen((char *)td+1));
				if (x != 0)
					break;
				return (1);

			default:
				dprintf(3, (ddt, "unknown type %d\n", type));
				return (0);
		}
		/* continue in loop if record did not match */
	}
	/* saw no record of interest in whole chain
	 * If the data we were trying to validate was negative, we succeeded!
	 * else we failed
	 */
	if ((data == NULL) || (dlen == 0)) {
		/* negative data, report success */
		return (1);
	} 
	/* positive data, no such RR, validation failed */
	return (0);
}

/******************************************************************
  * get a list of databufs that have ns addresses for the closest domain
  * you know about, get their addresses and confirm that server indeed
  * is one of them. if yes return 1 else 0. 
  * first checks the cache that we build in nslookup() earlier 
  * when we ns_forw(). if unableto find it there, it checks the entire
  * hash table to do address translations.
  *******************************************************************/
static int
check_addr_ns(nsp, server, dname)
	struct databuf **nsp;
	struct sockaddr_in *server;
	char *dname;
{
	int i, found=0;
	char sname[MAXDNAME];
	struct in_addr *saddr = &(server->sin_addr);
	struct databuf **nsdp;

	dprintf(5, (ddt, "check_addr_ns: s:[%s], db:0x%lx, d:\"%s\"\n", 
		    inet_ntoa(*saddr), (u_long)nsp, dname));

	for(i = lastNA; i != firstNA; i = (i+1) % MAXNAMECACHE) {
		if (!bcmp((char *)saddr,
			  (char *)&(nameaddrlist[i].ns_addr),
			  INADDRSZ)) {
			strcpy(sname, nameaddrlist[i].nsname);
			found = 1;
			break;
		}
	}
	if (found) {
		dprintf(3, (ddt,
			    "check_addr_ns: found address:[%s]\n",
			    inet_ntoa(*saddr)));
		for (nsdp = nsp; *nsdp != NULL;nsdp++) {
			dprintf(5, (ddt,
				    "check_addr_ns:names are:%s, %s\n",
				    sname,(*nsdp)->d_data));
			if (!strcasecmp(sname,(char *)((*nsdp)->d_data))) {
				return (1);
			}
		}
	}
	/* could not find name in my cache of servers, must go through the
	 * whole grind
	 */

	dprintf(2, (ddt, "check_addr_ns:calling check_in_tables()\n"));
	return (check_in_tables(nsp, server, dname));
}

/*************************************************************************
 * checks in hash tables for the address of servers whose name is in the 
 * data section of nsp records. borrows code from nslookup()/ns_forw.c
 * largely.
 *************************************************************************/
static int
check_in_tables(nsp, server, syslogdname)
	struct databuf *nsp[];
	struct sockaddr_in *server;
	char *syslogdname;
{
	register struct namebuf *np;
	register struct databuf *dp, *nsdp;
	struct hashbuf *tmphtp;
	const char *dname, *fname;
	int class;
	int qcomp();
  
	dprintf(3, (ddt, "check_in_tables(nsp=x%lx, qp=x%x, '%s')\n",
		    (u_long)nsp, server, syslogdname));
  
	while ((nsdp = *nsp++) != NULL) {
		class = nsdp->d_class;
		dname = (char *)nsdp->d_data;
		dprintf(3, (ddt, "check_in_tables: NS %s c%d t%d (x%x)\n",
			    dname, class, nsdp->d_type, nsdp->d_flags));
		tmphtp = ((nsdp->d_flags & DB_F_HINT) ? fcachetab : hashtab);
		np = nlookup(dname, &tmphtp, &fname, 1);
		if (np == NULL || fname != dname) {
			dprintf(3, (ddt, "%s: not found %s %x\n",
				    dname, fname, np));
			continue;
		}
		/* look for name server addresses */
		for (dp = np->n_data;  dp != NULL;  dp = dp->d_next) {
			if (stale(dp))
				continue;
			if (dp->d_type != T_A || dp->d_class != class)
				continue;
#ifdef NCACHE
			if (dp->d_rcode)
				continue;
#endif
			if (!bcmp((char *)dp->d_data,
				  (char *)&(server->sin_addr),
				  INADDRSZ)) {
				return (1);
			}
		}
	}
	return (0); /* haven't been able to locate the right address */
}

/************************************************************************
 * is called in nslookup() and stores the name vs address of a name server
 *           --& check_in_tables above--
 * we contact, in a list of a maximum MAXNAMECACHE entries. we later refer
 *             -- NAMEADDR nameaddrlist[MAXNAMECACHE]; --
 * to this list when we are trying to resolve the name in check_addr_ns().
 *************************************************************************/
void
store_name_addr(servername, serveraddr, syslogdname, sysloginfo)
	const char *servername;
	struct in_addr serveraddr;
	const char *syslogdname;
	const char *sysloginfo;
{
	int i;

	dprintf(3, (ddt,
		    "store_name_addr:s:%s, a:[%s]\n",
		    servername, inet_ntoa(serveraddr)));

	/* if we already have the name address pair in cache, return */
	for (i = lastNA;  i != firstNA;  i = (i+1) % MAXNAMECACHE) {
		if (strcasecmp(servername, nameaddrlist[i].nsname) == 0) {
			if (serveraddr.s_addr
			    ==
			    nameaddrlist[i].ns_addr.s_addr) {
				dprintf(5, (ddt,
			  "store_name_addr:found n and a [%s] [%s] in our $\n",
					    inet_ntoa(nameaddrlist[i].ns_addr),
					    inet_ntoa(serveraddr)));
				return;
			} /* if */
		} else if (serveraddr.s_addr
			   ==
			   nameaddrlist[i].ns_addr.s_addr) {
#ifdef BAD_IDEA
			/*
			 * log this as it needs to be fixed.
			 * replace old name by new, next query likely to have
			 * NS record matching new
			 */
			if (!haveComplained((char*)
					     nhash(nameaddrlist[i].nsname),
					    (char*)nhash(servername)))
				syslog(LOG_INFO,
	       "%s: server name mismatch for [%s]: (%s != %s) (server for %s)",
				       sysloginfo,
				       inet_ntoa(serveraddr),
				       nameaddrlist[i].nsname, servername,
				       syslogdname);
#endif
			free(nameaddrlist[i].nsname);
			nameaddrlist[i].nsname = savestr(servername);
			return;
		} 
	}
	/* we have to add this one to our cache */

	nameaddrlist[firstNA].nsname = savestr(servername);
	bcopy((char *)&serveraddr,
	      (char *)&(nameaddrlist[firstNA].ns_addr),
	      INADDRSZ);

	dprintf(2, (ddt, "store_name_addr:added entry #:%d n:%s a:[%s]\n",
		    firstNA, nameaddrlist[firstNA].nsname,
		    inet_ntoa(nameaddrlist[firstNA].ns_addr)));

	firstNA = (firstNA+1) % MAXNAMECACHE;
	if (firstNA == lastNA) {
		free(nameaddrlist[firstNA].nsname);
		nameaddrlist[firstNA].nsname = 0;
		lastNA = (lastNA+1) % MAXNAMECACHE;
	}
	return;
}

/*
 * Decode the resource record 'rrp' and validate the RR.
 * Borrows code almost entirely from doupdate(). is a rather
 * non-invasive routine since it just goes thru the same motions
 * as doupdate but just marks the array validatelist entry as 
 * the return code from validate(). This is later used in doupdate
 * to cache/not cache the entry. also used in update_msg() to 
 * delete/keep the record from the outgoing message.
 */
int
dovalidate(msg, msglen, rrp, zone, flags, qdomain, server, VCode)
	u_char *msg, *rrp;
	int  msglen, zone, flags;
	char *qdomain;
	struct sockaddr_in *server;
	int *VCode;
{
	register u_char *cp;
	register int n;
	int class, type, dlen, n1;
	u_int32_t ttl;
	char dname[MAXDNAME];
	u_char *cp1;
	u_char data[BUFSIZ];
	register HEADER *hp = (HEADER *) msg;

	dprintf(2, (ddt, "dovalidate(zone %d, flags %x)\n",
		    zone, flags));
#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(msg, msglen, ddt);
#endif

	cp = rrp;
	n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
	if (n < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);
	dprintf(2, (ddt, "dovalidate: dname %s type %d class %d ttl %d\n",
		    dname, type, class, ttl));
	/*
	 * Convert the resource record data into the internal
	 * database format.
	 */
	switch (type) {
	case T_A:
	case T_WKS:
	case T_HINFO:
	case T_UINFO:
	case T_UID:
	case T_GID:
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_LOC:
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
#endif
		cp1 = cp;
		n = dlen;
		cp += n;
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *)data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
	case T_RP:
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data + (n = strlen((char *)data) + 1);
		n1 = sizeof(data) - n;
		if (type == T_SOA)
			n1 -= 5 * INT32SZ;
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		if (type == T_SOA) {
			bcopy((char *)cp, (char *)cp1, n = 5 * INT32SZ);
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		/* grab preference */
		bcopy((char *)cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, sizeof(data) - INT16SZ);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	case T_PX:
		/* grab preference */
		bcopy((char *)cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get first name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, sizeof(data) - INT16SZ);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += (n = strlen((char *)cp1) + 1);
		n1 = sizeof(data) - n;
		
		/* get second name */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		n = cp1 - data;
		cp1 = data;
		break;

	default:
		dprintf(3, (ddt, "unknown type %d\n", type));
		return ((cp - rrp) + dlen);
	}
	if (n > MAXDATA) {
		dprintf(2, (ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n));
		hp->rcode = FORMERR;
		return (-1);
	}

	*VCode = validate(dname, qdomain, server, type, class,(char *)cp1, n
#ifdef NCACHE
			  ,NOERROR
#endif
			  );
	if (*VCode == INVALID) {
		dprintf(2, (ddt,
			    "validation failed d:%s, t:%d, c:%d\n",
			    dname, type, class));
	} else {
		dprintf(2, (ddt,
			    "validation succeeded d:%s, t:%d, c:%d\n",
			    dname, type, class));
	}
	return (cp - rrp);
}

#if 0
/******************************************************************
  * This manages a data structure that stores all RRs that we were
  * unable to validate. Am not sure exactly what purpose this might
  * serve but until such time as we are sure it will not help, let
  * me do it anyway.
  *****************************************************************/
static void
stick_in_queue(dname, type, class, data)
	char *dname;
	int type;
	int class;
	char *data;
{
	struct timeval tp;
	struct _TIMEZONE tzp;
	TO_Validate *tempVQ;
	u_long leasttime;

	if (validateQ == NULL) {
		validateQ = (TO_Validate *)malloc(sizeof(TO_Validate));
		if (!validateQ)
			panic(errno, "malloc(validateQ)");
		validateQ->type = type;
		validateQ->class = class;
		validateQ->dname = savestr(dname);
		validateQ->data = savestr(data);	/* XXX no \0 */
		gettimeofday(&tp, &tzp);
		validateQ->time = tp.tv_sec;
		VQcount = 1;
		validateQ->next = validateQ->prev = NULL;
		currentVQ = validateQ;
		return;
	}
	if (VQcount < MAXVQ) {
		tempVQ =(TO_Validate *)malloc(sizeof(TO_Validate));
		if (!tempVQ)
			panic(errno, "malloc(tempVQ)");
		tempVQ->type = type;
		tempVQ->class = class;
		tempVQ->dname = savestr(dname);
		tempVQ->data = savestr(data);	/* XXX no \0 */
		gettimeofday(&tp,&tzp);
		tempVQ->time = tp.tv_sec;
		tempVQ->next = currentVQ->next;
		tempVQ->prev = currentVQ;
		if (currentVQ->next != NULL)
			currentVQ->next->prev = tempVQ;
		currentVQ->next = tempVQ;
		currentVQ = tempVQ;
		VQcount++;
		return;
	}
	gettimeofday(&tp, &tzp);
	leasttime = validateQ->time;
	currentVQ = validateQ;
	for (tempVQ = validateQ;  tempVQ != NULL;  tempVQ = tempVQ->next) {
		if (tp.tv_sec >= tempVQ->time +VQEXPIRY) {
			tempVQ->type = type;
			tempVQ->class = class;
			strcpy(tempVQ->dname, dname);
			strcpy(tempVQ->data, data);
			tempVQ->time = tp.tv_sec;
			currentVQ = tempVQ;
			return;
		}
		if (tempVQ->time < leasttime) {
			leasttime = tempVQ->time;
			currentVQ = tempVQ;
		}
	}
	currentVQ->type = type;
	currentVQ->class = class;
	strcpy(currentVQ->dname, dname);
	strcpy(currentVQ->data, data);
	currentVQ->time = tp.tv_sec;
	return;
}
#endif

#ifdef BAD_IDEA
/* removes any INVALID RR's from the msg being returned, updates msglen to
 * reflect the new message length.
 */
int
update_msg(msg, msglen, Vlist, c)
	u_char *msg;
	int *msglen;
	int Vlist[];
	int c;
{
	register HEADER *hp;
	register u_char *cp;
	int i;
	int n = 0;
	u_char *tempcp, *newcp;
	int *RRlen;
	int qlen; /* the length of the query section*/
	u_int16_t rdlength;
	u_int16_t ancount, nscount;
	u_int16_t new_ancount, new_nscount, new_arcount;
	char dname[MAXDNAME], qname[MAXDNAME];
	u_char data[MAXDNAME];
	u_char **dpp;
	u_char *dnptrs[40];
	u_char **edp = dnptrs + sizeof(dnptrs)/sizeof(dnptrs[0]);
	u_char *eom = msg + *msglen;
	int n_new;
	int rembuflen, newlen;
	u_char *newmsg;
	u_int16_t type, class, dlen;
	u_int32_t ttl;
	int inv = 0;

#ifdef DEBUG
	if (debug) {
		fprintf(ddt, "update_msg: msglen:%d, c:%d\n", *msglen, c);
		if (debug >= 10)
			fp_nquery(msg, *msglen, ddt);
	}
#endif
	/* just making sure we do not do all the work for nothing */
	for (i=0;  i<c;  i++) { 
		if (Vlist[i] == INVALID) {
			inv = 1;
			break;
		}
	}
	if (inv != 1) {
		/* no invalid records, go about your job */
		return (0);
	}

	dprintf(2, (ddt, "update_msg: NEEDS updating:\n"));

	RRlen = (int *)malloc((unsigned)c*sizeof(int));
	if (!RRlen)
		panic(errno, "malloc(RRlen)");
	hp = (HEADER *)msg;
	new_ancount = ancount = ntohs(hp->ancount);
	new_nscount = nscount = ntohs(hp->nscount);
	new_arcount = ntohs(hp->arcount);

	cp = msg + HFIXEDSZ;
	newlen = HFIXEDSZ;
	/* skip the query section */
	qlen = dn_expand(msg, eom, cp, qname, sizeof qname);
	if (qlen <= 0) {
		dprintf(2, (ddt, "dn_expand() failed, bad record\n"));
		goto badend;
	}
	cp +=qlen;
	GETSHORT(type,cp);
	GETSHORT(class,cp);
	qlen += 2 * INT16SZ;
	newlen += qlen;

	for (i = 0;  i < c;  i++) {
		if (Vlist[i] == INVALID) {
			if (i < ancount)
				new_ancount--;
			else if (i < ancount+nscount)
				new_nscount--;
			else
				new_arcount--;
		}

		RRlen[i] = dn_skipname(cp, msg + *msglen);
		if (RRlen[i] <= 0) {
			dprintf(2, (ddt,
				    "dn_skipname() failed, bad record\n"));
			goto badend;
		}
		RRlen[i] += 2 * INT16SZ + INT32SZ;
				/*type+class+TTL*/
		cp += RRlen[i];
		GETSHORT(rdlength, cp);
		RRlen[i] += INT16SZ; /*rdlength*/
		RRlen[i] += rdlength; /*rdata field*/
		dprintf(3, (ddt, "RRlen[%d]=%d\n", i, RRlen[i]));
		if (Vlist[i] != INVALID)
			newlen += RRlen[i];
		cp += rdlength; /*increment pointer to next RR*/
	}
	hp->ancount = htons(new_ancount);
	hp->nscount = htons(new_nscount);
	hp->arcount = htons(new_arcount);
	/* get new buffer */
	dprintf(3, (ddt,
		    "newlen:%d, if no RR is INVALID == msglen\n", newlen));
	newmsg = (u_char *)calloc(1,newlen + MAXDNAME);
	if (newmsg == NULL)
		goto badend;
	dpp = dnptrs;
	*dpp++ = newmsg;
	*dpp = NULL;
	/* bcopy the header, with all the length fields correctly put in */
	bcopy((char *)msg, (char*)newmsg, HFIXEDSZ); /*header copied */
	newcp = newmsg +HFIXEDSZ;  /*need a pointer in the new buffer */
	rembuflen = newlen +MAXDNAME - HFIXEDSZ; /*buflen we can workin*/
	newlen = HFIXEDSZ; /* this will now contain the length of msg */
	n_new = dn_comp(qname, newcp, rembuflen, dnptrs, edp);
	if (n_new < 0)
		goto badend;
	newcp += n_new;
	PUTSHORT(type, newcp);
	PUTSHORT(class, newcp); /*query section complete*/
	newlen += (n_new+2*INT16SZ);
	rembuflen -= (n_new+2*INT16SZ);
	/* have to decode and copy every Valid RR from here */
  
	cp = msg +HFIXEDSZ +qlen;	/*skip header and query section*/
	for (i = 0;  i < c;  i++) {
		if (Vlist[i] == INVALID) {
			/* go to next RR if this one is not INVALID */
			cp += RRlen[i];
			continue;
		}
		/* we have a valid record, must put it in the newmsg */
		n = dn_expand(msg, eom, cp, dname, sizeof dname);
		if (n < 0) {
			hp->rcode = FORMERR;
			goto badend;
		}
		n_new = dn_comp(dname, newcp, rembuflen, dnptrs, edp); 
		if (n_new < 0)
			goto badend;
		cp += n;
		newcp += n_new;
		dprintf(5, (ddt,
			    "cp:0x%x newcp:0x%x after getting name\n",
			    cp, newcp));
		GETSHORT(type, cp);
		PUTSHORT(type, newcp);
		dprintf(5, (ddt,
			    "cp:0x%x newcp:0x%x after getting type\n",
			    cp, newcp));
		GETSHORT(class, cp);
		PUTSHORT(class, newcp);
		dprintf(5, (ddt,
			    "cp:0x%x newcp:0x%x after getting class\n",
			    cp, newcp));
		GETLONG(ttl, cp);
		PUTLONG(ttl, newcp);
		dprintf(5, (ddt,
			    "cp:0x%x newcp:0x%x after getting ttl\n",
			    cp, newcp));
		/* this will probably be modified for newmsg,
		 * will put this in later, after compression
		 */
		GETSHORT(dlen, cp);
		newlen += (n_new+3*INT16SZ + INT32SZ);
		rembuflen -= (n_new+3*INT16SZ+ INT32SZ);
		tempcp = newcp;
		newcp += INT16SZ; /*advance to rdata field*/
		dprintf(5, (ddt, "tempcp:0x%x newcp:0x%x\n",
			    tempcp, newcp));
		dprintf(3, (ddt,
			    "update_msg: dname %s type %d class %d ttl %d\n",
			    dname, type, class, ttl));
		/* read off the data section */
		switch (type) {
		case T_A:
		case T_WKS:
		case T_HINFO:
		case T_UINFO:
		case T_UID:
		case T_GID:
		case T_TXT:
		case T_X25:
		case T_ISDN:
		case T_LOC:
#ifdef ALLOW_T_UNSPEC
		case T_UNSPEC:
#endif
			n = dlen;
			PUTSHORT(n, tempcp); /*time to put in the dlen*/
			bcopy(cp, newcp,n); /*done here*/
			cp +=n;
			newcp +=n;
			newlen += n;
			rembuflen -= n;
			dprintf(3, (ddt, "\tcp:0x%x newcp:0x%x dlen:%d\n",
				    cp, newcp, dlen));
			break;

		case T_CNAME:
		case T_MB:
		case T_MG:
		case T_MR:
		case T_NS:
		case T_PTR:
			 /*read off name from data section */
			n = dn_expand(msg, eom, cp,
				      (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n; /*advance pointer*/
			/* fill in new packet */
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			PUTSHORT(n_new,tempcp);	/*put in dlen field*/
			newcp += n_new; /*advance new pointer*/
			newlen += n_new;
			rembuflen -= n_new;
			break;

		case T_MINFO:
		case T_SOA:
		case T_RP:
			n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n;
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			newcp += n_new;
			newlen += n_new;
			rembuflen -= n_new;
			dlen = n_new;
			n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n;
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			newcp += n_new;
			newlen += n_new;
			rembuflen -= n_new;
			dlen += n_new;
			if (type == T_SOA) {
				bcopy(cp, newcp, n = 5*INT32SZ);
				cp += n;
				newcp += n;
				newlen +=n;
				rembuflen -= n;
				dlen +=n;
			}
			PUTSHORT(dlen, tempcp);
			break;

		case T_MX:
		case T_AFSDB:
		case T_RT:
			/* grab preference */
			bcopy(cp,newcp,INT16SZ);
			cp += INT16SZ;
			newcp += INT16SZ;
		
			/* get name */
			n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n;
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			PUTSHORT(n_new+INT16SZ, tempcp);
			newcp += n_new;
			newlen += n_new+INT16SZ;
			rembuflen -= n_new+INT16SZ;
			break;

		case T_PX:
			/* grab preference */
			bcopy(cp, newcp, INT16SZ);
			cp += INT16SZ;
			newcp += INT16SZ;

			/* get first name */
			n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n;
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			newcp += n_new;
			newlen += n_new+INT16SZ;
			rembuflen -= n_new+INT16SZ;
			dlen = n_new+INT16SZ;
			n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
			if (n < 0) {
				hp->rcode = FORMERR;
				goto badend;
			}
			cp += n;
			n_new = dn_comp((char *)data, newcp, rembuflen,
					dnptrs, edp);
			if (n_new < 0)
				goto badend;
			newcp += n_new;
			newlen += n_new;
			rembuflen -= n_new;
			dlen += n_new;
			PUTSHORT(dlen, tempcp);
			break;

		default:
			dprintf(3, (ddt, "unknown type %d\n", type));
			goto badend;
		}
		dprintf(2, (ddt,
			    "newlen:%d, i:%d newcp:0x%x cp:0x%x\n\n",
			    newlen, i, newcp, cp));
	}
	bcopy(newmsg, msg, newlen);
	n = *msglen - newlen;
	if (n < 0) {
		dprintf(2, (ddt,
			"update_msg():newmsg longer than old: n:%d o:%d ???\n",
			    newlen, *msglen));
	}
	*msglen = newlen;
	free((char *)newmsg);
  
#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(msg, *msglen, ddt);
#endif
	free((char *)RRlen);
	return (n);
badend:
	dprintf(2, (ddt, "encountered problems: UPDATE_MSG\n"));
	free((char *)RRlen);
	return (-1);
}
#endif /*BAD_IDEA*/

#endif /*VALIDATE*/

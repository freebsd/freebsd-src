/*
** server.c			YP server routines.
**
** Copyright (c) 1993 Signum Support AB, Sweden
**
** This file is part of the NYS YP Server.
**
** The NYS YP Server is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License as
** published by the Free Software Foundation; either version 2 of the
** License, or (at your option) any later version.
**
** The NYS YP Server is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public
** License along with the NYS YP Server; see the file COPYING.  If
** not, write to the Free Software Foundation, Inc., 675 Mass Ave,
** Cambridge, MA 02139, USA.
**
** Author: Peter Eriksson <pen@signum.se>
** Ported to FreeBSD and hacked all to pieces
** by Bill Paul <wpaul@ctr.columbia.edu>
**
**	$Id: server.c,v 1.6.4.3 1997/04/10 14:34:28 wpaul Exp $
**
*/

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <db.h>
#include <unistd.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include "yp.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024, 	/* cachesize */
	NULL,		/* hash */
	0,		/* lorder */
};

#if TCP_WRAPPER
#include "tcpd.h"
int allow_severity=LOG_INFO;
int deny_severity=LOG_WARNING;
#endif

void            verr __P((const char *, _BSD_VA_LIST_));
void            Perror __P((const char *, ...));

extern char *dnsname();
extern char *dnsaddr();
extern char *_gethostbydnsaddr();

extern char *progname;
extern int errno;

int debug_flag = 0;
int dns_flag   = 0;
int children   = 0;
int forked     = 0;

void verr(fmt, ap)
    const char *fmt;
    _BSD_VA_LIST_ ap;

{
    if (debug_flag)
	vfprintf(stderr, fmt, ap);
    else
	vsyslog(LOG_NOTICE, fmt, ap);
}

void
#ifdef __STDC__
Perror(const char *fmt, ...)
#else
Perror(fmt, va_list)
    const char *fmt;
    va_dcl
#endif
{
    va_list ap;
#ifdef __STDC__
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
    verr(fmt,ap);
    va_end(ap);
}


/*
** Return 1 if request comes from an authorized host
**
** XXX This function should implement the "securenets" functionality
*/
static int is_valid_host(struct sockaddr_in *sin)
{
#if TCP_WRAPPER
    extern int hosts_ctl(char *, char *, char *, char *);
    int status;
    static long oldaddr=0;	/* so we dont log multiple times */
    static int oldstatus=-1;
    char *h=NULL;

#ifdef TRYRESOLVE
    struct hostent *hp;

    hp = _gethostbydnsaddr((char *) &sin->sin_addr.s_addr,
		       sizeof (sin->sin_addr.s_addr), AF_INET);

    h = (hp && hp->h_name) ? hp->h_name : NULL;
#endif

#ifndef FROM_UNKNOWN
#define FROM_UNKNOWN STRING_UNKNOWN
#endif

    status = hosts_ctl(progname,
		       h?h:FROM_UNKNOWN,
		       inet_ntoa(sin->sin_addr),
		       "");

    if (!status && (sin->sin_addr.s_addr != oldaddr || status != oldstatus)) {
	syslog(status?allow_severity:deny_severity,
	       "%sconnect from %s\n",status?"":"refused ",
	       h?h:inet_ntoa(sin->sin_addr));
	oldaddr=sin->sin_addr.s_addr;
	oldstatus=status;
    }
    return status;
#else
    return 1;
#endif
}


void *ypproc_null_2_svc(void *dummy,
			struct svc_req *rqstp)
{
    static int foo;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);
    if (!is_valid_host(rqhost))
	return NULL;

    if (debug_flag)
	Perror("ypproc_null() [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

    return (void *) &foo;
}


/*
** Return 1 if the name is a valid domain name served by us, else 0.
*/
static int is_valid_domain(const char *domain)
{
    struct stat sbuf;


    if (domain == NULL ||
	strcmp(domain, "binding") == 0 ||
	strcmp(domain, "..") == 0 ||
	strcmp(domain, ".") == 0 ||
	strchr(domain, '/'))
	return 0;

    if (stat(domain, &sbuf) < 0 || !S_ISDIR(sbuf.st_mode))
	return 0;

    return 1;
}



bool_t *ypproc_domain_2_svc(domainname *name,
			    struct svc_req *rqstp)
{
    static bool_t result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
	Perror("ypproc_domain(\"%s\") [From: %s:%d]\n",
		*name,
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    if (is_valid_domain(*name))
	result = TRUE;
    else
	result = FALSE;

    if (debug_flag)
	Perror("\t-> %s.\n",
		(result == TRUE ? "Ok" : "Not served by us"));

    return &result;
}


bool_t *ypproc_domain_nonack_2_svc(domainname *name,
				   struct svc_req *rqstp)
{
    static bool_t result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
	Perror("ypproc_domain_nonack(\"%s\") [From: %s:%d]\n",
		*name,
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    if (!is_valid_domain(*name))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid domain)\n");

	/* Bail out and don't return any RPC value */
	return NULL;
    }

    if (debug_flag)
	Perror("\t-> OK.\n");

    result = TRUE;
    return &result;
}


/*
** Open a DB database
*/
static DB *open_database(const char *domain,
			       const char *map)
{
    DB *dbp;
    char buf[1025];


    if (map[0] == '.' || strchr(map, '/'))
	return 0;

    sprintf (buf, "%s/%s", domain, map);

    dbp = dbopen(buf,O_RDONLY|O_EXCL, PERM_SECURE, DB_HASH, &openinfo);

    if (debug_flag > 1 && dbp == NULL)
	Perror("dbopen(%s): %s", map, strerror(errno));

    return dbp;
}


#define F_ALL   0x01
#define F_NEXT  0x02
#define F_YPALL 0x08

/*
** Get a record from a DB database.
** This looks ugly because it emulates the behavior of the original
** GDBM-based routines. Blech.
*/
int read_database(DB *dbp,
		  const DBT *ikey,
		  DBT *okey,
		  DBT *dval,
		  int flags)
{
    int first_flag = 0;
    DBT nkey, ckey, dummyval;


    if (ikey == NULL || ikey->data == NULL)
    {
	(dbp->seq)(dbp,&ckey,&dummyval,R_FIRST);
	first_flag = 1;
    }
    else
    {
	if ((flags & F_NEXT))
	{
	/*
	** This crap would be unnecessary if R_CURSOR actually worked.
	*/
	    if (flags < F_YPALL)
	    {
		(dbp->seq)(dbp,&ckey,&dummyval,R_FIRST);
		while(strncmp((char *)ikey->data,ckey.data,(int)ikey->size) ||
		    ikey->size != ckey.size)
		    (dbp->seq)(dbp,&ckey,&dummyval,R_NEXT);
	    }
	    if ((dbp->seq)(dbp,&ckey,&dummyval,R_NEXT))
		ckey.data = NULL;
#ifdef GNU_YPSERV_ARTIFACT
		free(dummyval.data);
#endif
	}
	else
	    ckey = *ikey;
    }

    if (ckey.data == NULL)
    {
	return (flags & F_NEXT) ? YP_NOMORE : YP_NOKEY;
    }

    while (1)
    {
	if ((dbp->get)(dbp,&ckey,dval,0))
	{
#ifdef GNU_YPSERV_ARTIFACT
	    /* Free key, unless it comes from the caller! */
	    if (ikey == NULL || ckey.data != ikey->data)
		free(ckey.data);
#endif
	    if (ikey && ikey->data != NULL)
	    {
		return YP_NOKEY;
	    }
	    else
		if (first_flag)
		    return YP_BADDB;
		else
		    return YP_FALSE;
	}

	if ((flags & F_ALL) || strncmp(ckey.data, "YP_", 3) != 0)
	{
	    if (okey)
		*okey = ckey;
#ifdef GNU_YPSERV_ARTIFACT
	    else if (ikey == NULL || ikey->data != ckey.data)
		free(ckey.data);
#endif
	    return YP_TRUE;
	}

	/* Free old value */
#ifdef GNU_YPSERV_ARTIFACT
	free(dval->data);
#endif
	if ((dbp->seq)(dbp,&nkey,&dummyval,R_NEXT))
		nkey.data = NULL;
#ifdef GNU_YPSERV_ARTIFACT
	free(dummyval.data);

	/* Free old key, unless it comes from the caller! */
	if (ikey == NULL || ckey.data != ikey->data)
	    free(ckey.data);
#endif
	if (ckey.data == NULL || nkey.data == NULL)
	    return YP_NOMORE;

	ckey = nkey;
    }
}


/*
** Get the DateTimeModified value for a certain map database
*/
static unsigned long get_dtm(const char *domain,
			     const char *map)
{
    struct stat sbuf;
    char buf[1025];


    strcpy(buf, domain);
    strcat(buf, "/");
    strcat(buf, map);

    if (stat(buf, &sbuf) < 0)
	return 0;
    else
	return (unsigned long) sbuf.st_mtime;
}


/*
** YP function "MATCH" implementation
*/
ypresp_val *ypproc_match_2_svc(ypreq_key *key,
			       struct svc_req *rqstp)
{
    static ypresp_val result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_match(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\t\tdomainname = \"%s\"\n",
		key->domain);
	Perror("\t\tmapname = \"%s\"\n",
		key->map);
	Perror("\t\tkeydat = \"%.*s\"\n",
		(int) key->key.keydat_len,
		key->key.keydat_val);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(key->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.stat = YP_YPERR;
	return &result;
    }

    result.val.valdat_len = 0;
    if (result.val.valdat_val)
    {
#ifdef GNU_YPSERV_ARTIFACT
	/*
	 * In general, if you malloc() data in an RPC service
	 * routine, you have to free() it the next time that
	 * routine is called since the XDR routines won't free
	 * it for you. However, in this case, we don't have to
	 * do that because the DB routines do garbage collection
	 * for us.
	 */
	free(result.val.valdat_val);
#endif
	result.val.valdat_val = NULL;
    }

    if (key->domain[0] == '\0' || key->map[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(key->domain))
	result.stat = YP_NODOM;
    else
    {
	DBT rdat, qdat;

	DB *dbp = open_database(key->domain, key->map);
	if (dbp == NULL)
	    result.stat = YP_NOMAP;
	else
	{
	    qdat.size = key->key.keydat_len;
	    qdat.data = key->key.keydat_val;

	    result.stat = read_database(dbp, &qdat, NULL, &rdat, F_ALL);

	    if (result.stat == YP_TRUE)
	    {
		result.val.valdat_len = rdat.size;
		result.val.valdat_val = rdat.data;
	    }

	    (void)(dbp->close)(dbp);
	}
    }

	if (debug_flag)
	{
	if (result.stat == YP_TRUE)
	    Perror("\t-> Value = \"%.*s\"\n",
		    (int) result.val.valdat_len,
		    result.val.valdat_val);
	else
	    Perror("\t-> Error #%d\n", result.stat);
    }


    /*
    ** Do the jive thing if we didn't find the host in the YP map
    ** and we have enabled the magic DNS lookup stuff.
    **
    ** DNS lookups are handled in a subprocess so that the server
    ** doesn't block while waiting for requests to complete.
    */
    if (result.stat != YP_TRUE && strstr(key->map, "hosts") && dns_flag)
    {
	char *cp = NULL;
	char			nbuf[YPMAXRECORD];

	if (children < MAX_CHILDREN && fork())
	{
	    children++;
	    return NULL;
	}
	else
	    forked++;

	bcopy(key->key.keydat_val, nbuf, key->key.keydat_len);
	nbuf[key->key.keydat_len] = '\0';

	if (debug_flag)
	    Perror("Doing DNS lookup of %s\n", nbuf);

	if (strcmp(key->map, "hosts.byname") == 0)
	    cp = dnsname(nbuf);
	else  if (strcmp(key->map, "hosts.byaddr") == 0)
	    cp = dnsaddr(nbuf);

	if (cp)
	{

	    if (debug_flag)
		Perror("\t-> OK (%s)\n", cp);

	    result.val.valdat_len = strlen(cp);
	    result.val.valdat_val = cp;
	    result.stat = YP_TRUE;
	}
	else
	{
	    if (debug_flag)
	    {
		Perror("\t-> Not Found\n");
		Perror("DNS lookup: %s",strerror(errno));
	    }

	    result.stat = YP_NOKEY;
	}
    }

    return &result;
}



ypresp_key_val *ypproc_first_2_svc(ypreq_nokey *key,
				   struct svc_req *rqstp)
{
    static ypresp_key_val result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_first(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\tdomainname = \"%s\"\n", key->domain);
	Perror("\tmapname = \"%s\"\n", key->map);
#if 0
	Perror("\tkeydat = \"%.*s\"\n",
		(int) key->key.keydat_len,
		key->key.keydat_val);
#endif
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(key->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.stat = YP_YPERR;
	return &result;
    }

    result.key.keydat_len = 0;
    if (result.key.keydat_val)
    {
#ifdef GNU_YPSERV_ARTIFACT
	free(result.key.keydat_val);
#endif
	result.key.keydat_val = NULL;
    }

    result.val.valdat_len = 0;
    if (result.val.valdat_val)
    {
#ifdef GNU_YPSERV_ARTIFACT
	free(result.val.valdat_val);
#endif
	result.val.valdat_val = NULL;
    }

    if (key->map[0] == '\0' || key->domain[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(key->domain))
	result.stat = YP_NODOM;
    else
    {
	DBT dkey, dval;

	DB *dbp = open_database(key->domain, key->map);
	if (dbp == NULL)
	    result.stat = YP_NOMAP;
	else
	{
	    result.stat = read_database(dbp, NULL, &dkey, &dval, 0);

	    if (result.stat == YP_TRUE)
	    {
		result.key.keydat_len = dkey.size;
		result.key.keydat_val = dkey.data;

		result.val.valdat_len = dval.size;
		result.val.valdat_val = dval.data;
	    }

	    (void)(dbp->close)(dbp);
	}
    }

    if (debug_flag)
    {
	if (result.stat == YP_TRUE)
	    Perror("\t-> Key = \"%.*s\", Value = \"%.*s\"\n",
		    (int) result.key.keydat_len,
		    result.key.keydat_val,
		    (int) result.val.valdat_len,
		    result.val.valdat_val);

	else
	    Perror("\t-> Error #%d\n", result.stat);
    }

    return &result;
}


ypresp_key_val *ypproc_next_2_svc(ypreq_key *key,
				  struct svc_req *rqstp)
{
    static ypresp_key_val result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_next(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\tdomainname = \"%s\"\n", key->domain);
	Perror("\tmapname = \"%s\"\n", key->map);
	Perror("\tkeydat = \"%.*s\"\n",
		(int) key->key.keydat_len,
		key->key.keydat_val);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(key->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.stat = YP_YPERR;
	return &result;
    }

    result.key.keydat_len = 0;
    if (result.key.keydat_val)
    {
#ifdef GNU_YPSERV_ARTIFACT
	free(result.key.keydat_val);
#endif
	result.key.keydat_val = NULL;
    }

    result.val.valdat_len = 0;
    if (result.val.valdat_val)
    {
#ifdef GNU_YPSERV_ARTIFACT
	free(result.val.valdat_val);
#endif
	result.val.valdat_val = NULL;
    }

    if (key->map[0] == '\0' || key->domain[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(key->domain))
	result.stat = YP_NODOM;
    else
    {
	DBT dkey, dval, okey;


	DB *dbp = open_database(key->domain, key->map);
	if (dbp == NULL)
	    result.stat = YP_NOMAP;
	else
	{
	    dkey.size = key->key.keydat_len;
	    dkey.data  = key->key.keydat_val;

	    result.stat = read_database(dbp, &dkey, &okey, &dval, F_NEXT);

	    if (result.stat == YP_TRUE)
	    {
		result.key.keydat_len = okey.size;
		result.key.keydat_val = okey.data;

		result.val.valdat_len = dval.size;
		result.val.valdat_val = dval.data;
	    }
	    (void)(dbp->close)(dbp);
	}
    }

    if (debug_flag)
    {
	if (result.stat == YP_TRUE)
	    Perror("\t-> Key = \"%.*s\", Value = \"%.*s\"\n",
		    (int) result.key.keydat_len,
		    result.key.keydat_val,
		    (int) result.val.valdat_len,
		    result.val.valdat_val);
	else
	    Perror("\t-> Error #%d\n", result.stat);
    }

    return &result;
}



static void print_ypmap_parms(const struct ypmap_parms *pp)
{
    Perror("\t\tdomain   = \"%s\"\n", pp->domain);
    Perror("\t\tmap      = \"%s\"\n", pp->map);
    Perror("\t\tordernum = %u\n", pp->ordernum);
    Perror("\t\tpeer     = \"%s\"\n", pp->peer);
}


/*
** Clean up after child processes signal their termination.
*/
void reapchild(sig)
int sig;
{
    int st;

    while (wait3(&st, WNOHANG, NULL) > 0)
	children--;
}

/*
** Stole the ypxfr implementation from the yps package.
*/
ypresp_xfr *ypproc_xfr_2_svc(ypreq_xfr *xfr,
			     struct svc_req *rqstp)
{
    static ypresp_xfr result;
    struct sockaddr_in *rqhost;
    char ypxfr_command[MAXPATHLEN];
    ypresp_master *mres;
    ypreq_nokey mreq;

    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_xfr_2_svc(): [From: %s:%d]\n\tmap_parms:\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	print_ypmap_parms(&xfr->map_parms);
	Perror("\t\ttransid = %u\n", xfr->transid);
	Perror("\t\tprog = %u\n", xfr->prog);
	Perror("\t\tport = %u\n", xfr->port);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request originates on a non-privileged port (< 1024),
    ** refuse it. We really only need to guard the master.passwd.*
    ** maps, but what the hell.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (ntohs(rqhost->sin_port) > 1023)
    {
	result.xfrstat = YPXFR_REFUSED;
	return &result;
    }

    mreq.domain = xfr->map_parms.domain;
    mreq.map = xfr->map_parms.map;

    mres = ypproc_master_2_svc(&mreq, rqstp);

    if (mres->stat != YP_TRUE) {
	Perror("couldn't find master for map %s@%s", xfr->map_parms.map,
						xfr->map_parms.domain);
	Perror("host at %s (%s) may be pulling my leg", xfr->map_parms.peer,
						inet_ntoa(rqhost->sin_addr));
	result.xfrstat = YPXFR_REFUSED;
	return &result;
    }

    switch(fork())
    {
	case 0:
	    {
	    char g[11], t[11], p[11];

	    sprintf (ypxfr_command, "%s/ypxfr", INSTDIR);
	    sprintf (t, "%u", xfr->transid);
	    sprintf (g, "%u", xfr->prog);
	    sprintf (p, "%u", xfr->port);
	    execl(ypxfr_command, "ypxfr", "-d", xfr->map_parms.domain, "-h",
		mres->peer, "-f", "-C", t, g,
		inet_ntoa(rqhost->sin_addr), p, xfr->map_parms.map, NULL);
	    Perror("ypxfr execl(): %s",strerror(errno));
	    exit(0);
	    }
	case -1:
	    Perror("fork(): %s",strerror(errno));
	    result.xfrstat = YPXFR_XFRERR;
	default:
	{
	    result.xfrstat = YPXFR_SUCC;
	    break;
	}
    }

    result.transid = xfr->transid;
    return &result;
}


void *ypproc_clear_2_svc(void *dummy,
			 struct svc_req *rqstp)
{
    static int foo;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    	Perror("ypproc_clear_2_svc() [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    return (void *) &foo;
}


static int ypall_close(void *data)
{
    DB *locptr;

    if (debug_flag && data == NULL)
    {
	Perror("ypall_close() called with NULL pointer.\n");
	return 0;
    }

    locptr = (DB *)data;
    (void)(locptr->close)(locptr);
    return 0;
}


static int ypall_encode(ypresp_key_val *val,
			void *data)
{
    DBT dkey, dval, okey;

    dkey.data = val->key.keydat_val;
    dkey.size = val->key.keydat_len;

    val->stat = read_database((DB *) data, &dkey, &okey, &dval, F_NEXT | F_YPALL);

    if (val->stat == YP_TRUE)
    {
	val->key.keydat_val = okey.data;
	val->key.keydat_len = okey.size;

	val->val.valdat_val = dval.data;
	val->val.valdat_len = dval.size;
    }


    return val->stat;
}


ypresp_all *ypproc_all_2_svc(ypreq_nokey *nokey,
			     struct svc_req *rqstp)
{
    static ypresp_all result;
    extern __xdr_ypall_cb_t __xdr_ypall_cb;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_all_2_svc(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\t\tdomain = \"%s\"\n", nokey->domain);
	Perror("\t\tmap = \"%s\"\n", nokey->map);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    if (children < MAX_CHILDREN && fork())
    {
	children++;
	return NULL;
    }
    else
	forked++;

    __xdr_ypall_cb.u.encode = NULL;
    __xdr_ypall_cb.u.close  = NULL;
    __xdr_ypall_cb.data = NULL;

    result.more = TRUE;

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(nokey->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.ypresp_all_u.val.stat = YP_YPERR;
	return &result;
    }

    if (nokey->map[0] == '\0' || nokey->domain[0] == '\0')
	result.ypresp_all_u.val.stat = YP_BADARGS;
    else if (!is_valid_domain(nokey->domain))
	result.ypresp_all_u.val.stat = YP_NODOM;
    else
    {
	DBT dkey, dval;

	DB *dbp = open_database(nokey->domain, nokey->map);
	if (dbp == NULL)
	    result.ypresp_all_u.val.stat = YP_NOMAP;
	else
	{
	    result.ypresp_all_u.val.stat = read_database(dbp,
							 NULL,
							 &dkey,
							 &dval,
							 0);

	    if (result.ypresp_all_u.val.stat == YP_TRUE)
	    {
		result.ypresp_all_u.val.key.keydat_len = dkey.size;
		result.ypresp_all_u.val.key.keydat_val = dkey.data;

		result.ypresp_all_u.val.val.valdat_len = dval.size;
		result.ypresp_all_u.val.val.valdat_val = dval.data;

		__xdr_ypall_cb.u.encode = ypall_encode;
		__xdr_ypall_cb.u.close  = ypall_close;
		__xdr_ypall_cb.data = (void *) dbp;

		return &result;
	    }

	    (void)(dbp->close)(dbp);
	}
    }

    return &result;
}


ypresp_master *ypproc_master_2_svc(ypreq_nokey *nokey,
				   struct svc_req *rqstp)
{
    static ypresp_master result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_master_2_svc(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\t\tdomain = \"%s\"\n", nokey->domain);
	Perror("\t\tmap = \"%s\"\n", nokey->map);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(nokey->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.stat = YP_YPERR;
	return &result;
    }

    if (result.peer)
    {
#ifdef GNU_YPSERV_ARTIFACT
	free(result.peer);
#endif
	result.peer = NULL;
    }

    if (nokey->domain[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(nokey->domain))
	result.stat = YP_NODOM;
    else
    {
	DB *dbp = open_database(nokey->domain, nokey->map);
	if (dbp == NULL)
	    result.stat = YP_NOMAP;
	else
	{
	    DBT key, val;

	    key.size = sizeof("YP_MASTER_NAME")-1;
	    key.data = "YP_MASTER_NAME";

	    if ((dbp->get)(dbp,&key,&val,0))
	    {
		/* No YP_MASTER_NAME record in map? Assume we are Master */
		static char hostbuf[1025];

		gethostname((char *)&hostbuf, sizeof(hostbuf)-1);
		Perror("Hostname: [%s]",hostbuf);
		result.peer = strdup(hostbuf);
	    }
	    else
	    {
		*(((char *)val.data)+val.size) = '\0';
		result.peer = val.data;
	    }

	    result.stat = YP_TRUE;
	    (void)(dbp->close)(dbp);
	}
    }

    if (result.peer == NULL)
	result.peer = strdup("");

    if (debug_flag)
	Perror("\t-> Peer = \"%s\"\n", result.peer);

    return &result;
}


ypresp_order *ypproc_order_2_svc(ypreq_nokey *nokey,
				 struct svc_req *rqstp)
{
    static ypresp_order result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_order_2_svc(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\t\tdomain = \"%s\"\n", nokey->domain);
	Perror("\t\tmap = \"%s\"\n", nokey->map);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    /*
    ** If this request deals with master.passwd.* and it didn't
    ** originate on a privileged port (< 1024), return a YP_YPERR.
    ** This is our half-assed way of preventing non-root users
    ** on NIS clients from getting at the real password map. Bah.
    */

    if (strstr(nokey->map, "master.passwd") != NULL &&
	ntohs(rqhost->sin_port) > 1023)
    {
	result.stat = YP_YPERR;
	return &result;
    }

    result.ordernum = 0;

    if (nokey->domain[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(nokey->domain))
	result.stat = YP_NODOM;
    else
    {
	DB *dbp = open_database(nokey->domain, nokey->map);
	if (dbp == NULL)
	    result.stat = YP_NOMAP;
	else
	{
	    DBT key, val;

	    key.size = sizeof("YP_LAST_MODIFIED")-1;
	    key.data = "YP_LAST_MODIFIED";

	    if ((dbp->get)(dbp,&key,&val,0))
	    {
		/* No YP_LAST_MODIFIED record in map? Use DTM timestamp.. */
		result.ordernum = get_dtm(nokey->domain, nokey->map);
	    }
	    else
	    {
		result.ordernum = atoi(val.data);
#ifdef GNU_YPSERV_ARTIFACT
		free(val.data);
#endif
	    }

	    result.stat = YP_TRUE;
	    (void)(dbp->close)(dbp);
	}
    }

    if (debug_flag)
	Perror("-> Order # %d\n", result.ordernum);

    return &result;
}


static void free_maplist(ypmaplist *mlp)
{
    ypmaplist *next;

    while (mlp != NULL)
    {
	next = mlp->next;
	free(mlp->map);
	free(mlp);
	mlp = next;
    }
}

static int add_maplist(ypmaplist **mlhp,
		       char *map)
{
    ypmaplist *mlp;

    if (!strncmp(map, ".", strlen(map)) || !strncmp(map, "..", strlen(map)))
	return 0;

    mlp = malloc(sizeof(*mlp));
    if (mlp == NULL)
	return -1;

    mlp->map = strdup(map);
    if (mlp->map == NULL)
    {
	free(mlp);
	return -1;
    }

    mlp->next = *mlhp;
    *mlhp = mlp;

    return 0;
}


ypresp_maplist *ypproc_maplist_2_svc(domainname *name,
				     struct svc_req *rqstp)
{
    static ypresp_maplist result;
    struct sockaddr_in *rqhost;


    rqhost = svc_getcaller(rqstp->rq_xprt);

    if (debug_flag)
    {
	Perror("ypproc_maplist_2_svc(): [From: %s:%d]\n",
		inet_ntoa(rqhost->sin_addr),
		ntohs(rqhost->sin_port));

	Perror("\t\tdomain = \"%s\"\n", *name);
    }

    if (!is_valid_host(rqhost))
    {
	if (debug_flag)
	    Perror("\t-> Ignored (not a valid source host)\n");

	return NULL;
    }

    if (result.maps)
	free_maplist(result.maps);

    result.maps = NULL;

    if ((*name)[0] == '\0')
	result.stat = YP_BADARGS;
    else if (!is_valid_domain(*name))
	result.stat = YP_NODOM;
    else
    {
	DIR *dp;
	char dirname[MAXPATHLEN];

	sprintf(dirname,"./%s",*name);
	dp = opendir(dirname);
	if (dp == NULL)
	{
	    if (debug_flag)
	    {
		Perror("%s: opendir: %s", progname,strerror(errno));
	    }

	    result.stat = YP_BADDB;
	}
	else
	{
	    struct dirent *dep;

	    while ((dep = readdir(dp)) != NULL)
		if (add_maplist(&result.maps, dep->d_name) < 0)
		{
		    result.stat = YP_YPERR;
		    break;
		}
	    closedir(dp);
	    result.stat = YP_TRUE;
	}
    }

    if (debug_flag)
    {
	if (result.stat == YP_TRUE)
	{
	    ypmaplist *p;

	    p = result.maps;
	    Perror("-> ");
	    while (p->next)
	    {
		Perror("%s,", p->map);
		p = p->next;
	    }
	    putc('\n', stderr);
	}
	else
	    Perror("\t-> Error #%d\n", result.stat);
    }

    return &result;
}
